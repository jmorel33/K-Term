#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "mock_situation.h"
#include <assert.h>
#include <stdio.h>

void test_signed_params(void) {
    printf("Testing Signed Parameters in Parser...\n");
    KTermConfig config = {0};
    config.width = 80;
    config.height = 25;
    KTerm* term = KTerm_Create(config);
    KTermSession* session = &term->sessions[term->active_session];

    // 1. Test Signed Param in CSI (Permissive Mode)
    // Default strict_mode is false.
    int params[16];
    // KTerm_ParseCSIParams takes param string (without CSI prefix usually, but handles prefix)
    int count = KTerm_ParseCSIParams(term, "10;-5;20", params, 16);

    printf("Parsed %d params: %d, %d, %d\n", count, params[0], params[1], params[2]);
    assert(count == 3);
    assert(params[0] == 10);
    assert(params[1] == -5);
    assert(params[2] == 20);

    // 2. Test Clamping in Strict Mode
    session->conformance.strict_mode = true;
    count = KTerm_ParseCSIParams(term, "10;-5;20", params, 16);
    printf("Strict Parsed: %d, %d, %d\n", params[0], params[1], params[2]);
    assert(count == 3);
    assert(params[0] == 10);
    assert(params[1] == 0); // Should be clamped
    assert(params[2] == 20);

    printf("SUCCESS: Signed Params passed.\n");
    KTerm_Destroy(term);
}

int main() {
    test_signed_params();
    return 0;
}
