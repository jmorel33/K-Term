#include <stdio.h>
// Include header. In a real project, this would be <kterm.h>
#include "../kterm.h"

// Example: Procedural Pattern Overlay
// This function writes directly to the grid after KTerm_Update has flushed the queue.
// This is useful for simulation layers, debug overlays, or special effects that
// don't need to go through the VT parser.
void ApplyOverlay(KTerm* term) {
    // Draw a diagonal line of 'X's
    for (int i = 0; i < 20; i++) {
        // Get current cell state
        EnhancedTermChar* cell = KTerm_GetCell(term, i, i);
        if (cell) {
            // Modify content
            cell->ch = 'X';
            cell->fg_color.color_mode = 0;
            cell->fg_color.value.index = 1; // Red (ANSI)
            cell->flags |= KTERM_ATTR_BOLD;

            // Write back (marks dirty)
            KTerm_SetCellDirect(term, i, i, *cell);
        }
    }
}

int main() {
    // Initialize KTerm
    KTermConfig config = { .width = 80, .height = 24 };
    KTerm* term = KTerm_Create(config);
    if (!term) {
        fprintf(stderr, "Failed to create terminal\n");
        return 1;
    }

    // Simulate a main loop
    printf("Running simulation...\n");
    for (int frame = 0; frame < 5; frame++) {
        // 1. Process Input (Writes to Queue)
        KTerm_WriteString(term, "Hello World from Input Queue!\r\n");

        // 2. Update Terminal (Flushes Queue to Grid)
        // At this point, "Hello World..." is applied to the grid.
        KTerm_Update(term);

        // 3. Post-Flush: Direct Grid Access
        // Safe here because queue is empty and we are before Draw.
        ApplyOverlay(term);

        // 4. Render
        // KTerm_Draw(term);
        // (Skipped in this headless example)

        printf("Frame %d: Processed.\n", frame);
    }

    KTerm_Destroy(term);
    printf("Done.\n");
    return 0;
}
