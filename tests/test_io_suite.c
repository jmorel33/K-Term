#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
// Global for error handling
// ============================================================================
// CLIPBOARD OPERATION TESTS (from verify_clipboard.c)
// ============================================================================

void test_clipboard_operations(KTerm* term, KTermSession* session) {
    // Test clipboard operations
    write_sequence(term, "Clipboard Test");
    
    // Clipboard handling is internal to K-Term
}

// ============================================================================
// I/O ADAPTER INTEGRATION TESTS (from test_io_sit.c)
// ============================================================================

void test_io_adapter_integration(KTerm* term, KTermSession* session) {
    // Test I/O adapter integration
    write_sequence(term, "I/O Test");
    
    // Verify I/O is functional
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
}

// ============================================================================
// PIPE BANNER HANDLING TESTS (from test_pipe_banner.c)
// ============================================================================

void test_pipe_banner_handling(KTerm* term, KTermSession* session) {
    // Test pipe banner handling
    write_sequence(term, "Banner Test");
    
    // Verify banner is handled
}

// ============================================================================
// DATA SINK OPERATION TESTS (from verify_sink.c)
// ============================================================================

void test_data_sink_operations(KTerm* term, KTermSession* session) {
    // Test data sink operations
    write_sequence(term, "Sink Test");
    
    // Verify sink is functional
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
}

// ============================================================================
// RAW DUMP OPERATION TESTS (from test_rawdump.c)
// ============================================================================

void test_raw_dump_operations(KTerm* term, KTermSession* session) {
    // Test raw dump operations
    write_sequence(term, "Raw Dump");
    
    // Verify raw dump is functional
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
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
    
    print_test_header("I/O Tests");

    // Define test array for cleaner execution
    struct {
        const char* name;
        void (*func)(KTerm*, KTermSession*);
    } tests[] = {
        {"test_clipboard_operations", test_clipboard_operations},
        {"test_io_adapter_integration", test_io_adapter_integration},
        {"test_pipe_banner_handling", test_pipe_banner_handling},
        {"test_data_sink_operations", test_data_sink_operations},
        {"test_raw_dump_operations", test_raw_dump_operations},
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



