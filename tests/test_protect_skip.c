#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "mock_situation.h"
#include <assert.h>
#include <stdio.h>

void test_cursor_skip_protect(void) {
    printf("Testing Cursor Skip Protected Fields...\n");
    KTermConfig config = {0};
    config.width = 20;
    config.height = 10;
    KTerm* term = KTerm_Create(config);
    KTermSession* session = &term->sessions[term->active_session];

    // 1. Setup Protected Fields
    // Set protected attribute (DECSCA 1)
    // CSI 1 " q  (Select Character Protection Attribute: 1 = Protected)
    KTerm_WriteString(term, "\x1B[1\"q");

    // Write Protected "P" at (0,1), (0,2), (0,3)
    KTerm_WriteString(term, "\x1B[1;2HP"); // (0,1)
    KTerm_WriteString(term, "\x1B[1;3HP"); // (0,2)
    KTerm_WriteString(term, "\x1B[1;4HP"); // (0,3)

    // Set Unprotected (DECSCA 0)
    KTerm_WriteString(term, "\x1B[0\"q");
    // Write Unprotected "U" at (0,0), (0,4)
    KTerm_WriteString(term, "\x1B[1;1HU"); // (0,0)
    KTerm_WriteString(term, "\x1B[1;5HU"); // (0,4)

    KTerm_Update(term);

    // Verify protection flags
    EnhancedTermChar* cell = GetScreenCell(session, 0, 1);
    if (!(cell->flags & KTERM_ATTR_PROTECTED)) printf("Setup Fail: (0,1) not protected\n");
    assert(cell->flags & KTERM_ATTR_PROTECTED);
    cell = GetScreenCell(session, 0, 0);
    assert(!(cell->flags & KTERM_ATTR_PROTECTED));

    // 2. Test Normal CUF (No Skip)
    // Move to (0,0)
    KTerm_WriteString(term, "\x1B[1;1H");
    // CUF 1 -> Should land on (0,1) Protected
    KTerm_WriteString(term, "\x1B[C");
    KTerm_Update(term);
    assert(session->cursor.x == 1);
    assert(session->cursor.y == 0);

    // 3. Enable Skip Protect (Gateway)
    // DCS GATE KTERM;1;SET;CURSOR;SKIP_PROTECT=1 ST
    KTerm_WriteString(term, "\x1BPGATE;KTERM;1;SET;CURSOR;SKIP_PROTECT=1\x1B\\");
    KTerm_Update(term);
    assert(session->skip_protect == true);

    // 4. Test CUF with Skip
    // Reset to (0,0)
    KTerm_WriteString(term, "\x1B[1;1H");
    KTerm_Update(term);

    // CUF 1 -> Should skip (0,1), (0,2), (0,3) and land on (0,4)
    KTerm_WriteString(term, "\x1B[C");
    KTerm_Update(term);

    printf("Cursor at (%d,%d)\n", session->cursor.x, session->cursor.y);
    assert(session->cursor.x == 4);
    assert(session->cursor.y == 0);

    // 5. Test CUB with Skip
    // CUB 1 -> Should skip back to (0,0)
    KTerm_WriteString(term, "\x1B[D");
    KTerm_Update(term);
    assert(session->cursor.x == 0);
    assert(session->cursor.y == 0);

    // 6. Test CUD with Skip
    // Setup protected line at row 1
    KTerm_WriteString(term, "\x1B[1\"q");
    KTerm_WriteString(term, "\x1B[2;1HP"); // (1,0) Protected
    KTerm_WriteString(term, "\x1B[0\"q");
    KTerm_Update(term);

    // Move to (0,0)
    KTerm_WriteString(term, "\x1B[1;1H");
    // CUD 1 -> (1,0) is protected. Should advance to (1,1) (next char)
    // (1,1) is empty/unprotected by default

    KTerm_WriteString(term, "\x1B[B");
    KTerm_Update(term);

    printf("CUD Cursor at (%d,%d)\n", session->cursor.x, session->cursor.y);
    assert(session->cursor.y == 1);
    assert(session->cursor.x == 1); // Should be at (1,1)

    printf("SUCCESS: Cursor Skip Protected passed.\n");
    KTerm_Destroy(term);
}

int main() {
    test_cursor_skip_protect();
    return 0;
}
