#define KTERM_NET_IMPLEMENTATION
// Define necessary structs normally provided by kterm.h if strictly needed, or include it
// Since we are compiling an example, we might need a minimal environment or rely on the build system to provide kterm.h path.
// For this example source, we assume standard layout.

#include <stdio.h>
#include <unistd.h>

#include "../kterm.h"
#include "../kt_net.h"

// Mock KTerm for standalone compilation if kterm.h dependencies are complex
// (In a real scenario, link against kterm object)

void my_mtu_callback(KTerm* term, KTermSession* session, const KTermMtuProbeResult* result, void* user_data) {
    if (result->error) {
        printf("[MTU] Error: %s\n", result->msg);
    } else if (result->done) {
        printf("[MTU] Result: Path MTU=%d, Local MTU=%d\n", result->path_mtu, result->local_mtu);
    }
}

void my_frag_callback(KTerm* term, KTermSession* session, const KTermFragTestResult* result, void* user_data) {
    if (result->error) {
        printf("[Frag] Error: %s\n", result->msg);
    } else {
        printf("[Frag] Result: Sent %d fragments, Reassembly: %s\n",
            result->fragments_sent, result->reassembly_success ? "Success" : "Failed");
    }
}

void my_ping_ext_callback(KTerm* term, KTermSession* session, const KTermPingExtResult* result, void* user_data) {
    if (result->done) {
        printf("[PingExt] Final Result:\n");
        printf("  Sent: %d, Recv: %d, Loss: %.1f%%\n", result->sent, result->received, result->loss_percent);
        printf("  RTT (ms): Min=%d, Avg=%d, Max=%d, StdDev=%d\n",
            result->min_rtt, result->avg_rtt, result->max_rtt, result->stddev_rtt);
        if (result->graph_line[0]) {
            printf("  Graph: %s\n", result->graph_line);
        }
        printf("  Histogram: 0-10ms:%d, 10-20ms:%d, 20-50ms:%d, 50-100ms:%d, >100ms:%d\n",
            result->hist_0_10, result->hist_10_20, result->hist_20_50, result->hist_50_100, result->hist_100_plus);
    }
}

int main() {
    printf("K-Term Network Diagnostics API Example\n");

    // Minimal Mock Initialization
    KTerm term = {0};
    KTermSession session = {0};
    // Link session to term if needed by implementation
    term.sessions = &session;
    session.user_data = NULL; // Net context will be created here

    KTerm_Net_Init(&term);

    // 1. MTU Probe
    printf("\n--- Starting MTU Probe (8.8.8.8) ---\n");
    if (KTerm_Net_MTUProbe(&term, &session, "8.8.8.8", true, 1000, 1500, my_mtu_callback, NULL)) {
        // Simulate event loop
        for (int i=0; i<50; i++) { // 5 seconds approx
            KTerm_Net_Process(&term);
            usleep(100000); // 100ms

            // Check if done (hacky access to internal state, or rely on callback printing)
            KTermNetSession* net = (KTermNetSession*)session.user_data;
            if (net && net->mtu_probe && net->mtu_probe->state == 5) break;
        }
    } else {
        printf("Failed to start MTU Probe.\n");
    }

    // 2. Fragmentation Test
    printf("\n--- Starting Frag Test (localhost) ---\n");
    if (KTerm_Net_FragTest(&term, &session, "127.0.0.1", 3000, 3, my_frag_callback, NULL)) {
        for (int i=0; i<30; i++) {
            KTerm_Net_Process(&term);
            usleep(100000);
            KTermNetSession* net = (KTermNetSession*)session.user_data;
            if (net && net->frag_test && net->frag_test->state == 5) break;
        }
    } else {
        printf("Failed to start Frag Test.\n");
    }

    // 3. Extended Ping
    printf("\n--- Starting Extended Ping (google.com) ---\n");
    // 5 packets, 200ms interval
    if (KTerm_Net_PingExt(&term, &session, "google.com", 5, 200, 64, true, my_ping_ext_callback, NULL)) {
        for (int i=0; i<60; i++) { // Allow enough time for 5 * 200ms + timeouts
            KTerm_Net_Process(&term);
            usleep(100000);
            KTermNetSession* net = (KTermNetSession*)session.user_data;
            if (net && net->ping_ext && net->ping_ext->state == 5) break;
        }
    } else {
        printf("Failed to start Extended Ping.\n");
    }

    // Cleanup
    // KTerm_Net_Disconnect(&term, &session); // Frees context
    return 0;
}
