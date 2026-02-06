#ifndef KT_OPS_H
#define KT_OPS_H

#include <stdint.h>
#include <stdbool.h>

// KTermRect definition for operations
typedef struct {
    int x, y, w, h;
} KTermRect;

// Operation Types
typedef enum {
    KTERM_OP_SET_CELL,
    KTERM_OP_SCROLL_REGION,
    KTERM_OP_COPY_RECT,
    KTERM_OP_FILL_RECT,
    KTERM_OP_SET_ATTR_RECT,
    KTERM_OP_INSERT_LINES,
    KTERM_OP_DELETE_LINES,
    KTERM_OP_RESIZE_GRID,
    KTERM_OP_FILL_RECT_MASKED,
    KTERM_OP_INVALID
} KTermOpType;

typedef struct {
    KTermRect region;   // Scrolling region (or full if none)
    int count;          // Lines to insert/delete
    bool respect_protected : 1;
    bool downward : 1;  // true for insert (push down), false for delete (pull up)
} KTermVerticalOp;

// Operation Payload
// Note: This relies on EnhancedTermChar being defined before this header is included.
typedef struct {
    KTermOpType type;
    union {
        struct {
            int x, y;
            EnhancedTermChar cell;
        } set_cell;
        struct {
            KTermRect rect;
            int dy; // +ve = Scroll Up (content moves up), -ve = Scroll Down
        } scroll;
        struct {
            KTermRect src;
            int dst_x;
            int dst_y;
        } copy;
        struct {
            KTermRect rect;
            EnhancedTermChar fill_char;
        } fill;
        struct {
            KTermRect rect;
            EnhancedTermChar fill_char;
            uint32_t mask;
        } fill_masked;
        struct {
            KTermRect rect;
            uint32_t attr_mask;      // Bits to modify (0=Ignore, 1=Set/Clear based on values)
            uint32_t attr_values;    // Target values for masked bits
            uint32_t attr_xor_mask;  // Bits to toggle (applied after mask/values)
            bool set_fg;
            ExtendedKTermColor fg;
            bool set_bg;
            ExtendedKTermColor bg;
        } set_attr;
        KTermVerticalOp vertical;
        struct {
            int new_width, new_height;
            bool reflow_scrollback;
        } resize;
    } u;
} KTermOp;

// Operation Queue (Ring Buffer)
#define KTERM_OP_QUEUE_SIZE 16384

typedef struct {
    KTermOp ops[KTERM_OP_QUEUE_SIZE];
    int head;
    int tail;
    int count;
} KTermOpQueue;

// Forward declaration of session
struct KTermSession_T;
typedef struct KTermSession_T KTermSession; // Assuming struct name match

// Function Prototypes
void KTerm_InitOpQueue(KTermOpQueue* queue);
bool KTerm_QueueOp(KTermOpQueue* queue, KTermOp op);
bool KTerm_IsOpQueueFull(KTermOpQueue* queue);
// FlushOps will be declared in kterm.h to avoid circular dependency issues with KTermSession definition

#endif // KT_OPS_H
