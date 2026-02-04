#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../tests/mock_situation.h"
#include "../kterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdatomic.h>

// Global Response Buffer
static char output_buffer[4096];
static int output_pos = 0;

static void reset_output_buffer() {
    output_pos = 0;
    memset(output_buffer, 0, sizeof(output_buffer));
}

static void response_callback(KTerm* term, const char* response, int length) {
    if (output_pos + length < (int)sizeof(output_buffer)) {
        memcpy(output_buffer + output_pos, response, length);
        output_pos += length;
        output_buffer[output_pos] = '\0';
    }
}

// Helper: Write sequence and process events
static void write_sequence(KTerm* term, const char* seq) {
    if (!seq) return;
    KTerm_WriteString(term, seq);
    KTerm_ProcessEvents(term);
    KTerm_Update(term);
}

// Helper: Check cell content
static void check_cell(KTerm* term, int y, int x, char expected_char, const char* message) {
    KTermSession* session = GET_SESSION(term);
    EnhancedTermChar* cell = GetScreenCell(session, y, x);
    if (!cell) {
        printf("FAIL: %s - Cell (%d,%d) is invalid (NULL)\n", message, y, x);
        exit(1);
    }
    if (cell->ch != expected_char) {
        printf("FAIL: %s - Cell (%d,%d) expected '%c' (0x%02X), got '%c' (0x%02X)\n",
               message, y, x, expected_char, expected_char, (char)cell->ch, cell->ch);
        exit(1);
    } else {
        // printf("PASS: %s - Cell (%d,%d) matches '%c'\n", message, y, x, expected_char);
    }
}

// Helper: Set cell content
static void set_cell(KTerm* term, int y, int x, char ch) {
    EnhancedTermChar* cell = GetScreenCell(GET_SESSION(term), y, x);
    if (cell) {
        cell->ch = ch;
    }
}

static void clear_screen_manual(KTerm* term) {
    for (int y = 0; y < term->height; y++) {
        for (int x = 0; x < term->width; x++) {
            set_cell(term, y, x, ' ');
        }
    }
}

// --- Tests ---

void test_deccolm(void) {
    printf("Starting DECCOLM/DECSCPP Test...\n");

    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    KTerm* term = KTerm_Create(config);
    if (!term) { printf("Failed to create KTerm\n"); exit(1); }
    KTerm_SetResponseCallback(term, response_callback);

    // Initial check
    assert(term->width == 80);

    // 1. Try to resize with DECCOLM (CSI ? 3 h) - Should fail without Mode 40
    write_sequence(term, "\x1B[?3h");
    assert(term->width == 80); // Should remain 80

    // 2. Enable Mode 40 (Allow 80/132)
    write_sequence(term, "\x1B[?40h");
    assert(term->sessions[0].dec_modes & KTERM_MODE_ALLOW_80_132);

    // 3. Resize to 132 via DECCOLM
    write_sequence(term, "\x1B[?3h");
    assert(term->sessions[0].cols == 132);

    // Verify cursor reset (side effect)
    term->sessions[0].cursor.x = 10;
    term->sessions[0].cursor.y = 10;

    // 4. Resize to 80 via DECCOLM
    write_sequence(term, "\x1B[?3l");
    assert(term->sessions[0].cols == 80);
    // Cursor should be homed
    assert(term->sessions[0].cursor.x == 0);
    assert(term->sessions[0].cursor.y == 0);

    // 5. Test DECSCPP (CSI 132 $ |)
    write_sequence(term, "\x1B[132$|");
    assert(term->sessions[0].cols == 132);
    assert(term->sessions[0].dec_modes & KTERM_MODE_DECCOLM);

    // 6. Test DECSCPP (CSI 80 $ |)
    write_sequence(term, "\x1B[80$|");
    assert(term->sessions[0].cols == 80);
    assert(!(term->sessions[0].dec_modes & KTERM_MODE_DECCOLM));

    // 7. Test DECNCSM (No Clear Screen)
    // Write some text at 0,0
    KTerm_WriteString(term, "Hello World");
    KTerm_ProcessEvents(term);

    // Move cursor manually
    term->sessions[0].cursor.x = 5;
    term->sessions[0].cursor.y = 5;

    // Enable DECNCSM (CSI ? 95 h)
    write_sequence(term, "\x1B[?95h");

    // Resize to 132
    write_sequence(term, "\x1B[132$|");

    assert(term->sessions[0].cols == 132);
    // Cursor should NOT be homed
    assert(term->sessions[0].cursor.x == 5);
    assert(term->sessions[0].cursor.y == 5);

    // Check if text persists (cell 0,0 should have 'H')
    check_cell(term, 0, 0, 'H', "DECNCSM Text Persistence");

    KTerm_Destroy(term);
    printf("DECCOLM/DECSCPP Test Completed Successfully.\n");
}

void test_deccra(void) {
    printf("Starting DECCRA Tests...\n");
    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    KTerm* term = KTerm_Create(config);

    // Enable VT420 Level (Rectangular Ops)
    KTerm_SetLevel(term, GET_SESSION(term), VT_LEVEL_420);

    // --- Test 1: Full 8 Parameters (Control) ---
    clear_screen_manual(term);
    set_cell(term, 0, 0, 'A'); // Source at 0,0 (1,1)

    // Copy (1,1)-(1,1) to (2,2)
    // CSI 1;1;1;1;1;2;2;1 $ v
    write_sequence(term, "\x1B[1;1;1;1;1;2;2;1$v");
    check_cell(term, 1, 1, 'A', "Test 1: Full 8 Params");

    // --- Test 2: Missing Trailing Parameters ---
    clear_screen_manual(term);
    set_cell(term, 0, 0, 'B');

    // Copy (1,1)-(1,1) to (3,3). Omit dest page (param 8).
    // CSI 1;1;1;1;1;3;3 $ v
    write_sequence(term, "\x1B[1;1;1;1;1;3;3$v");
    check_cell(term, 2, 2, 'B', "Test 2: Missing Trailing Params");

    // --- Test 3: Default Bottom/Right ---
    clear_screen_manual(term);
    set_cell(term, 0, 0, 'C');

    // Copy (1,1)-Default to (2,1).
    // CSI 1;1;;;1;2;1 $ v
    write_sequence(term, "\x1B[1;1;;;1;2;1$v");
    check_cell(term, 1, 0, 'C', "Test 3: Default Bottom/Right");

    // --- Test 4: DECOM (Origin Mode) ---
    clear_screen_manual(term);

    // Set Scroll Region (Top/Bottom): CSI 2 ; H-1 r
    char buf[64];
    snprintf(buf, sizeof(buf), "\x1B[2;%dr", term->height - 1);
    write_sequence(term, buf);

    // Enable DECSLRM (needed for left/right margins)
    write_sequence(term, "\x1B[?69h");

    // Set Left/Right Margins: CSI 2 ; W-1 s
    snprintf(buf, sizeof(buf), "\x1B[2;%ds", term->width - 1);
    write_sequence(term, buf);

    // Enable DECOM: CSI ? 6 h
    write_sequence(term, "\x1B[?6h");

    // Now origin (1,1) is physically (2,2) (indices 1,1).
    // Put 'O' at physical (1,1).
    set_cell(term, 1, 1, 'O');

    // DECCRA with (1,1) should copy 'O'.
    // Copy 'O' (Origin) to (2,2) (Relative).
    // Relative (2,2) is Physical (2+1, 2+1) = (3,3).
    // CSI 1;1;1;1;1;2;2 $ v
    write_sequence(term, "\x1B[1;1;1;1;1;2;2$v");

    check_cell(term, 2, 2, 'O', "Test 4: DECOM Origin Mode");

    KTerm_Destroy(term);
    printf("DECCRA Tests Completed Successfully.\n");
}

void test_decdld(void) {
    printf("Starting DECDLD Test...\n");

    KTermConfig config = {
        .width = 80,
        .height = 24,
        .response_callback = response_callback
    };
    KTerm* term = KTerm_Create(config);
    if (!KTerm_Init(term)) { printf("Init failed\n"); exit(1); }
    KTermSession* session = &term->sessions[term->active_session];

    // DECDLD Sequence
    const char* decdld = "\x1BP1;33;1{@A/B\x1B\\";
    // write_sequence calls KTerm_Update which clears the dirty flag, so we do it manually to check the flag
    KTerm_WriteString(term, decdld);
    KTerm_ProcessEvents(term);

    assert(session->soft_font.dirty == true);
    assert(strcmp(session->soft_font.name, "@") == 0);

    unsigned char byte_33_1 = session->soft_font.font_data[33][1];
    assert((byte_33_1 & 0x80) != 0);

    unsigned char byte_34_0 = session->soft_font.font_data[34][0];
    unsigned char byte_34_1 = session->soft_font.font_data[34][1];
    assert((byte_34_0 & 0x80) != 0);
    assert((byte_34_1 & 0x80) != 0);

    // Trigger Atlas Update
    KTerm_Update(term);

    assert(session->soft_font.dirty == false);

    unsigned char* pixels = term->font_atlas_pixels;
    assert(pixels != NULL);

    int pixel_idx = (1 * 1024 + 264) * 4;
    unsigned char r = pixels[pixel_idx];
    assert(r == 255);

    unsigned char r_next = pixels[pixel_idx + 4];
    assert(r_next == 0);

    KTerm_Destroy(term);
    printf("DECDLD Test PASS.\n");
}

void test_deceskm(void) {
    printf("Starting DECESKM Test...\n");
    KTermConfig config = { .width = 80, .height = 25 };
    KTerm* term = KTerm_Create(config);
    KTermSession* session = GET_SESSION(term);
    KTerm_SetLevel(term, session, VT_LEVEL_XTERM);

    if (session->dec_modes & KTERM_MODE_DECESKM) {
        printf("FAIL: DECESKM should be OFF initially\n"); exit(1);
    }

    write_sequence(term, "\x1B[?104h");
    if (!(session->dec_modes & KTERM_MODE_DECESKM)) {
        printf("FAIL: DECESKM not set by CSI ? 104 h\n"); exit(1);
    }

    write_sequence(term, "\x1B[?104l");
    if (session->dec_modes & KTERM_MODE_DECESKM) {
        printf("FAIL: DECESKM not cleared by CSI ? 104 l\n"); exit(1);
    }

    KTerm_Destroy(term);
    printf("DECESKM Test PASS.\n");
}

void test_dechdpxm(void) {
    printf("Starting DECHDPXM Test...\n");
    KTermConfig config = { .width = 80, .height = 25 };
    KTerm* term = KTerm_Create(config);
    KTermSession* session = GET_SESSION(term);
    KTerm_SetLevel(term, session, VT_LEVEL_510);

    session->dec_modes &= ~KTERM_MODE_LOCALECHO;
    session->dec_modes &= ~KTERM_MODE_DECHDPXM;

    // Test 1: No Echo
    KTermEvent event1 = {0};
    event1.key_code = 'A';
    snprintf(event1.sequence, sizeof(event1.sequence), "A");
    KTerm_QueueInputEvent(term, event1);
    KTerm_Update(term);

    EnhancedTermChar* cell = GetActiveScreenCell(session, 0, 0);
    if (cell->ch == 'A') {
        printf("FAIL: 'A' echoed when Local Echo is OFF\n"); exit(1);
    }

    // Test 2: Enable Mode 103
    write_sequence(term, "\x1B[?103h");
    if (!(session->dec_modes & KTERM_MODE_DECHDPXM)) {
        printf("FAIL: Mode 103 not set\n"); exit(1);
    }

    // Test 3: Echo
    KTermEvent event2 = {0};
    event2.key_code = 'B';
    snprintf(event2.sequence, sizeof(event2.sequence), "B");
    KTerm_QueueInputEvent(term, event2);
    KTerm_Update(term);
    KTerm_Update(term);

    cell = GetActiveScreenCell(session, 0, 0);
    if (cell->ch != 'B') {
        printf("FAIL: 'B' not echoed when Mode 103 is ON. char=%c\n", cell->ch); exit(1);
    }

    // Test 4: Disable Mode 103
    write_sequence(term, "\x1B[?103l");
    if (session->dec_modes & KTERM_MODE_DECHDPXM) {
        printf("FAIL: Mode 103 not cleared\n"); exit(1);
    }

    // Test 5: No Echo Again
    KTermEvent event3 = {0};
    event3.key_code = 'C';
    snprintf(event3.sequence, sizeof(event3.sequence), "C");
    KTerm_QueueInputEvent(term, event3);
    KTerm_Update(term);

    cell = GetActiveScreenCell(session, 0, 1);
    if (cell->ch == 'C') {
        printf("FAIL: 'C' echoed after Mode 103 disabled\n"); exit(1);
    }

    KTerm_Destroy(term);
    printf("DECHDPXM Test PASS.\n");
}

void test_deckbum(void) {
    printf("Starting DECKBUM Test...\n");
    KTermConfig config = { .width = 80, .height = 25 };
    KTerm* term = KTerm_Create(config);
    KTermSession* session = GET_SESSION(term);
    KTerm_SetLevel(term, session, VT_LEVEL_XTERM);

    if (session->dec_modes & KTERM_MODE_DECKBUM) {
        printf("FAIL: DECKBUM (Mode 68) should be OFF initially\n"); exit(1);
    }

    write_sequence(term, "\x1B[?68h");
    if (!(session->dec_modes & KTERM_MODE_DECKBUM)) {
        printf("FAIL: DECKBUM (Mode 68) not set by CSI ? 68 h\n"); exit(1);
    }

    write_sequence(term, "\x1B[?68l");
    if (session->dec_modes & KTERM_MODE_DECKBUM) {
        printf("FAIL: DECKBUM (Mode 68) not cleared by CSI ? 68 l\n"); exit(1);
    }

    KTerm_Destroy(term);
    printf("DECKBUM Test PASS.\n");
}

void test_decsera(void) {
    printf("Starting DECSERA Test...\n");
    KTermConfig config = { .width = 80, .height = 25 };
    KTerm* term = KTerm_Create(config);
    KTerm_SetLevel(term, GET_SESSION(term), VT_LEVEL_420);
    KTermSession* session = GET_SESSION(term);

    write_sequence(term, "\x1B[H"); // Home
    write_sequence(term, "AAAAA");
    write_sequence(term, "\x1B[2;1H"); // 2,1
    write_sequence(term, "\x1B[1\"q"); // Protect on
    write_sequence(term, "PPPPP");
    write_sequence(term, "\x1B[0\"q"); // Protect off

    check_cell(term, 0, 0, 'A', "Setup Row 0");
    check_cell(term, 1, 0, 'P', "Setup Row 1");
    if (!(GetActiveScreenCell(session, 1, 0)->flags & KTERM_ATTR_PROTECTED)) {
        printf("FAIL: Row 1 not protected\n"); exit(1);
    }

    // Execute DECSERA on 1,1 to 2,5
    write_sequence(term, "\x1B[1;1;2;5${");

    check_cell(term, 0, 0, ' ', "Row 0 Erased");
    check_cell(term, 1, 0, 'P', "Row 1 Preserved (Protected)");

    KTerm_Destroy(term);
    printf("DECSERA Test PASS.\n");
}

void test_vt420_fixes(void) {
    printf("Starting VT420 Fixes Tests...\n");
    KTermConfig config = { .width = 80, .height = 25, .response_callback = response_callback };
    KTerm* term = KTerm_Create(config);
    KTerm_SetLevel(term, GET_SESSION(term), VT_LEVEL_420);
    KTermSession* session = GET_SESSION(term);

    // DECLRMM
    if (session->dec_modes & KTERM_MODE_DECLRMM) { printf("FAIL: DECLRMM default on\n"); exit(1); }
    write_sequence(term, "\x1B[?69h");
    if (!(session->dec_modes & KTERM_MODE_DECLRMM)) { printf("FAIL: DECLRMM enable failed\n"); exit(1); }

    write_sequence(term, "\x1B[2;10s");
    if (session->left_margin != 1 || session->right_margin != 9) {
        printf("FAIL: DECSLRM failed L=%d R=%d\n", session->left_margin, session->right_margin); exit(1);
    }

    write_sequence(term, "\x1B[?69l");
    if (session->left_margin != 0 || session->right_margin != term->width - 1) {
        printf("FAIL: DECLRMM disable reset failed\n"); exit(1);
    }

    // DECCOLM & DECNCSM (Repeated coverage but confirms valid logic)
    write_sequence(term, "\x1B[?95h");
    set_cell(term, 0, 0, 'X');
    write_sequence(term, "\x1B[?3h");
    check_cell(term, 0, 0, 'X', "DECNCSM Retain Text");

    // DECRQCRA
    reset_output_buffer();
    write_sequence(term, "\x1B[1;1;1;1;1;1*y");
    if (output_pos == 0) {
        printf("FAIL: No response to DECRQCRA\n"); exit(1);
    }
    // Expected format: DCS 1 ! ~ ... ST
    if (strncmp(output_buffer, "\x1BP1!~", 5) != 0) {
        printf("FAIL: DECRQCRA Response format wrong: %s\n", output_buffer); exit(1);
    }

    KTerm_Destroy(term);
    printf("VT420 Fixes PASS.\n");
}

void test_vt510_gems(void) {
    printf("Starting VT510 Gems Tests...\n");
    KTermConfig config = { .width = 80, .height = 25, .response_callback = response_callback };
    KTerm* term = KTerm_Create(config);
    KTermSession* session = GET_SESSION(term);

    // DECSNLS
    write_sequence(term, "\x1B[36*|");
    if (session->rows != 36) { printf("FAIL: DECSNLS 36 failed\n"); exit(1); }

    // DECSLPP
    write_sequence(term, "\x1B[66*{");
    if (session->lines_per_page != 66) { printf("FAIL: DECSLPP 66 failed\n"); exit(1); }

    // DECRQPKU
    // Setup key
    session->programmable_keys.capacity = 1;
    session->programmable_keys.count = 1;
    session->programmable_keys.keys = (ProgrammableKey*)calloc(1, sizeof(ProgrammableKey));
    session->programmable_keys.keys[0].key_code = KTERM_KEY_F6;
    session->programmable_keys.keys[0].sequence = strdup("HELLO");
    session->programmable_keys.keys[0].active = true;

    reset_output_buffer();
    write_sequence(term, "\x1B[?26;17u");
    if (!strstr(output_buffer, "\x1BP17;1;HELLO\x1B\\")) {
        printf("FAIL: DECRQPKU Response wrong: %s\n", output_buffer); exit(1);
    }

    // DECSKCV
    write_sequence(term, "\x1B[5 =");
    if (session->input.keyboard_variant != 5) { printf("FAIL: DECSKCV 5 failed\n"); exit(1); }

    // DECXRLM
    write_sequence(term, "\x1B[?88h");
    if (!(session->dec_modes & KTERM_MODE_DECXRLM)) { printf("FAIL: DECXRLM enable failed\n"); exit(1); }

    reset_output_buffer();
    atomic_store(&session->pipeline_head, 800000);
    atomic_store(&session->pipeline_tail, 0);
    KTerm_Update(term);

    bool found_xoff = false;
    for(int i=0; i<output_pos; i++) if (output_buffer[i] == 0x13) found_xoff = true;
    if (!found_xoff) { printf("FAIL: DECXRLM XOFF not sent\n"); exit(1); }

    reset_output_buffer();
    atomic_store(&session->pipeline_head, 100);
    atomic_store(&session->pipeline_tail, 0);
    KTerm_Update(term);

    bool found_xon = false;
    for(int i=0; i<output_pos; i++) if (output_buffer[i] == 0x11) found_xon = true;
    if (!found_xon) { printf("FAIL: DECXRLM XON not sent\n"); exit(1); }

    KTerm_Destroy(term);
    printf("VT510 Gems PASS.\n");
}

int main(void) {
    test_deccolm();
    test_deccra();
    test_decdld();
    test_deceskm();
    test_dechdpxm();
    test_deckbum();
    test_decsera();
    test_vt420_fixes();
    test_vt510_gems();

    printf("\nAll DEC Feature Tests Passed Successfully!\n");
    return 0;
}
