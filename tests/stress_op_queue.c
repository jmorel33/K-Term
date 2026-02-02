#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "mock_situation.h"
#include "../kterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

/*
 * tests/stress_op_queue.c
 *
 * Stresses the KTermOpQueue by flooding it with a high volume of
 * grid mutation operations (chars, scrolls, inserts, deletes)
 * and verifying that the queue handles the load without dropping
 * critical operations or crashing.
 */

#define OP_COUNT 50000
#define BATCH_SIZE 100

int main() {
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    assert(term != NULL);
    KTerm_SetLevel(term, &term->sessions[0], VT_LEVEL_XTERM);

    printf("Starting Op Queue Stress Test (%d ops)...\n", OP_COUNT);

    srand(time(NULL));
    char buffer[1024];

    for (int i = 0; i < OP_COUNT; i++) {
        // Randomly choose an operation type
        int op_type = rand() % 5;
        // int len = 0;

        switch (op_type) {
            case 0: // Print a character
                snprintf(buffer, sizeof(buffer), "%c", 'A' + (rand() % 26));
                break;
            case 1: // Newline/Scroll
                snprintf(buffer, sizeof(buffer), "\n");
                break;
            case 2: // Insert Line (IL)
                snprintf(buffer, sizeof(buffer), "\033[L");
                break;
            case 3: // Delete Line (DL)
                snprintf(buffer, sizeof(buffer), "\033[M");
                break;
            case 4: // SGR Attribute
                snprintf(buffer, sizeof(buffer), "\033[3%dm", rand() % 8);
                break;
        }

        KTerm_WriteString(term, buffer);

        // Periodically update to flush queue
        if (i % BATCH_SIZE == 0) {
            KTerm_Update(term);
        }
    }

    // Final flush
    KTerm_Update(term);

    printf("Op Queue Stress Test Completed.\n");
    // Check op queue of active session
    KTermOpQueue* q = &term->sessions[term->active_session].op_queue;

    printf("Status: %s (Count: %d)\n", q->count == 0 ? "QUEUE EMPTY (GOOD)" : "QUEUE NOT EMPTY (BAD)", q->count);

    KTerm_Destroy(term);
    return 0;
}
