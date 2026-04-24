#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

int test_event_overflow_flag(KTerm* term, KTermSession* session) {
    (void)session;

    // 1. Initially, overflow should be false
    if (KTerm_IsEventOverflow(term)) {
        fprintf(stderr, "FAIL: Overflow detected on fresh terminal\n");
        return 0;
    }

    // 2. Push data to trigger overflow
    // KTERM_INPUT_PIPELINE_SIZE is 1MB
    size_t overflow_size = KTERM_INPUT_PIPELINE_SIZE + 100;
    char* data = malloc(overflow_size);
    if (!data) {
        fprintf(stderr, "FAIL: Could not allocate memory for test data\n");
        return 0;
    }
    memset(data, 'A', overflow_size);

    size_t pushed = KTerm_PushInput(term, data, overflow_size);
    free(data);

    // 3. Check if overflow is now true
    if (!KTerm_IsEventOverflow(term)) {
        fprintf(stderr, "FAIL: Overflow NOT detected after pushing %zu bytes (pushed %zu)\n", overflow_size, pushed);
        return 0;
    }

    if (pushed >= overflow_size) {
        fprintf(stderr, "FAIL: PushInput did not indicate clamping on overflow (pushed %zu, expected < %zu)\n", pushed, overflow_size);
        return 0;
    }

    // 4. Clear events and check if overflow is reset
    KTerm_ClearEvents(term);
    if (KTerm_IsEventOverflow(term)) {
        fprintf(stderr, "FAIL: Overflow flag still set after KTerm_ClearEvents\n");
        return 0;
    }

    return 1;
}

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

    print_test_header("Event Overflow Tests");

    run_test("test_event_overflow_flag", test_event_overflow_flag, term, session, &results);

    destroy_test_term(term);

    print_test_summary(results.total, results.passed, results.failed);

    return results.failed > 0 ? 1 : 0;
}
