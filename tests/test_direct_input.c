#define _POSIX_C_SOURCE 200809L
#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static char last_response[256];
static int response_count = 0;

static void test_response_callback(KTerm* term, const char* response, int length) {
    if (length < 256) {
        memcpy(last_response, response, length);
        last_response[length] = '\0';
    }
    response_count++;
}

void test_direct_input(KTerm* term) {
    KTermSession* session = GET_SESSION(term);
    session->direct_input = true;

    // Clear response tracking
    last_response[0] = '\0';
    response_count = 0;

    // 1. Type 'A'
    KTermEvent ev = {0};
    ev.type = KTERM_EVENT_KEY;
    ev.key.key_code = 'A';
    KTerm_ProcessEvent(term, session, &ev);
    KTerm_Update(term); // Flush ops

    // Verify Grid
    EnhancedTermChar* cell = KTerm_GetCell(term, 0, 0);
    assert(cell->ch == 'A');
    assert(session->cursor.x == 1);

    // Verify No Response (Local Echo only)
    assert(response_count == 0);

    // 2. Type 'B'
    ev.key.key_code = 'B';
    KTerm_ProcessEvent(term, session, &ev);
    KTerm_Update(term);
    cell = KTerm_GetCell(term, 1, 0);
    assert(cell->ch == 'B');
    assert(session->cursor.x == 2);

    // 3. Backspace
    ev.key.key_code = KTERM_KEY_BACKSPACE;
    KTerm_ProcessEvent(term, session, &ev);
    KTerm_Update(term);
    assert(session->cursor.x == 1);
    cell = KTerm_GetCell(term, 1, 0);
    assert(cell->ch == ' '); // Erasure

    // 4. Arrow Keys
    ev.key.key_code = KTERM_KEY_LEFT;
    KTerm_ProcessEvent(term, session, &ev);
    KTerm_Update(term);
    assert(session->cursor.x == 0); // Moved back to start

    printf("PASS: Direct Input Mode\n");
}

int main() {
    KTermConfig config = {0};
    config.width = 80; config.height = 25;
    config.response_callback = test_response_callback;
    KTerm* term = KTerm_Create(config);

    test_direct_input(term);

    KTerm_Destroy(term);
    return 0;
}
