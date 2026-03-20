#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#define KTERM_ENABLE_GATEWAY
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

void benchmark_sink_callback(KTerm* term, const char* response, int length) {
    (void)term; (void)response; (void)length;
}

int main() {
    KTermConfig config = {0};
    config.width = 80;
    config.height = 25;
    config.response_callback = benchmark_sink_callback;

    KTerm* term = KTerm_Create(config);
    if (!term) {
        fprintf(stderr, "Failed to create terminal\n");
        return 1;
    }
    KTerm_Init(term);
    KTermSession* session = &term->sessions[0];

    printf("Running Font Listing Benchmark...\n");

    clock_t start = clock();
    int iterations = 1000000;

    for (int i = 0; i < iterations; i++) {
        // Direct call to bypassing parser
        KTerm_GatewayProcess(term, session, "KTERM", "0", "GET", "FONTS");
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("  GET;FONTS: %d iterations in %.3f seconds (%.0f iterations/sec)\n",
           iterations, elapsed, iterations / (elapsed > 0 ? elapsed : 0.001));

    KTerm_Destroy(term);
    return 0;
}
