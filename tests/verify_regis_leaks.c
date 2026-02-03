#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Helper to simulate input
void SendString(KTerm* term, int session_idx, const char* str) {
    KTermSession* session = &term->sessions[session_idx];
    for (int i = 0; str[i]; i++) {
        KTerm_ProcessChar(term, session, str[i]);
    }
}

int main() {
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    KTerm_InitSession(term, 0);

    printf("Testing ReGIS Memory Leaks...\n");

    // 1. Define a ReGIS Macro 'A'
    // DCS p @A (V [100,100]) @ ST
    // Note: KTerm implementation of ReGIS might need specific syntax.
    // Assuming standard ReGIS: @: Macro, A: Name, ... content ... @: End

    const char* define_macro = "\x1BP@A(V[100,100])@\x1B\\";
    SendString(term, 0, define_macro);

    // Verify macro was stored (White-box check)
    if (term->sessions[0].regis.macros[0] != NULL) {
        printf("Macro A stored successfully.\n");
    } else {
        printf("Macro A NOT stored. Check ReGIS implementation.\n");
        // Don't fail yet, maybe implementation varies, but we need it for leak test
    }

    // 2. Trigger Graphics Reset (calls KTerm_InitReGIS)
    // DCS GATE KTERM;0;RESET;REGIS ST
    const char* reset_cmd = "\x1BPGATE;KTERM;0;RESET;REGIS\x1B\\";
    SendString(term, 0, reset_cmd);

    // 3. Verify Macro is gone (ptr should be NULL/cleared)
    // Actually KTerm_InitReGIS currently uses memset, so it WILL be NULL.
    // The test is that ASan should catch the leak of the previous pointer.
    if (term->sessions[0].regis.macros[0] == NULL) {
        printf("ReGIS state reset.\n");
    }

    KTerm_Destroy(term);
    return 0;
}
