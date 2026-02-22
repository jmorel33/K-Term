#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#define KTERM_ENABLE_GATEWAY
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>

void benchmark_sink_callback(KTerm* term, const char* response, int length) {
    (void)term; (void)response; (void)length;
}

void benchmark_banner_generation(KTerm* term, KTermSession* session) {
    clock_t start = clock();
    int iterations = 5;

    // Set high throughput for benchmark
    KTerm_SetPipelineTargetFPS(term, 0); // Disable limits if 0 or set high budget
    // Actually direct access to session config might be needed or just loop ProcessEvents
    session->VTperformance.chars_per_frame = 1000000;
    session->VTperformance.time_budget = 1.0;

    for (int i = 0; i < iterations; i++) {
        // DCS GATE;KTERM;0;PIPE;BANNER;TEXT=Benchmark;GRADIENT=#FF0000|#0000FF ST
        const char* cmd = "\033PGATE;KTERM;0;PIPE;BANNER;TEXT=Benchmark Test String;GRADIENT=#FF0000|#0000FF\033\\";
        KTerm_WriteString(term, cmd);

        // Ensure processing
        while (KTerm_GetPendingEventCount(term) > 0) {
            KTerm_ProcessEvents(term);
        }
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("  Banner Generation: %d banners in %.3f seconds (%.0f banners/sec)\n",
           iterations, elapsed, iterations / elapsed);
}

int main() {
    KTermConfig config = {0};
    config.width = 132;
    config.height = 50;
    config.response_callback = benchmark_sink_callback;

    KTerm* term = KTerm_Create(config);
    if (!term) {
        fprintf(stderr, "Failed to create terminal\n");
        return 1;
    }
    KTerm_Init(term);
    KTermSession* session = &term->sessions[0];

    printf("Running Banner Benchmark...\n");
    benchmark_banner_generation(term, session);

    KTerm_Destroy(term);
    return 0;
}
