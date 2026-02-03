#define KTERM_TESTING
#define KTERM_IMPLEMENTATION

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "../kterm.h"

// Callback for terminal responses
void FuzzResponseCallback(KTerm* term, const char* data, int len) {
    // Drop response
    (void)term; (void)data; (void)len;
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    // 1. Setup
    // Note: Recreating KTerm for every input is slow but ensures isolation.
    // Ideally we would reset, but KTerm_ResetGraphics only resets graphics.
    // For robust fuzzing of state corruption, a fresh start is safer.

    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    config.response_callback = FuzzResponseCallback;

    // Set Limits to prevent OOM/DoS during fuzzing
    config.max_sixel_width = 1024;
    config.max_sixel_height = 1024;
    config.max_kitty_image_pixels = 1024 * 1024; // 1MP
    config.max_ops_per_flush = 1000;

    KTerm* term = KTerm_Create(config);
    if (!term) return 0;

    // 2. Feed Data
    for (size_t i = 0; i < Size; i++) {
        KTerm_WriteChar(term, Data[i]);
    }

    // 3. Process
    // KTerm_Update processes the pipeline and flushes ops.
    KTerm_Update(term);

    // 4. Cleanup
    KTerm_Destroy(term);

    return 0;
}
