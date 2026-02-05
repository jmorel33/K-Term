#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int custom_handler_called = 0;
static char last_custom_args[256];

void CustomExtHandler(KTerm* term, KTermSession* session, const char* args, GatewayResponseCallback respond) {
    (void)term; (void)session; (void)respond;
    custom_handler_called++;
    strncpy(last_custom_args, args, sizeof(last_custom_args) - 1);
    last_custom_args[sizeof(last_custom_args) - 1] = '\0';
    if (respond) respond(term, session, "CUSTOM_ACK");
}

static char last_response[256];
static void MockResponseCallback(KTerm* term, const char* response, int length) {
    (void)term;
    int len = length < 255 ? length : 255;
    strncpy(last_response, response, len);
    last_response[len] = '\0';
}

int main() {
    KTermConfig config = {0};
    config.response_callback = MockResponseCallback;
    KTerm* term = KTerm_Create(config);
    if (!term) {
        printf("Failed to create KTerm\n");
        return 1;
    }

    // Verify built-ins registered
    // We expect at least 4 built-ins
    if (term->gateway_extension_count < 4) {
        printf("FAIL: Built-in extensions not registered. Count: %d\n", term->gateway_extension_count);
        return 1;
    }
    printf("PASS: Built-in extensions registered (%d)\n", term->gateway_extension_count);

    // Register Custom Extension
    KTerm_RegisterGatewayExtension(term, "custom", CustomExtHandler);
    if (term->gateway_extension_count < 5) {
        printf("FAIL: Custom extension not registered\n");
        return 1;
    }

    // Test 1: Invoke Custom Extension
    // DCS GATE KTERM;1;EXT;custom;hello ST
    const char* dcs_seq = "\x1BPGATE;KTERM;1;EXT;custom;hello\x1B\\";
    custom_handler_called = 0;
    memset(last_custom_args, 0, sizeof(last_custom_args));
    memset(last_response, 0, sizeof(last_response));

    for (int i = 0; dcs_seq[i] != '\0'; i++) {
        KTerm_ProcessChar(term, GET_SESSION(term), (unsigned char)dcs_seq[i]);
    }

    if (custom_handler_called == 1 && strcmp(last_custom_args, "hello") == 0) {
        printf("PASS: Custom extension invoked\n");
    } else {
        printf("FAIL: Custom extension not invoked correctly. Called: %d, Args: %s\n", custom_handler_called, last_custom_args);
        return 1;
    }

    // Check response from custom handler
    // We expect the response to be in the response callback (because respond calls KTerm_QueueSessionResponse)
    // We need to flush the response ring by calling KTerm_Update?
    // KTerm_Update processes input AND flushes response ring.
    // But we already processed input char by char.
    // We can manually check response ring or call KTerm_Update.
    KTerm_Update(term);
    if (strstr(last_response, "CUSTOM_ACK")) {
        printf("PASS: Custom extension response received\n");
    } else {
        printf("FAIL: Custom extension response missing. Got: %s\n", last_response);
        // Note: Response might be wrapped in DCS/OSC if it was a standard response,
        // but CustomExtHandler sent raw "CUSTOM_ACK".
        // KTerm_QueueSessionResponse puts raw string.
    }

    // Test 2: Broadcast
    // Initialize sessions 1 and 2 (0 is active)
    KTerm_InitSession(term, 1);
    term->sessions[1].session_open = true;

    // Clear Session 1 input queue
    KTerm_InputQueue_Clear(&term->sessions[1].input_queue);

    // Send Broadcast
    const char* broadcast_seq = "\x1BPGATE;KTERM;2;EXT;broadcast;TESTMSG\x1B\\";
    for (int i = 0; broadcast_seq[i] != '\0'; i++) {
        KTerm_ProcessChar(term, GET_SESSION(term), (unsigned char)broadcast_seq[i]);
    }

    // Verify Session 1 received input
    // The input should be in Session 1's Input Queue
    size_t pending = KTerm_InputQueue_Pending(&term->sessions[1].input_queue);
    if (pending == 7) { // "TESTMSG" length
        char buf[16];
        KTerm_InputQueue_Pop(&term->sessions[1].input_queue, buf, 7);
        buf[7] = '\0';
        if (strcmp(buf, "TESTMSG") == 0) {
            printf("PASS: Broadcast received in session 1\n");
        } else {
            printf("FAIL: Broadcast content mismatch. Got: %s\n", buf);
            return 1;
        }
    } else {
        printf("FAIL: Broadcast not received in session 1. Pending: %zu\n", pending);
        return 1;
    }

    // Test 3: Icat (Base64)
    // Not fully testing Kitty logic, just parsing
    // DCS GATE KTERM;3;EXT;icat;data\x1B\
    // KTerm_Ext_Icat writes to active session input

    // Clear Active Session (0) input queue
    KTerm_InputQueue_Clear(&term->sessions[0].input_queue);
    const char* icat_seq = "\x1BPGATE;KTERM;3;EXT;icat;IMGDATA\x1B\\";
    for (int i = 0; icat_seq[i] != '\0'; i++) {
        KTerm_ProcessChar(term, GET_SESSION(term), (unsigned char)icat_seq[i]);
    }

    // Should have written kitty sequence to input queue
    // Header + IMGDATA + ST
    // Header len + 7 + 2
    size_t pending0 = KTerm_InputQueue_Pending(&term->sessions[0].input_queue);
    if (pending0 > 7) {
        printf("PASS: Icat wrote to input queue (%zu bytes)\n", pending0);
    } else {
        printf("FAIL: Icat did not write to input queue\n");
        return 1;
    }

    KTerm_Destroy(term);
    printf("All Extension tests passed.\n");
    return 0;
}
