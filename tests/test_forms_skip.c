#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

static KTerm* term = NULL;

void MockResponseCallback(KTerm* term, const char* response, int length) {
    // printf("MockResponse: %.*s\n", length, response);
}

void CheckCursor(int expected_x, int expected_y, const char* msg) {
    KTermSession* s = GET_SESSION(term);
    if (s->cursor.x != expected_x || s->cursor.y != expected_y) {
        printf("FAIL: %s - Cursor at (%d,%d), expected (%d,%d)\n", msg, s->cursor.x, s->cursor.y, expected_x, expected_y);
        exit(1);
    } else {
        printf("PASS: %s\n", msg);
    }
}

int main() {
    KTermConfig config = {0};
    config.response_callback = MockResponseCallback;
    config.width = 80;
    config.height = 24;
    term = KTerm_Create(config);
    KTermSession* s = GET_SESSION(term);

    printf("Testing Forms Mode Skip Protect...\n");

    // 1. Enable SKIP_PROTECT
    // Using Gateway command: DCS GATE KTERM;0;SET;CURSOR;SKIP_PROTECT=1 ST
    const char* enable_skip = "\x1BPGATE;KTERM;0;SET;CURSOR;SKIP_PROTECT=1\x1B\\";
    KTerm_WriteString(term, enable_skip);
    KTerm_ProcessEvents(term);

    if (!s->skip_protect) {
        printf("FAIL: SKIP_PROTECT not enabled via Gateway\n");
        exit(1);
    }
    printf("PASS: SKIP_PROTECT enabled\n");

    // 2. Setup Grid with Protected Cells
    // We want: [Unprotected] [Protected] [Unprotected]
    // Pos:     (0,0)         (1,0)       (2,0)

    // Mark (1,0) as protected using DECSCA
    // Select Protection Attribute: CSI 1 " q (DECSCA 1 = Protected)
    // Write a char there.
    KTerm_WriteString(term, "\x1B[1\"q"); // DECSCA 1 (Protected)
    // Move to 1,0
    KTerm_WriteString(term, "\x1B[1;2H"); // CUP 1,2 (1-based) -> (1,0) 0-based
    KTerm_WriteString(term, "P"); // Write 'P' (Protected)

    // Reset Protection: CSI 0 " q (DECSCA 0 = Unprotected)
    KTerm_WriteString(term, "\x1B[0\"q");
    // Write U at 0,0 and 2,0
    KTerm_WriteString(term, "\x1B[1;1H"); // CUP 1,1 -> (0,0)
    KTerm_WriteString(term, "U");
    KTerm_WriteString(term, "\x1B[1;3H"); // CUP 1,3 -> (2,0)
    KTerm_WriteString(term, "U");

    KTerm_ProcessEvents(term);
    KTerm_Update(term); // Flush ops

    // Verify setup
    EnhancedTermChar* c_prot = KTerm_GetCell(term, 1, 0);
    if (!(c_prot->flags & KTERM_ATTR_PROTECTED)) {
        printf("FAIL: Cell (1,0) not protected\n");
        exit(1);
    }
    EnhancedTermChar* c_unprot = KTerm_GetCell(term, 2, 0);
    if (c_unprot->flags & KTERM_ATTR_PROTECTED) {
        printf("FAIL: Cell (2,0) marked protected incorrectly\n");
        exit(1);
    }

    // 3. Test Right Arrow / CUF
    // Move cursor to (0,0)
    KTerm_WriteString(term, "\x1B[1;1H");
    KTerm_ProcessEvents(term);
    KTerm_Update(term);
    CheckCursor(0, 0, "Reset to 0,0");

    // Send Right Arrow (CUF)
    KTerm_WriteString(term, "\x1B[C");
    KTerm_ProcessEvents(term);
    KTerm_Update(term);

    // Expectation: Skip (1,0) and land on (2,0)
    CheckCursor(2, 0, "Right Arrow (CUF) should skip protected cell");

    // 4. Test Tab
    // Move cursor to (0,0)
    KTerm_WriteString(term, "\x1B[1;1H");
    KTerm_ProcessEvents(term);
    KTerm_Update(term);

    // Ensure tab stops are set (default every 8). So tab from 0 should go to 8.
    // If 8 is protected, it should skip.
    // Let's set tab stop at 1 (normally invalid if tabs are fixed, but we can clear and set).
    // TBC 3 (Clear all)
    KTerm_WriteString(term, "\x1B[3g");
    // Move to 1,0 and set tab (HTS)
    KTerm_WriteString(term, "\x1B[1;2H\x1BH"); // Tab at 1
    // Move to 2,0 and set tab
    KTerm_WriteString(term, "\x1B[1;3H\x1BH"); // Tab at 2

    // Move back to 0,0
    KTerm_WriteString(term, "\x1B[1;1H");
    KTerm_ProcessEvents(term);
    KTerm_Update(term);

    // Send Tab (\t)
    KTerm_WriteString(term, "\t");
    KTerm_ProcessEvents(term);
    KTerm_Update(term);

    // Expectation: Tab skips protected (1,0) and lands on (2,0)
    CheckCursor(2, 0, "Tab should skip protected cell");

    printf("All setup done.\n");
    KTerm_Destroy(term);
    return 0;
}
