#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <assert.h>

int test_decsca_protected(KTerm* term, KTermSession* session) {
    reset_terminal(term);
    write_sequence(term, "\x1B[2J"); // Clear screen

    // DECSCA 1: Set Protected attribute
    write_sequence(term, "\x1B[1\"q");
    if (!(session->current_attributes & KTERM_ATTR_PROTECTED)) return 0;

    // Write a character and verify it has the protected flag
    write_sequence(term, "P");
    KTerm_FlushOps(term, session);
    EnhancedTermChar* cell = GetScreenCell(session, 0, 0);
    if (cell->ch != 'P') return 0;
    if (!(cell->flags & KTERM_ATTR_PROTECTED)) return 0;
    return 1;
}

int test_decsca_unprotected(KTerm* term, KTermSession* session) {
    reset_terminal(term);
    write_sequence(term, "\x1B[2J"); // Clear screen

    // First set it
    write_sequence(term, "\x1B[1\"q");
    if (!(session->current_attributes & KTERM_ATTR_PROTECTED)) return 0;

    // DECSCA 0: Clear Protected attribute
    write_sequence(term, "\x1B[0\"q");
    if (session->current_attributes & KTERM_ATTR_PROTECTED) return 0;

    // Write a character and verify it does NOT have the flag
    write_sequence(term, "U");
    KTerm_FlushOps(term, session);
    EnhancedTermChar* cell = GetScreenCell(session, 0, 0);
    if (cell->ch != 'U') return 0;
    if (cell->flags & KTERM_ATTR_PROTECTED) return 0;
    return 1;
}

int test_decsca_ps2_unprotected(KTerm* term, KTermSession* session) {
    reset_terminal(term);
    write_sequence(term, "\x1B[2J"); // Clear screen

    // First set it
    write_sequence(term, "\x1B[1\"q");
    if (!(session->current_attributes & KTERM_ATTR_PROTECTED)) return 0;

    // DECSCA 2: Clear Protected attribute
    write_sequence(term, "\x1B[2\"q");
    if (session->current_attributes & KTERM_ATTR_PROTECTED) return 0;
    return 1;
}

int test_decsca_default(KTerm* term, KTermSession* session) {
    reset_terminal(term);
    write_sequence(term, "\x1B[2J"); // Clear screen

    // First set it
    write_sequence(term, "\x1B[1\"q");
    if (!(session->current_attributes & KTERM_ATTR_PROTECTED)) return 0;

    // DECSCA default (0): Clear Protected attribute
    write_sequence(term, "\x1B[\"q");
    if (session->current_attributes & KTERM_ATTR_PROTECTED) return 0;
    return 1;
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
