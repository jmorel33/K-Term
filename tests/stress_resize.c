#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "mock_situation.h"
#include "../kterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

/*
 * tests/stress_resize.c
 *
 * Hammers the KTerm_Resize logic to verify memory safety,
 * leak freedom, and stability under rapid dimension changes.
 */

#define RESIZE_COUNT 1000

int main() {
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    assert(term != NULL);

    printf("Starting Resize Stress Test (%d iterations)...\n", RESIZE_COUNT);
    srand(time(NULL));

    for (int i = 0; i < RESIZE_COUNT; i++) {
        // Random dimensions between 20x10 and 200x60
        int w = 20 + (rand() % 180);
        int h = 10 + (rand() % 50);

        MockSetTime(i * 0.1); // Advance 100ms per step

        // KTerm_Resize takes cols/rows
        KTerm_Resize(term, w, h);

        // Occasionally update to force layout recalc and buffer usage
        if (i % 10 == 0) {
            KTerm_Update(term);
        }
    }

    printf("Resize Stress Test Completed.\n");
    printf("Final Size: %dx%d\n", term->width, term->height);

    KTerm_Destroy(term);
    return 0;
}
