#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "mock_situation.h"
#include <assert.h>
#include <stdio.h>

void test_insert_lines_op(void) {
    printf("Testing INSERT_LINES (IL)...\n");
    KTermConfig config = {0};
    config.width = 20;
    config.height = 10;
    KTerm* term = KTerm_Create(config);
    KTermSession* session = &term->sessions[term->active_session];

    // 1. Fill screen with line numbers
    for(int i=0; i<10; i++) {
        char buf[20];
        snprintf(buf, 20, "Line %d", i);
        KTerm_WriteString(term, buf);
        if(i < 9) KTerm_WriteString(term, "\r\n");
    }
    KTerm_Update(term);

    // Verify initial state
    EnhancedTermChar* cell = GetScreenCell(session, 5, 0); // "Line 5"
    assert(cell->ch == 'L');
    cell = GetScreenCell(session, 5, 5); // " 5"
    assert(cell->ch == '5');

    // 2. Move cursor to row 5 (1-based: 6) and Insert 2 Lines
    KTerm_WriteString(term, "\x1B[6H\x1B[2L");
    KTerm_Update(term);

    // Expected:
    // Row 0-4: Unchanged ("Line 0" to "Line 4")
    // Row 5-6: Empty (Inserted)
    // Row 7: Was Row 5 ("Line 5")
    // Row 8: Was Row 6 ("Line 6")
    // Row 9: Was Row 7 ("Line 7")
    // Row 8 ("Line 8") and 9 ("Line 9") pushed off bottom

    // Verify Row 4 (Unchanged)
    cell = GetScreenCell(session, 4, 5);
    assert(cell->ch == '4');

    // Verify Row 5 (Empty)
    cell = GetScreenCell(session, 5, 0);
    assert(cell->ch == ' ');

    // Verify Row 7 (Was 5)
    cell = GetScreenCell(session, 7, 5);
    assert(cell->ch == '5');

    printf("SUCCESS: INSERT_LINES passed.\n");
    KTerm_Destroy(term);
}

void test_delete_lines_op(void) {
    printf("Testing DELETE_LINES (DL)...\n");
    KTermConfig config = {0};
    config.width = 20;
    config.height = 10;
    KTerm* term = KTerm_Create(config);
    KTermSession* session = &term->sessions[term->active_session];

    // 1. Fill screen with line numbers
    for(int i=0; i<10; i++) {
        char buf[20];
        snprintf(buf, 20, "Line %d", i);
        KTerm_WriteString(term, buf);
        if(i < 9) KTerm_WriteString(term, "\r\n");
    }
    KTerm_Update(term);

    // 2. Move cursor to row 2 (1-based: 3) and Delete 2 Lines
    KTerm_WriteString(term, "\x1B[3H\x1B[2M");
    KTerm_Update(term);

    // Expected:
    // Row 0-1: Unchanged ("Line 0", "Line 1")
    // Row 2: Was Row 4 ("Line 4") -> Since 2 and 3 deleted.
    // Row 3: Was Row 5 ("Line 5")
    // ...
    // Row 8: Was Row 10 (N/A) -> Empty
    // Row 9: Empty

    // Verify Row 1 (Unchanged)
    EnhancedTermChar* cell = GetScreenCell(session, 1, 5);
    assert(cell->ch == '1');

    // Verify Row 2 (Was 4)
    cell = GetScreenCell(session, 2, 5);
    assert(cell->ch == '4');

    // Verify Row 9 (Empty)
    cell = GetScreenCell(session, 9, 0);
    assert(cell->ch == ' ');

    printf("SUCCESS: DELETE_LINES passed.\n");
    KTerm_Destroy(term);
}

void test_protected_lines_op(void) {
    printf("Testing Protected Lines (DECSCA)...\n");
    KTermConfig config = {0};
    config.width = 20;
    config.height = 10;
    KTerm* term = KTerm_Create(config);
    KTermSession* session = &term->sessions[term->active_session];

    // 1. Fill with A
    for(int i=0; i<10*20; i++) KTerm_WriteChar(term, 'A');
    KTerm_Update(term);

    // 2. Protect Row 5 (Write 'P' with DECSCA 1)
    KTerm_WriteString(term, "\x1B[6H\x1B[1\"qPPPPPPPPPPPPPPPPPPPP\x1B[0\"q");
    KTerm_Update(term);

    // Verify Row 5 has 'P' and protected flag
    EnhancedTermChar* cell = GetScreenCell(session, 5, 0);
    assert(cell->ch == 'P');
    assert(cell->flags & KTERM_ATTR_PROTECTED);

    // 3. Try Insert Line at Row 4 (Should fail/abort because Row 5 is in scroll region and protected)
    // IL affects cursor row through bottom. Row 5 is affected.
    KTerm_WriteString(term, "\x1B[5H\x1B[1L");
    KTerm_Update(term);

    // Verify Row 4 is still 'A' (No insert happened)
    cell = GetScreenCell(session, 4, 0);
    assert(cell->ch == 'A');
    // Verify Row 5 is still 'P'
    cell = GetScreenCell(session, 5, 0);
    assert(cell->ch == 'P');

    printf("SUCCESS: Protected Lines passed.\n");
    KTerm_Destroy(term);
}

void test_insert_lines_overflow(void) {
    printf("Testing INSERT_LINES Overflow (count >= height)...\n");
    KTermConfig config = {0};
    config.width = 10;
    config.height = 5;
    KTerm* term = KTerm_Create(config);
    KTermSession* session = &term->sessions[term->active_session];

    // Fill with 'A'
    for(int i=0; i<10*5; i++) KTerm_WriteChar(term, 'A');
    KTerm_Update(term);

    // Verify
    assert(GetScreenCell(session, 0, 0)->ch == 'A');

    // Insert 10 lines at Row 0 (height is 5). Should clear everything.
    KTerm_WriteString(term, "\x1B[1H\x1B[10L");
    KTerm_Update(term);

    // Verify all empty
    for(int y=0; y<5; y++) {
        for(int x=0; x<10; x++) {
            EnhancedTermChar* cell = GetScreenCell(session, y, x);
            if (cell->ch != ' ') {
                printf("FAILURE: Cell at (%d,%d) is '%c', expected ' '\n", y, x, (char)cell->ch);
            }
            assert(cell->ch == ' ');
        }
    }

    printf("SUCCESS: INSERT_LINES Overflow passed.\n");
    KTerm_Destroy(term);
}

int main() {
    test_insert_lines_op();
    test_delete_lines_op();
    test_protected_lines_op();
    test_insert_lines_overflow();
    return 0;
}
