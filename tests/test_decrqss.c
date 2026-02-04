#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"

static KTerm* term = NULL;

// Mock callback to capture response
char last_response[1024];
int last_response_len = 0;

void MockResponseCallback(KTerm* term, const char* response, int length) {
    if (length < sizeof(last_response) - 1) {
        memcpy(last_response, response, length);
        last_response[length] = '\0';
        last_response_len = length;
    }
}

void FlushResponse() {
    // KTerm now buffers responses in a ring buffer, flushed by KTerm_Update
    KTerm_Update(term);
}

int main() {

    KTermConfig config = {0};
    term = KTerm_Create(config);

    KTerm_SetResponseCallback(term, MockResponseCallback);

    // 1. Enable Overline Mode (SGR 53) using high-level pipeline
    // We can use KTerm_ProcessChar directly to simulate input stream
    printf("Setting Overline Mode (SGR 53)...\n");

    const char* sgr_seq = "\x1B[53m";
    for (int i = 0; sgr_seq[i]; i++) {
        KTerm_ProcessChar(term, GET_SESSION(term), sgr_seq[i]);
    }

    // Verify manually if overline_mode is set
    if (term->sessions[0].current_attributes & KTERM_ATTR_OVERLINE) {
        printf("Overline Mode is ACTIVE.\n");
    } else {
        printf("Overline Mode is NOT ACTIVE (Failed to set via CSI).\n");
        return 1;
    }

    // 2. Send DECRQSS for SGR
    // DCS $ q m ST
    printf("Sending DECRQSS for SGR...\n");

    // Clear response buffer
    last_response[0] = '\0';

    const char* dcs_seq = "\x1BP$qm\x1B\\";
    for (int i = 0; dcs_seq[i]; i++) {
        KTerm_ProcessChar(term, GET_SESSION(term), dcs_seq[i]);
    }

    FlushResponse();

    // 3. Check Response
    printf("Response: %s\n", last_response);

    // Expected: DCS 1 $ r ... ;53 ... m ST
    // Note: The response format includes DCS ... ST.
    if (strstr(last_response, ";53") != NULL) {
        printf("SUCCESS: Response contains ';53' (Overline).\n");
    } else {
        printf("FAILURE: Response does NOT contain ';53'.\n");
        return 1;
    }

    return 0;
}
