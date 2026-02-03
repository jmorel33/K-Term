#define KTERM_IMPLEMENTATION
#include "../kterm.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define TEST_WIDTH 100
#define TEST_HEIGHT 100

void test_sixel_limits() {
    printf("Testing Sixel Limits...\n");
    KTermConfig config = {0};
    config.width = TEST_WIDTH;
    config.height = TEST_HEIGHT;
    config.max_sixel_width = 10;
    config.max_sixel_height = 6; // 1 row of sixels

    KTerm* term = KTerm_Create(config);
    KTermSession* session = &term->sessions[0];

    // Initialize Sixel
    KTerm_InitSixelGraphics(term, session);
    session->sixel.active = true;
    session->sixel.width = 100;
    session->sixel.height = 100;

    // Reset parser
    session->sixel.parse_state = SIXEL_STATE_NORMAL;
    session->sixel.pos_x = 0;
    session->sixel.pos_y = 0;

    // 1. Test Width Limit
    // Send 15 pixels of '?'
    // We expect it to stop at 10.
    for (int i = 0; i < 15; i++) {
        KTerm_ProcessSixelChar(term, session, '?');
    }

    printf("Sixel pos_x: %d (Expected 10)\n", session->sixel.pos_x);
    assert(session->sixel.pos_x == 10);
    printf("Sixel Strip Count: %zu (Expected 10)\n", session->sixel.strip_count);
    assert(session->sixel.strip_count == 10);

    // 2. Test Height Limit
    // Send '-' to go to next row (y += 6) -> y=6
    KTerm_ProcessSixelChar(term, session, '-');
    printf("Sixel pos_y: %d (Expected 6)\n", session->sixel.pos_y);
    assert(session->sixel.pos_y == 6);

    // Try to draw at y=6. Should be blocked by max_height=6.
    // Send 5 pixels.
    for (int i = 0; i < 5; i++) {
        KTerm_ProcessSixelChar(term, session, '?');
    }

    // pos_x should increment (because we are parsing), but strips should NOT be added.
    // Wait, if we break from loop, pos_x increments by actual_repeat.
    // If we break early due to height, actual_repeat is full (since width is fine).
    // So pos_x becomes 5.
    // But no strips added.
    printf("Sixel Strip Count after blocked draw: %zu (Expected 10)\n", session->sixel.strip_count);
    assert(session->sixel.strip_count == 10);

    KTerm_Destroy(term);
    printf("Sixel Limits Passed.\n");
}

void test_flush_limits() {
    printf("Testing Flush Limits...\n");
    KTermConfig config = {0};
    config.width = TEST_WIDTH;
    config.height = TEST_HEIGHT;
    config.max_ops_per_flush = 5;

    KTerm* term = KTerm_Create(config);
    KTermSession* session = &term->sessions[0];

    // Queue 10 ops
    for (int i = 0; i < 10; i++) {
        KTermRect r = {0,0,1,1};
        KTerm_QueueFillRect(session, r, (EnhancedTermChar){'A', {0}, {0}, 0});
    }

    assert(session->op_queue.count == 10);

    // Flush
    KTerm_FlushOps(term, session);

    // Should have processed 5, remaining 5
    printf("Op Queue Count: %zu (Expected 5)\n", (size_t)session->op_queue.count);
    assert(session->op_queue.count == 5);

    // Flush again
    KTerm_FlushOps(term, session);
    assert(session->op_queue.count == 0);

    KTerm_Destroy(term);
    printf("Flush Limits Passed.\n");
}

void test_kitty_limits() {
    printf("Testing Kitty Limits...\n");
    KTermConfig config = {0};
    config.max_kitty_image_pixels = 100; // 10x10 max

    KTerm* term = KTerm_Create(config);
    KTermSession* session = &term->sessions[0];
    KTerm_InitSession(term, 0); // Re-init to ensure clean state

    // 1. Set width
    session->kitty.cmd.width = 20;
    session->kitty.cmd.height = 20;
    session->kitty.cmd.action = 't';
    session->kitty.state = 1; // In Value

    // Send ';' to finish value and trigger payload
    // This calls KTerm_PrepareKittyUpload, which should check limits.
    KTerm_ProcessKittyChar(term, session, ';');

    // If rejected, image count should be 0
    if (session->kitty.image_count == 0) {
        printf("Kitty Image Rejected (Count=0) - Passed\n");
    } else {
        printf("Kitty Image Count: %d (Expected 0)\n", session->kitty.image_count);
    }
    assert(session->kitty.image_count == 0);

    KTerm_Destroy(term);
    printf("Kitty Limits Passed.\n");
}

int main() {
    test_sixel_limits();
    test_flush_limits();
    test_kitty_limits();
    return 0;
}
