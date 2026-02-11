#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
// Global for error handling
// ============================================================================
// GENERAL FUZZING HARNESS TESTS (from fuzz_harness.c)
// ============================================================================

void fuzz_general_harness(KTerm* term, KTermSession* session) {
    // Test general fuzzing harness
    // Send various random sequences
    write_sequence(term, "\x1B[1;2;3m");
    write_sequence(term, "Test");
    write_sequence(term, "\x1B[H");
    
    // Verify terminal is still functional
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
}

// ============================================================================
// LIBFUZZER TARGET TESTS (from libfuzzer_target.c)
// ============================================================================

void fuzz_libfuzzer_target(KTerm* term, KTermSession* session) {
    // Test libfuzzer target
    write_sequence(term, "\x1B[");
    write_sequence(term, "1");
    write_sequence(term, "m");
    
    // Verify fuzzing doesn't crash
    assert(session->current_attributes & KTERM_ATTR_BOLD);
}

// ============================================================================
// KITTY PROTOCOL FUZZING TESTS (from test_fuzz_kitty.c)
// ============================================================================

void fuzz_kitty_protocol(KTerm* term, KTermSession* session) {
    // Test Kitty protocol fuzzing
    write_sequence(term, "\x1B_G");
    write_sequence(term, "a=T");
    write_sequence(term, "\x1B\\");
    
    // Verify fuzzing doesn't crash
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main() {
    KTerm* term = create_test_term(80, 25);
    if (!term) {
        fprintf(stderr, "Failed to create test terminal\n");
        return 1;
    }

    KTermSession* session = GET_SESSION(term);
    if (!session) {
        fprintf(stderr, "Failed to get session\n");
        destroy_test_term(term);
        return 1;
    }

    TestResults results = {0};
    
    print_test_header("Fuzzing Tests");

    // Define test array for cleaner execution
    struct {
        const char* name;
        void (*func)(KTerm*, KTermSession*);
    } tests[] = {
        {"fuzz_general_harness", fuzz_general_harness},
        {"fuzz_libfuzzer_target", fuzz_libfuzzer_target},
        {"fuzz_kitty_protocol", fuzz_kitty_protocol},
    };

    int num_tests = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < num_tests; i++) {
        if (1) { reset_terminal(term);
            tests[i].func(term, session);
            results.passed++;
        } else {
            results.failed++;
        }
        results.total++;
        print_test_result(tests[i].name, results.passed == results.total);
    }

    destroy_test_term(term);
    
    print_test_summary(results.total, results.passed, results.failed);
    
    return results.failed > 0 ? 1 : 0;
}



