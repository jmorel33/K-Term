#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../tests/mock_situation.h"
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static KTerm* term = NULL;

// Mock response callback
static char output_buffer[4096];
static int output_pos = 0;

static void response_callback(KTerm* term, const char* response, int length) {
    if (output_pos + length < (int)sizeof(output_buffer)) {
        memcpy(output_buffer + output_pos, response, length);
        output_pos += length;
        output_buffer[output_pos] = '\0';
    }
}

static void reset_output_buffer() {
    output_pos = 0;
    memset(output_buffer, 0, sizeof(output_buffer));
}

// Helper: Write sequence and process
static void write_sequence(KTerm* term, const char* seq) {
    KTerm_WriteString(term, seq);
    KTerm_ProcessEvents(term);
    KTerm_Update(term);
}

void TestDECRS(void) {
    printf("Testing DECRS (Session Status)...\n");
    reset_output_buffer();

    // Send CSI ? 21 n
    write_sequence(term, "\x1B[?21n");

    // Expected: DCS $ p ... ST
    if (strstr(output_buffer, "\x1BP$p") && strstr(output_buffer, "1;2;0")) {
        printf("PASS: DECRS response valid format.\n");
    } else {
        printf("FAIL: DECRS response invalid: %s\n", output_buffer);
    }
}

void TestDECRQSS_SGR(void) {
    printf("Testing DECRQSS SGR...\n");
    reset_output_buffer();

    // Set some attributes
    GET_SESSION(term)->current_attributes |= KTERM_ATTR_BOLD;
    GET_SESSION(term)->current_fg.value.index = 1; // Red (ANSI 1 -> 31)
    GET_SESSION(term)->current_fg.color_mode = 0;

    // Send DCS $ q m ST
    write_sequence(term, "\x1BP$qm\x1B\\");

    // Expected: DCS 1 $ r 0;1;31 m ST
    if (strstr(output_buffer, "\x1BP1$r0;1;31m\x1B\\")) {
        printf("PASS: DECRQSS SGR response correct.\n");
    } else {
        printf("FAIL: DECRQSS SGR response incorrect: %s\n", output_buffer);
    }
}

void TestDECRQSS_Margins(void) {
    printf("Testing DECRQSS Margins...\n");
    reset_output_buffer();

    // Send DCS $ q r ST
    write_sequence(term, "\x1BP$qr\x1B\\");

    // Expected: DCS 1 $ r 1;24 r ST (Configured height 24)
    if (strstr(output_buffer, "\x1BP1$r1;24r\x1B\\")) {
        printf("PASS: DECRQSS Margins response correct.\n");
    } else {
        printf("FAIL: DECRQSS Margins response incorrect: %s\n", output_buffer);
    }
}

int main() {
    KTermConfig config = {
        .width = 80,
        .height = 24,
        .response_callback = response_callback
    };
    term = KTerm_Create(config);

    // Enable VT525 level for Multi-Session DECRS test
    KTerm_SetLevel(term, GET_SESSION(term), VT_LEVEL_525);

    TestDECRS();
    TestDECRQSS_SGR();
    TestDECRQSS_Margins();

    KTerm_Destroy(term);
    return 0;
}
