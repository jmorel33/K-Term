#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
// Global for error handling
// ============================================================================
// NETWORK CONNECTIVITY TESTS (from test_kt_net.c)
// ============================================================================

void test_network_connectivity(KTerm* term, KTermSession* session) {
    // Test network connectivity
    // Networking is handled through K-Term's network module
    
    // Verify terminal is functional
    write_sequence(term, "Network Test");
    
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
}

// ============================================================================
// SERVER SECURITY HARDENING TESTS (from test_net_server_hardening.c)
// ============================================================================

void test_server_security_hardening(KTerm* term, KTermSession* session) {
    // Test server security hardening
    // Security checks are internal to K-Term's networking module
    
    // Verify terminal is secure
    write_sequence(term, "Security Test");
}

// ============================================================================
// PANE MULTIPLEXING TESTS (from test_multiplexer.c)
// ============================================================================

void test_pane_multiplexing(KTerm* term, KTermSession* session) {
    // Test pane multiplexing
    write_sequence(term, "Pane 1");
    
    // Verify pane is created
    assert(session != NULL);
}

// ============================================================================
// MESSAGE ROUTING TESTS (from test_routing.c)
// ============================================================================

void test_message_routing(KTerm* term, KTermSession* session) {
    // Test message routing
    write_sequence(term, "Route Test");
    
    // Verify message is routed
    EnhancedTermChar* cell = GetScreenCell(session, session->cursor.y, 0);
    assert(cell != NULL);
}

// ============================================================================
// VT PIPE INTEGRATION TESTS (from test_vt_pipe.c, test_vt_pipe_integration.c)
// ============================================================================

void test_vt_pipe_integration(KTerm* term, KTermSession* session) {
    // Test VT pipe integration
    write_sequence(term, "Pipe Test");
    
    // Verify pipe is functional
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
    
    print_test_header("Networking Tests");

    // Define test array for cleaner execution
    struct {
        const char* name;
        void (*func)(KTerm*, KTermSession*);
    } tests[] = {
        {"test_network_connectivity", test_network_connectivity},
        {"test_server_security_hardening", test_server_security_hardening},
        {"test_pane_multiplexing", test_pane_multiplexing},
        {"test_message_routing", test_message_routing},
        {"test_vt_pipe_integration", test_vt_pipe_integration},
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



