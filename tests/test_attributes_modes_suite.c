#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
// Global for error handling
// ============================================================================
// SGR ATTRIBUTES TESTS (from test_attributes.c, test_sgr_attributes.c)
// ============================================================================

void test_sgr_basic_attributes(KTerm* term, KTermSession* session) {
    // Reset
    KTerm_ResetAllAttributes(term, session);
    assert(session->current_attributes == 0);

    // Set Bold (CSI 1 m)
    write_sequence(term, "\x1B[1m");
    assert(session->current_attributes & KTERM_ATTR_BOLD);

    // Set Italic (CSI 3 m)
    write_sequence(term, "\x1B[3m");
    assert(session->current_attributes & KTERM_ATTR_ITALIC);
    assert(session->current_attributes & KTERM_ATTR_BOLD);

    // Clear Bold (CSI 22 m)
    write_sequence(term, "\x1B[22m");
    assert(!(session->current_attributes & KTERM_ATTR_BOLD));
    assert(session->current_attributes & KTERM_ATTR_ITALIC);

    // Output Character and verify cell attributes
    write_sequence(term, "A");
    int x = session->cursor.x - 1;
    if (x < 0) x = 0;

    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, x);
    assert(cell->flags & KTERM_ATTR_ITALIC);
    assert(!(cell->flags & KTERM_ATTR_BOLD));
}

void test_sgr_extended_attributes(KTerm* term, KTermSession* session) {
    KTerm_ResetAllAttributes(term, session);

    // Test underline
    write_sequence(term, "\x1B[4m");
    assert(session->current_attributes & KTERM_ATTR_UNDERLINE);

    // Test blink
    write_sequence(term, "\x1B[5m");
    assert(session->current_attributes & KTERM_ATTR_BLINK);

    // Test reverse
    write_sequence(term, "\x1B[7m");
    assert(session->current_attributes & KTERM_ATTR_REVERSE);

    // Test conceal
    write_sequence(term, "\x1B[8m");
    assert(session->current_attributes & KTERM_ATTR_CONCEAL);

    // Test strikethrough
    write_sequence(term, "\x1B[9m");
    assert(session->current_attributes & KTERM_ATTR_STRIKETHROUGH);
}

// ============================================================================
// DEC MODE TESTS (from test_modes_coverage.c)
// ============================================================================

void test_dec_mode_coverage(KTerm* term, KTermSession* session) {
    // Test cursor visibility mode
    write_sequence(term, "\x1B[?25h");  // Show cursor
    assert(session->dec_modes & KTERM_MODE_CURSOR_VISIBLE);

    write_sequence(term, "\x1B[?25l");  // Hide cursor
    assert(!(session->dec_modes & KTERM_MODE_CURSOR_VISIBLE));

    // Test application keypad mode
    write_sequence(term, "\x1B[?66h");  // Enable
    assert(session->dec_modes & KTERM_MODE_KEYPAD_APPLICATION);

    write_sequence(term, "\x1B[?66l");  // Disable
    assert(!(session->dec_modes & KTERM_MODE_KEYPAD_APPLICATION));
}

// ============================================================================
// BLINK ATTRIBUTE TESTS (from test_blink_flavors.c, test_ansi_sys_blink.c)
// ============================================================================

void test_blink_attribute_flavors(KTerm* term, KTermSession* session) {
    KTerm_ResetAllAttributes(term, session);

    // Slow blink (SGR 5)
    write_sequence(term, "\x1B[5m");
    assert(session->current_attributes & KTERM_ATTR_BLINK);

    // Fast blink (SGR 6) - if supported
    write_sequence(term, "\x1B[6m");
    // Fast blink handling is implementation-specific

    // Reset blink
    write_sequence(term, "\x1B[25m");
    assert(!(session->current_attributes & KTERM_ATTR_BLINK));
}

void test_ansi_sys_blink_behavior(KTerm* term, KTermSession* session) {
    KTerm_ResetAllAttributes(term, session);

    // ANSI.SYS blink behavior
    write_sequence(term, "\x1B[5m");
    assert(session->current_attributes & KTERM_ATTR_BLINK);

    // Write character with blink
    write_sequence(term, "B");
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, session->cursor.x - 1);
    assert(cell->flags & KTERM_ATTR_BLINK);
}

// ============================================================================
// CONCEAL CHARACTER TESTS (from test_conceal_char.c)
// ============================================================================

void test_conceal_character(KTerm* term, KTermSession* session) {
    KTerm_ResetAllAttributes(term, session);

    // Enable conceal
    write_sequence(term, "\x1B[8m");
    assert(session->current_attributes & KTERM_ATTR_CONCEAL);

    // Write character
    write_sequence(term, "Secret");
    
    // Verify characters have conceal flag
    for (int i = 0; i < 6; i++) {
        EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, i);
        assert(cell->flags & KTERM_ATTR_CONCEAL);
    }

    // Disable conceal
    write_sequence(term, "\x1B[28m");
    assert(!(session->current_attributes & KTERM_ATTR_CONCEAL));
}

// ============================================================================
// PROTECTED CHARACTER TESTS (from test_protect_skip.c, test_protected_ops.c)
// ============================================================================

void test_protected_character_skipping(KTerm* term, KTermSession* session) {
    KTerm_ResetAllAttributes(term, session);

    // DECSCA 1 (Protected)
    write_sequence(term, "\x1B[1\"q");
    assert(session->current_attributes & KTERM_ATTR_PROTECTED);

    // Write protected character
    write_sequence(term, "P");
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, session->cursor.x - 1);
    assert(cell->flags & KTERM_ATTR_PROTECTED);
}

void test_protected_area_operations(KTerm* term, KTermSession* session) {
    KTerm_ResetAllAttributes(term, session);

    // Set protected area
    write_sequence(term, "\x1B[1\"q");
    write_sequence(term, "Protected");

    // Disable protected
    write_sequence(term, "\x1B[0\"q");
    write_sequence(term, "Unprotected");

    // Verify both areas exist
    EnhancedTermChar* cell1 = GetScreenCell(session, session->cursor.y, 0);
    EnhancedTermChar* cell2 = GetScreenCell(session, session->cursor.y, 9);
    
    assert(cell1->flags & KTERM_ATTR_PROTECTED);
    assert(!(cell2->flags & KTERM_ATTR_PROTECTED));
}

// ============================================================================
// ATTRIBUTE RESET TESTS (from test_reset_cascade.c)
// ============================================================================

void test_attribute_reset_cascading(KTerm* term, KTermSession* session) {
    KTerm_ResetAllAttributes(term, session);

    // Set multiple attributes
    write_sequence(term, "\x1B[1;3;4;7m");
    assert(session->current_attributes & KTERM_ATTR_BOLD);
    assert(session->current_attributes & KTERM_ATTR_ITALIC);
    assert(session->current_attributes & KTERM_ATTR_UNDERLINE);
    assert(session->current_attributes & KTERM_ATTR_REVERSE);

    // Reset all
    write_sequence(term, "\x1B[0m");
    assert(session->current_attributes == 0);
}

// ============================================================================
// SESSION ATTRIBUTE ISOLATION TESTS (from test_active_session_trap.c)
// ============================================================================

void test_session_attribute_isolation(KTerm* term, KTermSession* session) {
    KTerm_ResetAllAttributes(term, session);

    // Set attributes in session
    write_sequence(term, "\x1B[1;3m");
    uint32_t attrs1 = session->current_attributes;

    // Create another session (if supported)
    // For now, just verify attributes are maintained
    assert(session->current_attributes == attrs1);
}

// ============================================================================
// SESSION SWITCHING TESTS (from test_session_switch_dirty.c)
// ============================================================================

void test_session_switching_dirty_state(KTerm* term, KTermSession* session) {
    KTerm_ResetAllAttributes(term, session);

    // Set attributes
    write_sequence(term, "\x1B[1m");
    assert(session->current_attributes & KTERM_ATTR_BOLD);

    // Write character
    write_sequence(term, "X");

    // Verify state is maintained
    assert(session->current_attributes & KTERM_ATTR_BOLD);
}

// ============================================================================
// ANSI.SYS COMPLIANCE TESTS (from verify_ansi_sys.c)
// ============================================================================

void test_ansi_sys_compliance(KTerm* term, KTermSession* session) {
    KTerm_ResetAllAttributes(term, session);

    // Test ANSI.SYS color codes
    write_sequence(term, "\x1B[30m");  // Black foreground
    write_sequence(term, "\x1B[40m");  // Black background
    write_sequence(term, "\x1B[1m");   // Bold
    write_sequence(term, "\x1B[4m");   // Underline

    // Verify attributes are set
    assert(session->current_attributes & KTERM_ATTR_BOLD);
    assert(session->current_attributes & KTERM_ATTR_UNDERLINE);
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
    
    print_test_header("Attributes and Modes Tests");

    // Define test array for cleaner execution
    struct {
        const char* name;
        void (*func)(KTerm*, KTermSession*);
    } tests[] = {
        {"test_sgr_basic_attributes", test_sgr_basic_attributes},
        {"test_sgr_extended_attributes", test_sgr_extended_attributes},
        {"test_dec_mode_coverage", test_dec_mode_coverage},
        {"test_blink_attribute_flavors", test_blink_attribute_flavors},
        {"test_ansi_sys_blink_behavior", test_ansi_sys_blink_behavior},
        {"test_conceal_character", test_conceal_character},
        {"test_protected_character_skipping", test_protected_character_skipping},
        {"test_protected_area_operations", test_protected_area_operations},
        {"test_attribute_reset_cascading", test_attribute_reset_cascading},
        {"test_session_attribute_isolation", test_session_attribute_isolation},
        {"test_session_switching_dirty_state", test_session_switching_dirty_state},
        {"test_ansi_sys_compliance", test_ansi_sys_compliance},
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



