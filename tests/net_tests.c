#define KTERM_IMPLEMENTATION
// KTERM_NET_IMPLEMENTATION is defined by kterm.h inside KTERM_IMPLEMENTATION block
#define KTERM_ENABLE_GATEWAY
#define KTERM_TESTING

#include "../kterm.h"
#include "test_utilities.h"
#include <assert.h>
#include <stdio.h>

// Tests for Network Diagnostics API and Gateway Integration

void test_mtu_probe_api(KTerm* term, KTermSession* session) {
    printf("  Testing MTU Probe API...\n");

    // Test API Call
    bool res = KTerm_Net_MTUProbe(term, session, "127.0.0.1", true, 1000, 1500, NULL, NULL);
    if (!res) {
        fprintf(stderr, "    Failed to start MTU Probe\n");
        exit(1);
    }

    // Verify Context Creation
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->mtu_probe) {
        fprintf(stderr, "    MTU Probe context not created\n");
        exit(1);
    }

    // Verify Parameters
    if (strcmp(net->mtu_probe->host, "127.0.0.1") != 0) {
        fprintf(stderr, "    Host mismatch: %s\n", net->mtu_probe->host);
        exit(1);
    }
    if (net->mtu_probe->df != true) {
        fprintf(stderr, "    DF flag mismatch\n");
        exit(1);
    }
    if (net->mtu_probe->min_size != 1000 || net->mtu_probe->max_size != 1500) {
        fprintf(stderr, "    Size range mismatch\n");
        exit(1);
    }

    // Verify State (Should be 2 = SOCKET or similar, depending on initial flow)
    // 0=IDLE, 1=RESOLVE, 2=SOCKET
    if (net->mtu_probe->state < 1) {
         fprintf(stderr, "    Invalid initial state: %d\n", net->mtu_probe->state);
         exit(1);
    }

    // Cleanup
    KTerm_Net_DestroyContext(session);
    if (session->user_data != NULL) {
        fprintf(stderr, "    Cleanup failed\n");
        exit(1);
    }
}

void test_frag_test_api(KTerm* term, KTermSession* session) {
    printf("  Testing Frag Test API...\n");

    bool res = KTerm_Net_FragTest(term, session, "localhost", 2000, 3, NULL, NULL);
    if (!res) {
        fprintf(stderr, "    Failed to start Frag Test\n");
        exit(1);
    }

    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->frag_test) {
        fprintf(stderr, "    Frag Test context not created\n");
        exit(1);
    }

    if (net->frag_test->size != 2000) {
        fprintf(stderr, "    Size mismatch\n");
        exit(1);
    }
    if (net->frag_test->fragments != 3) {
        fprintf(stderr, "    Fragments mismatch\n");
        exit(1);
    }

    KTerm_Net_DestroyContext(session);
}

void test_ping_ext_api(KTerm* term, KTermSession* session) {
    printf("  Testing Extended Ping API...\n");

    bool res = KTerm_Net_PingExt(term, session, "8.8.8.8", 5, 200, 64, true, NULL, NULL);
    if (!res) {
        fprintf(stderr, "    Failed to start Ping Ext\n");
        exit(1);
    }

    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->ping_ext) {
        fprintf(stderr, "    Ping Ext context not created\n");
        exit(1);
    }

    if (net->ping_ext->count != 5) {
        fprintf(stderr, "    Count mismatch\n");
        exit(1);
    }
    if (net->ping_ext->interval_ms != 200) {
        fprintf(stderr, "    Interval mismatch\n");
        exit(1);
    }
    if (net->ping_ext->graph != true) {
        fprintf(stderr, "    Graph flag mismatch\n");
        exit(1);
    }

    KTerm_Net_DestroyContext(session);
}

void test_gateway_parsing_mtu(KTerm* term, KTermSession* session) {
    printf("  Testing Gateway Parsing (MTU Probe)...\n");

    // Command: EXT;net;mtu_probe;target=1.1.1.1;df=1;start_size=500;max_size=1400
    // DCS GATE (Escape P GATE)
    const char* seq = "\x1BPGATE;KTERM;1;EXT;net;mtu_probe;target=1.1.1.1;df=1;start_size=500;max_size=1400\x1B\\";

    write_sequence(term, seq);

    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->mtu_probe) {
        fprintf(stderr, "    MTU Probe not triggered via Gateway\n");
        exit(1);
    }

    if (strcmp(net->mtu_probe->host, "1.1.1.1") != 0) {
        fprintf(stderr, "    Host mismatch: %s\n", net->mtu_probe->host);
        exit(1);
    }
    if (net->mtu_probe->min_size != 500) {
        fprintf(stderr, "    Start size mismatch\n");
        exit(1);
    }
    if (net->mtu_probe->max_size != 1400) {
        fprintf(stderr, "    Max size mismatch\n");
        exit(1);
    }

    KTerm_Net_DestroyContext(session);
}

void test_gateway_parsing_frag(KTerm* term, KTermSession* session) {
    printf("  Testing Gateway Parsing (Frag Test)...\n");

    const char* seq = "\x1BPGATE;KTERM;1;EXT;net;frag_test;target=10.0.0.1;size=4000;fragments=4\x1B\\";
    write_sequence(term, seq);

    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->frag_test) {
        fprintf(stderr, "    Frag Test not triggered via Gateway\n");
        exit(1);
    }

    if (strcmp(net->frag_test->host, "10.0.0.1") != 0) {
        fprintf(stderr, "    Host mismatch\n");
        exit(1);
    }
    if (net->frag_test->size != 4000) {
        fprintf(stderr, "    Size mismatch\n");
        exit(1);
    }
    if (net->frag_test->fragments != 4) {
        fprintf(stderr, "    Fragments mismatch\n");
        exit(1);
    }

    KTerm_Net_DestroyContext(session);
}

void test_gateway_parsing_ping_ext(KTerm* term, KTermSession* session) {
    printf("  Testing Gateway Parsing (Ping Ext)...\n");

    const char* seq = "\x1BPGATE;KTERM;1;EXT;net;ping_ext;target=google.com;count=20;interval=0.5;graph=1\x1B\\";
    write_sequence(term, seq);

    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->ping_ext) {
        fprintf(stderr, "    Ping Ext not triggered via Gateway\n");
        exit(1);
    }

    if (strcmp(net->ping_ext->host, "google.com") != 0) {
        fprintf(stderr, "    Host mismatch\n");
        exit(1);
    }
    if (net->ping_ext->count != 20) {
        fprintf(stderr, "    Count mismatch\n");
        exit(1);
    }
    // interval 0.5 sec -> 500 ms
    if (net->ping_ext->interval_ms != 500) {
        fprintf(stderr, "    Interval mismatch (expected 500, got %d)\n", net->ping_ext->interval_ms);
        exit(1);
    }
    if (!net->ping_ext->graph) {
        fprintf(stderr, "    Graph flag mismatch\n");
        exit(1);
    }

    KTerm_Net_DestroyContext(session);
}

void test_cancel_diag(KTerm* term, KTermSession* session) {
    printf("  Testing Cancel Diag...\n");

    // Start something
    KTerm_Net_MTUProbe(term, session, "1.1.1.1", false, 0, 0, NULL, NULL);
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->mtu_probe) { fprintf(stderr, "Setup failed\n"); exit(1); }

    // Send Cancel
    // EXT;net;cancel_diag
    const char* seq = "\x1BPGATE;KTERM;1;EXT;net;cancel_diag\x1B\\";
    write_sequence(term, seq);

    // Verify
    net = KTerm_Net_GetContext(session);
    // cancel_diag might not destroy the whole context, but it should free mtu_probe
    if (net && net->mtu_probe) {
        fprintf(stderr, "    MTU Probe not cleared after cancel\n");
        exit(1);
    }

    // Also check other contexts are null (they should be)
    if (net && (net->frag_test || net->ping_ext)) {
        fprintf(stderr, "    Other contexts unexpectedly present\n");
        exit(1);
    }

    KTerm_Net_DestroyContext(session);
}

int main() {
    printf("========================================\n");
    printf("Starting Network Diagnostics Tests\n");
    printf("========================================\n");

    KTerm* term = create_test_term(80, 24);
    if (!term) {
        fprintf(stderr, "Failed to create terminal\n");
        return 1;
    }

    KTermSession* session = &term->sessions[0];

    test_mtu_probe_api(term, session);
    test_frag_test_api(term, session);
    test_ping_ext_api(term, session);

    // Reset terminal/parser state for gateway tests
    reset_terminal(term);

    test_gateway_parsing_mtu(term, session);
    test_gateway_parsing_frag(term, session);
    test_gateway_parsing_ping_ext(term, session);
    test_cancel_diag(term, session);

    destroy_test_term(term);

    printf("\nAll Network Tests Passed!\n");
    return 0;
}
