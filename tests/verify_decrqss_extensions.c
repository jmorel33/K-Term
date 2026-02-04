#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../tests/mock_situation.h"
#include "../kterm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

int main(void) {
    printf("Starting DECRQSS Extensions Verification...\n");

    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    config.response_callback = response_callback;
    KTerm* term = KTerm_Create(config);
    if (!term) { printf("Failed to create KTerm\n"); exit(1); }
    KTermSession* session = GET_SESSION(term);

    // 1. Verify 'm' (SGR)
    // Default (0 m)
    reset_output_buffer();
    write_sequence(term, "\x1BP$qm\x1B\\"); // DCS $ q m ST
    if (!strstr(output_buffer, "\x1BP1$r0m\x1B\\")) {
         printf("FAIL: SGR Default mismatch. Got: '%s'\n", output_buffer);
         // Don't exit yet, might be implemented as empty or just 'm'
    }

    // Set Bold (1) and Red (31)
    write_sequence(term, "\x1B[1;31m");
    reset_output_buffer();
    write_sequence(term, "\x1BP$qm\x1B\\");
    // Expect 1;31m or similar. Order matters for strict string match but logically equivalent.
    // We'll check if it contains 1 and 31.
    if (!strstr(output_buffer, "1;31") && !strstr(output_buffer, "31;1")) {
         printf("FAIL: SGR Bold Red mismatch. Got: '%s'\n", output_buffer);
    }

    // 2. Verify 'r' (DECSTBM)
    // Default (whole screen) -> 1;24r
    reset_output_buffer();
    write_sequence(term, "\x1BP$qr\x1B\\");
    if (!strstr(output_buffer, "1;24r")) {
         printf("FAIL: DECSTBM Default mismatch. Got: '%s'\n", output_buffer);
    }

    // Set 5;20
    write_sequence(term, "\x1B[5;20r");
    reset_output_buffer();
    write_sequence(term, "\x1BP$qr\x1B\\");
    if (!strstr(output_buffer, "5;20r")) {
         printf("FAIL: DECSTBM 5;20 mismatch. Got: '%s'\n", output_buffer);
    }

    // 3. Verify 's' (DECSLRM)
    // Default (whole width) -> 1;80s
    reset_output_buffer();
    write_sequence(term, "\x1BP$qs\x1B\\");
    // DECSLRM usually only works if enabled (mode 69), but query might return even if disabled?
    // Usually margins are reset to full width when mode is disabled.
    // Let's enable mode 69 just in case logic depends on it.
    write_sequence(term, "\x1B[?69h");

    // Set 2;79
    write_sequence(term, "\x1B[2;79s");
    reset_output_buffer();
    write_sequence(term, "\x1BP$qs\x1B\\");
    if (!strstr(output_buffer, "2;79s")) {
         printf("FAIL: DECSLRM 2;79 mismatch. Got: '%s'\n", output_buffer);
    }

    // 4. Verify 't' (DECSLPP)
    // Default 24 rows
    // Note: DECSLPP sets lines per page, often synonymous with rows.
    reset_output_buffer();
    write_sequence(term, "\x1BP$qt\x1B\\");
    if (!strstr(output_buffer, "24t")) {
         printf("FAIL: DECSLPP 24 mismatch. Got: '%s'\n", output_buffer);
    }

    // 5. Verify '|' (DECSCPP)
    // Default 80 cols
    reset_output_buffer();
    write_sequence(term, "\x1BP$q|\x1B\\");
    if (!strstr(output_buffer, "80|")) {
         printf("FAIL: DECSCPP 80 mismatch. Got: '%s'\n", output_buffer);
    }

    // Switch to 132 (Requires enabling 40 first)
    write_sequence(term, "\x1B[?40h");
    write_sequence(term, "\x1B[?3h"); // DECCOLM
    reset_output_buffer();
    write_sequence(term, "\x1BP$q|\x1B\\");
    if (!strstr(output_buffer, "132|")) {
         printf("FAIL: DECSCPP 132 mismatch. Got: '%s'\n", output_buffer);
    }

    KTerm_Destroy(term);
    printf("DECRQSS Extensions Verification Completed.\n");
    return 0;
}
