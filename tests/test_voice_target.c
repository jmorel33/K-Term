#define KTERM_TESTING
#define KTERM_IMPLEMENTATION
#define KTERM_ENABLE_GATEWAY

#include "kterm.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main() {
    printf("Starting Voice Target Verification...\n");

    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    if (!term) {
        fprintf(stderr, "Failed to create KTerm\n");
        return 1;
    }
    KTermSession* session = &term->sessions[0];

    // 1. Test Normal Target Setting
    printf("Testing normal target setting...\n");
    const char* test_ip = "192.168.1.100";
    if (KTerm_Voice_SetTarget(session, test_ip) != SITUATION_SUCCESS) {
        fprintf(stderr, "KTerm_Voice_SetTarget failed for valid input\n");
        return 1;
    }

    KTermVoiceContext* ctx = KTerm_Voice_GetContext(session);
    if (!ctx) {
        fprintf(stderr, "Failed to get voice context\n");
        return 1;
    }

    if (strcmp(ctx->target, test_ip) != 0) {
        fprintf(stderr, "Target mismatch: expected '%s', got '%s'\n", test_ip, ctx->target);
        return 1;
    }

    // 2. Test Clearing Target (NULL)
    printf("Testing clearing target (NULL)...\n");
    if (KTerm_Voice_SetTarget(session, NULL) != SITUATION_SUCCESS) {
        fprintf(stderr, "KTerm_Voice_SetTarget failed for NULL input\n");
        return 1;
    }
    if (ctx->target[0] != '\0') {
        fprintf(stderr, "Target not cleared after NULL input\n");
        return 1;
    }

    // 3. Test Long Target Truncation
    printf("Testing long target truncation...\n");
    char long_target[512];
    memset(long_target, 'A', 511);
    long_target[511] = '\0';

    if (KTerm_Voice_SetTarget(session, long_target) != SITUATION_SUCCESS) {
        fprintf(stderr, "KTerm_Voice_SetTarget failed for long input\n");
        return 1;
    }

    if (strlen(ctx->target) != 255) {
        fprintf(stderr, "Truncation failed: expected length 255, got %zu\n", strlen(ctx->target));
        return 1;
    }
    for(int i=0; i<255; i++) {
        if (ctx->target[i] != 'A') {
            fprintf(stderr, "Truncated content mismatch at index %d\n", i);
            return 1;
        }
    }
    if (ctx->target[255] != '\0') {
        fprintf(stderr, "Truncated string not null-terminated\n");
        return 1;
    }

    // 4. Test Invalid Session
    printf("Testing invalid session...\n");
    if (KTerm_Voice_SetTarget(NULL, "test") == SITUATION_SUCCESS) {
        fprintf(stderr, "KTerm_Voice_SetTarget should fail for NULL session\n");
        return 1;
    }

    printf("Voice Target Verification Passed!\n");

    KTerm_Destroy(term);
    return 0;
}
