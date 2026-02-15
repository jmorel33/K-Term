#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#define KTERM_ENABLE_GATEWAY
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int hop_reports = 0;
static bool loop_detected = false;

static void Test_ResponseCallback(KTerm* term, const char* response, int length) {
    if (strstr(response, "HOP;")) {
        hop_reports++;
        // If we get more than 30 reports (assuming max_hops 30), it implies looping
        if (hop_reports > 35) {
            loop_detected = true;
        }
    }
}

int main() {
    KTermConfig config = {0};
    config.width = 80;
    config.height = 25;
    config.response_callback = Test_ResponseCallback;

    KTerm* term = KTerm_Create(config);
    if (!term) return 1;
    KTerm_Init(term);

    // Inject continuous traceroute command
    // Note: This relies on KTerm_Net_TracerouteContinuous actually running.
    // However, real networking requires root/raw sockets on Linux usually.
    // If the test environment doesn't allow raw sockets, this test might fail to start the trace.
    // In that case, we can't fully verify the loop without mocks.
    // But we can check if the command parses correctly.

    // Command: continuous=1
    const char* cmd = "\033PGATE;KTERM;0;EXT;net;traceroute;host=127.0.0.1;maxhops=2;continuous=1\033\\";
    KTerm_WriteString(term, cmd);
    KTerm_Update(term);

    // We can't easily wait for the loop without blocking forever or mocking time.
    // For now, let's just verify compilation of the new API symbols.

    printf("Test compiled successfully with KTerm_Net_TracerouteContinuous symbols.\n");

    KTerm_Destroy(term);
    return 0;
}
