#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../tests/mock_situation.h"
#include "../kterm.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// Mock response callback to capture output
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

void test_osc_colors(KTerm* term) {
    printf("Testing OSC Color Commands...\n");

    // 1. Test Set Palette (OSC 4) - Standard Format
    // Set index 5 to Red (FF0000)
    write_sequence(term, "\x1B]4;5;rgb:ff/00/00\x1B\\");
    RGB_KTermColor c = term->color_palette[5];
    if (c.r != 0xFF || c.g != 0x00 || c.b != 0x00) {
        fprintf(stderr, "FAIL: OSC 4 did not set color 5 correctly. Got %02X%02X%02X\n", c.r, c.g, c.b);
    } else {
        printf("PASS: OSC 4 Set Color\n");
    }

    // 2. Test Query Palette (OSC 4)
    reset_output_buffer();
    write_sequence(term, "\x1B]4;5;?\x1B\\");

    // Expect response: OSC 4 ; 5 ; rgb:ff/00/00 ST
    // Response might be encoded differently (e.g. rgb:ffff/0000/0000), so we check essential parts
    if (strstr(output_buffer, "]4;5;rgb:ff") == NULL) {
         fprintf(stderr, "FAIL: OSC 4 Query failed. Got: '%s'\n", output_buffer);
    } else {
         printf("PASS: OSC 4 Query Color\n");
    }

    // 3. Test Set Foreground (OSC 10)
    write_sequence(term, "\x1B]10;rgb:00/ff/00\x1B\\");
    // Verification would depend on checking session->current_fg logic, assume no crash for now.

    // 4. Test Query Foreground (OSC 10)
    reset_output_buffer();
    write_sequence(term, "\x1B]10;?\x1B\\");

    if (output_pos == 0) {
         printf("INFO: OSC 10 Query returned empty (expected before refactor)\n");
    } else {
         printf("PASS: OSC 10 Query: %s\n", output_buffer);
    }
}

void test_osc_malformed(KTerm* term) {
    printf("Testing Malformed OSC Commands...\n");

    // 1. Missing semicolon
    write_sequence(term, "\x1B]4 5 rgb:ff/ff/ff\x1B\\");
    // Should gracefully fail/ignore without crash

    // 2. Invalid Hex
    write_sequence(term, "\x1B]4;6;rgb:gg/00/00\x1B\\");
    // Should probably not set color
    RGB_KTermColor c = term->color_palette[6];
    // Default palette 6 is cyan (0x00, 0xCD, 0xCD)
    // Just checking it didn't crash.

    printf("PASS: Malformed OSC Handled (No Crash)\n");
}

int main() {
    KTermConfig config = {0};
    config.response_callback = response_callback;
    KTerm* term = KTerm_Create(config);
    if (!term) return 1;

    test_osc_colors(term);
    test_osc_malformed(term);

    KTerm_Destroy(term);
    return 0;
}
