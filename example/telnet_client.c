#define KTERM_IMPLEMENTATION
#include "../kterm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// --- Telnet Logic ---

// Options (RFC 854 / 857 / 858)
#define TELNET_OPT_ECHO 1
#define TELNET_OPT_SGA  3

bool on_telnet_command(KTerm* term, KTermSession* session, unsigned char command, unsigned char option) {
    (void)term;
    printf("[Telnet] Command: %d Option: %d\n", command, option);

    // Negotiate ECHO
    if (option == TELNET_OPT_ECHO) {
        if (command == KTERM_TELNET_WILL) {
            // Server wants to echo back. We agree (DO).
            // We should disable local echo.
            printf("[Telnet] Enabling Remote Echo (Server WILL ECHO)\n");
            KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_DO, TELNET_OPT_ECHO);
            // TODO: Disable local echo in KTerm session if implemented (KTERM_MODE_LOCALECHO)
            return true;
        } else if (command == KTERM_TELNET_DO) {
            // Server wants US to echo? Usually server echoes.
            // We refuse (WONT).
            KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_WONT, TELNET_OPT_ECHO);
            return true;
        }
    }

    // Negotiate Suppress Go Ahead (SGA)
    if (option == TELNET_OPT_SGA) {
        if (command == KTERM_TELNET_WILL) {
            KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_DO, TELNET_OPT_SGA);
            return true;
        }
    }

    // Default behavior will reject (DONT/WONT) anything else in kt_net.h
    return false;
}

// --- Async Callbacks ---

void on_connect(KTerm* term, KTermSession* session) {
    printf("[Client] Connected to Telnet server!\n");

    // We assume the server might initiate negotiation.
    // If we wanted to initiate, we would send WILL/DO here.
}

void on_disconnect(KTerm* term, KTermSession* session) {
    printf("[Client] Disconnected from server.\n");
    exit(0);
}

void on_data(KTerm* term, KTermSession* session, const char* data, size_t len) {
    // Debug raw data if needed
    // printf("[Client] Received %zu bytes\n", len);
}

void on_error(KTerm* term, KTermSession* session, const char* msg) {
    fprintf(stderr, "[Client] Error: %s\n", msg);
}

int main(int argc, char** argv) {
    const char* host = "towel.blinkenlights.nl"; // Classic Star Wars Telnet
    int port = 23;

    if (argc > 1) host = argv[1];
    if (argc > 2) port = atoi(argv[2]);

    printf("Starting KTerm Telnet Client connecting to %s:%d...\n", host, port);

    // 1. Initialize KTerm
    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    KTerm* term = KTerm_Create(config);
    if (!term) return 1;

    // 2. Setup Session 0
    KTermSession* session = &term->sessions[0];

    // Register Callbacks
    KTermNetCallbacks callbacks = {
        .on_connect = on_connect,
        .on_disconnect = on_disconnect,
        .on_data = on_data,
        .on_error = on_error,
        .on_telnet_command = on_telnet_command
    };
    KTerm_Net_SetCallbacks(term, session, callbacks);

    // Set Protocol to Telnet
    KTerm_Net_SetProtocol(term, session, KTERM_NET_PROTO_TELNET);

    // 3. Connect
    KTerm_Net_Connect(term, session, host, port, NULL, NULL);

    // 4. Main Loop
    printf("Running... Press Ctrl+C to exit.\n");

    // Basic loop
    while (1) {
        KTerm_Update(term);
        usleep(10000); // 10ms sleep
    }

    KTerm_Destroy(term);
    return 0;
}
