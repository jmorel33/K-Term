#define KTERM_IMPLEMENTATION
#define KTERM_NET_IMPLEMENTATION
#define KTERM_TESTING

#include "../kterm.h"
#include "../kt_net.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

// Mock Response Callback
static void MockResponseCallback(KTerm* term, const char* response, int length) {
    (void)term; (void)response; (void)length;
}

// Simple TCP Echo Server Thread
// Binds to localhost on a random port and echos data back
static int server_port = 0;
static volatile bool server_running = true;

void* tcp_echo_server(void* arg) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        return NULL;
    }

    // Bind to random port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = 0; // Random

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return NULL;
    }

    // Get the assigned port
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(server_fd, (struct sockaddr *)&sin, &len) == -1) {
        perror("getsockname");
        return NULL;
    }
    server_port = ntohs(sin.sin_port);
    printf("[TestServer] Listening on port %d\n", server_port);

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        return NULL;
    }

    // Accept loop (single connection for test)
    if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        return NULL;
    }

    printf("[TestServer] Accepted connection\n");

    while (server_running) {
        int valread = read(new_socket, buffer, 1024);
        if (valread > 0) {
            // Echo back
            send(new_socket, buffer, valread, 0);
        } else if (valread == 0) {
            break; // Closed
        }
    }

    close(new_socket);
    close(server_fd);
    return NULL;
}

int main() {
    printf("Starting Real Networking test...\n");

    // 1. Start Echo Server
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, tcp_echo_server, NULL);

    // Wait for server to start
    while (server_port == 0) usleep(1000);

    // 2. Initialize KTerm
    KTermConfig config = {0};
    config.response_callback = MockResponseCallback;
    KTerm* term = KTerm_Create(config);
    if (!term) return 1;

    KTerm_Net_Init(term);

    // 3. Connect via Gateway
    char connect_cmd[128];
    // Use 127.0.0.1 to match AF_INET server and avoid IPv6 resolution issues in test env
    snprintf(connect_cmd, sizeof(connect_cmd), "\x1BPGATE;KTERM;1;EXT;ssh;connect;127.0.0.1:%d\x1B\\", server_port);
    printf("Sending Connect Command: %s\n", connect_cmd);

    for (int i = 0; connect_cmd[i] != '\0'; i++) {
        KTerm_ProcessChar(term, GET_SESSION(term), (unsigned char)connect_cmd[i]);
    }

    // 4. Drive Logic Loop until Connected
    printf("Waiting for connection...\n");
    KTermNetSession* net = NULL;
    int ticks = 0;
    while (ticks < 100) { // 100 ticks ~ 1 sec? depends on sleep
        KTerm_Net_Process(term);
        KTerm_Update(term);

        net = (KTermNetSession*)term->sessions[0].user_data;
        if (net && net->state == KTERM_NET_STATE_CONNECTED) break;

        usleep(10000); // 10ms
        ticks++;
    }

    if (net && net->state == KTERM_NET_STATE_CONNECTED) {
        printf("PASS: Connected to local TCP server.\n");
    } else {
        printf("FAIL: Connection timeout. State: %d\n", net ? net->state : -1);
        server_running = false;
        return 1;
    }

    // 5. Verify Data Transmission (Round Trip)
    // Send "HELLO" from KTerm -> Server -> Echo -> KTerm Screen
    printf("Sending Data...\n");

    // Inject into Sink (Simulate user typing)
    const char* msg = "HELLO";
    KTerm_QueueResponse(term, msg); // Queues to output ring

    // Drive loop to flush Tx and read Rx
    bool found_echo = false;
    ticks = 0;
    while (ticks < 50) {
        KTerm_Net_Process(term); // Sends TX, Reads RX
        KTerm_Update(term); // Flushes Output Ring to Sink

        // Check Screen
        char screen_content[1024] = {0};
        int pos = 0;
        KTermSession* s = &term->sessions[0];
        // Scan last few lines
        for(int y=0; y < s->rows && pos < 1000; y++) {
            for(int x=0; x < s->cols; x++) {
                if (pos >= 1023) break; // Prevent overflow
                EnhancedTermChar* c = KTerm_GetCell(term, x, y);
                if (c && c->ch >= 32 && c->ch < 127) {
                    screen_content[pos++] = (char)c->ch;
                }
            }
        }
        screen_content[pos] = '\0';

        if (strstr(screen_content, "HELLO")) {
            found_echo = true;
            break;
        }

        usleep(10000);
        ticks++;
    }

    if (found_echo) {
        printf("PASS: Data Round-Trip Verified (Echo received).\n");
    } else {
        printf("FAIL: Echo not received.\n");
        return 1;
    }

    // Cleanup
    server_running = false;
    KTerm_Destroy(term);
    // pthread_join(server_thread, NULL); // Might hang if accept blocks, detach/cancel in real app

    printf("All Real Networking tests passed.\n");
    return 0;
}
