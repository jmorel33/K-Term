#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

// Mock callbacks
void mock_response(KTerm* term, const char* response, int length) {
    (void)term;
    (void)response;
    (void)length;
}

int main() {
    printf("Starting Multiplexer Test...\n");

    KTermConfig config = {0};
    config.width = 100;
    config.height = 50;
    config.response_callback = mock_response;

    KTerm* term = KTerm_Create(config);
    if (!term) {
        fprintf(stderr, "Failed to create KTerm\n");
        return 1;
    }

    // Step 1: Verify Initial Layout
    // Force resize to ensure layout geometry is calculated
    KTerm_Resize(term, 100, 50);

    printf("Initial Term Size: %d x %d\n", term->width, term->height);
    assert(term->width == 100);
    assert(term->height == 50);

    // Verify root pane
    assert(term->layout->root != NULL);
    assert(term->layout->root->type == PANE_LEAF);
    assert(term->layout->root->session_index == 0);
    assert(term->layout->root->width == 100);
    assert(term->layout->root->height == 50);
    assert(term->sessions[0].cols == 100);
    assert(term->sessions[0].rows == 50);

    printf("Legacy checks passed.\n");

    // Step 2: Split Pane
    printf("Splitting Root Pane (Vertical)...\n");
    KTermPane* root = term->layout->root;
    KTermPane* new_pane = KTerm_SplitPane(term, root, PANE_SPLIT_VERTICAL, 0.5f);

    assert(new_pane != NULL);
    assert(root->type == PANE_SPLIT_VERTICAL);
    assert(root->child_a != NULL);
    assert(root->child_b != NULL);
    assert(root->child_b == new_pane);

    // Flush ops to apply resize
    KTerm_Update(term);

    // Verify Children
    // Child A (Original)
    assert(root->child_a->type == PANE_LEAF);
    assert(root->child_a->session_index == 0);
    assert(root->child_a->height == 25);
    assert(root->child_a->width == 100);

    // Child B (New)
    assert(root->child_b->type == PANE_LEAF);
    int new_sess_idx = root->child_b->session_index;
    assert(new_sess_idx != 0);
    assert(root->child_b->height == 25);
    assert(root->child_b->width == 100);

    // Verify Sessions Resized
    assert(term->sessions[0].rows == 25);
    assert(term->sessions[new_sess_idx].rows == 25);

    printf("Split check passed.\n");

    /*
    // Step 3: Resize Terminal
    printf("Resizing Terminal to 200 x 100...\n");
    MockSetTime(1.0); // Advance time to bypass throttle
    KTerm_Resize(term, 200, 100);

    assert(term->width == 200);
    assert(term->height == 100);

    // Verify Layout Adjusted
    assert(root->width == 200);
    assert(root->height == 100);

    assert(root->child_a->width == 200);
    assert(root->child_a->height == 50); // 0.5 * 100

    assert(root->child_b->width == 200);
    assert(root->child_b->height == 50);

    // Flush ops
    KTerm_Update(term);

    // Verify Sessions Resized
    assert(term->sessions[0].cols == 200);
    assert(term->sessions[0].rows == 50);
    assert(term->sessions[new_sess_idx].cols == 200);
    assert(term->sessions[new_sess_idx].rows == 50);

    printf("Resize check passed.\n");
    */

    KTerm_Destroy(term);
    printf("Multiplexer Test Completed Successfully.\n");
    return 0;
}
