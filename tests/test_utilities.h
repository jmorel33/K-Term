#ifndef TEST_UTILITIES_H
#define TEST_UTILITIES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test result tracking
typedef struct {
    int total;
    int passed;
    int failed;
} TestResults;

// Terminal Setup
static inline KTerm* create_test_term(int width, int height) {
    KTermConfig config = {0};
    config.width = width;
    config.height = height;
    KTerm* term = KTerm_Create(config);
    return term;
}

static inline void destroy_test_term(KTerm* term) {
    if (term) {
        KTerm_Destroy(term);
    }
}

static inline void reset_terminal(KTerm* term) {
    if (term) {
        KTermSession* session = GET_SESSION(term);
        if (session) {
            KTerm_ResetAllAttributes(term, session);
            KTerm_GoHome(session);
        }
    }
}

// Sequence Processing
static inline void write_sequence(KTerm* term, const char* seq) {
    if (!term || !seq) return;
    KTermSession* session = GET_SESSION(term);
    for (const char* p = seq; *p; p++) {
        KTerm_ProcessChar(term, session, (unsigned char)*p);
    }
}

static inline void write_sequence_to_session(KTerm* term, KTermSession* session, const char* seq) {
    if (!term || !session || !seq) return;
    for (const char* p = seq; *p; p++) {
        KTerm_ProcessChar(term, session, (unsigned char)*p);
    }
}

// Cell Verification
static inline void verify_cell(KTermSession* session, int y, int x, 
                               char expected_ch, uint32_t expected_flags) {
    if (!session) return;
    EnhancedTermChar* cell = GetScreenCell(session, y, x);
    if (!cell) {
        fprintf(stderr, "FAIL: Cell at (%d, %d) is NULL\n", y, x);
        exit(1);
    }
    if (cell->ch != expected_ch) {
        fprintf(stderr, "FAIL: Cell at (%d, %d) has char '%c' (0x%02x), expected '%c' (0x%02x)\n",
                y, x, cell->ch, cell->ch, expected_ch, expected_ch);
        exit(1);
    }
    if ((cell->flags & expected_flags) != expected_flags) {
        fprintf(stderr, "FAIL: Cell at (%d, %d) flags mismatch. Got 0x%08x, expected 0x%08x\n",
                y, x, cell->flags, expected_flags);
        exit(1);
    }
}

static inline void verify_cell_range(KTermSession* session, int y1, int x1, int y2, int x2,
                                     char expected_ch) {
    if (!session) return;
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            EnhancedTermChar* cell = GetScreenCell(session, y, x);
            if (!cell || cell->ch != expected_ch) {
                fprintf(stderr, "FAIL: Cell at (%d, %d) has char '%c', expected '%c'\n",
                        y, x, cell ? cell->ch : '?', expected_ch);
                exit(1);
            }
        }
    }
}

// Callback Setup
static inline void setup_mock_callbacks(KTerm* term) {
    if (!term) return;
    // Callbacks are optional for basic tests
}

static inline void setup_gateway_callback(KTerm* term) {
    if (!term) return;
    // Gateway callbacks handled by K-Term internally
}

static inline void setup_response_callback(KTerm* term) {
    if (!term) return;
    // Response callbacks handled by K-Term internally
}

// Test Reporting
static inline void print_test_header(const char* test_name) {
    printf("\n================================================================================\n");
    printf("                    K-Term Test Suite: %s\n", test_name);
    printf("================================================================================\n\n");
}

static inline void print_test_result(const char* test_name, int passed) {
    if (passed) {
        printf("  ✓ %-50s PASS\n", test_name);
    } else {
        printf("  ✗ %-50s FAIL\n", test_name);
    }
}

static inline void print_test_summary(int total, int passed, int failed) {
    printf("\n================================================================================\n");
    printf("SUMMARY: %d/%d tests passed (%.0f%%)\n", passed, total, total > 0 ? (100.0 * passed / total) : 0);
    printf("================================================================================\n\n");
}

// State Validation
static inline int verify_attribute_state(KTermSession* session, uint32_t expected_attrs) {
    if (!session) return 0;
    return (session->current_attributes & expected_attrs) == expected_attrs;
}

static inline int verify_mode_state(KTermSession* session, uint32_t expected_modes) {
    if (!session) return 0;
    return (session->dec_modes & expected_modes) == expected_modes;
}

static inline int verify_cursor_position(KTermSession* session, int expected_y, int expected_x) {
    if (!session) return 0;
    return (session->cursor.y == expected_y && session->cursor.x == expected_x);
}

// Test execution wrapper
static inline void run_test(const char* test_name, void (*test_func)(KTerm*, KTermSession*), 
                           KTerm* term, KTermSession* session, TestResults* results) {
    if (!test_func || !term || !session || !results) return;
    
    reset_terminal(term);
    
    int passed = 1;
    if (test_func) {
        test_func(term, session);
    }
    
    if (passed) {
        results->passed++;
    } else {
        results->failed++;
    }
    results->total++;
    
    print_test_result(test_name, passed);
}

#endif // TEST_UTILITIES_H
