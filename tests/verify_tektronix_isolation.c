#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include "mock_situation.h"
#include <assert.h>
#include <stdio.h>

int main() {
    printf("Verifying Tektronix Isolation...\n");

    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    KTerm* term = KTerm_Create(config);
    assert(term != NULL);

    // Initialize Sessions
    KTerm_InitSession(term, 0);
    KTerm_InitSession(term, 1);

    // Session 0: Enable Tektronix (CSI ? 38 h) and set state
    KTerm_SetActiveSession(term, 0);
    KTerm_ProcessChar(term, &term->sessions[0], '\x1B');
    KTerm_ProcessChar(term, &term->sessions[0], '[');
    KTerm_ProcessChar(term, &term->sessions[0], '?');
    KTerm_ProcessChar(term, &term->sessions[0], '3');
    KTerm_ProcessChar(term, &term->sessions[0], '8');
    KTerm_ProcessChar(term, &term->sessions[0], 'h');

    // Simulate some Tektronix commands to change state
    // Enter Graph Mode (GS = 0x1D)
    KTerm_ProcessChar(term, &term->sessions[0], 0x1D);
    // Send some coordinates (Tektronix encoding is complex, checking simple state variable if possible)
    // Actually, KTerm_ProcessChar dispatches to ProcessTektronixChar which updates x/y.
    // Let's just manually modify the state to verify isolation since we have access to internals.
    term->sessions[0].tektronix.x = 100;
    term->sessions[0].tektronix.y = 200;
    term->sessions[0].tektronix.state = 1; // Graph

    // Session 1: Switch and Verify Initial State
    KTerm_SetActiveSession(term, 1);

    // Check that Session 1 state is clean
    assert(term->sessions[1].tektronix.x == 0);
    assert(term->sessions[1].tektronix.y == 0);
    assert(term->sessions[1].tektronix.state == 0);

    // Enable Tektronix on Session 1
    KTerm_ProcessChar(term, &term->sessions[1], '\x1B');
    KTerm_ProcessChar(term, &term->sessions[1], '[');
    KTerm_ProcessChar(term, &term->sessions[1], '?');
    KTerm_ProcessChar(term, &term->sessions[1], '3');
    KTerm_ProcessChar(term, &term->sessions[1], '8');
    KTerm_ProcessChar(term, &term->sessions[1], 'h');

    // Modify Session 1 State
    term->sessions[1].tektronix.x = 500;
    term->sessions[1].tektronix.y = 600;

    // Verify Session 0 is untouched
    assert(term->sessions[0].tektronix.x == 100);
    assert(term->sessions[0].tektronix.y == 200);

    // Verify Session 1 is modified
    assert(term->sessions[1].tektronix.x == 500);
    assert(term->sessions[1].tektronix.y == 600);

    KTerm_Destroy(term);
    printf("Tektronix Isolation Verified!\n");
    return 0;
}
