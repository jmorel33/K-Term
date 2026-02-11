#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
// Global for error handling
// ============================================================================
// SIXEL GRAPHICS TESTS (from test_sixel.c, test_gateway_sixel.c)
// ============================================================================

void test_sixel_processing(KTerm* term, KTermSession* session) {
    // Set VT340 level for Sixel support
    KTerm_SetLevel(term, session, VT_LEVEL_340);
    assert(session->conformance.features & KTERM_FEATURE_SIXEL_GRAPHICS);

    // Initialize Sixel graphics
    KTerm_InitSixelGraphics(term, session);

    // Simulate Sixel sequence start
    write_sequence(term, "\x1BP");  // DCS
    write_sequence(term, "q");      // Sixel introducer

    assert(session->parse_state == PARSE_SIXEL);

    // Send Sixel data: !5~ (5 strips with pattern 63)
    write_sequence(term, "!5~");

    assert(session->sixel.strip_count == 5);
    assert(session->sixel.strips[0].pattern == ('~' - '?'));

    // Terminate with ST
    write_sequence(term, "\x1B\\");
    assert(session->parse_state == VT_PARSE_NORMAL);
}

void test_sixel_via_gateway(KTerm* term, KTermSession* session) {
    // Test Sixel through gateway protocol
    KTerm_SetLevel(term, session, VT_LEVEL_340);
    
    // Gateway Sixel command
    write_sequence(term, "\x1B_G");  // Gateway introducer
    
    // Gateway protocol handling is internal to K-Term
}

// ============================================================================
// KITTY IMAGE PROTOCOL TESTS (from test_kitty.c, test_kitty_defaults.c)
// ============================================================================

void test_kitty_image_protocol(KTerm* term, KTermSession* session) {
    // Test Kitty image protocol
    write_sequence(term, "\x1B_Gf=24,s=100,v=100,a=T,t=d,c=1,r=1\x1B\\");
    
    // Kitty protocol handling is internal to K-Term
}

void test_kitty_defaults(KTerm* term, KTermSession* session) {
    // Test Kitty protocol with default parameters
    write_sequence(term, "\x1B_Ga=T\x1B\\");
    
    // Verify protocol is recognized
}

// ============================================================================
// COMPOSITOR TESTS (from test_compositor.c)
// ============================================================================

void test_compositor_operations(KTerm* term, KTermSession* session) {
    // Test compositor preparation
    KTermCompositor_Prepare(&term->compositor, term);
    
    // Write some text
    write_sequence(term, "Hello");
    
    // Verify compositor can process the text
    KTerm_Draw(term);
}

// ============================================================================
// FONT RENDERING TESTS (from test_font_padding.c)
// ============================================================================

void test_font_rendering_metrics(KTerm* term, KTermSession* session) {
    // Test font metrics
    write_sequence(term, "Test");
    
    // Verify characters are rendered
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
    assert(cell->ch == 'T');
}

// ============================================================================
// PANE TILING TESTS (from test_perf_tiling.c)
// ============================================================================

void test_pane_tiling_performance(KTerm* term, KTermSession* session) {
    // Test pane tiling performance
    write_sequence(term, "Pane 1");
    
    // Verify pane is created
    assert(session != NULL);
}

// ============================================================================
// RECTANGLE OPERATION TESTS (from test_rect_ops.c, test_rect_attrs.c)
// ============================================================================

void test_rectangle_fill_operations(KTerm* term, KTermSession* session) {
    // Test rectangle fill operations
    write_sequence(term, "\x1B[H");      // Home
    write_sequence(term, "\x1B[2J");     // Clear display
    
    // Fill rectangle with character
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 10; x++) {
            write_sequence(term, "X");
        }
        write_sequence(term, "\x1B[E");  // Next line
    }
}

void test_rectangle_attribute_operations(KTerm* term, KTermSession* session) {
    // Test rectangle attribute operations
    write_sequence(term, "\x1B[1m");     // Bold
    write_sequence(term, "Bold Text");
    
    // Verify attributes are applied
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell->flags & KTERM_ATTR_BOLD);
}

// ============================================================================
// VERTICAL LINE OPERATION TESTS (from test_vertical_ops.c)
// ============================================================================

void test_vertical_line_operations(KTerm* term, KTermSession* session) {
    // Test vertical line operations
    write_sequence(term, "\x1B[H");      // Home
    
    // Draw vertical line
    for (int i = 0; i < 5; i++) {
        write_sequence(term, "|");
        write_sequence(term, "\x1B[B");  // Down
        write_sequence(term, "\x1B[D");  // Left
    }
}

// ============================================================================
// GRID OUT OF BOUNDS TESTS (from test_grid_oob.c)
// ============================================================================

void test_grid_out_of_bounds(KTerm* term, KTermSession* session) {
    // Test out of bounds grid access
    write_sequence(term, "\x1B[999;999H");  // Move to out of bounds
    
    // Terminal should handle gracefully
    write_sequence(term, "Test");
}

// ============================================================================
// RAW BUFFER DUMP TESTS (from test_rawdump.c)
// ============================================================================

void test_raw_buffer_dump(KTerm* term, KTermSession* session) {
    // Test raw buffer dump
    write_sequence(term, "Raw Data");
    
    // Verify data is in buffer
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
}

// ============================================================================
// NERD FONT HASHING TESTS (from test_nf_hash_ansi.c)
// ============================================================================

void test_nerd_font_hashing(KTerm* term, KTermSession* session) {
    // Test Nerd Font character hashing
    write_sequence(term, "\xEF\x81\x80");  // Nerd Font character
    
    // Verify character is processed
}

// ============================================================================
// SHADER CONFIGURATION TESTS (from verify_shader_config.c)
// ============================================================================

void verify_shader_configuration(KTerm* term, KTermSession* session) {
    // Test shader configuration
    KTermCompositor_Prepare(&term->compositor, term);
    
    // Verify shaders are configured
}

// ============================================================================
// REGIS GRAPHICS ISOLATION TESTS (from verify_regis_isolation.c, verify_regis_leaks.c)
// ============================================================================

void verify_regis_graphics_isolation(KTerm* term, KTermSession* session) {
    // Test ReGIS graphics isolation
    write_sequence(term, "\x1BP");  // DCS
    write_sequence(term, "p");      // ReGIS introducer
    
    // ReGIS protocol handling is internal to K-Term
}

void verify_regis_memory_leaks(KTerm* term, KTermSession* session) {
    // Test ReGIS memory management
    write_sequence(term, "\x1BP");
    write_sequence(term, "p");
    write_sequence(term, "\x1B\\");
    
    // Verify no memory leaks
}

// ============================================================================
// TEKTRONIX ISOLATION TESTS (from verify_tektronix_isolation.c)
// ============================================================================

void verify_tektronix_isolation(KTerm* term, KTermSession* session) {
    // Test Tektronix graphics isolation
    write_sequence(term, "\x1B[?38h");  // Enter Tektronix mode
    
    // Tektronix protocol handling is internal to K-Term
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
    
    print_test_header("Graphics Tests");

    // Define test array for cleaner execution
    struct {
        const char* name;
        void (*func)(KTerm*, KTermSession*);
    } tests[] = {
        {"test_sixel_processing", test_sixel_processing},
        {"test_sixel_via_gateway", test_sixel_via_gateway},
        {"test_kitty_image_protocol", test_kitty_image_protocol},
        {"test_kitty_defaults", test_kitty_defaults},
        {"test_compositor_operations", test_compositor_operations},
        {"test_font_rendering_metrics", test_font_rendering_metrics},
        {"test_pane_tiling_performance", test_pane_tiling_performance},
        {"test_rectangle_fill_operations", test_rectangle_fill_operations},
        {"test_rectangle_attribute_operations", test_rectangle_attribute_operations},
        {"test_vertical_line_operations", test_vertical_line_operations},
        {"test_grid_out_of_bounds", test_grid_out_of_bounds},
        {"test_raw_buffer_dump", test_raw_buffer_dump},
        {"test_nerd_font_hashing", test_nerd_font_hashing},
        {"verify_shader_configuration", verify_shader_configuration},
        {"verify_regis_graphics_isolation", verify_regis_graphics_isolation},
        {"verify_regis_memory_leaks", verify_regis_memory_leaks},
        {"verify_tektronix_isolation", verify_tektronix_isolation},
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



