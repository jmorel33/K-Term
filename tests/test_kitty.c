#include <stdio.h>
#include <assert.h>
#include <string.h>

#define KTERM_TESTING
#define KTERM_IMPLEMENTATION
#include "../kterm.h"

static KTerm* term = NULL;

void TestChunkedTransmission() {
    printf("Testing Chunked Transmission...\n");
    // Reset session state
    KTermSession* session = GET_SESSION(term);
    // Manually clear kitty images if any
    if (session->kitty.images) {
        free(session->kitty.images);
        session->kitty.images = NULL;
        session->kitty.image_count = 0;
        session->kitty.current_memory_usage = 0;
    }

    // Sequence 1: APC G a=t,i=1,m=1; base64_chunk1 ST
    // chunk1: "SGVsbG8=" (Hello)
    const char* seq1 = "\x1B_Ga=t,i=1,m=1;SGVsbG8=\x1B\\";
    KTerm_WriteString(term, seq1);
    KTerm_ProcessEvents(term);

    // Verify state
    assert(session->kitty.image_count == 1);
    assert(session->kitty.images[0].id == 1);
    assert(session->kitty.images[0].frame_count > 0);
    assert(session->kitty.images[0].frames[0].size == 5); // "Hello"
    assert(session->kitty.continuing == true);
    assert(session->kitty.active_upload == &session->kitty.images[0]);
    assert(session->kitty.images[0].complete == false); // Not complete yet

    // Sequence 2: APC G m=0; base64_chunk2 ST (Continuation, no params needed except m=0 to finish)
    // chunk2: "IFdvcmxk" ( World)
    const char* seq2 = "\x1B_Gm=0;IFdvcmxk\x1B\\";
    KTerm_WriteString(term, seq2);
    KTerm_ProcessEvents(term);

    // Verify
    assert(session->kitty.image_count == 1); // Should still be 1 image
    assert(session->kitty.images[0].frames[0].size == 11); // "Hello World"
    assert(session->kitty.continuing == false);
    assert(session->kitty.images[0].complete == true); // Complete now

    // Check content
    const char* expected = "Hello World";
    if (memcmp(session->kitty.images[0].frames[0].data, expected, 11) != 0) {
        printf("Expected 'Hello World', got '%.*s'\n", (int)session->kitty.images[0].frames[0].size, session->kitty.images[0].frames[0].data);
        assert(0);
    }

    printf("Chunked Transmission Verified.\n");
}

void TestPlacement() {
    printf("Testing Placement...\n");
    KTermSession* session = GET_SESSION(term);

    // Clear images
    // (Simulate reset)
    if (session->kitty.images) {
        for(int i=0; i<session->kitty.image_count; i++) {
            if (session->kitty.images[i].frames) {
                for (int f=0; f<session->kitty.images[i].frame_count; f++) free(session->kitty.images[i].frames[f].data);
                free(session->kitty.images[i].frames);
            }
        }
        free(session->kitty.images);
        session->kitty.images = NULL;
        session->kitty.image_count = 0;
        session->kitty.current_memory_usage = 0;
    }

    // Upload an image first (a=t)
    const char* upload = "\x1B_Ga=t,i=10;SGVsbG8=\x1B\\";
    KTerm_WriteString(term, upload);
    KTerm_ProcessEvents(term);

    assert(session->kitty.images[0].x == 0);
    assert(session->kitty.images[0].visible == false); // a=t should be invisible

    // Move it with a=p
    // APC G a=p,i=10,x=100,y=200 ST
    const char* place = "\x1B_Ga=p,i=10,x=100,y=200\x1B\\";
    KTerm_WriteString(term, place);
    KTerm_ProcessEvents(term);

    assert(session->kitty.images[0].x == 100);
    assert(session->kitty.images[0].y == 200);
    assert(session->kitty.images[0].visible == true);

    // Test Transmit & Place (a=T)
    // APC G a=T,i=20,x=50,y=50;... ST
    const char* transmit_place = "\x1B_Ga=T,i=20,x=50,y=50;SGVsbG8=\x1B\\";
    KTerm_WriteString(term, transmit_place);
    KTerm_ProcessEvents(term);

    assert(session->kitty.image_count == 2);
    assert(session->kitty.images[1].id == 20);
    assert(session->kitty.images[1].x == 50);
    assert(session->kitty.images[1].y == 50);
    assert(session->kitty.images[1].visible == true);

    printf("Placement Verified.\n");
}

int main() {
    printf("Initializing Terminal...\n");
    KTermConfig config = {0};
    term = KTerm_Create(config);
    KTerm_SetLevel(term, GET_SESSION(term), VT_LEVEL_XTERM);

    TestChunkedTransmission();
    TestPlacement();

    KTerm_Cleanup(term);
    printf("All Kitty Tests Passed.\n");
    return 0;
}
