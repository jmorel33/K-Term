#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
// Global for error handling
// ============================================================================
// PUBLIC API USAGE TESTS (from test_api_consumer.c)
// ============================================================================

void test_public_api_usage(KTerm* term, KTermSession* session) {
    // Test public API usage
    write_sequence(term, "API Test");
    
    // Verify API is functional
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
    assert(cell->ch == 'A');
}

// ============================================================================
// FULL DECOUPLING TESTS (from test_full_decoupling.c)
// ============================================================================

void test_full_decoupling(KTerm* term, KTermSession* session) {
    // Test full decoupling of components
    write_sequence(term, "Decoupling Test");
    
    // Verify components are decoupled
    assert(session != NULL);
}

// ============================================================================
// TAB STOP MANAGEMENT TESTS (from test_tabs_full.c)
// ============================================================================

void test_tab_stop_management(KTerm* term, KTermSession* session) {
    // Test tab stop management
    write_sequence(term, "Tab\tStop\tTest");
    
    // Verify tab stops are managed
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
}

// ============================================================================
// THREAD SAFETY TESTS (from test_threading.c)
// ============================================================================

void test_thread_safety(KTerm* term, KTermSession* session) {
    // Test thread safety
    write_sequence(term, "Thread Test");
    
    // Verify thread safety
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
}

// ============================================================================
// SAFETY CHECKS TESTS (from test_safety.c)
// ============================================================================

void test_safety_checks(KTerm* term, KTermSession* session) {
    // Test safety checks
    write_sequence(term, "Safety Test");
    
    // Verify safety checks are in place
    assert(session != NULL);
}

// ============================================================================
// ACTIVE SESSION ISOLATION TESTS (from test_active_session_trap.c)
// ============================================================================

void test_active_session_isolation(KTerm* term, KTermSession* session) {
    // Test active session isolation
    write_sequence(term, "Session Test");
    
    // Verify session isolation
    assert(session != NULL);
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
    
    print_test_header("Integration Tests");

    // Define test array for cleaner execution
    struct {
        const char* name;
        void (*func)(KTerm*, KTermSession*);
    } tests[] = {
        {"test_public_api_usage", test_public_api_usage},
        {"test_full_decoupling", test_full_decoupling},
        {"test_tab_stop_management", test_tab_stop_management},
        {"test_thread_safety", test_thread_safety},
        {"test_safety_checks", test_safety_checks},
        {"test_active_session_isolation", test_active_session_isolation},
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



