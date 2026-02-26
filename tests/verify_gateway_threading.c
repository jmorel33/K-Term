/*
 * verify_gateway_threading.c
 *
 * Verifies thread safety of KTerm Gateway functions, specifically ensuring
 * that tokenizers used in parsing (like KTerm_Strtok/strtok_r) are reentrant
 * and do not cause data corruption when called concurrently.
 */

#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#define KTERM_ENABLE_GATEWAY
#include "../kterm.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_THREADS 8
#define NUM_ITERATIONS 10000

/*
 * KTerm_ParseAttributeString is a static function in kt_gateway.h.
 * Because we included kt_gateway.h (via kterm.h with KTERM_ENABLE_GATEWAY),
 * the function body is available in this translation unit.
 */

void* thread_func(void* arg) {
    (void)arg;
    // Input string contains mixed case and multiple attributes to exercise the parser loop
    const char* input = "BOLD|DIM|ITALIC|UNDERLINE|BLINK|REVERSE|HIDDEN|STRIKE";

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        uint32_t flags = KTerm_ParseAttributeString(input);

        // Expected flags based on KTerm_ParseAttributeString logic
        // DIM maps to KTERM_ATTR_FAINT
        // HIDDEN maps to KTERM_ATTR_CONCEAL
        uint32_t expected = KTERM_ATTR_BOLD | KTERM_ATTR_FAINT | KTERM_ATTR_ITALIC |
                            KTERM_ATTR_UNDERLINE | KTERM_ATTR_BLINK | KTERM_ATTR_REVERSE |
                            KTERM_ATTR_CONCEAL | KTERM_ATTR_STRIKE;

        if (flags != expected) {
            fprintf(stderr, "Thread safety violation in thread %lu! Expected 0x%x, got 0x%x\n", (unsigned long)pthread_self(), expected, flags);
            exit(1);
        }
    }
    return NULL;
}

int main() {
    printf("Starting Gateway threading verification with %d threads, %d iterations each...\n", NUM_THREADS, NUM_ITERATIONS);

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, thread_func, NULL) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Gateway threading verification passed.\n");
    return 0;
}
