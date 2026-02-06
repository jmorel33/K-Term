#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

static KTerm* term = NULL;
static char last_response[4096];

void MockResponseCallback(KTerm* term, const char* response, int length) {
    strncpy(last_response, response, sizeof(last_response) - 1);
    last_response[length] = '\0';
}

void CheckGrid(int x, int y, unsigned int expected_ch, int check_ch, 
               int expected_fg_mode, int expected_fg_idx, int check_fg,
               int expected_bg_mode, RGB_KTermColor expected_bg_rgb, int check_bg) {
               
    EnhancedTermChar* cell = KTerm_GetCell(term, x, y);
    assert(cell != NULL);
    
    if (check_ch) {
        if (cell->ch != expected_ch) {
            printf("FAIL at (%d,%d): Char expected %u, got %u\n", x, y, expected_ch, cell->ch);
            exit(1);
        }
    }
    
    if (check_fg) {
        if (cell->fg_color.color_mode != expected_fg_mode) {
             printf("FAIL at (%d,%d): FG Mode expected %d, got %d\n", x, y, expected_fg_mode, cell->fg_color.color_mode);
             exit(1);
        }
        if (expected_fg_mode == 0 && cell->fg_color.value.index != expected_fg_idx) {
             printf("FAIL at (%d,%d): FG Index expected %d, got %d\n", x, y, expected_fg_idx, cell->fg_color.value.index);
             exit(1);
        }
    }
    
    if (check_bg) {
        if (cell->bg_color.color_mode != expected_bg_mode) {
             printf("FAIL at (%d,%d): BG Mode expected %d, got %d\n", x, y, expected_bg_mode, cell->bg_color.color_mode);
             exit(1);
        }
        if (expected_bg_mode == 1) {
            if (cell->bg_color.value.rgb.r != expected_bg_rgb.r ||
                cell->bg_color.value.rgb.g != expected_bg_rgb.g ||
                cell->bg_color.value.rgb.b != expected_bg_rgb.b) {
                 printf("FAIL at (%d,%d): BG RGB expected %d,%d,%d, got %d,%d,%d\n", x, y, 
                        expected_bg_rgb.r, expected_bg_rgb.g, expected_bg_rgb.b,
                        cell->bg_color.value.rgb.r, cell->bg_color.value.rgb.g, cell->bg_color.value.rgb.b);
                 exit(1);
            }
        }
    }
}

int main() {
    KTermConfig config = {0};
    config.response_callback = MockResponseCallback;
    term = KTerm_Create(config);
    
    // Ensure 'grid' extension is registered (it's built-in now)
    
    printf("Testing Gateway Grid Extension...\n");
    
    // 1. Fill Char 'A' in 5x5 rect at 0,0
    // DCS GATE;KTERM;0;EXT;grid;fill;0;0;0;5;5;1;65;0;0;0;0;0 ST
    // Mask 1 = CH
    // Char 65 = 'A'
    const char* cmd1 = "\x1BPGATE;KTERM;0;EXT;grid;fill;0;0;0;5;5;1;65;0;0;0;0;0\x1B\\";
    KTerm_WriteString(term, cmd1);
    KTerm_ProcessEvents(term);
    KTerm_Update(term); // Flush ops
    
    CheckGrid(0, 0, 'A', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    CheckGrid(4, 4, 'A', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0);
    CheckGrid(5, 5, ' ', 1, 0, 0, 0, 0, (RGB_KTermColor){0}, 0); // Outside
    printf("PASS: Fill Char 'A'\n");
    
    // 2. Set FG Red (1) in 3x3 rect at 1,1 without changing Char
    // Mask 2 = FG
    // FG = pal:1
    const char* cmd2 = "\x1BPGATE;KTERM;0;EXT;grid;fill;0;1;1;3;3;2;0;pal:1;0;0;0;0\x1B\\";
    printf("Sending cmd2: %s\n", cmd2 + 1); // Skip ESC for print
    printf("State before cmd2: %d\n", GET_SESSION(term)->parse_state);
    if (!KTerm_WriteString(term, cmd2)) printf("Failed to write cmd2\n");
    KTerm_ProcessEvents(term);
    printf("State after cmd2: %d\n", GET_SESSION(term)->parse_state);
    KTerm_Update(term);
    
    // Center of 5x5 (filled with A)
    // Should be 'A' with Red FG
    CheckGrid(1, 1, 'A', 1, 0, 1, 1, 0, (RGB_KTermColor){0}, 0);
    CheckGrid(2, 2, 'A', 1, 0, 1, 1, 0, (RGB_KTermColor){0}, 0);
    CheckGrid(3, 3, 'A', 1, 0, 1, 1, 0, (RGB_KTermColor){0}, 0);
    
    // Outside 3x3 but inside 5x5 (should be 'A' with default FG)
    // Default FG is usually 7 (White) or whatever InitSession set (White=7)
    // wait, init session sets White (index 7) or similar.
    // Let's check what default is.
    EnhancedTermChar* cell = KTerm_GetCell(term, 0, 0);
    int default_fg = cell->fg_color.value.index;
    
    CheckGrid(0, 0, 'A', 1, 0, default_fg, 1, 0, (RGB_KTermColor){0}, 0);
    printf("PASS: Fill FG Red Preserving Char\n");
    
    // 3. Set BG RGB Green in 1x1 at 2,2
    // Mask 4 = BG
    // BG = rgb:00ff00
    const char* cmd3 = "\x1BPGATE;KTERM;0;EXT;grid;fill;0;2;2;1;1;4;0;0;rgb:00ff00;0;0;0\x1B\\";
    KTerm_WriteString(term, cmd3);
    KTerm_ProcessEvents(term);
    KTerm_Update(term);
    
    CheckGrid(2, 2, 'A', 1, 0, 1, 1, 1, (RGB_KTermColor){0, 255, 0, 255}, 1);
    printf("PASS: Fill BG RGB Preserving Char & FG\n");
    
    printf("All Gateway Grid tests passed.\n");
    KTerm_Destroy(term);
    return 0;
}
