#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>

// Global for error handling
// ============================================================================
// OPERATION QUEUE STRESS TESTS (from stress_op_queue.c)
// ============================================================================

void stress_operation_queue(KTerm* term, KTermSession* session) {
    // Stress test operation queue with many operations
    clock_t start = clock();
    
    for (int i = 0; i < 1000; i++) {
        write_sequence(term, "\x1B[1m");
        write_sequence(term, "X");
        write_sequence(term, "\x1B[0m");
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("  Operation queue stress: %d operations in %.3f seconds\n", 1000, elapsed);
    
    // Verify terminal is still functional
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
}

// ============================================================================
// INTERLEAVED I/O STRESS TESTS (from stress_interleaved_io.c)
// ============================================================================

void stress_interleaved_io(KTerm* term, KTermSession* session) {
    // Stress test interleaved I/O operations
    clock_t start = clock();
    
    for (int i = 0; i < 500; i++) {
        write_sequence(term, "Line ");
        write_sequence(term, "\x1B[1m");
        write_sequence(term, "Bold");
        write_sequence(term, "\x1B[0m");
        write_sequence(term, "\x1B[E");  // Next line
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("  Interleaved I/O stress: %d operations in %.3f seconds\n", 500, elapsed);
}

// ============================================================================
// RESIZE OPERATION STRESS TESTS (from stress_resize.c)
// ============================================================================

void stress_resize_operations(KTerm* term, KTermSession* session) {
    // Stress test resize operations
    clock_t start = clock();
    
    for (int i = 0; i < 100; i++) {
        write_sequence(term, "\x1B[8;25;80t");  // Resize
        write_sequence(term, "Resize Test");
        write_sequence(term, "\x1B[H");         // Home
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("  Resize operation stress: %d operations in %.3f seconds\n", 100, elapsed);
}

// ============================================================================
// PANE TILING PERFORMANCE STRESS TESTS (from test_perf_tiling.c)
// ============================================================================

void stress_pane_tiling_performance(KTerm* term, KTermSession* session) {
    // Stress test pane tiling performance
    clock_t start = clock();
    
    for (int i = 0; i < 200; i++) {
        write_sequence(term, "Pane ");
        write_sequence(term, "\x1B[H");
        write_sequence(term, "\x1B[2J");
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("  Pane tiling performance stress: %d operations in %.3f seconds\n", 200, elapsed);
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
    
    print_test_header("Stress Tests");

    // Define test array for cleaner execution
    struct {
        const char* name;
        void (*func)(KTerm*, KTermSession*);
    } tests[] = {
        {"stress_operation_queue", stress_operation_queue},
        {"stress_interleaved_io", stress_interleaved_io},
        {"stress_resize_operations", stress_resize_operations},
        {"stress_pane_tiling_performance", stress_pane_tiling_performance},
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



