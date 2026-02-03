#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

// Mock callbacks
void mock_response(KTerm* term, const char* response, int length) {
    (void)term;
    (void)response;
    (void)length;
}

// Helper to fill session with a char
void fill_session(KTerm* term, int session_idx, char c) {
    KTermSession* s = &term->sessions[session_idx];
    for(int y=0; y<s->rows; y++) {
        for(int x=0; x<s->cols; x++) {
            EnhancedTermChar* cell = GetActiveScreenCell(s, y, x);
            cell->ch = c;
            cell->dirty = true;
        }
        s->row_dirty[y] = true;
    }
}

// Helper to check staging buffer region
void check_region(KTerm* term, int start_x, int start_y, int w, int h, char expected) {
    for(int y=0; y<h; y++) {
        for(int x=0; x<w; x++) {
            int global_y = start_y + y;
            int global_x = start_x + x;
            int idx = global_y * term->width + global_x;

            if (global_y >= term->height || global_x >= term->width) continue;

            GPUCell* cell = &term->gpu_staging_buffer[idx];

            if (cell->char_code != (uint32_t)expected) {
                fprintf(stderr, "Check failed at (%d, %d). Expected '%c' (%d), Got %d\n",
                        global_x, global_y, expected, expected, cell->char_code);
                exit(1);
            }
        }
    }
}

int main() {
    printf("Starting Compositor Test...\n");

    KTermConfig config = {0};
    config.width = 100;
    config.height = 50;
    config.response_callback = mock_response;

    KTerm* term = KTerm_Create(config);
    if (!term) {
        fprintf(stderr, "Failed to create KTerm\n");
        return 1;
    }

    // Force resize to init buffers
    KTerm_Resize(term, 100, 50);

    if (!term->gpu_staging_buffer) {
         fprintf(stderr, "GPU Staging Buffer not allocated.\n");
         return 1;
    }

    // Test 1: Single Session Rendering
    printf("Test 1: Single Session Rendering... ");
    fill_session(term, 0, 'A');
    KTerm_Draw(term);

    check_region(term, 0, 0, 100, 50, 'A');
    printf("Passed.\n");

    // Test 2: Split Screen (Vertical)
    printf("Test 2: Recursive Split Rendering... ");
    KTermPane* root = term->layout->root;
    KTermPane* new_pane = KTerm_SplitPane(term, root, PANE_SPLIT_VERTICAL, 0.5f);
    int s1_idx = new_pane->session_index;

    fill_session(term, 0, 'A');
    fill_session(term, s1_idx, 'B');

    KTerm_Draw(term);

    // Top Half (Rows 0-24) should be 'A'
    check_region(term, 0, 0, 100, 25, 'A');
    // Bottom Half (Rows 25-49) should be 'B'
    check_region(term, 0, 25, 100, 25, 'B');

    printf("Passed.\n");

    KTerm_Destroy(term);
    printf("Compositor Test Completed Successfully.\n");
    return 0;
}
