#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../tests/mock_situation.h"
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

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

void test_ed2_ansi_sys_homing(KTerm* term) {
    // Set to ANSI.SYS mode
    KTerm_SetLevel(term, GET_SESSION(term), VT_LEVEL_ANSI_SYS);

    // Move cursor away from 0,0
    GET_SESSION(term)->cursor.x = 10;
    GET_SESSION(term)->cursor.y = 10;

    // Send ED 2 (CSI 2 J)
    write_sequence(term, "\x1B[2J");

    // Check if cursor moved to 0,0 (1,1)
    if (GET_SESSION(term)->cursor.x != 0 || GET_SESSION(term)->cursor.y != 0) {
        fprintf(stderr, "FAIL: ED 2 in ANSI.SYS mode did not home cursor. Got %d,%d\n",
                GET_SESSION(term)->cursor.x, GET_SESSION(term)->cursor.y);
        exit(1);
    }
    printf("PASS: ED 2 ANSI.SYS Cursor Homing\n");
}

void test_ed3_scrollback_clear(KTerm* term) {
    // Set to xterm mode (supports ED 3)
    KTerm_SetLevel(term, GET_SESSION(term), VT_LEVEL_XTERM);

    // Set some dirty data everywhere
    int total_cells = GET_SESSION(term)->buffer_height * GET_SESSION(term)->cols;
    for (int i=0; i<total_cells; i++) {
        GET_SESSION(term)->screen_buffer[i].ch = 'X';
    }

    // Send ED 3 (CSI 3 J)
    write_sequence(term, "\x1B[3J");

    // Verify all cells are cleared (space)
    for (int i=0; i<total_cells; i++) {
        if (GET_SESSION(term)->screen_buffer[i].ch != ' ') {
            fprintf(stderr, "FAIL: ED 3 did not clear entire buffer. Found char '%c' at index %d\n",
                    GET_SESSION(term)->screen_buffer[i].ch, i);
            exit(1);
        }
    }
    printf("PASS: ED 3 Scrollback Clear\n");
}

void test_aux_port(KTerm* term) {
    KTerm_SetLevel(term, GET_SESSION(term), VT_LEVEL_100);
    GET_SESSION(term)->printer_available = true;

    // Test CSI 5 i (Auto Print On)
    write_sequence(term, "\x1B[5i");

    if (!GET_SESSION(term)->auto_print_enabled) {
        fprintf(stderr, "FAIL: CSI 5 i did not enable auto print\n");
        exit(1);
    }

    // Test CSI 4 i (Auto Print Off)
    write_sequence(term, "\x1B[4i");

    if (GET_SESSION(term)->auto_print_enabled) {
        fprintf(stderr, "FAIL: CSI 4 i did not disable auto print\n");
        exit(1);
    }
    printf("PASS: AUX Port On/Off\n");
}

void test_dsr(KTerm* term) {
    KTerm_SetLevel(term, GET_SESSION(term), VT_LEVEL_100);
    reset_output_buffer();

    // Move cursor to 5,5 (indices 4,4)
    GET_SESSION(term)->cursor.x = 4;
    GET_SESSION(term)->cursor.y = 4;

    // Send CSI 6 n
    write_sequence(term, "\x1B[6n");

    if (strcmp(output_buffer, "\x1B[5;5R") != 0) {
        fprintf(stderr, "FAIL: DSR 6n response incorrect. Got '%s', expected '\\x1B[5;5R'\n", output_buffer);
        exit(1);
    }
    printf("PASS: DSR 6n\n");
}

int main() {
    KTermConfig config = {0};
    config.width = 80;
    config.height = 25;
    config.response_callback = response_callback;

    KTerm* term = KTerm_Create(config);
    if (!term) return 1;

    test_ed2_ansi_sys_homing(term);

    // Re-create term for clean state
    KTerm_Destroy(term);
    term = KTerm_Create(config);

    test_ed3_scrollback_clear(term);

    KTerm_Destroy(term);
    term = KTerm_Create(config);

    test_aux_port(term);
    test_dsr(term);

    KTerm_Destroy(term);

    printf("All CSI coverage tests passed.\n");
    return 0;
}
