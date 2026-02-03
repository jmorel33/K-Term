#define _POSIX_C_SOURCE 200809L
#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

// Mock response callback
static void noop_response_callback(KTerm* term, const char* response, int length) {
    (void)term; (void)response; (void)length;
}

// Simple pseudo-random generator
static unsigned int rand_state = 1234;
static int my_rand() {
    rand_state = rand_state * 1103515245 + 12345;
    return (unsigned int)(rand_state / 65536) % 32768;
}

int main() {
    KTermConfig config = {0};
    config.width = 132;
    config.height = 40;
    config.response_callback = noop_response_callback;

    printf("Creating Terminal...\n");
    KTerm* term = KTerm_Create(config);
    if (!term) return 1;

    // Initialize logic
    KTerm_Update(term);

    const int ITERATIONS = 1000; // Reduced for debugging speed
    const int CHUNK_SIZE = 64;
    char text_chunk[65];

    printf("Starting Stress Test: Interleaved I/O (%d iterations)...\n", ITERATIONS);

    clock_t start = clock();

    for (int i = 0; i < ITERATIONS; i++) {
        // 1. Simulate Host Output (Streaming Text)
        // Write random text chunk
        for (int k = 0; k < CHUNK_SIZE; k++) {
            text_chunk[k] = ' ' + (my_rand() % 95); // Printable ASCII
        }
        text_chunk[CHUNK_SIZE] = '\0';
        KTerm_WriteString(term, text_chunk);

        // 2. Simulate User Input (Typing / Mouse)
        if (i % 5 == 0) {
            KTermEvent ev = {0};
            ev.key_code = KTERM_KEY_A + (my_rand() % 26);
            ev.ctrl = (my_rand() % 5) == 0;
            KTerm_QueueInputEvent(term, ev);
        }

        // 3. Simulate Mouse (Selection)
        if (i % 10 == 0) {
             GET_SESSION(term)->mouse.cursor_x = my_rand() % config.width;
             GET_SESSION(term)->mouse.cursor_y = my_rand() % config.height;
             GET_SESSION(term)->mouse.buttons[0] = (my_rand() % 2) == 0;
             // Logic updates internally handle selection state
        }

        // 4. Update Frame
        KTerm_Update(term);

        // 5. Simulate Resize occasionally
        /*
        if (i == ITERATIONS / 2) {
            printf("Resizing...\n");
            KTerm_Resize(term, 80, 24);
            printf("Resize complete.\n");
        }
        */
    }

    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    printf("Stress Test Completed in %.2f seconds.\n", cpu_time_used);

    KTerm_Destroy(term);
    return 0;
}
