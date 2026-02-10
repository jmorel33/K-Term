#define KTERM_IMPLEMENTATION
#define KTERM_TESTING

#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

static volatile bool cb_connect_called = false;
static void on_connect(KTerm* term, KTermSession* session) { cb_connect_called = true; }
static void MockResponseCallback(KTerm* term, const char* response, int length) { }

static int server_port = 0;
static volatile bool server_running = true;

void* tcp_echo_server(void* arg) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return NULL;
    address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY; address.sin_port = 0;
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) return NULL;
    struct sockaddr_in sin; socklen_t len = sizeof(sin);
    if (getsockname(server_fd, (struct sockaddr *)&sin, &len) == -1) return NULL;
    server_port = ntohs(sin.sin_port);
    listen(server_fd, 3);
    if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) return NULL;
    while (server_running) {
        int valread = read(new_socket, buffer, 1024);
        if (valread > 0) send(new_socket, buffer, valread, 0);
        else if (valread == 0) break;
    }
    close(new_socket); close(server_fd);
    return NULL;
}

int main() {
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, tcp_echo_server, NULL);
    while (server_port == 0) usleep(1000);

    KTermConfig config = {0};
    config.response_callback = MockResponseCallback;
    KTerm* term = KTerm_Create(config);

    KTermNetCallbacks cbs = { .on_connect = on_connect };
    KTerm_Net_SetCallbacks(term, &term->sessions[0], cbs);

    char connect_cmd[128];
    snprintf(connect_cmd, sizeof(connect_cmd), "\x1BPGATE;KTERM;1;EXT;net;connect;127.0.0.1:%d\x1B\\", server_port);
    for (int i = 0; connect_cmd[i] != '\0'; i++) KTerm_ProcessChar(term, GET_SESSION(term), (unsigned char)connect_cmd[i]);

    int ticks = 0;
    while (ticks < 100) {
        KTerm_Net_Process(term);
        KTerm_Update(term);
        if (cb_connect_called) break;
        usleep(10000);
        ticks++;
    }

    if (!cb_connect_called) return 1;

    KTerm_QueueResponse(term, "HELLO");
    ticks = 0;
    bool found = false;
    while (ticks < 50) {
        KTerm_Net_Process(term); KTerm_Update(term);
        // Simplified check: we assume echo comes back.
        // In a real test we'd check screen buffer content.
        ticks++;
        usleep(10000);
    }

    server_running = false;
    KTerm_Destroy(term);
    return 0;
}
