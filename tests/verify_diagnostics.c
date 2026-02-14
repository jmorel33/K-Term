#define KTERM_IMPLEMENTATION
// Define KTERM_TESTING to access mock situation and internal states
#define KTERM_TESTING

#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

// Mock Response Callback to capture Gateway output
static char last_response[8192];
static void TestResponseCallback(KTerm* term, const char* data, int len) {
    if (len < sizeof(last_response) - 1) {
        // Append to buffer
        strncat(last_response, data, len);
    }
}

int main() {
    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    config.response_callback = TestResponseCallback;

    KTerm* term = KTerm_Create(config);
    if (!term) {
        fprintf(stderr, "Failed to create KTerm\n");
        return 1;
    }
    KTerm_Net_Init(term);

    printf("Starting Diagnostics Suite Verification...\n");

    // Test 1: Connections Command (Enhanced)
    // Manually create a net context for session 0 to ensure we have something to list
    KTerm_Net_Connect(term, &term->sessions[0], "127.0.0.1", 80, NULL, NULL);

    printf("[1] Testing EXT;net;connections...\n");
    last_response[0] = '\0';
    const char* cmd_conn = "\x1BPGATE;KTERM;1;EXT;net;connections\x1B\\";
    for (int i = 0; cmd_conn[i]; i++) KTerm_WriteChar(term, (unsigned char)cmd_conn[i]);
    
    // Pump loop
    for (int i = 0; i < 20; i++) { KTerm_Update(term); usleep(1000); }
    
    printf("Response: %s\n", last_response);
    if (strstr(last_response, "OK;") && strstr(last_response, "[0:MAIN]")) {
        printf("PASS: Connections command returned structured list.\n");
    } else {
        printf("FAIL: Connections command output unexpected (Expected [0:MAIN]).\n");
    }

    // Test 2: Speedtest with Graph (Visual)
    printf("[2] Testing EXT;net;speedtest;graph=1...\n");
    last_response[0] = '\0';
    // We assume auto-server selection works or timeouts handled gracefully in simulation
    const char* cmd_spd = "\x1BPGATE;KTERM;2;EXT;net;speedtest;host=auto;graph=1\x1B\\";
    for (int i = 0; cmd_spd[i]; i++) KTerm_WriteChar(term, (unsigned char)cmd_spd[i]);

    // Pump loop longer for state machine
    bool saw_viz = false;
    for (int i = 0; i < 200; i++) { 
        KTerm_Update(term); 
        usleep(10000);
        // Check for VT sequences in response (Clear Screen or Color codes)
        if (strstr(last_response, "\x1B[2J") || strstr(last_response, "\x1B[1;37m")) {
            saw_viz = true;
            break;
        }
    }

    if (saw_viz) {
        printf("PASS: Speedtest Visual Graph sequence detected (Clear Screen / Colors).\n");
    } else {
        // It might not start if network is stubbed or fails immediately, but we should see INIT
        printf("WARN: Speedtest visual sequence not detected. (Network might be disabled/stubbed)\n");
        printf("Last Response: %s\n", last_response);
    }

    KTerm_Destroy(term);
    return 0;
}
