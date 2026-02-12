#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
// Mock Situation to avoid linking dependencies
#include "../mock_situation.h"
#include "../kterm.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

void my_response_callback(KTerm* term, const char* response, int length) {
    // Print response to stdout
    // Check if it's a Gateway response
    // Response might be chunked or full escape sequence
    // traceroute response: ESC P GATE ; KTERM ; 123 ; TRACEROUTE ; ... ST

    // Just dump it visibly
    printf("[Response] ");
    for(int i=0; i<length; i++) {
        if (response[i] == '\x1B') printf("\\e");
        else if (response[i] == '\\') printf("\\");
        else if (response[i] >= 32 && response[i] <= 126) putchar(response[i]);
        else printf(".");
    }
    printf("\n");
    fflush(stdout);
}

int main() {
    printf("Starting Traceroute Verification...\n");

    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    config.response_callback = my_response_callback;

    KTerm* term = KTerm_Create(config);
    if (!term) {
        fprintf(stderr, "Failed to create KTerm\n");
        return 1;
    }

    KTerm_Net_Init(term);
    KTermSession* session = &term->sessions[0];

    // Simulate Gateway Command
    // ID="TEST1", Command="EXT", Params="net;traceroute;host=8.8.8.8;maxhops=3;timeout=1000"
    // Note: host=8.8.8.8 might timeout or give stars in container, but it should try.
    printf("Sending Gateway Command...\n");
    KTerm_GatewayProcess(term, session, "KTERM", "TEST1", "EXT", "net;traceroute;host=8.8.8.8;maxhops=3;timeout=1000");

    // Loop for 5 seconds
    printf("Processing Network Events...\n");
    for (int i = 0; i < 50; i++) {
        KTerm_Net_Process(term);

        // Also flush session responses from ring buffer to callback
        // KTerm_Update does this, but we are headless and don't want full Update logic overhead/dependencies if possible.
        // But KTerm_Update is where response ring is drained.
        // Let's manually drain it or call KTerm_Update?
        // KTerm_Update calls KTerm_Net_Process.
        // Let's call KTerm_Update. It uses mock situation so it should be fine.
        KTerm_Update(term);

        usleep(100000); // 100ms
    }

    KTerm_Destroy(term);
    printf("Done.\n");
    return 0;
}
