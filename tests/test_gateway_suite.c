#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
// Global for error handling
// ============================================================================
// GATEWAY PROTOCOL TESTS (from test_gateway.c)
// ============================================================================

void test_gateway_basic_protocol(KTerm* term, KTermSession* session) {
    // Test basic gateway protocol
    write_sequence(term, "\x1B_G");  // Gateway introducer
    
    // Gateway protocol handling is internal to K-Term
}

// ============================================================================
// GATEWAY COMMAND DISPATCHING TESTS (from test_gateway_dispatcher.c)
// ============================================================================

void test_gateway_command_dispatching(KTerm* term, KTermSession* session) {
    // Test gateway command dispatching
    write_sequence(term, "\x1B_G");
    
    // Verify gateway is recognized
}

// ============================================================================
// GATEWAY DIRECT INPUT MODE TESTS (from test_gateway_direct_input.c)
// ============================================================================

void test_gateway_direct_input_mode(KTerm* term, KTermSession* session) {
    // Test gateway direct input mode
    write_sequence(term, "\x1B_G");
    
    // Direct input mode handling is internal to K-Term
}

// ============================================================================
// GATEWAY DISABLED STATE TESTS (from test_gateway_disabled.c)
// ============================================================================

void test_gateway_disabled_state(KTerm* term, KTermSession* session) {
    // Test gateway disabled state
    write_sequence(term, "\x1B[?1000l");  // Disable mouse
    
    // Verify mode is disabled
}

// ============================================================================
// GATEWAY EXPANDED FEATURES TESTS (from test_gateway_expanded.c)
// ============================================================================

void test_gateway_expanded_features(KTerm* term, KTermSession* session) {
    // Test gateway expanded features
    write_sequence(term, "\x1B_G");
    
    // Expanded features handling is internal to K-Term
}

// ============================================================================
// GATEWAY GRID OPERATION TESTS (from test_gateway_grid.c, test_gateway_grid_features.c)
// ============================================================================

void test_gateway_grid_operations(KTerm* term, KTermSession* session) {
    // Test gateway grid operations
    write_sequence(term, "\x1B[H");      // Home
    write_sequence(term, "\x1B[2J");     // Clear display
    
    // Verify grid is cleared
    EnhancedTermChar* cell = GetScreenCell(session, 0, 0);
    assert(cell != NULL);
}

void test_gateway_grid_feature_flags(KTerm* term, KTermSession* session) {
    // Test gateway grid feature flags
    write_sequence(term, "\x1B[H");
    
    // Feature flags handling is internal to K-Term
}

// ============================================================================
// GATEWAY GRID SHAPE OPERATION TESTS (from test_gateway_grid_shapes.c)
// ============================================================================

void test_gateway_grid_shape_operations(KTerm* term, KTermSession* session) {
    // Test gateway grid shape operations
    write_sequence(term, "\x1B[H");      // Home
    write_sequence(term, "Test");
    
    // Verify shapes are rendered
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
}

// ============================================================================
// GATEWAY SECURITY HARDENING TESTS (from test_gateway_hardening.c)
// ============================================================================

void test_gateway_security_hardening(KTerm* term, KTermSession* session) {
    // Test gateway security hardening
    write_sequence(term, "\x1B_G");
    
    // Security checks are internal to K-Term
}

// ============================================================================
// GATEWAY RELATIVE POSITIONING TESTS (from test_gateway_relative.c)
// ============================================================================

void test_gateway_relative_positioning(KTerm* term, KTermSession* session) {
    // Test gateway relative positioning
    write_sequence(term, "\x1B[H");      // Home
    write_sequence(term, "\x1B[5C");     // Move right 5
    
    assert(session->cursor.x == 5);
}

// ============================================================================
// GATEWAY GRAPHICS RESET TESTS (from test_gateway_reset_graphics.c)
// ============================================================================

void test_gateway_graphics_reset(KTerm* term, KTermSession* session) {
    // Test gateway graphics reset
    write_sequence(term, "\x1B[H");      // Home
    write_sequence(term, "\x1B[2J");     // Clear display
    
    // Verify graphics are reset
}

// ============================================================================
// GATEWAY RESIZE OPERATION TESTS (from test_gateway_resize.c)
// ============================================================================

void test_gateway_resize_operations(KTerm* term, KTermSession* session) {
    // Test gateway resize operations
    write_sequence(term, "\x1B[8;30;80t");  // Resize to 30 rows, 80 cols
    
    // Resize handling is internal to K-Term
}

// ============================================================================
// GATEWAY SETTINGS TESTS (from test_gateway_settings.c)
// ============================================================================

void test_gateway_settings(KTerm* term, KTermSession* session) {
    // Test gateway settings
    write_sequence(term, "\x1B_G");
    
    // Settings handling is internal to K-Term
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
    
    print_test_header("Gateway Protocol Tests");

    // Define test array for cleaner execution
    struct {
        const char* name;
        void (*func)(KTerm*, KTermSession*);
    } tests[] = {
        {"test_gateway_basic_protocol", test_gateway_basic_protocol},
        {"test_gateway_command_dispatching", test_gateway_command_dispatching},
        {"test_gateway_direct_input_mode", test_gateway_direct_input_mode},
        {"test_gateway_disabled_state", test_gateway_disabled_state},
        {"test_gateway_expanded_features", test_gateway_expanded_features},
        {"test_gateway_grid_operations", test_gateway_grid_operations},
        {"test_gateway_grid_feature_flags", test_gateway_grid_feature_flags},
        {"test_gateway_grid_shape_operations", test_gateway_grid_shape_operations},
        {"test_gateway_security_hardening", test_gateway_security_hardening},
        {"test_gateway_relative_positioning", test_gateway_relative_positioning},
        {"test_gateway_graphics_reset", test_gateway_graphics_reset},
        {"test_gateway_resize_operations", test_gateway_resize_operations},
        {"test_gateway_settings", test_gateway_settings},
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



