#include <stdio.h>
#include <string.h>
#include <assert.h>

#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "kterm.h"
// kt_gateway.h is included by kterm.h if KTERM_ENABLE_GATEWAY is defined (default)
// but checking if we need explicit include. kterm.h includes it.

int main() {
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    if (!term) return 1;
    KTerm_InitSession(term, 0);
    KTermSession* s = &term->sessions[0];
    s->cols = 80;
    s->rows = 24;
    s->cursor.x = 10;
    s->cursor.y = 10;

    // Test KTerm_ParseGridCoord via KTerm_Ext_Grid calls
    // We can't call static KTerm_ParseGridCoord directly, but we can verify side effects.

    // 1. Absolute fill
    // "grid;fill;0;5;5;2;2;..." -> x=5, y=5
    // Mask=1 (CH), Char=32 (Space)
    KTerm_GatewayProcess(term, s, "KTERM", "1", "EXT", "grid;fill;0;5;5;2;2;1;32");
    // Verify Queue (simplified check: just ensuring it ran without crash and queued op)
    // To properly verify values, we need to inspect the op queue.

    KTermOp op = s->op_queue.ops[s->op_queue.head];
    if (s->op_queue.count > 0 && op.type == KTERM_OP_FILL_RECT_MASKED) {
        printf("PASS: Op Queued\n");
        if (op.u.fill_masked.rect.x == 5 && op.u.fill_masked.rect.y == 5) printf("PASS: Absolute x=5, y=5\n");
        else printf("FAIL: Absolute x=%d, y=%d\n", op.u.fill_masked.rect.x, op.u.fill_masked.rect.y);

        // Consume
        s->op_queue.head = (s->op_queue.head + 1) % KTERM_OP_QUEUE_SIZE;
        s->op_queue.count--;
    } else {
        printf("FAIL: No Op Queued\n");
    }

    // 2. Relative fill (+5)
    // cursor is 10,10. x=+5 -> 15. y=-2 -> 8.
    KTerm_GatewayProcess(term, s, "KTERM", "2", "EXT", "grid;fill;0;+5;-2;2;2;1;32");

    op = s->op_queue.ops[s->op_queue.head];
    if (s->op_queue.count > 0) {
        if (op.u.fill_masked.rect.x == 15 && op.u.fill_masked.rect.y == 8) printf("PASS: Relative x=+5 (15), y=-2 (8)\n");
        else printf("FAIL: Relative x=%d, y=%d\n", op.u.fill_masked.rect.x, op.u.fill_masked.rect.y);
        s->op_queue.head = (s->op_queue.head + 1) % KTERM_OP_QUEUE_SIZE;
        s->op_queue.count--;
    } else {
        printf("FAIL: No Op Queued for Relative\n");
    }

    // 3. Negative Width (Mirror)
    // x=20, w=-5. Should result in x=15, w=5.
    KTerm_GatewayProcess(term, s, "KTERM", "3", "EXT", "grid;fill;0;20;5;-5;2;1;32");

    op = s->op_queue.ops[s->op_queue.head];
    if (s->op_queue.count > 0) {
        if (op.u.fill_masked.rect.x == 15 && op.u.fill_masked.rect.w == 5) printf("PASS: Negative Width x=15, w=5\n");
        else printf("FAIL: Negative Width x=%d, w=%d\n", op.u.fill_masked.rect.x, op.u.fill_masked.rect.w);
        s->op_queue.head = (s->op_queue.head + 1) % KTERM_OP_QUEUE_SIZE;
        s->op_queue.count--;
    } else {
        printf("FAIL: No Op Queued for Neg Width\n");
    }

    KTerm_Destroy(term);
    return 0;
}
