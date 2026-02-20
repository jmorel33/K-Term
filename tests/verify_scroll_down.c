#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <assert.h>

void reset_margins(KTerm* term, KTermSession* session) {
    session->left_margin = 0;
    session->right_margin = term->width - 1;
    session->scroll_top = 0;
    session->scroll_bottom = term->height - 1;
}

void test_scroll_down_basic(KTerm* term, KTermSession* session) {
    printf("  Running test_scroll_down_basic...\n");
    fflush(stdout);
    reset_margins(term, session);

    // Fill region with data using explicit positions
    write_sequence(term, "\x1B[10;1HLine 10\x1B[11;1HLine 11");
    KTerm_Update(term);

    // Verify setup
    assert(GetActiveScreenCell(session, 9, 0)->ch == 'L');
    assert(GetActiveScreenCell(session, 10, 0)->ch == 'L');

    // Scroll down region [10, 11] (1-based for VT, 0-based internal)
    // Internal 0-based: top=9, bottom=10
    KTerm_ScrollDownRegion(term, 9, 10, 1);
    KTerm_Update(term);

    // Line 9 should be cleared, Line 10 should have "Line 10"
    assert(GetActiveScreenCell(session, 9, 0)->ch == ' ');
    assert(GetActiveScreenCell(session, 10, 0)->ch == 'L');
    assert(GetActiveScreenCell(session, 10, 5)->ch == '1');
    assert(GetActiveScreenCell(session, 10, 6)->ch == '0');
    printf("    ✓ PASSED\n");
    fflush(stdout);
}

void test_scroll_down_margins(KTerm* term, KTermSession* session) {
    printf("  Running test_scroll_down_margins...\n");
    fflush(stdout);
    reset_margins(term, session);

    // 1. Fill data first using explicit positions
    write_sequence(term, "\x1B[10;11HINSIDE\x1B[10;1HOUT");
    KTerm_Update(term);

    // 2. Now set margins (0-based)
    session->left_margin = 10;
    session->right_margin = 20;

    // 3. Scroll down region [10, 11] by 1
    KTerm_ScrollDownRegion(term, 9, 10, 1);
    KTerm_Update(term);

    // (9, 0) should still be 'O'
    assert(GetActiveScreenCell(session, 9, 0)->ch == 'O');
    // (10, 10) should be 'I' (moved from (9, 10))
    assert(GetActiveScreenCell(session, 10, 10)->ch == 'I');
    // (9, 10) should be ' ' (cleared)
    assert(GetActiveScreenCell(session, 9, 10)->ch == ' ');
    printf("    ✓ PASSED\n");
    fflush(stdout);
}

void test_scroll_down_partial(KTerm* term, KTermSession* session) {
    printf("  Running test_scroll_down_partial...\n");
    fflush(stdout);
    reset_margins(term, session);

    // Fill lines 5, 6, 7
    write_sequence(term, "\x1B[5;1HL5\x1B[6;1HL6\x1B[7;1HL7");
    KTerm_Update(term);

    // Scroll down region [5, 6] by 1
    KTerm_ScrollDownRegion(term, 4, 5, 1);
    KTerm_Update(term);

    // Line 4 cleared, Line 5 has "L5", Line 6 still has "L7"
    assert(GetActiveScreenCell(session, 4, 0)->ch == ' ');
    assert(GetActiveScreenCell(session, 5, 0)->ch == 'L');
    assert(GetActiveScreenCell(session, 5, 1)->ch == '5');
    assert(GetActiveScreenCell(session, 6, 0)->ch == 'L');
    assert(GetActiveScreenCell(session, 6, 1)->ch == '7');
    printf("    ✓ PASSED\n");
    fflush(stdout);
}

void test_scroll_down_overscroll(KTerm* term, KTermSession* session) {
    printf("  Running test_scroll_down_overscroll...\n");
    fflush(stdout);
    reset_margins(term, session);

    // Fill lines 5, 6
    write_sequence(term, "\x1B[5;1HL5\x1B[6;1HL6");
    KTerm_Update(term);

    // Scroll down region [5, 6] by 5 lines (more than region height 2)
    KTerm_ScrollDownRegion(term, 4, 5, 5);
    KTerm_Update(term);

    // Both lines should be cleared
    assert(GetActiveScreenCell(session, 4, 0)->ch == ' ');
    assert(GetActiveScreenCell(session, 5, 0)->ch == ' ');
    printf("    ✓ PASSED\n");
    fflush(stdout);
}

int main() {
    KTerm* term = create_test_term(80, 25);
    KTermSession* session = GET_SESSION(term);

    print_test_header("Scroll Down Region Verification");

    reset_terminal(term);
    test_scroll_down_basic(term, session);

    reset_terminal(term);
    test_scroll_down_margins(term, session);

    reset_terminal(term);
    test_scroll_down_partial(term, session);

    reset_terminal(term);
    test_scroll_down_overscroll(term, session);

    destroy_test_term(term);

    printf("\nAll Scroll Down Region tests PASSED!\n");

    return 0;
}
