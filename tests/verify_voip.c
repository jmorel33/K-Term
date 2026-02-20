#include <stdio.h>
#include <string.h>

#define KTERM_TESTING
#define KTERM_IMPLEMENTATION
#define KTERM_ENABLE_GATEWAY
// #define KTERM_DISABLE_NET
#define KTERM_DISABLE_VOICE

#include "kterm.h"

// Define a simple output callback to capture response
void test_response_callback(KTerm* term, const char* response, int length) {
    (void)term;
    printf("Gateway Response: %.*s\n", length, response);
}

int main() {
    printf("Initializing KTerm...\n");
    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    config.response_callback = test_response_callback;

    KTerm* term = KTerm_Create(config);
    if (!term) {
        fprintf(stderr, "Failed to create KTerm\n");
        return 1;
    }

    // Initialize session 0
    KTerm_InitSession(term, 0);
    term->active_session = 0;
    term->sessions[0].session_open = true;

    printf("\n--- Testing EXT;voip;register ---\n");
    KTerm_GatewayProcess(term, &term->sessions[0], "KTERM", "1", "EXT", "voip;register;user=alice;pass=123;domain=example.com");
    KTerm_Update(term); // Flush responses

    printf("\n--- Testing EXT;voip;dial ---\n");
    KTerm_GatewayProcess(term, &term->sessions[0], "KTERM", "2", "EXT", "voip;dial;sip:bob@example.com");
    KTerm_Update(term);

    printf("\n--- Testing EXT;voip;dtmf ---\n");
    KTerm_GatewayProcess(term, &term->sessions[0], "KTERM", "3", "EXT", "voip;dtmf;5");
    KTerm_Update(term);

    printf("\n--- Testing EXT;voip;hangup ---\n");
    KTerm_GatewayProcess(term, &term->sessions[0], "KTERM", "4", "EXT", "voip;hangup");
    KTerm_Update(term);

    KTerm_Destroy(term);
    printf("\nTest Complete.\n");
    return 0;
}
