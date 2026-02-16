#define KTERM_IMPLEMENTATION
#define KTERM_ENABLE_GATEWAY
#include "../kterm.h"
#include <stdio.h>
#include <assert.h>

// Mock callbacks
void error_callback(KTerm* term, KTermErrorLevel level, KTermErrorSource source, const char* msg, void* user_data) {
    fprintf(stderr, "Error: %s\n", msg);
}

int main() {
    KTermConfig config = {0};
    config.width = 132;
    config.height = 24;
    config.strict_mode = false;

    KTerm* term = KTerm_Create(config);
    if (!term) {
        fprintf(stderr, "Failed to create KTerm\n");
        return 1;
    }
    KTerm_SetErrorCallback(term, error_callback, NULL);

    printf("Testing Buffer Hardening...\n");

    // 1. Enable 80/132 switching (DECSET 40)
    KTerm_PushInput(term, "\x1B[?40h", 6);
    KTerm_Update(term);

    // 2. Ensure we are in 132 mode (DECSET 3)
    KTerm_PushInput(term, "\x1B[?3h", 5);
    KTerm_Update(term);

    if (term->width != 132) {
        printf("Failed to switch to 132 columns (Width: %d)\n", term->width);
    }

    // 3. The Exploit Sequence:
    //    a. Switch to 80 columns (DECSET 3 l -> CSI ? 3 l)
    //       This queues a RESIZE op. Session is still 132 cols during parsing.
    //    b. Copy Rect (DECCRA) using 132 width.
    //       Parsing uses current width (132). Queues COPY op with w=132.
    //
    //    Execution (FlushOps):
    //       1. RESIZE -> Session becomes 80 cols. Buffer reallocated.
    //       2. COPY -> Tries to access index > 80.
    //       3. CRASH (if not hardened).

    // DECCRA: CSI Pt;Pl;Pb;Pr;Ps$v
    // Top=1, Left=1, Bot=24, Right=130, SourcePage=1
    // Copy 1,1-24,130 to 1,1.
    // Length of copy line = 130.
    // Session width will be 80.
    const char* exploit = "\x1B[?3l\x1B[1;1;24;130;1$v";

    printf("Sending exploit sequence...\n");
    KTerm_PushInput(term, exploit, strlen(exploit));

    // This Update triggers the crash
    KTerm_Update(term);

    printf("Survived!\n");

    KTerm_Destroy(term);
    return 0;
}
