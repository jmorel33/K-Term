#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

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

    KTermSession* session = GET_SESSION(term);

    // Initial State
    session->direct_input = false;

    // Enable Direct Input
    // DCS GATE KTERM;1;EXT;direct;1 ST
    const char* enable_seq = "\x1BPGATE;KTERM;1;EXT;direct;1\x1B\\";
    memset(last_response, 0, sizeof(last_response));

    printf("Sending enable sequence...\n");
    for (int i = 0; enable_seq[i] != '\0'; i++) {
        KTerm_ProcessChar(term, session, (unsigned char)enable_seq[i]);
    }
    KTerm_Update(term); // Flush responses

    if (session->direct_input) {
        printf("PASS: Direct Input Enabled\n");
    } else {
        printf("FAIL: Direct Input NOT Enabled\n");
        // Don't return 1 yet, try disable to see if it even runs
    }

    if (strcmp(last_response, "OK") == 0) {
        printf("PASS: Response OK received\n");
    } else {
        printf("FAIL: Response mismatch. Got: '%s'\n", last_response);
    }

    // Disable Direct Input
    // DCS GATE KTERM;2;EXT;direct;0 ST
    const char* disable_seq = "\x1BPGATE;KTERM;2;EXT;direct;0\x1B\\";
    memset(last_response, 0, sizeof(last_response));

    printf("Sending disable sequence...\n");
    for (int i = 0; disable_seq[i] != '\0'; i++) {
        KTerm_ProcessChar(term, session, (unsigned char)disable_seq[i]);
    }
    KTerm_Update(term);

    if (!session->direct_input) {
        printf("PASS: Direct Input Disabled\n");
    } else {
        printf("FAIL: Direct Input NOT Disabled\n");
        return 1;
    }

    if (strcmp(last_response, "OK") == 0) {
        printf("PASS: Response OK received\n");
    } else {
        printf("FAIL: Response mismatch. Got: '%s'\n", last_response);
    }

    // Test invalid argument (should assume enable or err? usually boolean treats unknown as true or ignore)
    // We'll test "true" just in case I implement flexible parsing
    const char* true_seq = "\x1BPGATE;KTERM;3;EXT;direct;true\x1B\\";
    for (int i = 0; true_seq[i] != '\0'; i++) {
        KTerm_ProcessChar(term, session, (unsigned char)true_seq[i]);
    }
    if (session->direct_input) {
        printf("PASS: Direct Input Enabled with 'true'\n");
    } else {
        printf("INFO: 'true' did not enable direct input (strict parsing?)\n");
    }

    KTerm_Destroy(term);
    return 0;
}
