# Op Queue Finalization (v2.4.0)

## Overview

As of v2.4.0, **all grid-affecting operations are now exclusively processed via the lock-free op queue.**
Direct cell mutations have been completely removed from production code. The queue is mandatory; there is no toggle or fallback configuration.

This architectural shift ensures that all terminal state modifications occur atomically and deterministically during the `KTerm_FlushOps` phase, eliminating race conditions between the input parser (which may run on a separate thread or at high frequency) and the rendering/logic loop.

## Queued Operations

The `KTermOpQueue` (defined in `kt_ops.h`) now handles the full spectrum of terminal mutations:

*   **SET_CELL**: Individual character writes (standard output).
*   **SCROLL_REGION**: Pan/Scroll operations (SU/SD).
*   **FILL_RECT**: Rectangular fills and clears (DECFRA, DECERA).
*   **COPY_RECT**: Rectangular copy/move (DECCRA).
*   **SET_ATTR_RECT**: Rectangular attribute modification (DECCARA, DECRARA).
*   **INSERT_LINES / DELETE_LINES**: Vertical line shifting (IL/DL).
*   **RESIZE_GRID**: Dynamic grid resizing.

## Post-Flush Consistency

A key benefit of this model is the guarantee of grid consistency. After `KTerm_FlushOps` executes (called automatically within `KTerm_Update` and `KTerm_ProcessEvents`), the internal `cells` array is guaranteed to be:

1.  **Clean**: No partial writes or half-applied escape sequences.
2.  **Consistent**: Geometry, cursor position, and cell content are synchronized.
3.  **Safe**: The grid is safe for direct external read/write access (e.g., by simulation layers, accessibility tools, or a bytecode VM) until the next update cycle begins.

## Migration & Breaking Changes

### For Library Consumers
*   **No Flag Needed**: The `use_op_queue` flag in `KTermSession` has been removed. You do not need to enable it; it is always active.
*   **Thread Safety**: You can confidently call write functions (`KTerm_WriteChar`, etc.) from any thread, as they now push to a thread-safe queue.

### For Contributors / Internals
*   **Direct Mutation Removed**: Functions like `KTerm_SetCellDirect` or direct array access (`session->screen_buffer[i] = ...`) are now strictly forbidden in parser code. They are wrapped in `#ifdef KTERM_DEBUG_DIRECT` for debugging/bisection only and should not be used in features.
*   **Mutation Logic**: If implementing a new sequence that modifies the grid, you **must** implement a corresponding `KTermOp` and queue it. Do not modify `session->screen_buffer` directly in the parsing stage.
