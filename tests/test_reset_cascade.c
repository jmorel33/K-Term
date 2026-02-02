#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static KTerm* term = NULL;

int main() {
    KTermConfig config = {0};
    term = KTerm_Create(config);
    KTermSession* session = GET_SESSION(term);

    printf("Testing Reset Cascade to Graphics...\n");

    // --- Test 1: RIS (ESC c) clears Graphics ---
    printf("Test 1: RIS (ESC c)\n");
    // Set some graphics state
    session->kitty.image_count = 5;
    term->sessions[0].regis.state = 1;
    term->tektronix.state = 1;

    // Send RIS
    KTerm_ProcessChar(term, session, '\x1B');
    KTerm_ProcessChar(term, session, 'c');

    if (session->kitty.image_count == 0 && term->sessions[0].regis.state == 0 && term->tektronix.state == 0) {
        printf("PASS: RIS cleared graphics\n");
    } else {
        printf("FAIL: RIS failed to clear graphics (Kitty=%d, ReGIS=%d, Tek=%d)\n",
               session->kitty.image_count, term->sessions[0].regis.state, term->tektronix.state);
        return 1;
    }

    // --- Test 2: DECSTR (CSI ! p) clears Graphics ---
    printf("Test 2: DECSTR (CSI ! p)\n");
    // Set some graphics state
    session->kitty.image_count = 3;
    term->sessions[0].regis.state = 2;
    term->tektronix.state = 2;

    // Send DECSTR: ESC [ ! p
    KTerm_ProcessChar(term, session, '\x1B');
    KTerm_ProcessChar(term, session, '[');
    KTerm_ProcessChar(term, session, '!');
    KTerm_ProcessChar(term, session, 'p');

    if (session->kitty.image_count == 0 && term->sessions[0].regis.state == 0 && term->tektronix.state == 0) {
        printf("PASS: DECSTR cleared graphics\n");
    } else {
        printf("FAIL: DECSTR failed to clear graphics (Kitty=%d, ReGIS=%d, Tek=%d)\n",
               session->kitty.image_count, term->sessions[0].regis.state, term->tektronix.state);
        return 1;
    }

    KTerm_Destroy(term);
    return 0;
}
