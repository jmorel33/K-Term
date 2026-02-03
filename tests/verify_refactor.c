#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

#define KTERM_IMPLEMENTATION
#define SITUATION_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"

// Helper to process string
void ProcessString(KTerm* term, int session_idx, const char* str) {
    KTermSession* session = &term->sessions[session_idx];
    for (int i = 0; str[i]; i++) {
        KTerm_ProcessChar(term, session, str[i]);
    }
}

void test_split_screen_input_leak() {
    printf("Testing Split Screen Input Leak...\n");
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);

    // Init session 1
    KTerm_InitSession(term, 1);
    term->sessions[1].session_open = true;

    // Create split: Root -> Split(Horizontal) -> [A:0, B:1]
    // Current layout root is Leaf 0.
    KTermPane* root = term->layout->root;
    // SplitPane(target, type, ratio)
    // We want to split root.
    KTermPane* new_pane = KTerm_SplitPane(term, root, PANE_SPLIT_HORIZONTAL, 0.5f);
    // SplitPane returns the new pane (Child B). Child A takes original content.
    // Child B is assigned a new session by default if available, logic in SplitPane:
    // "Find a free session... Create new child panes... Child B session_index = new_session_idx"
    // Since we already opened session 1, SplitPane might pick session 2 if 1 is "open"?
    // Logic: "if (!term->sessions[i].session_open)"
    // So if session 1 is open, it will pick session 2.
    // Let's NOT open session 1 manually, let SplitPane do it.

    KTerm_Cleanup(term); free(term);
    term = KTerm_Create(config);
    root = term->layout->root;
    new_pane = KTerm_SplitPane(term, root, PANE_SPLIT_HORIZONTAL, 0.5f);

    // Now Child A has session 0, Child B has session 1.
    // Verify layout
    assert(term->layout->root->type == PANE_SPLIT_HORIZONTAL);
    assert(term->layout->root->child_a->session_index == 0);
    assert(term->layout->root->child_b->session_index == 1);

    // Focus session 1 (Child B)
    term->layout->focused = term->layout->root->child_b;
    KTerm_SetActiveSession(term, 1);

    // Queue Input Event
    KTermEvent evt = {0};
    evt.key_code = 'A';
    evt.sequence[0] = 'A';
    KTerm_QueueInputEvent(term, evt);

    // Verify it went to Session 1's buffer
    KTermSession* s0 = &term->sessions[0];
    KTermSession* s1 = &term->sessions[1];

    int s0_head = atomic_load(&s0->input.buffer_head);
    int s0_tail = atomic_load(&s0->input.buffer_tail);
    int s1_head = atomic_load(&s1->input.buffer_head);
    int s1_tail = atomic_load(&s1->input.buffer_tail);

    if (s0_head == s0_tail && s1_head != s1_tail) {
        printf("PASS: Input routed to focused pane (Session 1)\n");
    } else {
        printf("FAIL: Input routing incorrect. S0 count: %d, S1 count: %d\n",
               (s0_head - s0_tail), (s1_head - s1_tail));
    }

    KTerm_Destroy(term);
}

void test_resize_stability() {
    printf("Testing Resize Stability...\n");
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    KTerm_InitSession(term, 1);

    for (int i = 0; i < 100; i++) {
        int w = 80 + (i % 20);
        int h = 24 + (i % 10);
        // Resize session 1
        // We can call KTerm_ResizeSession directly (public wrapper)
        // It locks internally, which is fine.
        // We are single threaded here.
        KTerm_ResizeSession(term, 1, w, h);

        // Write something
        KTerm_WriteCharToSession(term, 1, 'X');
    }
    printf("PASS: Resize loop completed without crash\n");
    KTerm_Destroy(term);
}

void test_gateway_targeting() {
    printf("Testing Gateway Protocol Targeting...\n");
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    KTerm_InitSession(term, 1);
    term->sessions[1].session_open = true;

    // Reset backgrounds
    term->sessions[0].current_bg.value.index = 0;
    term->sessions[1].current_bg.value.index = 0;

    // Send Gateway Command to Session 0 (Active) to target Session 1
    // DCS GATE;KTERM;0;SET;SESSION;1 ST
    const char* cmd1 = "\x1BPGATE;KTERM;0;SET;SESSION;1\x1B\\";
    ProcessString(term, 0, cmd1);

    // Verify target set
    if (term->gateway_target_session != 1) {
        printf("FAIL: Gateway target session not set to 1. Got %d\n", term->gateway_target_session);
        return;
    }

    // Set Attribute on Target
    // DCS GATE;KTERM;0;SET;ATTR;BG=1 ST (Red)
    const char* cmd2 = "\x1BPGATE;KTERM;0;SET;ATTR;BG=1\x1B\\";
    ProcessString(term, 0, cmd2);

    // Verify Session 1 changed, Session 0 didn't
    if (term->sessions[1].current_bg.value.index == 1) {
        if (term->sessions[0].current_bg.value.index == 0) {
            printf("PASS: Gateway targeted correct session\n");
        } else {
             printf("FAIL: Session 0 also changed (Leak)\n");
        }
    } else {
        printf("FAIL: Session 1 did not change. Value: %d\n", term->sessions[1].current_bg.value.index);
    }

    KTerm_Destroy(term);
}

void test_save_restore_isolation() {
    printf("Testing Save/Restore Cursor Isolation...\n");
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    KTerm_InitSession(term, 1);
    term->sessions[1].session_open = true;

    // Session 0
    term->active_session = 0;
    ProcessString(term, 0, "\x1B[10;10H"); // Move to 10,10 (0-based 9,9)
    ProcessString(term, 0, "\x1B" "7");       // Save (DECSC)

    // Session 1
    term->active_session = 1;
    ProcessString(term, 1, "\x1B[5;5H");   // Move to 5,5 (0-based 4,4)
    ProcessString(term, 1, "\x1B" "7");       // Save

    // Switch back 0
    term->active_session = 0;
    ProcessString(term, 0, "\x1B[1;1H");   // Move away
    ProcessString(term, 0, "\x1B" "8");       // Restore

    if (term->sessions[0].cursor.x == 9 && term->sessions[0].cursor.y == 9) {
        // Switch to 1
        term->active_session = 1;
        ProcessString(term, 1, "\x1B[1;1H"); // Move away
        ProcessString(term, 1, "\x1B" "8");     // Restore

        if (term->sessions[1].cursor.x == 4 && term->sessions[1].cursor.y == 4) {
            printf("PASS: Save/Restore is isolated\n");
        } else {
            printf("FAIL: Session 1 restore incorrect: %d,%d\n", term->sessions[1].cursor.x, term->sessions[1].cursor.y);
        }
    } else {
        printf("FAIL: Session 0 restore incorrect: %d,%d\n", term->sessions[0].cursor.x, term->sessions[0].cursor.y);
    }

    KTerm_Destroy(term);
}

int main() {
    test_split_screen_input_leak();
    test_resize_stability();
    test_gateway_targeting();
    test_save_restore_isolation();
    return 0;
}
