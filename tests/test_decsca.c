#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <assert.h>

void test_decsca_protected(KTerm* term, KTermSession* session) {
    reset_terminal(term);
    write_sequence(term, "\x1B[2J"); // Clear screen

    // DECSCA 1: Set Protected attribute
    write_sequence(term, "\x1B[1\"q");
    assert(session->current_attributes & KTERM_ATTR_PROTECTED);

    // Write a character and verify it has the protected flag
    write_sequence(term, "P");
    KTerm_FlushOps(term, session);
    EnhancedTermChar* cell = GetScreenCell(session, 0, 0);
    assert(cell->ch == 'P');
    assert(cell->flags & KTERM_ATTR_PROTECTED);
}

void test_decsca_unprotected(KTerm* term, KTermSession* session) {
    reset_terminal(term);
    write_sequence(term, "\x1B[2J"); // Clear screen

    // First set it
    write_sequence(term, "\x1B[1\"q");
    assert(session->current_attributes & KTERM_ATTR_PROTECTED);

    // DECSCA 0: Clear Protected attribute
    write_sequence(term, "\x1B[0\"q");
    assert(!(session->current_attributes & KTERM_ATTR_PROTECTED));

    // Write a character and verify it does NOT have the flag
    write_sequence(term, "U");
    KTerm_FlushOps(term, session);
    EnhancedTermChar* cell = GetScreenCell(session, 0, 0);
    assert(cell->ch == 'U');
    assert(!(cell->flags & KTERM_ATTR_PROTECTED));
}

void test_decsca_ps2_unprotected(KTerm* term, KTermSession* session) {
    reset_terminal(term);
    write_sequence(term, "\x1B[2J"); // Clear screen

    // First set it
    write_sequence(term, "\x1B[1\"q");
    assert(session->current_attributes & KTERM_ATTR_PROTECTED);

    // DECSCA 2: Clear Protected attribute
    write_sequence(term, "\x1B[2\"q");
    assert(!(session->current_attributes & KTERM_ATTR_PROTECTED));
}

void test_decsca_default(KTerm* term, KTermSession* session) {
    reset_terminal(term);
    write_sequence(term, "\x1B[2J"); // Clear screen

    // First set it
    write_sequence(term, "\x1B[1\"q");
    assert(session->current_attributes & KTERM_ATTR_PROTECTED);

    // DECSCA default (0): Clear Protected attribute
    write_sequence(term, "\x1B[\"q");
    assert(!(session->current_attributes & KTERM_ATTR_PROTECTED));
}

int main() {
    KTerm* term = create_test_term(80, 25);
    KTermSession* session = GET_SESSION(term);

    TestResults results = {0};
    print_test_header("DECSCA (Select Character Protection Attribute) Tests");

    run_test("test_decsca_protected", test_decsca_protected, term, session, &results);
    run_test("test_decsca_unprotected", test_decsca_unprotected, term, session, &results);
    run_test("test_decsca_ps2_unprotected", test_decsca_ps2_unprotected, term, session, &results);
    run_test("test_decsca_default", test_decsca_default, term, session, &results);

    destroy_test_term(term);
    print_test_summary(results.total, results.passed, results.failed);

    return results.failed > 0 ? 1 : 0;
}
