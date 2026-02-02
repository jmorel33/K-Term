#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "mock_situation.h"
#include "../kterm.h"
#include <stdio.h>
#include <assert.h>

/*
 * tests/verify_regis_isolation.c
 *
 * Verifies that ReGIS graphics state (e.g., current color, position)
 * is maintained independently per session in a multi-session environment.
 */

int main() {
    // 1. Setup
    KTermConfig config = {
        .width = 80,
        .height = 24
    };
    KTerm* term = KTerm_Create(config);
    assert(term != NULL);

    // Enable VT520 (4 sessions) and ReGIS
    // Use session 0 for setup
    KTermSession* s0 = &term->sessions[0];
    KTerm_SetLevel(term, s0, VT_LEVEL_520);

    // 2. Access Session 0
    term->active_session = 0;
    // Note: term->session member does not exist, we use local vars or array access

    printf("Step 1: Session 0 - Set ReGIS Color to Red (Index 1)\n");
    // Enter ReGIS: DCS p ...
    // S(I1) -> Screen Command, Index 1
    // ST
    KTerm_WriteString(term, "\033P0pS(I1)\033\\");
    KTerm_Update(term);

    // Check internal state of S0
    printf("Session 0 ReGIS Color: 0x%08X\n", s0->regis.color);
    uint32_t s0_color_after_set = s0->regis.color;

    // 3. Switch to Session 1
    printf("Step 2: Switch to Session 1\n");
    KTermSession* s1 = &term->sessions[1];

    // Switch active session
    term->active_session = 1;
    s1->session_open = true; // Simulating open

    // 4. Verify Session 1 ReGIS State is clean
    printf("Session 1 ReGIS Color (Should be Default): 0x%08X\n", s1->regis.color);

    // Use a heuristic check. Usually default is 0 or White (0xFFFFFFFF) or index 7.
    // If S0 color was set, S1 should NOT match it (unless default happens to match).
    // Assuming S(I1) changed S0 color from default.

    if (s1->regis.color == s0_color_after_set && s0_color_after_set != 0xFFFFFFFF && s0_color_after_set != 0) {
        // Only fail if we are sure they shouldn't match.
        // If s0 set to Red, and s1 is White, they differ.
        printf("FAILURE: Session 1 inherited Session 0 color!\n");
        return 1;
    }

    // 5. Modify Session 1
    printf("Step 3: Session 1 - Set ReGIS Color to Green (Index 2)\n");
    KTerm_WriteString(term, "\033P0pS(I2)\033\\");
    KTerm_Update(term);

    printf("Session 1 ReGIS Color: 0x%08X\n", s1->regis.color);
    uint32_t s1_color_after_set = s1->regis.color;

    // 6. Switch back to Session 0
    printf("Step 4: Switch back to Session 0\n");
    term->active_session = 0;

    // 7. Verify Session 0 retained its state
    printf("Session 0 ReGIS Color (Should be orig S0 color): 0x%08X\n", s0->regis.color);

    if (s0->regis.color != s0_color_after_set) {
        printf("FAILURE: Session 0 state was corrupted! Expected 0x%08X, got 0x%08X\n", s0_color_after_set, s0->regis.color);
        return 1;
    }

    if (s0->regis.color == s1_color_after_set) {
         // This check assumes Red != Green.
         printf("FAILURE: Session 0 adopted Session 1 color!\n");
         return 1;
    }

    printf("SUCCESS: ReGIS state is isolated per session.\n");

    KTerm_Destroy(term);
    return 0;
}
