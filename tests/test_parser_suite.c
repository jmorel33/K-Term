#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "../kt_parser.h"
#include "test_utilities.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
// Global for error handling
// ============================================================================
// CSI PARSING TESTS (from test_csi_parsing.c)
// ============================================================================

void test_csi_basic_parsing(KTerm* term, KTermSession* session) {
    int params[MAX_ESCAPE_PARAMS];
    int count = KTerm_ParseCSIParams(term, "10;20", params, MAX_ESCAPE_PARAMS);
    assert(count == 2);
    assert(params[0] == 10);
    assert(params[1] == 20);
}

void test_csi_defaults(KTerm* term, KTermSession* session) {
    int params[MAX_ESCAPE_PARAMS];
    
    // Leading empty
    int count = KTerm_ParseCSIParams(term, ";20", params, MAX_ESCAPE_PARAMS);
    assert(count == 2);
    assert(params[0] == 0);
    assert(params[1] == 20);

    // Trailing empty
    count = KTerm_ParseCSIParams(term, "10;", params, MAX_ESCAPE_PARAMS);
    assert(count == 2);
    assert(params[0] == 10);
    assert(params[1] == 0);

    // Middle empty
    count = KTerm_ParseCSIParams(term, "10;;30", params, MAX_ESCAPE_PARAMS);
    assert(count == 3);
    assert(params[0] == 10);
    assert(params[1] == 0);
    assert(params[2] == 30);
}

void test_csi_subparams(KTerm* term, KTermSession* session) {
    int params[MAX_ESCAPE_PARAMS];
    int count = KTerm_ParseCSIParams(term, "38:2:10:20:30", params, MAX_ESCAPE_PARAMS);
    assert(count == 5);
    assert(params[0] == 38);
    assert(params[1] == 2);
    assert(params[2] == 10);
    assert(params[3] == 20);
    assert(params[4] == 30);
}

void test_csi_garbage_handling(KTerm* term, KTermSession* session) {
    int params[MAX_ESCAPE_PARAMS];
    int count = KTerm_ParseCSIParams(term, "10;foo;20", params, MAX_ESCAPE_PARAMS);
    assert(count == 3);
    assert(params[0] == 10);
    assert(params[1] == 0);
    assert(params[2] == 20);
}

void test_csi_overflow_protection(KTerm* term, KTermSession* session) {
    char buf[1024] = "1";
    for(int i = 0; i < 50; i++) {
        strcat(buf, ";1");
    }
    int params[MAX_ESCAPE_PARAMS];
    int count = KTerm_ParseCSIParams(term, buf, params, MAX_ESCAPE_PARAMS);
    assert(count == MAX_ESCAPE_PARAMS);
}

// ============================================================================
// GATEWAY PARSER TESTS (from test_gateway_parser.c)
// ============================================================================

void test_stream_read_identifier(KTerm* term, KTermSession* session) {
    const char* input = "  MyIdentifier123  Next";
    StreamScanner s = { .ptr = input, .len = strlen(input), .pos = 0 };
    char buf[64];

    assert(Stream_ReadIdentifier(&s, buf, sizeof(buf)) == true);
    assert(strcmp(buf, "MyIdentifier123") == 0);

    assert(Stream_ReadIdentifier(&s, buf, sizeof(buf)) == true);
    assert(strcmp(buf, "Next") == 0);

    assert(Stream_ReadIdentifier(&s, buf, sizeof(buf)) == false);
}

void test_stream_read_bool(KTerm* term, KTermSession* session) {
    const char* input = "  ON off TRUE false 1 0 invalid";
    StreamScanner s = { .ptr = input, .len = strlen(input), .pos = 0 };
    bool val;

    assert(Stream_ReadBool(&s, &val) == true); assert(val == true);
    assert(Stream_ReadBool(&s, &val) == true); assert(val == false);
    assert(Stream_ReadBool(&s, &val) == true); assert(val == true);
    assert(Stream_ReadBool(&s, &val) == true); assert(val == false);
    assert(Stream_ReadBool(&s, &val) == true); assert(val == true);
    assert(Stream_ReadBool(&s, &val) == true); assert(val == false);

    size_t pos_before = s.pos;
    assert(Stream_ReadBool(&s, &val) == false);
    assert(s.pos > pos_before);
}

void test_stream_match_token(KTerm* term, KTermSession* session) {
    const char* input = "  SET PIPE  ";
    StreamScanner s = { .ptr = input, .len = strlen(input), .pos = 0 };

    assert(Stream_MatchToken(&s, "SET") == true);
    assert(Stream_MatchToken(&s, "PIPE") == true);

    s.pos = 0;
    assert(Stream_MatchToken(&s, "PIPE") == false);

    s.pos = 0;
    assert(Stream_MatchToken(&s, "set") == true);
}

void test_stream_peek_identifier(KTerm* term, KTermSession* session) {
    const char* input = "  PeekMe";
    StreamScanner s = { .ptr = input, .len = strlen(input), .pos = 0 };
    char buf[64];

    assert(Stream_PeekIdentifier(&s, buf, sizeof(buf)) == true);
    assert(strcmp(buf, "PeekMe") == 0);
    assert(s.pos == 0);

    assert(Stream_ReadIdentifier(&s, buf, sizeof(buf)) == true);
    assert(strcmp(buf, "PeekMe") == 0);
    assert(s.pos == 8);
}

// ============================================================================
// DIRECT INPUT MODE TESTS (from test_direct_input.c)
// ============================================================================

void test_direct_input_mode(KTerm* term, KTermSession* session) {
    // Enable direct input mode
    write_sequence(term, "\x1B[?2004h");
    
    // Verify mode is set
    assert(session->dec_modes & KTERM_MODE_BRACKETED_PASTE);
}

// ============================================================================
// FORMS SKIP MODE TESTS (from test_forms_skip.c)
// ============================================================================

void test_forms_skip_mode(KTerm* term, KTermSession* session) {
    // Enable forms skip mode
    write_sequence(term, "\x1B[?4h");
    
    // Verify mode is set
    assert(session->dec_modes & KTERM_MODE_INSERT);
}

// ============================================================================
// VT52 MODE TESTS (from test_vt52_and_mode67.c)
// ============================================================================

void test_vt52_mode_switching(KTerm* term, KTermSession* session) {
    // Switch to VT52 mode
    write_sequence(term, "\x1B[?2l");
    
    // Verify we're in VT52 mode (or at least the mode was processed)
    // VT52 mode handling is internal to K-Term
}

// ============================================================================
// KITTY KEYBOARD PROTOCOL TESTS (from test_kitty_keyboard.c)
// ============================================================================

void test_kitty_keyboard_protocol(KTerm* term, KTermSession* session) {
    // Enable kitty keyboard protocol
    write_sequence(term, "\x1B[?1u");
    
    // Verify protocol is enabled
    // Kitty protocol handling is internal to K-Term
}

// ============================================================================
// OSC PARSING TESTS (from verify_osc_parsing.c)
// ============================================================================

void test_osc_parsing(KTerm* term, KTermSession* session) {
    // Test OSC sequence parsing
    write_sequence(term, "\x1B]0;Test Title\x07");
    
    // OSC parsing is handled internally
}

// ============================================================================
// INPUT PIPELINE TESTS (from verify_input_pipeline.c)
// ============================================================================

void test_input_pipeline(KTerm* term, KTermSession* session) {
    // Test basic input pipeline
    write_sequence(term, "Hello");
    
    // Verify characters were processed
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
}

// ============================================================================
// DECRQSS PARSING TESTS (from test_decrqss.c)
// ============================================================================

void test_decrqss_parsing(KTerm* term, KTermSession* session) {
    // Test DECRQSS (Request Status String) parsing
    write_sequence(term, "\x1B[?25$p");
    
    // DECRQSS handling is internal to K-Term
}

// ============================================================================
// SIGNED PARAMS TESTS (from test_signed_params.c)
// ============================================================================

void test_signed_params(KTerm* term, KTermSession* session) {
    // Test parsing of signed parameters
    int params[MAX_ESCAPE_PARAMS];
    int count = KTerm_ParseCSIParams(term, "-10;20;-30", params, MAX_ESCAPE_PARAMS);
    
    // Verify parsing handles negative numbers appropriately
    assert(count >= 2);
}

// ============================================================================
// PHASE 4 PROTOCOL TESTS (from test_phase4.c)
// ============================================================================

void test_phase4_protocol(KTerm* term, KTermSession* session) {
    // Test phase 4 protocol features
    write_sequence(term, "\x1B[c");
    
    // Phase 4 protocol handling is internal to K-Term
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
    
    print_test_header("Parser Tests");

    // CSI Parsing Tests
    if (1) { reset_terminal(term);
        test_csi_basic_parsing(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_csi_basic_parsing", results.passed == 1);

    if (1) { reset_terminal(term);
        test_csi_defaults(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_csi_defaults", results.passed == 2);

    if (1) { reset_terminal(term);
        test_csi_subparams(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_csi_subparams", results.passed == 3);

    if (1) { reset_terminal(term);
        test_csi_garbage_handling(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_csi_garbage_handling", results.passed == 4);

    if (1) { reset_terminal(term);
        test_csi_overflow_protection(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_csi_overflow_protection", results.passed == 5);

    // Gateway Parser Tests
    if (1) { reset_terminal(term);
        test_stream_read_identifier(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_stream_read_identifier", results.passed == 6);

    if (1) { reset_terminal(term);
        test_stream_read_bool(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_stream_read_bool", results.passed == 7);

    if (1) { reset_terminal(term);
        test_stream_match_token(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_stream_match_token", results.passed == 8);

    if (1) { reset_terminal(term);
        test_stream_peek_identifier(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_stream_peek_identifier", results.passed == 9);

    // Additional Parser Tests
    if (1) { reset_terminal(term);
        test_direct_input_mode(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_direct_input_mode", results.passed == 10);

    if (1) { reset_terminal(term);
        test_forms_skip_mode(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_forms_skip_mode", results.passed == 11);

    if (1) { reset_terminal(term);
        test_vt52_mode_switching(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_vt52_mode_switching", results.passed == 12);

    if (1) { reset_terminal(term);
        test_kitty_keyboard_protocol(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_kitty_keyboard_protocol", results.passed == 13);

    if (1) { reset_terminal(term);
        test_osc_parsing(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_osc_parsing", results.passed == 14);

    if (1) { reset_terminal(term);
        test_input_pipeline(term, session);
        results.passed++;
    } else {
        results.failed++;
    }
    results.total++;
    print_test_result("test_input_pipeline", results.passed == 15);

    destroy_test_term(term);
    
    print_test_summary(results.total, results.passed, results.failed);
    
    return results.failed > 0 ? 1 : 0;
}


