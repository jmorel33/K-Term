#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "mock_situation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_rawdump_behavior(void) {
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    if (!term) {
        printf("Failed to create KTerm\n");
        return;
    }

    // Initialize session 1 as target
    KTerm_InitSession(term, 1);

    // Ensure active session is 0
    KTerm_SetActiveSession(term, 0);

    // Send DCS to activate RAWDUMP on session 0 (source) targeting session 1
    // DCS GATE KTERM;RAWDUMP;START;SESSION=1 ST
    const char* cmd = "\x1BPGATE;KTERM;1;RAWDUMP;START;SESSION=1\x1B\\";
    KTerm_PushInput(term, cmd, strlen(cmd));
    KTerm_Update(term);

    // Verify session 0 has RAWDUMP active
    assert(term->sessions[0].raw_dump.raw_dump_mirror_active == true);
    assert(term->sessions[0].raw_dump.raw_dump_target_session_id == 1);

    // Feed raw bytes to session 0
    const char* raw_data = "HelloRaw\x1B[31mRed"; // Contains ESC sequence, but rawdump should ignore it
    KTerm_PushInput(term, raw_data, strlen(raw_data));
    KTerm_Update(term); // Process events -> Dump to session 1

    // Check Session 1 Grid
    KTermSession* s1 = &term->sessions[1];

    // Dump should have written chars literally
    // "HelloRaw" + ESC + "[31mRed"
    // H e l l o R a w ESC [ 3 1 m R e d

    // Check first char 'H' at (0,0)
    EnhancedTermChar* cell = &s1->screen_buffer[0];
    printf("Cell 0: '%c' (0x%02X)\n", (char)cell->ch, cell->ch);
    assert(cell->ch == 'H');
    // Default WOB (White on Black)
    assert(cell->fg_color.value.index == 15);
    assert(cell->bg_color.value.index == 0);

    // Check ESC (index 8)
    cell = &s1->screen_buffer[8];
    printf("Cell 8: 0x%02X\n", cell->ch);
    assert(cell->ch == '\x1B');

    // Check '[' (index 9)
    cell = &s1->screen_buffer[9];
    printf("Cell 9: '%c'\n", (char)cell->ch);
    assert(cell->ch == '[');

    // Also verify Session 0 (Source) processed them normally (parsed)
    // "HelloRaw" -> Printed
    // ESC [ 3 1 m -> Red Color set
    // "Red" -> Printed in Red

    KTermSession* s0 = &term->sessions[0];
    cell = &s0->screen_buffer[0];
    assert(cell->ch == 'H');

    // Check "Red" (starts at index 8 of printed chars? No, "HelloRaw" is 8 chars. ESC... is consumed.)
    // So 'R' is at index 8.
    cell = &s0->screen_buffer[8];
    assert(cell->ch == 'R');
    // Should be Red (Index 1)
    printf("Cell 8 (Source) FG: %d\n", cell->fg_color.value.index);
    assert(cell->fg_color.value.index == 1);

    printf("SUCCESS: RawDump Basic Behavior Verified.\n");
    KTerm_Destroy(term);
}

int main() {
    test_rawdump_behavior();
    return 0;
}
