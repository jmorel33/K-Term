#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static KTerm* term = NULL;

void MockGatewayCallback(KTerm* term, const char* class_id, const char* id, const char* command, const char* params) {
    (void)term; (void)class_id; (void)id; (void)command; (void)params;
    // No-op for this test
}

int main() {
    KTermConfig config = {0};
    term = KTerm_Create(config);
    KTerm_SetGatewayCallback(term, MockGatewayCallback);

    printf("Testing Gateway Graphics Reset...\n");

    KTermSession* session = GET_SESSION(term);

    // --- Test 1: Reset Kitty ---
    printf("Test 1: Reset Kitty\n");
    // Simulate some state
    session->kitty.image_count = 5;
    // Send Reset Command: DCS GATE KTERM;0;RESET;KITTY ST
    const char* seq1 = "\x1BPGATE;KTERM;0;RESET;KITTY\x1B\\";
    for (int i = 0; seq1[i] != '\0'; i++) KTerm_ProcessChar(term, session, (unsigned char)seq1[i]);

    if (session->kitty.image_count == 0) {
        printf("PASS: Kitty Reset (image_count=0)\n");
    } else {
        printf("FAIL: Kitty Reset (image_count=%d)\n", session->kitty.image_count);
        return 1;
    }

    // --- Test 2: Reset ReGIS ---
    printf("Test 2: Reset ReGIS\n");
    term->sessions[0].regis.state = 1; // Something non-zero
    const char* seq2 = "\x1BPGATE;KTERM;0;RESET;REGIS\x1B\\";
    for (int i = 0; seq2[i] != '\0'; i++) KTerm_ProcessChar(term, session, (unsigned char)seq2[i]);

    if (term->sessions[0].regis.state == 0) {
        printf("PASS: ReGIS Reset\n");
    } else {
        printf("FAIL: ReGIS Reset\n");
        return 1;
    }

    // --- Test 3: Reset Tektronix ---
    printf("Test 3: Reset Tektronix\n");
    term->tektronix.state = 1;
    const char* seq3 = "\x1BPGATE;KTERM;0;RESET;TEK\x1B\\";
    for (int i = 0; seq3[i] != '\0'; i++) KTerm_ProcessChar(term, session, (unsigned char)seq3[i]);

    if (term->tektronix.state == 0) {
        printf("PASS: Tektronix Reset\n");
    } else {
        printf("FAIL: Tektronix Reset\n");
        return 1;
    }

    // --- Test 4: Reset All Graphics ---
    printf("Test 4: Reset All Graphics\n");
    session->kitty.image_count = 3;
    term->sessions[0].regis.state = 2;
    term->tektronix.state = 2;

    const char* seq4 = "\x1BPGATE;KTERM;0;RESET;GRAPHICS\x1B\\";
    for (int i = 0; seq4[i] != '\0'; i++) KTerm_ProcessChar(term, session, (unsigned char)seq4[i]);

    if (session->kitty.image_count == 0 && term->sessions[0].regis.state == 0 && term->tektronix.state == 0) {
        printf("PASS: All Graphics Reset\n");
    } else {
        printf("FAIL: All Graphics Reset (Kitty=%d, ReGIS=%d, Tek=%d)\n",
               session->kitty.image_count, term->sessions[0].regis.state, term->tektronix.state);
        return 1;
    }

    // --- Test 5: Reset All Graphics (via ALL_GRAPHICS) ---
    printf("Test 5: Reset All Graphics (Alias)\n");
    session->kitty.image_count = 3;
    const char* seq5 = "\x1BPGATE;KTERM;0;RESET;ALL_GRAPHICS\x1B\\";
    for (int i = 0; seq5[i] != '\0'; i++) KTerm_ProcessChar(term, session, (unsigned char)seq5[i]);

    if (session->kitty.image_count == 0) {
        printf("PASS: All Graphics Reset (Alias)\n");
    } else {
        printf("FAIL: All Graphics Reset (Alias)\n");
        return 1;
    }

    KTerm_Destroy(term);
    return 0;
}
