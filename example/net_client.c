#define KTERM_IMPLEMENTATION
// KTERM_NET_IMPLEMENTATION is automatically defined by kterm.h when KTERM_IMPLEMENTATION is used.
#include "../kterm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// --- Async Callbacks ---

void on_connect(KTerm* term, KTermSession* session) {
    printf("[Client] Connected to server!\n");
    // Send a greeting
    KTerm_QueueResponse(term, "Hello from KTerm Client!\r\n");
}

void on_disconnect(KTerm* term, KTermSession* session) {
    printf("[Client] Disconnected from server.\n");
    exit(0); // Exit example on disconnect
}

void on_data(KTerm* term, KTermSession* session, const char* data, size_t len) {
    // In a real app, this data is automatically piped to KTerm's input.
    // This callback is for inspection or debugging.
    printf("[Client] Received %zu bytes\n", len);
}

void on_error(KTerm* term, KTermSession* session, const char* msg) {
    fprintf(stderr, "[Client] Error: %s\n", msg);
}

int main(int argc, char** argv) {
    const char* host = "127.0.0.1";
    int port = 9090;

    if (argc > 1) host = argv[1];
    if (argc > 2) port = atoi(argv[2]);

    printf("Starting KTerm Network Client connecting to %s:%d...\n", host, port);

    // 1. Initialize KTerm
    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    KTerm* term = KTerm_Create(config);
    if (!term) return 1;

    // 2. Initialize Networking
    // Already called in KTerm_Create -> KTerm_Init -> KTerm_Net_Init
    // But we can configure it further.

    // 3. Setup Session 0
    KTermSession* session = &term->sessions[0];

    // Register Callbacks
    KTermNetCallbacks callbacks = {
        .on_connect = on_connect,
        .on_disconnect = on_disconnect,
        .on_data = on_data,
        .on_error = on_error
    };
    KTerm_Net_SetCallbacks(term, session, callbacks);

    // Set Protocol (Default is RAW, but here's how to change it)
    // KTerm_Net_SetProtocol(term, session, KTERM_NET_PROTO_FRAMED);

    // 4. Connect
    KTerm_Net_Connect(term, session, host, port, "user", NULL);

    // 5. Main Loop
    printf("Running... Press Ctrl+C to exit.\n");

    // Simulate user input loop (just keep running)
    while (1) {
        KTerm_Update(term); // Processes Network I/O and Terminal Logic
        usleep(10000); // 10ms sleep
    }

    KTerm_Destroy(term);
    return 0;
}
