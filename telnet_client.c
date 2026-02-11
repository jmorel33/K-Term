// KTerm Telnet Client Example
// A full-featured, graphical Telnet client showcasing KTerm's networking and rendering capabilities.
//
// Features:
// - Graphical Window (via Situation) with Resizing
// - Telnet Negotiation: NAWS (Window Size), TTYPE (Terminal Type), ECHO, SGA
// - CRT Retro Effects (Toggle with F12)
// - Status Bar with Connection Info
// - Auto-connects to 'towel.blinkenlights.nl' for Star Wars fun
//
// Compile: gcc telnet_client.c -o telnet_client -lkterm -lsituation -lm

#define KTERM_IMPLEMENTATION
#include "kterm.h"

// Integrate Situation Input Adapter
#define KTERM_IO_SIT_IMPLEMENTATION
#include "kt_io_sit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// --- Configuration ---
#define DEFAULT_HOST "towel.blinkenlights.nl"
#define DEFAULT_PORT 23

// --- Global State ---
typedef struct {
    char host[256];
    int port;
    bool connected;
    bool crt_enabled;
    char status_msg[128];
    int term_width;
    int term_height;
    bool negotiation_debug;
} ClientState;

static ClientState client_state = {0};

// --- Helper Functions ---

void update_status_bar(KTerm* term, KTermSession* session) {
    // We can write to the last line or use a reserved area.
    // For this demo, we'll just overlay on the bottom line if we can,
    // or rely on the window title. Let's use the window title for status
    // to avoid messing up the ASCII art stream of the movie.

    char title[512];
    snprintf(title, sizeof(title), "K-Term Telnet: %s:%d | %s | %s | [F12] CRT",
             client_state.host, client_state.port,
             client_state.connected ? "CONNECTED" : "DISCONNECTED",
             client_state.status_msg);

    KTerm_SetWindowTitle(term, title);
}

void log_debug(const char* fmt, ...) {
    if (!client_state.negotiation_debug) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

// --- Telnet Logic ---

// Options
#define TELNET_OPT_ECHO 1
#define TELNET_OPT_SGA  3
#define TELNET_OPT_TTYPE 24
#define TELNET_OPT_NAWS 31

void send_naws(KTerm* term, KTermSession* session) {
    // RFC 1073: IAC SB NAWS <16-bit width> <16-bit height> IAC SE
    unsigned char buf[9];
    int w = client_state.term_width;
    int h = client_state.term_height;

    buf[0] = KTERM_TELNET_IAC;
    buf[1] = KTERM_TELNET_SB;
    buf[2] = TELNET_OPT_NAWS;
    buf[3] = (w >> 8) & 0xFF;
    buf[4] = w & 0xFF;
    buf[5] = (h >> 8) & 0xFF;
    buf[6] = h & 0xFF;
    buf[7] = KTERM_TELNET_IAC;
    buf[8] = KTERM_TELNET_SE;

    // Send raw packet directly to network sink
    // We can use KTerm_Net_SendPacket if framed, but for Telnet we write raw.
    // KTerm_Net_SendTelnetCommand only sends IAC CMD OPT.
    // We need to inject raw bytes.
    // Since we are inside the client, we can't easily access the internal net socket buffer
    // without exposing it. However, kt_net.h should handle this.
    // Actually, KTerm_Net currently doesn't expose a "Send Raw" for Subnegotiation easily
    // unless we use KTerm_PushInput (which goes TO terminal) or writing to the session response.

    // KTerm_QueueSessionResponse queues data to be sent TO the host.
    // This is exactly what we need.
    KTerm_QueueResponseBytes(term, (const char*)buf, 9);
    log_debug("[Telnet] Sent NAWS: %dx%d", w, h);
}

void send_ttype(KTerm* term, KTermSession* session) {
    // RFC 1091: IAC SB TTYPE IS <type> IAC SE
    // Type: KTERM
    const char* type = "XTERM-256COLOR";
    unsigned char header[] = { KTERM_TELNET_IAC, KTERM_TELNET_SB, TELNET_OPT_TTYPE, 0x00 }; // 0x00 = IS
    unsigned char footer[] = { KTERM_TELNET_IAC, KTERM_TELNET_SE };

    KTerm_QueueResponseBytes(term, (const char*)header, 4);
    KTerm_QueueResponseBytes(term, type, strlen(type));
    KTerm_QueueResponseBytes(term, (const char*)footer, 2);
    log_debug("[Telnet] Sent TTYPE: %s", type);
}

bool on_telnet_command(KTerm* term, KTermSession* session, unsigned char command, unsigned char option) {
    log_debug("[Telnet] Recv CMD: %d OPT: %d", command, option);

    if (option == TELNET_OPT_ECHO) {
        if (command == KTERM_TELNET_WILL) {
            KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_DO, TELNET_OPT_ECHO);
            session->dec_modes &= ~KTERM_MODE_LOCALECHO; // Server handles echo
            snprintf(client_state.status_msg, sizeof(client_state.status_msg), "Remote Echo");
            return true;
        } else if (command == KTERM_TELNET_WONT) {
            KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_DONT, TELNET_OPT_ECHO);
            session->dec_modes |= KTERM_MODE_LOCALECHO; // Local echo
            snprintf(client_state.status_msg, sizeof(client_state.status_msg), "Local Echo");
            return true;
        }
    }

    if (option == TELNET_OPT_SGA) {
        if (command == KTERM_TELNET_WILL) {
            KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_DO, TELNET_OPT_SGA);
            return true;
        }
    }

    if (option == TELNET_OPT_NAWS) {
        if (command == KTERM_TELNET_DO) {
            KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_WILL, TELNET_OPT_NAWS);
            send_naws(term, session);
            return true;
        }
    }

    if (option == TELNET_OPT_TTYPE) {
        if (command == KTERM_TELNET_DO) {
            KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_WILL, TELNET_OPT_TTYPE);
            return true;
        }
    }

    return false; // Let default handler reject unknown options
}

void on_telnet_sb(KTerm* term, KTermSession* session, unsigned char option, const char* data, size_t len) {
    if (option == TELNET_OPT_TTYPE && len > 0 && data[0] == 0x01) { // SEND
        send_ttype(term, session);
    }
}

// --- Net Callbacks ---

void on_connect(KTerm* term, KTermSession* session) {
    client_state.connected = true;
    snprintf(client_state.status_msg, sizeof(client_state.status_msg), "Negotiating...");
    update_status_bar(term, session);
    KTerm_WriteString(term, "\x1B[32m[KTerm] Connected! Handshaking...\x1B[0m\r\n");
}

void on_disconnect(KTerm* term, KTermSession* session) {
    client_state.connected = false;
    snprintf(client_state.status_msg, sizeof(client_state.status_msg), "Disconnected");
    update_status_bar(term, session);
    KTerm_WriteString(term, "\r\n\x1B[31m[KTerm] Connection Closed.\x1B[0m\r\n");
}

void on_error(KTerm* term, KTermSession* session, const char* msg) {
    fprintf(stderr, "Error: %s\n", msg);
    char buf[256];
    snprintf(buf, sizeof(buf), "\r\n\x1B[31m[Error] %s\x1B[0m\r\n", msg);
    KTerm_WriteString(term, buf);
}

// --- Main ---

int main(int argc, char** argv) {
    // Defaults
    strncpy(client_state.host, DEFAULT_HOST, sizeof(client_state.host));
    client_state.port = DEFAULT_PORT;
    client_state.crt_enabled = true; // Enable effects by default for "Wow"
    client_state.negotiation_debug = false;

    // Args
    if (argc > 1) strncpy(client_state.host, argv[1], sizeof(client_state.host));
    if (argc > 2) client_state.port = atoi(argv[2]);

    // 1. Initialize Window (Situation)
    KTermInitInfo init_info = {
        .window_width = 1024,
        .window_height = 768,
        .window_title = "K-Term Telnet Client",
        .initial_active_window_flags = KTERM_WINDOW_STATE_RESIZABLE | KTERM_WINDOW_STATE_VSYNC_HINT
    };

    if (KTerm_Platform_Init(0, NULL, &init_info) != KTERM_SUCCESS) {
        fprintf(stderr, "Failed to initialize platform.\n");
        return 1;
    }
    KTerm_SetTargetFPS(60);

    // 2. Initialize Terminal
    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    KTerm* term = KTerm_Create(config);
    if (!term) return 1;

    // CRT Effects Setup
    term->visual_effects.flags = SHADER_FLAG_CRT | SHADER_FLAG_SCANLINE | SHADER_FLAG_GLOW;
    term->visual_effects.curvature = 0.1f;
    term->visual_effects.scanline_intensity = 0.3f;
    term->visual_effects.glow_intensity = 0.4f;

    // 3. Network Setup
    KTerm_Net_Init(term);
    KTermSession* session = &term->sessions[0];

    // Store initial size
    client_state.term_width = config.width;
    client_state.term_height = config.height;

    KTermNetCallbacks callbacks = {
        .on_connect = on_connect,
        .on_disconnect = on_disconnect,
        .on_error = on_error,
        .on_telnet_command = on_telnet_command,
        .on_telnet_sb = on_telnet_sb
    };
    KTerm_Net_SetCallbacks(term, session, callbacks);
    KTerm_Net_SetProtocol(term, session, KTERM_NET_PROTO_TELNET);

    // Connect
    char msg[256];
    snprintf(msg, sizeof(msg), "Connecting to %s:%d...\r\n", client_state.host, client_state.port);
    KTerm_WriteString(term, msg);
    update_status_bar(term, session);

    KTerm_Net_Connect(term, session, client_state.host, client_state.port, NULL, NULL);

    // 4. Main Loop
    while (!WindowShouldClose()) {

        // Handle Global Input
        if (SituationIsKeyPressed(SIT_KEY_F12)) {
            client_state.crt_enabled = !client_state.crt_enabled;
            if (client_state.crt_enabled) {
                term->visual_effects.flags = SHADER_FLAG_CRT | SHADER_FLAG_SCANLINE | SHADER_FLAG_GLOW;
                KTerm_WriteString(term, "\x1B[33m[SYS] CRT Effects ON\x1B[0m\r\n");
            } else {
                term->visual_effects.flags = 0;
                KTerm_WriteString(term, "\x1B[33m[SYS] CRT Effects OFF\x1B[0m\r\n");
            }
            update_status_bar(term, session);
        }

        // Handle Resize
        if (SituationIsWindowResized()) {
            int w, h;
            SituationGetWindowSize(&w, &h);
            // Calculate new cols/rows based on font size (approx 10x20 for 2x scale or similar)
            // KTerm default scale is 1, font 10x10.
            int cols = w / (10 * DEFAULT_WINDOW_SCALE);
            int rows = h / (10 * DEFAULT_WINDOW_SCALE);

            if (cols != client_state.term_width || rows != client_state.term_height) {
                client_state.term_width = cols;
                client_state.term_height = rows;
                KTerm_Resize(term, cols, rows);

                // Send NAWS update if connected
                if (client_state.connected) {
                    send_naws(term, session);
                }
            }
        }

        // Process Input
        KTermSit_ProcessInput(term);

        // Update Terminal (Net Processing, Animation, etc)
        KTerm_Update(term);

        // Render
        KTerm_BeginFrame();
            ClearBackground((Color){0, 0, 0, 255});
            KTerm_Draw(term);
        KTerm_EndFrame();
    }

    // Cleanup
    KTerm_Net_Disconnect(term, session);
    KTerm_Destroy(term);
    KTerm_Platform_Shutdown();

    return 0;
}
