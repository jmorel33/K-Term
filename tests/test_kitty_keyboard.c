#define _POSIX_C_SOURCE 200809L
#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static char last_response[256];
static void test_response_callback(KTerm* term, const char* response, int length) {
    if (length < 256) {
        memcpy(last_response, response, length);
        last_response[length] = '\0';
    }
}

void test_kitty_protocol_state(KTerm* term) {
    KTermSession* session = GET_SESSION(term);

    // Initial State
    assert(session->input.kitty_keyboard_flags == 0);
    assert(session->input.kitty_keyboard_stack_depth == 0);

    // 1. Push and Set (CSI > 1 u)
    KTerm_WriteString(term, "\x1B[>1u");
    KTerm_Update(term);

    assert(session->input.kitty_keyboard_flags == 1);
    assert(session->input.kitty_keyboard_stack_depth == 1);
    assert(session->input.kitty_keyboard_stack[0] == 0);

    // 2. Set Mode (CSI = 2;2 u) -> Set flag 2 (OR)
    KTerm_WriteString(term, "\x1B[=2;2u");
    KTerm_Update(term);
    assert(session->input.kitty_keyboard_flags == 3);

    // 3. Pop (CSI < u)
    KTerm_WriteString(term, "\x1B[<u");
    KTerm_Update(term);
    assert(session->input.kitty_keyboard_flags == 0);
    assert(session->input.kitty_keyboard_stack_depth == 0);

    printf("PASS: Kitty Protocol State\n");
}

void test_key_translation(KTerm* term) {
    KTermSession* session = GET_SESSION(term);

    // Enable Kitty Mode
    KTerm_WriteString(term, "\x1B[>1u");
    KTerm_Update(term);

    // Test 1: 'a' (Simple)
    KTermKeyEvent ev1 = {0};
    ev1.key_code = KTERM_KEY_A;
    KTerm_QueueInputEvent(term, ev1);
    KTerm_Update(term); KTerm_Update(term);
    assert(strcmp(last_response, "a") == 0);

    // Test 2: Ctrl+A
    KTermKeyEvent ev2 = {0};
    ev2.key_code = KTERM_KEY_A;
    ev2.ctrl = true;
    KTerm_QueueInputEvent(term, ev2);
    KTerm_Update(term); KTerm_Update(term);
    assert(strcmp(last_response, "\x1B[97;5u") == 0);

    // Test 3: Left Arrow
    KTermKeyEvent ev3 = {0};
    ev3.key_code = KTERM_KEY_LEFT;
    KTerm_QueueInputEvent(term, ev3);
    KTerm_Update(term); KTerm_Update(term);
    assert(strcmp(last_response, "\x1B[57351;1u") == 0);

    // Test 4: Shift+Left Arrow
    KTermKeyEvent ev4 = {0};
    ev4.key_code = KTERM_KEY_LEFT;
    ev4.shift = true;
    KTerm_QueueInputEvent(term, ev4);
    KTerm_Update(term); KTerm_Update(term);
    assert(strcmp(last_response, "\x1B[57351;2u") == 0);

    // Test 5: F1
    KTermKeyEvent ev5 = {0};
    ev5.key_code = KTERM_KEY_F1;
    KTerm_QueueInputEvent(term, ev5);
    KTerm_Update(term); KTerm_Update(term);
    assert(strcmp(last_response, "\x1B[57370;1u") == 0);

    printf("PASS: Key Translation\n");
}

int main() {
    KTermConfig config = {0};
    config.width = 80; config.height = 25;
    config.response_callback = test_response_callback;
    KTerm* term = KTerm_Create(config);

    test_kitty_protocol_state(term);
    test_key_translation(term);

    KTerm_Destroy(term);
    return 0;
}
