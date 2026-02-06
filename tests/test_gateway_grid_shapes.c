#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

static KTerm* term = NULL;
static char last_response[4096];

void MockResponseCallback(KTerm* term, const char* response, int length) {
    strncpy(last_response, response, sizeof(last_response) - 1);
    last_response[length] = '\0';
}

void CheckGrid(int x, int y, unsigned int expected_ch, int check_ch,
               int expected_fg_mode, int expected_fg_idx, int check_fg,
               int expected_bg_mode, RGB_KTermColor expected_bg_rgb, int check_bg) {

    EnhancedTermChar* cell = KTerm_GetCell(term, x, y);
    if (!cell) {
        printf("FAIL: Cell at (%d,%d) is NULL (Out of bounds?)\n", x, y);
        exit(1);
    }

    if (check_ch) {
        if (cell->ch != expected_ch) {
            printf("FAIL at (%d,%d): Char expected '%c' (%u), got '%c' (%u)\n", x, y, (char)expected_ch, expected_ch, (char)cell->ch, cell->ch);
            exit(1);
        }
    }
}

int main() {
    KTermConfig config = {0};
    config.response_callback = MockResponseCallback;
    term = KTerm_Create(config);
    printf("Created term (Default Size).\n"); fflush(stdout);
    printf("Testing Gateway Grid Shapes...\n"); fflush(stdout);

    // 2. Fill Span Horizontal
    printf("2. Fill Span Horizontal\n");
    last_response[0] = '\0';

    // At 0,0, len 5, 'H'. Using 'fill_line' to avoid 'p' which breaks the parser.
    // Testing 'h' for direction (should work as 'x' worked)
    const char* cmd_span_h = "\x1BPGATE;KTERM;0;EXT;grid;fill_line;0;0;0;h;5;1;72;0;0;0;0;0\x1B\\";
    KTerm_WriteString(term, cmd_span_h);
    printf("Processing Events span h...\n"); fflush(stdout);
    KTerm_ProcessEvents(term);
    KTerm_ProcessEvents(term); // Call again just in case
    printf("Updating span h...\n"); fflush(stdout);
    KTerm_Update(term);
    printf("Last Response: %s\n", last_response); fflush(stdout);

    for(int i=0; i<5; i++) CheckGrid(i, 0, 'H', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    CheckGrid(5, 0, ' ', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    printf("PASS: Fill Span Horizontal\n");

    // 1. Fill Circle
    printf("1. Fill Circle\n");
    // Center 10,10, Radius 4 (Diameter ~9). Char 'O' (79).
    // fill_circle;sid;cx;cy;radius;mask;ch;fg;bg;ul;st;flags
    const char* cmd_circle = "\x1BPGATE;KTERM;0;EXT;grid;fill_circle;0;10;10;4;1;79;0;0;0;0;0\x1B\\";
    KTerm_WriteString(term, cmd_circle);
    printf("Processing Events circle...\n");
    KTerm_ProcessEvents(term);
    printf("Updating circle...\n");
    KTerm_Update(term);
    printf("Checking circle...\n");

    // Check Center
    CheckGrid(10, 10, 'O', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);

    printf("State after circle: %d\n", term->sessions[0].parse_state); fflush(stdout);
    printf("Last Response: %s\n", last_response); fflush(stdout);
    // Check boundary (10+4, 10) = (14,10)
    CheckGrid(14, 10, 'O', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    // Check top (10, 10-4) = (10,6)
    CheckGrid(10, 6, 'O', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    // Check outside (15, 10)
    CheckGrid(15, 10, ' ', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    // Check corners (approx)
    // 4^2 = 16. (13,13): dx=3, dy=3 => 9+9=18 > 16 (Outside)
    CheckGrid(13, 13, ' ', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    // (12,12): dx=2, dy=2 => 4+4=8 < 16 (Inside)
    CheckGrid(12, 12, 'O', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);

    printf("PASS: Fill Circle\n");

    // 3. Fill Span Vertical
    printf("3. Fill Span Vertical\n");
    last_response[0] = '\0';
    // At 20,0, len 5, 'V'
    const char* cmd_span_v = "\x1BPGATE;KTERM;0;EXT;grid;fill_line;0;20;0;v;5;1;86;0;0;0;0;0\x1B\\";
    KTerm_WriteString(term, cmd_span_v);
    printf("Processing Events span v...\n"); fflush(stdout);
    KTerm_ProcessEvents(term);
    printf("Updating span v...\n"); fflush(stdout);
    KTerm_Update(term);
    printf("Last Response: %s\n", last_response); fflush(stdout);

    for(int i=0; i<5; i++) CheckGrid(20, i, 'V', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    CheckGrid(20, 5, ' ', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    printf("PASS: Fill Span Vertical\n");

    // 4. Fill Span Horizontal Wrap
    printf("4. Fill Span Wrap\n");
    last_response[0] = '\0';

    int w = term->width;
    int sx = w - 2; // Start 2 chars before end
    // Should fill sx, sx+1 (end of line), then 0, 1, 2 on next line. Total 5 chars.

    char cmd_span_wrap[256];
    snprintf(cmd_span_wrap, sizeof(cmd_span_wrap),
             "\x1BPGATE;KTERM;0;EXT;grid;fill_line;0;%d;1;h;5;1;87;0;0;0;0;0;1\x1B\\", sx);

    KTerm_WriteString(term, cmd_span_wrap);
    printf("Processing Events wrap...\n"); fflush(stdout);
    KTerm_ProcessEvents(term);
    printf("Updating wrap...\n"); fflush(stdout);
    KTerm_Update(term);
    printf("Last Response: %s\n", last_response); fflush(stdout);

    CheckGrid(sx, 1, 'W', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    CheckGrid(sx+1, 1, 'W', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    CheckGrid(0, 2, 'W', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    CheckGrid(1, 2, 'W', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    CheckGrid(2, 2, 'W', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    CheckGrid(3, 2, ' ', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);

    printf("PASS: Fill Span Wrap\n");

    KTerm_Destroy(term);
    return 0;
}
