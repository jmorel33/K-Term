#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdatomic.h>

#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../tests/mock_situation.h"
#include "../kterm.h"

// Sink Context
typedef struct {
    char buffer[1024];
    size_t length;
} SinkContext;

// Sink Callback
void MySink(void* ctx, const char* data, size_t len) {
    SinkContext* sc = (SinkContext*)ctx;
    if (sc->length + len < sizeof(sc->buffer)) {
        memcpy(sc->buffer + sc->length, data, len);
        sc->length += len;
    }
}

void test_sink_output() {
    printf("Testing Sink Output...\n");
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    KTermSession* session = GET_SESSION(term);

    // 1. Test Ring Buffer Mode (Default)
    KTerm_QueueResponse(term, "Hello");
    // Verify ring buffer has data
    int head = atomic_load(&session->response_ring.head);
    int tail = atomic_load(&session->response_ring.tail);
    if (head != tail) {
        printf("PASS: Ring buffer active.\n");
    } else {
        printf("FAIL: Data not queued to ring buffer.\n");
    }

    // 2. Test Flush on SetOutputSink
    SinkContext sc = {0};
    // Setting sink should trigger flush of pending ring buffer data
    KTerm_SetOutputSink(term, MySink, &sc);

    // Manually trigger update to ensure flush logic runs if SetOutputSink doesn't do it implicitly?
    // KTerm_SetOutputSink documentation says "triggers an immediate flush".
    // Let's verify.

    if (sc.length == 5 && strncmp(sc.buffer, "Hello", 5) == 0) {
        printf("PASS: Flush to Sink worked.\n");
    } else {
        // Retry with Update if immediate flush isn't guaranteed by API (but spec says it is)
        // KTerm_Update(term);
        printf("FAIL: Flush content mismatch. Got: '%.*s'\n", (int)sc.length, sc.buffer);
    }

    // 3. Test Direct Sink Output
    // Since sink is set, subsequent writes might go to sink via Update loop?
    // KTerm_WriteInternal writes to ring buffer.
    // KTerm_Update flushes ring buffer to sink.

    sc.length = 0;
    KTerm_QueueResponse(term, "World");

    // Trigger flush
    KTerm_Update(term);

    if (sc.length == 5 && strncmp(sc.buffer, "World", 5) == 0) {
        printf("PASS: Direct Sink output worked.\n");
    } else {
        printf("FAIL: Direct Sink output mismatch. Got: '%.*s'\n", (int)sc.length, sc.buffer);
    }

    KTerm_Destroy(term);
}

void test_binary_safety() {
    printf("Testing Binary Safety...\n");
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);

    // Use a Sink to verify data integrity
    SinkContext sc = {0};
    KTerm_SetOutputSink(term, MySink, &sc);

    // Write binary data (0xFF)
    char bin_data[] = {0xFF, 0x00, 0xAA};
    KTerm_QueueResponseBytes(term, bin_data, 3);

    KTerm_Update(term);

    if (sc.length == 3 &&
        (unsigned char)sc.buffer[0] == 0xFF &&
        (unsigned char)sc.buffer[1] == 0x00 &&
        (unsigned char)sc.buffer[2] == 0xAA) {
        printf("PASS: Binary data preserved.\n");
    } else {
        printf("FAIL: Binary data corrupted.\n");
    }

    KTerm_Destroy(term);
}

int main() {
    test_sink_output();
    test_binary_safety();
    return 0;
}
