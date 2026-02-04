#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "mock_situation.h"
#include <assert.h>
#include <stdio.h>

void test_op_queue_integration(void) {
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    KTermSession* session = &term->sessions[term->active_session];

    // Write 'A'
    KTerm_WriteChar(term, 'A');
    KTerm_Update(term);

    EnhancedTermChar* cell = GetScreenCell(session, 0, 0);
    assert(cell->ch == 'A');
    assert(session->dirty_rect.w > 0);

    printf("SUCCESS: Grid updated correctly.\n");

    KTerm_Destroy(term);
}

void test_scroll_op(void) {
    KTermConfig config = {0};
    config.height = 10;
    KTerm* term = KTerm_Create(config);
    KTermSession* session = &term->sessions[term->active_session];

    // Fill screen with 'A'
    for(int i=0; i<term->height; i++) {
        KTerm_WriteString(term, "A\r\n");
    }
    KTerm_Update(term);

    // Scroll up (induce scroll by writing at bottom)
    // Actually KTerm_ProcessNormalChar handles autoscroll.
    // Let's force scroll via LF at bottom margin
    session->cursor.y = session->scroll_bottom;
    KTerm_WriteChar(term, '\n');

    KTerm_Update(term);

    // Top line should have moved up (into history), new bottom line empty?
    // Wait, if we scroll up, the top line 'A' moves to history.
    // The previous second line becomes top.

    // Let's check history or just check that a Scroll Op was processed.
    // If FlushOps works for scroll, the screen head should have moved or data shifted.

    // KTerm_ScrollUpRegion queues KTERM_OP_SCROLL_REGION.

    // Verify that the operation was applied.
    // If we filled with 'A's, and scrolled up 1 line, we have a new blank line at bottom?
    EnhancedTermChar* bottom_cell = GetScreenCell(session, session->scroll_bottom, 0);
    if (bottom_cell->ch != ' ') {
         printf("FAILURE: Scroll op failed? Bottom cell is '%c', expected ' '\n", (char)bottom_cell->ch);
    }
    assert(bottom_cell->ch == ' ');

    printf("SUCCESS: Scroll Op processed.\n");
    KTerm_Destroy(term);
}

int main() {
    test_op_queue_integration();
    test_scroll_op();
    return 0;
}
