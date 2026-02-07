#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

static KTerm* term = NULL;

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void base64_encode(const unsigned char *src, size_t len, char *out_buff) {
    size_t i = 0, j = 0;
    while (i < len) {
        uint32_t octet_a = i < len ? src[i++] : 0;
        uint32_t octet_b = i < len ? src[i++] : 0;
        uint32_t octet_c = i < len ? src[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        out_buff[j++] = b64_table[(triple >> 3 * 6) & 0x3F];
        out_buff[j++] = b64_table[(triple >> 2 * 6) & 0x3F];
        out_buff[j++] = b64_table[(triple >> 1 * 6) & 0x3F];
        out_buff[j++] = b64_table[(triple >> 0 * 6) & 0x3F];
    }
    // Padding
    if (len % 3 == 1) {
        out_buff[j-1] = '=';
        out_buff[j-2] = '=';
    } else if (len % 3 == 2) {
        out_buff[j-1] = '=';
    }
    out_buff[j] = '\0';
}

void InjectDCS(const char* dcs) {
    for (int i = 0; dcs[i] != '\0'; i++) {
        KTerm_ProcessChar(term, GET_SESSION(term), (unsigned char)dcs[i]);
    }
}

int main() {
    KTermConfig config = {0};
    term = KTerm_Create(config);
    if (!term) return 1;

    printf("Testing Advanced Grid Features...\n");

    // Enable Grid
    GET_SESSION(term)->grid_enabled = true;

    // ---------------------------------------------------------
    // Test 1: Fill Masked (Existing Logic check + Verify Mask usage)
    // ---------------------------------------------------------
    printf("Test 1: Fill Masked (CH only)\n");
    // EXT;grid;fill;<sid>;x;y;w;h;mask;ch;...
    // Mask 0x1 = CH. Ch = 65 ('A')
    InjectDCS("\x1BPGATE;KTERM;1;EXT;grid;fill;0;0;0;5;1;0x1;65\x1B\\");

    // Flush
    KTerm_FlushOps(term, GET_SESSION(term));

    EnhancedTermChar* cell = KTerm_GetCell(term, 0, 0);
    if (cell && cell->ch == 'A') {
        printf("PASS: Fill CH 'A'\n");
    } else {
        printf("FAIL: Fill CH 'A'. Got %c (%d)\n", cell ? cell->ch : 0, cell ? cell->ch : 0);
        return 1;
    }

    // Test Color Mask (0x2 = FG). Set FG to index 1 (Red).
    // fill;0;0;0;5;1;0x2;;pal:1
    // Note: ch param skipped (empty).
    InjectDCS("\x1BPGATE;KTERM;2;EXT;grid;fill;0;0;0;5;1;0x2;;pal:1\x1B\\");
    KTerm_FlushOps(term, GET_SESSION(term));

    cell = KTerm_GetCell(term, 0, 0);
    if (cell && cell->ch == 'A' && cell->fg_color.value.index == 1) {
        printf("PASS: Fill FG Red, preserve CH\n");
    } else {
        printf("FAIL: Fill FG Red. Got CH=%c FG=%d\n", cell->ch, cell->fg_color.value.index);
        return 1;
    }

    // ---------------------------------------------------------
    // Test 2: Stream (New Feature)
    // ---------------------------------------------------------
    printf("Test 2: Stream Cells\n");
    // stream;sid;x;y;w;h;mask;count;compress;data
    // Mask 0x1 (CH). 5 cells.
    // Data: 'H', 'E', 'L', 'L', 'O' (int32)
    uint32_t data[] = { 'H', 'E', 'L', 'L', 'O' };
    char b64[128];
    base64_encode((unsigned char*)data, sizeof(data), b64);

    char seq[256];
    // Write to (0, 1) -> Row 1
    snprintf(seq, sizeof(seq), "\x1BPGATE;KTERM;3;EXT;grid;stream;0;0;1;5;1;0x1;5;0;%s\x1B\\", b64);
    InjectDCS(seq);
    KTerm_FlushOps(term, GET_SESSION(term));

    cell = KTerm_GetCell(term, 0, 1);
    if (cell && cell->ch == 'H') printf("PASS: Stream[0] = H\n");
    else { printf("FAIL: Stream[0]. Got %c\n", cell->ch); return 1; }

    cell = KTerm_GetCell(term, 4, 1);
    if (cell && cell->ch == 'O') printf("PASS: Stream[4] = O\n");
    else { printf("FAIL: Stream[4]. Got %c\n", cell->ch); return 1; }

    // ---------------------------------------------------------
    // Test 3: Copy Rect
    // ---------------------------------------------------------
    printf("Test 3: Copy Rect\n");
    // copy;sid;src_x;src_y;dst_x;dst_y;w;h;mode
    // Copy "HELLO" from (0,1) to (0,2)
    // w=5, h=1
    InjectDCS("\x1BPGATE;KTERM;4;EXT;grid;copy;0;0;1;0;2;5;1;0\x1B\\");
    KTerm_FlushOps(term, GET_SESSION(term));

    cell = KTerm_GetCell(term, 0, 2);
    if (cell && cell->ch == 'H') printf("PASS: Copy[0] = H\n");
    else { printf("FAIL: Copy[0]. Got %c\n", cell ? cell->ch : 0); return 1; }

    // Source should remain
    cell = KTerm_GetCell(term, 0, 1);
    if (cell && cell->ch == 'H') printf("PASS: Copy Source Preserved\n");
    else { printf("FAIL: Copy Source Lost\n"); return 1; }

    // ---------------------------------------------------------
    // Test 4: Move Rect (Copy + Clear Source)
    // ---------------------------------------------------------
    printf("Test 4: Move Rect\n");
    // move;sid;src_x;src_y;dst_x;dst_y;w;h;mode
    // Move "HELLO" from (0,2) to (0,3). Mode 0 (default move implies clear? No, param mode)
    // Wait, PR says "move = copy + clear source if mode&clear".
    // Actually, usually "Move" implies clear source.
    // Spec: DCS > EXT;grid;move;...
    // Implementation note: "bool clear_src = (sub == GRID_SUB_MOVE);"
    // So MOVE command automatically clears source regardless of mode?
    // "move = copy + clear source if mode&clear" -> This line in PR desc is ambiguous.
    // "copy/move: New rectangular copy/move operations... <mode>: Bitflags (e.g., 0x1=overwrite protected, 0x2=clear source)."
    // "DCS > EXT;grid;move;... (move = copy + clear source if mode&clear)"
    // Or maybe "move" is just an alias for "copy with clear source bit set"?
    // The pseudo code says: `bool clear_src = (sub == GRID_SUB_MOVE);`
    // This implies explicit MOVE command sets the flag.
    // I will implement it so MOVE command forces clear_source = true.

    InjectDCS("\x1BPGATE;KTERM;5;EXT;grid;move;0;0;2;0;3;5;1;0\x1B\\");
    KTerm_FlushOps(term, GET_SESSION(term));

    cell = KTerm_GetCell(term, 0, 3);
    if (cell && cell->ch == 'H') printf("PASS: Move Dest = H\n");
    else { printf("FAIL: Move Dest. Got %c\n", cell ? cell->ch : 0); return 1; }

    cell = KTerm_GetCell(term, 0, 2);
    // Should be empty/space now (if cleared)
    if (cell && (cell->ch == 0 || cell->ch == ' ')) printf("PASS: Move Source Cleared\n");
    else { printf("FAIL: Move Source Not Cleared. Got %c\n", cell ? cell->ch : 0); return 1; }

    // ---------------------------------------------------------
    // Test 5: Stream Zero Width (Crash Check)
    // ---------------------------------------------------------
    printf("Test 5: Stream Zero Width\n");
    // stream... w=0. Should default to 1 and not crash.
    char b64_zero[16];
    uint32_t val = 'Z';
    base64_encode((unsigned char*)&val, 4, b64_zero);
    snprintf(seq, sizeof(seq), "\x1BPGATE;KTERM;6;EXT;grid;stream;0;0;0;0;1;0x1;1;0;%s\x1B\\", b64_zero);
    InjectDCS(seq);
    KTerm_FlushOps(term, GET_SESSION(term));
    cell = KTerm_GetCell(term, 0, 0);
    if (cell && cell->ch == 'Z') printf("PASS: Stream Zero Width Handled\n");
    else { printf("FAIL: Stream Zero Width. Got %c\n", cell ? cell->ch : 0); return 1; }

    // ---------------------------------------------------------
    // Test 6: Stream Concurrency (Read-Modify-Write Race)
    // ---------------------------------------------------------
    printf("Test 6: Stream Concurrency\n");
    // 1. Queue Fill (BG Blue, Index 4) at (5,0)
    // 2. Queue Stream (Mask CH only 'C') at (5,0) without intermediate flush in test code
    // If stream handler flushes, it sees BG Blue. If not, it sees default.

    // Fill (5,0) 1x1 with BG=4
    InjectDCS("\x1BPGATE;KTERM;7;EXT;grid;fill;0;5;0;1;1;0x4;;;pal:4\x1B\\");

    // Stream 'C' at (5,0) Mask=0x1 (CH). It should preserve BG=4.
    uint32_t c_val = 'C';
    char b64_c[16];
    base64_encode((unsigned char*)&c_val, 4, b64_c);

    snprintf(seq, sizeof(seq), "\x1BPGATE;KTERM;8;EXT;grid;stream;0;5;0;1;1;0x1;1;0;%s\x1B\\", b64_c);
    InjectDCS(seq);

    // Now Flush everything
    KTerm_FlushOps(term, GET_SESSION(term));

    cell = KTerm_GetCell(term, 5, 0);
    if (cell && cell->ch == 'C' && cell->bg_color.value.index == 4) {
        printf("PASS: Stream Concurrency (BG Preserved)\n");
    } else {
        printf("FAIL: Stream Concurrency. CH=%c (Exp C) BG=%d (Exp 4)\n",
               cell ? cell->ch : 0, cell ? cell->bg_color.value.index : -1);
        return 1;
    }

    printf("All Advanced Grid tests passed.\n");
    KTerm_Destroy(term);
    return 0;
}
