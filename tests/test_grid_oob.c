#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "mock_situation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Mock response callback to capture output
static char last_response[256];
static void MockResponseCallback(KTerm* term, const char* response, int length) {
    if (length < 255) {
        memcpy(last_response, response, length);
        last_response[length] = '\0';
    }
}

void test_grid_oob(void) {
    printf("Testing Grid Out-of-Bounds Plotting...\n");
    KTermConfig config = {0};
    config.width = 20;
    config.height = 10;
    config.response_callback = MockResponseCallback;
    KTerm* term = KTerm_Create(config);

    // 1. Fill Rect OOB (Partially)
    // fill;sid;x;y;w;h;mask...
    // x=-5, w=10 -> Should fill 0 to 4 (5 pixels)
    // y=0, h=1
    // Mask=1 (CH), Ch=65 ('A')
    KTerm_WriteString(term, "\x1BPGATE;KTERM;1;EXT;grid;fill;0;-5;0;10;1;1;65\x1B\\");
    KTerm_Update(term);

    // Check Response
    printf("Response 1: %s\n", last_response);
    assert(strstr(last_response, "OK;QUEUED;5"));

    // Verify Grid
    EnhancedTermChar* cell = GetScreenCell(&term->sessions[0], 0, 0);
    assert(cell->ch == 'A');
    cell = GetScreenCell(&term->sessions[0], 0, 4);
    assert(cell->ch == 'A');
    cell = GetScreenCell(&term->sessions[0], 0, 5); // Should be empty/default
    assert(cell->ch != 'A');

    // 2. Fill Rect Fully OOB
    // x=-20, w=10 -> Ends at -10. Completely offscreen.
    KTerm_WriteString(term, "\x1BPGATE;KTERM;2;EXT;grid;fill;0;-20;0;10;1;1;66\x1B\\"); // 'B'
    KTerm_Update(term);

    printf("Response 2: %s\n", last_response);
    assert(strstr(last_response, "OK;QUEUED;0"));

    // 3. Fill Circle OOB
    // Center at (-5, 0), Radius 5.
    // Should draw partial circle on left edge.
    // Area of full circle r=5 is ~78. Half is ~39.
    KTerm_WriteString(term, "\x1BPGATE;KTERM;3;EXT;grid;fill_circle;0;-5;0;5;1;67\x1B\\"); // 'C'
    KTerm_Update(term);

    printf("Response 3: %s\n", last_response);
    // Extract count
    int count = 0;
    char* p = strstr(last_response, "QUEUED;");
    if(p) sscanf(p + 7, "%d", &count);
    assert(count > 0);
    assert(count < 70);

    // 4. Fill Span OOB
    // x=15, w=10 (cols=20). Ends at 24.
    // Wrap=0 (default if not provided? No, code checks index 13).
    // fill_line;sid;sx;sy;dir;len;mask;ch;fg;bg;ul;st;flags;wrap
    // indices: 0..13.
    // We pass explicit empty args to reach wrap at 13.
    KTerm_WriteString(term, "\x1BPGATE;KTERM;4;EXT;grid;fill_line;0;15;0;h;10;1;68;;;;;;0\x1B\\");
    KTerm_Update(term);

    printf("Response 4: %s\n", last_response);
    assert(strstr(last_response, "OK;QUEUED;5"));

    cell = GetScreenCell(&term->sessions[0], 0, 19);
    assert(cell->ch == 'D');
    // Wrapped to (1,0) should be empty if wrap=0
    cell = GetScreenCell(&term->sessions[0], 1, 0);
    assert(cell->ch != 'D');

    printf("SUCCESS: Grid OOB passed.\n");
    KTerm_Destroy(term);
}

int main() {
    test_grid_oob();
    return 0;
}
