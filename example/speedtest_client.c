/*
 * KTerm Speedtest Client Example
 * ------------------------------
 * A dedicated speedtest utility using KTerm's networking stack.
 *
 * Features:
 * - Latency & Jitter Measurement (via ICMP/TCP Probe)
 * - Multi-Stream Download Throughput Test (TCP Stream x4)
 * - Multi-Stream Upload Throughput Test (TCP Stream x4)
 * - Real-time Visualization (Graphs/Bars on Terminal Grid)
 *
 * Usage:
 *   ./speedtest_client [host] [port]
 *
 * Default Host: speedtest.tele2.net (Public speedtest server)
 */

#define _POSIX_C_SOURCE 200809L // For clock_gettime and getaddrinfo

#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"

// Integrate Situation Input Adapter
#define KTERM_IO_SIT_IMPLEMENTATION
#include "../kt_io_sit.h"

#define KTERM_NET_IMPLEMENTATION
#include "../kt_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

// --- Configuration ---
#define DEFAULT_HOST "speedtest.tele2.net"
#define DEFAULT_PORT 80
#define TEST_DURATION_SEC 5.0
#define NUM_STREAMS 4

// --- State Machine ---
typedef enum {
    STATE_IDLE = 0,
    STATE_LATENCY,
    STATE_LATENCY_DONE,
    STATE_DOWNLOAD_INIT,
    STATE_DOWNLOAD_CONNECTING,
    STATE_DOWNLOAD_RUNNING,
    STATE_DOWNLOAD_DONE,
    STATE_UPLOAD_INIT,
    STATE_UPLOAD_CONNECTING,
    STATE_UPLOAD_RUNNING,
    STATE_UPLOAD_DONE,
    STATE_FINISHED
} TestState;

typedef struct {
    uint64_t bytes;
    bool connected;
} StreamContext;

typedef struct {
    TestState state;
    char host[256];
    int port;

    // Latency
    double latency_min;
    double latency_avg;
    double latency_max;
    double jitter;
    bool latency_finished;

    // Download
    double dl_speed_mbps;
    double dl_start_time;
    double dl_progress; // 0.0 - 1.0
    StreamContext dl_streams[NUM_STREAMS];
    int dl_connected_count;

    // Upload
    double ul_speed_mbps;
    double ul_start_time;
    double ul_progress; // 0.0 - 1.0
    StreamContext ul_streams[NUM_STREAMS];
    int ul_connected_count;

    // UI
    char status_msg[256];

} SpeedtestContext;

static SpeedtestContext ctx = {0};

// --- Helpers ---

static double GetTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int GetSessionIndex(KTerm* term, KTermSession* session) {
    for (int i=0; i<MAX_SESSIONS; i++) {
        if (&term->sessions[i] == session) return i;
    }
    return -1;
}

// --- Callbacks ---

static void cb_latency_result(KTerm* term, KTermSession* session, const ResponseTimeResult* result, void* user_data) {
    (void)term; (void)session; (void)user_data;
    if (result->received > 0) {
        ctx.latency_min = result->min_rtt_ms;
        ctx.latency_avg = result->avg_rtt_ms;
        ctx.latency_max = result->max_rtt_ms;
        ctx.jitter = result->jitter_ms;
        snprintf(ctx.status_msg, sizeof(ctx.status_msg), "Latency: %.1f ms | Jitter: %.1f ms", ctx.latency_avg, ctx.jitter);
    } else {
        snprintf(ctx.status_msg, sizeof(ctx.status_msg), "Latency Test Failed (Packet Loss)");
    }
    ctx.state = STATE_LATENCY_DONE;
}

static bool cb_dl_on_data(KTerm* term, KTermSession* session, const char* data, size_t len) {
    int idx = GetSessionIndex(term, session);
    if (idx >= 0 && idx < NUM_STREAMS) {
        ctx.dl_streams[idx].bytes += len;
    }
    (void)data;
    return true; // Consumed
}

static void cb_dl_on_connect(KTerm* term, KTermSession* session) {
    int idx = GetSessionIndex(term, session);
    if (idx >= 0 && idx < NUM_STREAMS) {
        if (!ctx.dl_streams[idx].connected) {
            ctx.dl_streams[idx].connected = true;
            ctx.dl_connected_count++;

            // Send HTTP GET
            const char* req = "GET /100MB.zip HTTP/1.1\r\nHost: speedtest.tele2.net\r\nConnection: close\r\n\r\n";
            int sock = (int)KTerm_Net_GetSocket(term, session);
            if (sock >= 0) {
                send(sock, req, strlen(req), 0);
            }
        }
    }

    if (ctx.dl_connected_count == NUM_STREAMS && ctx.state == STATE_DOWNLOAD_CONNECTING) {
        snprintf(ctx.status_msg, sizeof(ctx.status_msg), "Download: All streams connected.");
        ctx.dl_start_time = GetTime();
        ctx.state = STATE_DOWNLOAD_RUNNING;
    }
}

static void cb_ul_on_connect(KTerm* term, KTermSession* session) {
    int idx = GetSessionIndex(term, session);
    if (idx >= 0 && idx < NUM_STREAMS) {
        if (!ctx.ul_streams[idx].connected) {
            ctx.ul_streams[idx].connected = true;
            ctx.ul_connected_count++;

            // Send Header
            const char* head = "POST /upload.php HTTP/1.1\r\nHost: speedtest.tele2.net\r\nContent-Length: 104857600\r\n\r\n";
            int sock = (int)KTerm_Net_GetSocket(term, session);
            if (sock >= 0) {
                send(sock, head, strlen(head), 0);
            }
        }
    }

    if (ctx.ul_connected_count == NUM_STREAMS && ctx.state == STATE_UPLOAD_CONNECTING) {
        snprintf(ctx.status_msg, sizeof(ctx.status_msg), "Upload: All streams connected.");
        ctx.ul_start_time = GetTime();
        ctx.state = STATE_UPLOAD_RUNNING;
    }
}

static bool cb_ul_on_data(KTerm* term, KTermSession* session, const char* data, size_t len) {
    (void)term; (void)session; (void)data; (void)len;
    // Ignore server response
    return true;
}

// --- Drawing ---

static void SetCursor(KTerm* term, int x, int y) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1B[%d;%dH", y + 1, x + 1);
    KTerm_WriteString(term, buf);
}

static void DrawProgressBar(KTerm* term, int x, int y, int w, double progress, const char* label, const char* value_str, int color_idx) {
    char buf[256];

    // Label
    SetCursor(term, x, y);
    // Color
    char color_seq[32]; snprintf(color_seq, sizeof(color_seq), "\x1B[38;5;%dm", color_idx);
    KTerm_WriteString(term, color_seq);
    KTerm_WriteString(term, label);
    KTerm_WriteString(term, "\x1B[0m");

    // Bar
    SetCursor(term, x, y + 1);
    KTerm_WriteString(term, "[");
    int fill = (int)((w - 2) * progress);
    for (int i=0; i < w - 2; i++) {
        if (i < fill) KTerm_WriteString(term, "=");
        else if (i == fill) KTerm_WriteString(term, ">");
        else KTerm_WriteString(term, " ");
    }
    KTerm_WriteString(term, "]");

    // Value
    if (value_str) {
        snprintf(buf, sizeof(buf), " %s", value_str);
        KTerm_WriteString(term, buf);
    }
}

static void DrawDashboard(KTerm* term) {
    // Basic clearing handled by main loop clear screen logic or partial updates
    // Here we just overwrite

    // Title
    SetCursor(term, 2, 1);
    KTerm_WriteString(term, "\x1B[1;37mK-TERM SPEEDTEST UTILITY (Multi-Stream)\x1B[0m");
    SetCursor(term, 2, 2);
    char sub[512]; snprintf(sub, sizeof(sub), "Target: %s:%d | Streams: %d", ctx.host, ctx.port, NUM_STREAMS);
    KTerm_WriteString(term, sub);

    // Status
    SetCursor(term, 2, 4);
    KTerm_WriteString(term, "\x1B[KStatus: "); // Clear line
    KTerm_WriteString(term, ctx.status_msg);

    // Results Box
    // Latency
    char lat_str[64];
    if (ctx.state >= STATE_LATENCY_DONE) snprintf(lat_str, sizeof(lat_str), "%.1f ms (J: %.1f)", ctx.latency_avg, ctx.jitter);
    else strcpy(lat_str, "Testing...");
    DrawProgressBar(term, 2, 6, 40, (ctx.state >= STATE_LATENCY_DONE) ? 1.0 : 0.0, "1. Latency", lat_str, 14); // Cyan

    // Download
    char dl_str[64];
    if (ctx.state >= STATE_DOWNLOAD_RUNNING) snprintf(dl_str, sizeof(dl_str), "%.2f Mbps", ctx.dl_speed_mbps);
    else strcpy(dl_str, "Waiting...");
    DrawProgressBar(term, 2, 9, 40, ctx.dl_progress, "2. Download", dl_str, 10); // Green

    // Upload
    char ul_str[64];
    if (ctx.state >= STATE_UPLOAD_RUNNING) snprintf(ul_str, sizeof(ul_str), "%.2f Mbps", ctx.ul_speed_mbps);
    else strcpy(ul_str, "Waiting...");
    DrawProgressBar(term, 2, 12, 40, ctx.ul_progress, "3. Upload", ul_str, 12); // Blue

    // Footer
    SetCursor(term, 2, 15);
    if (ctx.state == STATE_FINISHED) {
        KTerm_WriteString(term, "\x1B[32mTEST COMPLETE. Press Ctrl+C to exit.\x1B[0m");
    }
}

// --- Main ---

int main(int argc, char** argv) {
    strncpy(ctx.host, DEFAULT_HOST, sizeof(ctx.host));
    ctx.port = DEFAULT_PORT;

    if (argc > 1) {
        strncpy(ctx.host, argv[1], sizeof(ctx.host) - 1);
        ctx.host[sizeof(ctx.host) - 1] = '\0';
    }
    if (argc > 2) ctx.port = atoi(argv[2]);

    // Init KTerm
    KTermInitInfo init_info = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "K-Term Speedtest",
        .initial_active_window_flags = KTERM_WINDOW_STATE_RESIZABLE
    };

    if (KTerm_Platform_Init(0, NULL, &init_info) != KTERM_SUCCESS) {
        fprintf(stderr, "Failed to init platform\n");
        return 1;
    }

    KTermConfig config = {0};
    config.width = 80; config.height = 24;
    KTerm* term = KTerm_Create(config);
    if (!term) return 1;

    // Enable Gateway for manual testing via console if needed
    // KTerm_EnableGateway(term, true);

    KTerm_Net_Init(term);

    // Ensure sessions are initialized (KTerm_Create does it, but we use them explicitly)
    // Sessions 0-3 are available.

    ctx.state = STATE_IDLE;
    KTerm_WriteString(term, "\x1B[2J"); // Clear Screen Once

    // --- Loop ---
    int frames = 0;
    // For testing (headless), terminate after a while if not finished
    int max_frames = 10000;

    while (!WindowShouldClose() && frames < max_frames) {
        frames++;
        KTerm_Net_Process(term);

        // State Machine
        switch (ctx.state) {
            case STATE_IDLE:
                snprintf(ctx.status_msg, sizeof(ctx.status_msg), "Starting Latency Test...");
                ctx.state = STATE_LATENCY;
                if (!KTerm_Net_ResponseTime(term, &term->sessions[0], ctx.host, 10, 200, 2000, cb_latency_result, NULL)) {
                    snprintf(ctx.status_msg, sizeof(ctx.status_msg), "Failed to start Latency Test (Check Root/Permissions)");
                    ctx.state = STATE_DOWNLOAD_INIT;
                }
                break;

            case STATE_LATENCY:
                break;

            case STATE_LATENCY_DONE:
                ctx.state = STATE_DOWNLOAD_INIT;
                break;

            case STATE_DOWNLOAD_INIT:
                snprintf(ctx.status_msg, sizeof(ctx.status_msg), "Connecting for Download (%d streams)...", NUM_STREAMS);

                ctx.dl_connected_count = 0;
                memset(ctx.dl_streams, 0, sizeof(ctx.dl_streams));

                KTermNetCallbacks dl_cb = {0};
                dl_cb.on_connect = cb_dl_on_connect;
                dl_cb.on_data = cb_dl_on_data;

                for(int i=0; i<NUM_STREAMS; i++) {
                    KTerm_Net_SetCallbacks(term, &term->sessions[i], dl_cb);
                    KTerm_Net_Connect(term, &term->sessions[i], ctx.host, ctx.port, "", "");
                }

                ctx.dl_progress = 0.0;
                ctx.state = STATE_DOWNLOAD_CONNECTING;
                break;

            case STATE_DOWNLOAD_CONNECTING:
                // Wait for all connected or timeout
                if (frames > 2000 && ctx.dl_connected_count < NUM_STREAMS) {
                     if (ctx.dl_connected_count == 0) {
                         snprintf(ctx.status_msg, sizeof(ctx.status_msg), "Connection Timeout");
                         ctx.state = STATE_FINISHED;
                     } else {
                         // Partial start
                         snprintf(ctx.status_msg, sizeof(ctx.status_msg), "Starting with %d streams...", ctx.dl_connected_count);
                         ctx.dl_start_time = GetTime();
                         ctx.state = STATE_DOWNLOAD_RUNNING;
                     }
                }
                break;

            case STATE_DOWNLOAD_RUNNING:
                // Monitor time
                {
                    double now = GetTime();
                    double elapsed = now - ctx.dl_start_time;

                    uint64_t total_bytes = 0;
                    for(int i=0; i<NUM_STREAMS; i++) total_bytes += ctx.dl_streams[i].bytes;

                    if (elapsed > 0) {
                        double bits = (double)total_bytes * 8.0;
                        ctx.dl_speed_mbps = (bits / elapsed) / 1000000.0;
                    }

                    if (elapsed >= TEST_DURATION_SEC) {
                        ctx.state = STATE_DOWNLOAD_DONE;
                        for(int i=0; i<NUM_STREAMS; i++) KTerm_Net_Disconnect(term, &term->sessions[i]);
                    } else {
                        ctx.dl_progress = elapsed / TEST_DURATION_SEC;
                    }
                }
                break;

            case STATE_DOWNLOAD_DONE:
                ctx.dl_progress = 1.0;
                ctx.state = STATE_UPLOAD_INIT;
                break;

            case STATE_UPLOAD_INIT:
                snprintf(ctx.status_msg, sizeof(ctx.status_msg), "Connecting for Upload (%d streams)...", NUM_STREAMS);

                ctx.ul_connected_count = 0;
                memset(ctx.ul_streams, 0, sizeof(ctx.ul_streams));

                KTermNetCallbacks ul_cb = {0};
                ul_cb.on_connect = cb_ul_on_connect;
                ul_cb.on_data = cb_ul_on_data;

                for(int i=0; i<NUM_STREAMS; i++) {
                    KTerm_Net_SetCallbacks(term, &term->sessions[i], ul_cb);
                    KTerm_Net_Connect(term, &term->sessions[i], ctx.host, ctx.port, "", "");
                }

                ctx.ul_progress = 0.0;
                ctx.state = STATE_UPLOAD_CONNECTING;
                break;

            case STATE_UPLOAD_CONNECTING:
                // Wait
                if (frames > 4000 && ctx.ul_connected_count < NUM_STREAMS) {
                     if (ctx.ul_connected_count == 0) {
                         snprintf(ctx.status_msg, sizeof(ctx.status_msg), "Connection Timeout");
                         ctx.state = STATE_FINISHED;
                     } else {
                         // Partial start
                         snprintf(ctx.status_msg, sizeof(ctx.status_msg), "Starting with %d streams...", ctx.ul_connected_count);
                         ctx.ul_start_time = GetTime();
                         ctx.state = STATE_UPLOAD_RUNNING;
                     }
                }
                break;

            case STATE_UPLOAD_RUNNING:
                // Pump data
                {
                    uint64_t total_bytes = 0;

                    for(int i=0; i<NUM_STREAMS; i++) {
                        if (!ctx.ul_streams[i].connected) continue;

                        int sock = (int)KTerm_Net_GetSocket(term, &term->sessions[i]);
                        if (sock >= 0) {
                            char chunk[16384];
                            memset(chunk, 'X', sizeof(chunk));

                            int burst = 0;
                            int limit = 256 * 1024; // 256KB per frame per stream

                            while(burst < limit) {
                                 int sent = send(sock, chunk, sizeof(chunk), MSG_DONTWAIT);
                                 if (sent > 0) {
                                     ctx.ul_streams[i].bytes += sent;
                                     burst += sent;
                                 } else {
                                     break;
                                 }
                            }
                        }
                        total_bytes += ctx.ul_streams[i].bytes;
                    }

                    double now = GetTime();
                    double elapsed = now - ctx.ul_start_time;

                    if (elapsed > 0) {
                        double bits = (double)total_bytes * 8.0;
                        ctx.ul_speed_mbps = (bits / elapsed) / 1000000.0;
                    }

                    if (elapsed >= TEST_DURATION_SEC) {
                        ctx.state = STATE_UPLOAD_DONE;
                        for(int i=0; i<NUM_STREAMS; i++) KTerm_Net_Disconnect(term, &term->sessions[i]);
                    } else {
                        ctx.ul_progress = elapsed / TEST_DURATION_SEC;
                    }
                }
                break;

            case STATE_UPLOAD_DONE:
                ctx.ul_progress = 1.0;
                snprintf(ctx.status_msg, sizeof(ctx.status_msg), "Tests Completed.");
                ctx.state = STATE_FINISHED;
                break;

            case STATE_FINISHED:
                break;
        }

        KTermSit_ProcessInput(term);

        // Draw Interface
        DrawDashboard(term);

        KTerm_Update(term);

        KTerm_BeginFrame();
            ClearBackground((Color){10, 10, 20, 255}); // Dark Blue-ish bg
            KTerm_Draw(term);
        KTerm_EndFrame();
    }

    KTerm_Destroy(term);
    KTerm_Platform_Shutdown();
    return 0;
}
