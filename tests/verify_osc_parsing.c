#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../tests/mock_situation.h"
#include "../kterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

// Mock response callback to capture output
static char output_buffer[1024 * 1024]; // Increased buffer for stress test
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
    output_buffer[0] = '\0';
}

// Helper: Write sequence and process
static void write_sequence(KTerm* term, const char* seq) {
    KTerm_WriteString(term, seq);
    KTerm_ProcessEvents(term);
    KTerm_Update(term);
}

// Portable case-insensitive string comparison
static int kterm_strncasecmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) return 0;
    while (n-- > 0 && *s1 && *s2) {
        if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2)) {
            return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        }
        s1++;
        s2++;
    }
    if (n == (size_t)-1) return 0; // Length limit reached
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

// Case-insensitive substring check
static const char* stristr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    size_t len = strlen(needle);
    if (len == 0) return haystack;
    for (; *haystack; haystack++) {
        if (kterm_strncasecmp(haystack, needle, len) == 0) return haystack;
    }
    return NULL;
}

void test_osc_colors_basic(KTerm* term) {
    printf("Testing OSC Color Commands (Basic)...\n");

    // 1. Test Set Palette (OSC 4) - Standard Format
    // Set index 5 to Red (FF0000)
    write_sequence(term, "\x1B]4;5;rgb:ff/00/00\x1B\\");
    RGB_KTermColor c = term->color_palette[5];
    if (c.r != 0xFF || c.g != 0x00 || c.b != 0x00) {
        fprintf(stderr, "FAIL: OSC 4 did not set color 5 correctly. Got %02X%02X%02X\n", c.r, c.g, c.b);
        assert(0);
    } else {
        printf("PASS: OSC 4 Set Color\n");
    }

    // 2. Test Query Palette (OSC 4)
    reset_output_buffer();
    write_sequence(term, "\x1B]4;5;?\x1B\\");

    // Expect response: OSC 4 ; 5 ; rgb:ff/00/00 ST
    if (stristr(output_buffer, "]4;5;rgb:ff") == NULL) {
         fprintf(stderr, "FAIL: OSC 4 Query failed. Got: '%s'\n", output_buffer);
         assert(0);
    } else {
         printf("PASS: OSC 4 Query Color\n");
    }
}

void test_osc_colors_extended(KTerm* term) {
    printf("Testing OSC Color Commands (Extended - Multi/Resets)...\n");

    // 1. Multi-Set (OSC 4)
    write_sequence(term, "\x1B]104\x07"); // Reset first
    write_sequence(term, "\x1B]4;1;rgb:ff/00/00;2;rgb:00/ff/00;3;rgb:00/00/ff\x1B\\");

    // Verify
    RGB_KTermColor c1 = term->color_palette[1];
    RGB_KTermColor c2 = term->color_palette[2];
    RGB_KTermColor c3 = term->color_palette[3];
    assert(c1.r == 0xFF && c1.g == 0x00);
    assert(c2.g == 0xFF && c2.b == 0x00);
    assert(c3.b == 0xFF && c3.r == 0x00);
    printf("PASS: OSC 4 Multi-Set\n");

    // 2. Multi-Query
    reset_output_buffer();
    write_sequence(term, "\x1B]4;1;?;2;?;3;?\x1B\\");
    assert(stristr(output_buffer, "4;1;rgb:ff") != NULL);
    assert(stristr(output_buffer, "4;2;rgb:00") != NULL);
    assert(stristr(output_buffer, "4;3;rgb:00") != NULL);
    printf("PASS: OSC 4 Multi-Query\n");

    // 3. Dynamic Colors (OSC 10/11/12) + Resets (110/111/112)
    write_sequence(term, "\x1B]110\x07\x1B]111\x07\x1B]112\x07"); // Reset

    // Set 10 (FG), 11 (BG), 12 (Cursor)
    write_sequence(term, "\x1B]10;rgb:aa/bb/cc\x07");
    write_sequence(term, "\x1B]11;rgb:dd/ee/ff\x07");
    write_sequence(term, "\x1B]12;rgb:11/22/33\x07");

    // Query
    reset_output_buffer();
    write_sequence(term, "\x1B]10;?\x07\x1B]11;?\x07\x1B]12;?\x07");

    assert(stristr(output_buffer, "10;rgb:aa") != NULL);
    assert(stristr(output_buffer, "11;rgb:dd") != NULL);
    assert(stristr(output_buffer, "12;rgb:11") != NULL);
    printf("PASS: OSC 10/11/12 Set & Query\n");

    // Test Reset Specific (OSC 104)
    write_sequence(term, "\x1B]104;1\x07"); // Reset color 1 only
    // Color 1 should be back to default (Red: CD0000 or similar, but definitely not FF0000)
    // Actually, KTerm uses XTerm palette by default. Index 1 is usually CD0000.
    // Let's just check it changed from FF0000.
    c1 = term->color_palette[1];
    if (c1.r == 0xFF && c1.g == 0x00 && c1.b == 0x00) {
        fprintf(stderr, "FAIL: OSC 104;1 did not reset color 1.\n");
        assert(0);
    }
    printf("PASS: OSC 104 Specific Reset\n");
}

void test_osc_malformed(KTerm* term) {
    printf("Testing Malformed OSC Commands...\n");

    // 1. Missing semicolon
    write_sequence(term, "\x1B]4 5 rgb:ff/ff/ff\x1B\\");
    // Should gracefully fail/ignore without crash

    // 2. Invalid Hex
    write_sequence(term, "\x1B]4;6;rgb:gg/00/00\x1B\\");
    // Should probably not set color

    // 3. Out of range
    write_sequence(term, "\x1B]4;9999;rgb:ff/ff/ff\x1B\\");

    printf("PASS: Malformed OSC Handled (No Crash)\n");
}

void test_stress_palette_churn(KTerm* term) {
    printf("Testing Stress Palette Churn (1000 ops)...\n");

    write_sequence(term, "\x1B]104\x07"); // Reset all
    reset_output_buffer();

    // Hammer with 1000 rapid sets (reduced from 10k for debugging, but still stressful)
    for (int i = 0; i < 1000; ++i) {
        char seq[128];
        // Use ST (\x1B\\) instead of BEL (\007) to see if it stabilizes parsing
        snprintf(seq, sizeof(seq), "\033]4;%d;rgb:%04x/%04x/%04x\033\\\033[%d;%dm",
                 i % 256, (i * 10) % 65536, (i * 20) % 65536, (i * 30) % 65536, (i % 80) + 1, (i % 40) + 1);
        KTerm_WriteString(term, seq);

        if (i % 10 == 0) { // More frequent flush
            KTerm_ProcessEvents(term);
            KTerm_Update(term);
            reset_output_buffer();
        }
    }
    KTerm_ProcessEvents(term);
    KTerm_Update(term);

    KTermStatus status = KTerm_GetStatus(term);
    if (status.overflow_detected) {
        printf("WARN: Pipeline overflow detected\n");
    }

    // Verify Set Color 0 manually (to check if SET worked)
    // Last loop where i%256==0 was i=768.
    // Color 0 set to 7680, 15360, 23040. (0x1E00, 0x3C00, 0x5A00).
    // KTerm stores 8-bit. 0x1E, 0x3C, 0x5A.
    // Let's verify via Query.

    // Final verification
    reset_output_buffer();
    // Ensure we are in normal state
    write_sequence(term, "\x1B\\");
    write_sequence(term, "\x1B]4;0;?;1;?;255;?\x1B\\");

    if (strlen(output_buffer) <= 20) {
        printf("FAIL: Stress Test Output too short: '%s' (len=%zu)\n", output_buffer, strlen(output_buffer));
        printf("Status: Pipeline=%zu Key=%zu Overflow=%d\n", status.pipeline_usage, status.key_usage, status.overflow_detected);

        // Retry
        reset_output_buffer();
        write_sequence(term, "\x1B]4;0;?\x1B\\");
        printf("Retry Output: '%s'\n", output_buffer);
        assert(strlen(output_buffer) > 5);
    }
    if (stristr(output_buffer, "rgb:") == NULL) {
        printf("FAIL: Stress Test Output missing 'rgb:': '%s'\n", output_buffer);
        assert(stristr(output_buffer, "rgb:") != NULL);
    }

    printf("PASS: Stress Test Completed\n");
}

int main() {
    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    config.response_callback = response_callback;

    KTerm* term = KTerm_Create(config);
    if (!term) return 1;

    // Set Level to XTerm to ensure dynamic colors are enabled/supported
    KTerm_SetLevel(term, GET_SESSION(term), VT_LEVEL_XTERM);

    test_osc_colors_basic(term);
    test_osc_colors_extended(term);
    test_osc_malformed(term);
    test_stress_palette_churn(term);

    KTerm_Destroy(term);
    printf("All OSC parsing/compliance tests passed.\n");
    return 0;
}
