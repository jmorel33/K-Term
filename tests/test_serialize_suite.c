#define KTERM_TESTING
#define KTERM_IMPLEMENTATION
#include "../kterm.h"

#define KTERM_SERIALIZE_IMPLEMENTATION
#include "../kt_serialize.h"

#include "mock_situation.h"
#include "test_utilities.h"

// Helper to ensure ops are applied
void flush_ops(KTerm* term, KTermSession* session) {
    KTerm_FlushOps(term, session);
}

int test_basic_serialization(KTerm* term, KTermSession* session) {
    // 1. Setup initial state
    reset_terminal(term);
    write_sequence(term, "\x1B[2J\x1B[H"); // Clear
    flush_ops(term, session);

    // Write text at 0,0
    write_sequence(term, "Hello World");
    flush_ops(term, session);

    // Move cursor and modify a cell
    session->cursor.x = 5;
    session->cursor.y = 2;
    EnhancedTermChar* cell = GetScreenCell(session, 2, 5);
    if (!cell) {
        fprintf(stderr, "Invalid cell coordinates\n");
        return 0;
    }
    cell->ch = 'X';
    cell->flags = KTERM_ATTR_BOLD;

    // 2. Serialize
    void* buffer = NULL;
    size_t len = 0;
    if (!KTerm_SerializeSession(session, &buffer, &len)) {
        fprintf(stderr, "Serialization failed\n");
        return 0;
    }

    // 3. Clear/Reset session
    reset_terminal(term);
    write_sequence(term, "\x1B[2J\x1B[H"); // Clear screen and home cursor
    flush_ops(term, session);

    // Verify it's cleared (cursor should be home 0,0)
    if (session->cursor.x != 0 || session->cursor.y != 0) {
        fprintf(stderr, "Reset failed to home cursor. Got %d,%d\n", session->cursor.x, session->cursor.y);
        return 0;
    }
    // Verify cell is cleared
    EnhancedTermChar* cell_cleared = GetScreenCell(session, 2, 5);
    if (cell_cleared->ch == 'X') {
        fprintf(stderr, "Reset failed to clear cell\n");
        return 0;
    }

    // 4. Deserialize
    if (!KTerm_DeserializeSession(session, buffer, len)) {
        fprintf(stderr, "Deserialization failed\n");
        return 0;
    }

    // 5. Verify
    // Check cursor
    if (session->cursor.x != 5 || session->cursor.y != 2) {
        fprintf(stderr, "Cursor mismatch: %d,%d vs 5,2\n", session->cursor.x, session->cursor.y);
        return 0;
    }
    // Check cell content at 2,5
    EnhancedTermChar* cell_restored = GetScreenCell(session, 2, 5);
    if (cell_restored->ch != 'X') {
        fprintf(stderr, "Cell char mismatch: Expected 'X', got '%c'\n", cell_restored->ch);
        return 0;
    }
    if (!(cell_restored->flags & KTERM_ATTR_BOLD)) {
        fprintf(stderr, "Cell attr mismatch: Expected BOLD\n");
        return 0;
    }

    // Check text "Hello World" at 0,0
    EnhancedTermChar* h = GetScreenCell(session, 0, 0);
    if (h->ch != 'H') {
        fprintf(stderr, "Text content lost at 0,0: %c (0x%02X)\n", h->ch, h->ch);
        return 0;
    }

    KTerm_Free(buffer);
    return 1;
}

int test_serialize_null_checks(KTerm* term, KTermSession* session) {
    void* buffer = NULL;
    size_t len = 0;
    int passed = 1;

    // Test null session
    if (KTerm_SerializeSession(NULL, &buffer, &len)) {
        fprintf(stderr, "KTerm_SerializeSession should fail on NULL session\n");
        passed = 0;
    }

    // Test null out_buf
    if (KTerm_SerializeSession(session, NULL, &len)) {
        fprintf(stderr, "KTerm_SerializeSession should fail on NULL out_buf\n");
        passed = 0;
    }

    // Test null out_len
    if (KTerm_SerializeSession(session, &buffer, NULL)) {
        fprintf(stderr, "KTerm_SerializeSession should fail on NULL out_len\n");
        passed = 0;
    }

    return passed;
}

int main() {
    TestResults results = {0};
    KTerm* term = create_test_term(80, 24);
    if (!term) {
        fprintf(stderr, "Failed to create terminal\n");
        return 1;
    }
    KTermSession* session = GET_SESSION(term);

    print_test_header("Serialization");

    run_test("Basic Serialization & Restore", test_basic_serialization, term, session, &results);
    run_test("Serialization Null Checks", test_serialize_null_checks, term, session, &results);

    print_test_summary(results.total, results.passed, results.failed);
    destroy_test_term(term);
    return results.failed;
}
