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

#ifndef GET_SESSION
#define GET_SESSION(term) (&term->sessions[term->active_session])
#endif

static char last_response[4096];
static void MockResponseCallback(KTerm* term, const char* response, int length) {
    if (length < sizeof(last_response)) {
        memcpy(last_response, response, length);
        last_response[length] = '\0';
    }
}

static void on_connect(KTerm* term, KTermSession* session) { }

int main() {
    KTermConfig config = {0};
    config.response_callback = MockResponseCallback;
    KTerm* term = KTerm_Create(config);

    KTermNetCallbacks cbs = { .on_connect = on_connect };
    KTerm_Net_SetCallbacks(term, &term->sessions[0], cbs);

    // Test MYIP
    printf("Testing MYIP...\n");
    const char* myip_cmd = "\x1BPGATE;KTERM;TEST1;EXT;net;myip\x1B\\";
    last_response[0] = '\0';
    for (int i=0; myip_cmd[i]; i++) KTerm_ProcessChar(term, GET_SESSION(term), (unsigned char)myip_cmd[i]);

    // Process loop a bit
    for(int i=0; i<10; i++) { KTerm_Net_Process(term); KTerm_Update(term); usleep(1000); }

    printf("Response: %s\n", last_response);

    // Test PING
    printf("Testing PING...\n");
    const char* ping_cmd = "\x1BPGATE;KTERM;TEST2;EXT;net;ping;127.0.0.1\x1B\\";
    last_response[0] = '\0';
    for (int i=0; ping_cmd[i]; i++) KTerm_ProcessChar(term, GET_SESSION(term), (unsigned char)ping_cmd[i]);

    // Process loop a bit
    for(int i=0; i<100; i++) { KTerm_Net_Process(term); KTerm_Update(term); usleep(10000); }

    printf("Response: %s\n", last_response);

    if (strstr(last_response, "PING") || strstr(last_response, "bytes from")) {
        printf("PING Success\n");
    } else {
        printf("PING Failed: %s\n", last_response);
    }

    // Test Invalid PING
    printf("Testing Invalid PING...\n");
    // Use & to bypass strtok splitting by ;
    const char* bad_ping_cmd = "\x1BPGATE;KTERM;TEST3;EXT;net;ping;127.0.0.1&rm -rf /\x1B\\";
    last_response[0] = '\0';
    for (int i=0; bad_ping_cmd[i]; i++) KTerm_ProcessChar(term, GET_SESSION(term), (unsigned char)bad_ping_cmd[i]);

    // Process loop a bit
    for(int i=0; i<10; i++) { KTerm_Net_Process(term); KTerm_Update(term); usleep(1000); }

    printf("Response: %s\n", last_response);
    if (strstr(last_response, "ERR;INVALID_HOST")) {
        printf("Invalid PING Blocked Success\n");
    } else {
        printf("Invalid PING Check Failed: %s\n", last_response);
    }

    KTerm_Destroy(term);
    return 0;
}
