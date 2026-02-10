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
#include <fcntl.h>

// Mock callbacks
static volatile bool cb_connect_called = false;
static volatile bool cb_auth_called = false;
static volatile bool cb_sb_called = false;
static char last_sb_data[256];
static int last_sb_len = 0;
static unsigned char last_sb_option = 0;

static void on_connect(KTerm* term, KTermSession* session) {
    printf("Callback: Connected\n");
    cb_connect_called = true;
}

static bool on_auth(KTerm* term, KTermSession* session, const char* user, const char* pass) {
    printf("Callback: Auth User='%s' Pass='%s'\n", user, pass);
    cb_auth_called = true;
    return (strcmp(user, "admin") == 0 && strcmp(pass, "secret") == 0);
}

static void on_telnet_sb(KTerm* term, KTermSession* session, unsigned char option, const char* data, size_t len) {
    printf("Callback: SB Option=%d Len=%zu\n", option, len);
    cb_sb_called = true;
    last_sb_option = option;
    last_sb_len = (len < 256) ? len : 255;
    memcpy(last_sb_data, data, last_sb_len);
    last_sb_data[last_sb_len] = '\0';
}

static void MockResponseCallback(KTerm* term, const char* response, int length) { }

// Client thread to simulate a Telnet client
void* client_thread_func(void* arg) {
    int port = *(int*)arg;
    usleep(100000); // Wait for server to listen

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Client connect failed");
        return NULL;
    }
    printf("Client connected to %d\n", port);

    // 1. Send Auth Data (Login)
    // Server sends "Login: ". We wait a bit then send user.
    usleep(50000);
    send(sock, "admin\r\n", 7, 0);
    usleep(50000);
    send(sock, "secret\r\n", 8, 0);

    // 2. Send SB (Subnegotiation)
    // IAC SB OPTION DATA... IAC SE
    // IAC=255, SB=250, SE=240
    // Example: NEW-ENVIRON (39) IS (0) VAR "USER" VAL "TEST"
    unsigned char sb_seq[] = {
        255, 250, // IAC SB
        39,       // NEW-ENVIRON
        0,        // IS
        'T', 'E', 'S', 'T',
        255, 240  // IAC SE
    };
    usleep(100000);
    send(sock, sb_seq, sizeof(sb_seq), 0);

    usleep(100000);
    close(sock);
    return NULL;
}

int main() {
    KTermConfig config = {0};
    config.response_callback = MockResponseCallback;
    KTerm* term = KTerm_Create(config);

    KTermNetCallbacks cbs = {
        .on_connect = on_connect,
        .on_auth = on_auth,
        .on_telnet_sb = on_telnet_sb
    };
    KTerm_Net_SetCallbacks(term, &term->sessions[0], cbs);
    KTerm_Net_SetProtocol(term, &term->sessions[0], KTERM_NET_PROTO_TELNET);

    // Start Listening on a random port (or fixed for test)
    int port = 9999;
    KTerm_Net_Listen(term, &term->sessions[0], port);

    // Start Client Thread
    pthread_t client;
    pthread_create(&client, NULL, client_thread_func, &port);

    // Server Loop
    int ticks = 0;
    while (ticks < 200) { // 2 seconds approx
        KTerm_Net_Process(term);
        usleep(10000);
        ticks++;
        if (cb_connect_called && cb_sb_called) break;
    }

    // Verifications
    if (!cb_auth_called) {
        fprintf(stderr, "FAIL: Auth callback not called\n");
        return 1;
    }
    if (!cb_connect_called) {
        fprintf(stderr, "FAIL: Connect callback not called (Auth failed?)\n");
        return 1;
    }
    if (!cb_sb_called) {
        fprintf(stderr, "FAIL: SB callback not called\n");
        return 1;
    }
    if (last_sb_option != 39) {
        fprintf(stderr, "FAIL: Wrong SB option %d\n", last_sb_option);
        return 1;
    }
    if (last_sb_data[0] != 0 || memcmp(last_sb_data+1, "TEST", 4) != 0) {
        fprintf(stderr, "FAIL: Wrong SB data\n");
        return 1;
    }

    printf("SUCCESS: Server Hardening Test Passed\n");

    KTerm_Destroy(term);
    return 0;
}
