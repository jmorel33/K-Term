#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "mock_situation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void test_basic_pipeline(void) {
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);

    // Write 10 chars
    for (int i = 0; i < 10; i++) {
        bool res = KTerm_WriteChar(term, 'A');
        assert(res == true);
    }

    assert(KTerm_GetPendingEventCount(term) == 10);

    KTerm_Update(term); // Should consume events

    assert(KTerm_GetPendingEventCount(term) == 0);

    KTerm_Destroy(term);
    printf("SUCCESS: Basic pipeline works.\n");
}

void test_overflow(void) {
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);

    // Size is KTERM_INPUT_PIPELINE_SIZE
    // Note: KTerm's internal buffer is fixed, so we can't change it easily for tests
    // unless we modify kterm.h or it supports dynamic sizing (which is the goal of this task).
    int capacity = KTERM_INPUT_PIPELINE_SIZE;

    printf("Filling pipeline of size %d...\n", capacity);

    // Fill up to capacity - 1 (Circular buffer typically uses one slot for empty/full distinction)
    for (int i = 0; i < capacity - 1; i++) {
        if (!KTerm_WriteChar(term, 'B')) {
            printf("Failed at %d\n", i);
            assert(false && "Premature overflow");
        }
    }

    // Next one should fail
    bool res = KTerm_WriteChar(term, 'X');
    assert(res == false);
    assert(KTerm_IsEventOverflow(term) == true);

    printf("SUCCESS: Overflow detected correctly.\n");
    KTerm_Destroy(term);
}

int main() {
    test_basic_pipeline();
    test_overflow();
    return 0;
}
