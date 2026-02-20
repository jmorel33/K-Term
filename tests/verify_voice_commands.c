#define KTERM_TESTING
#define KTERM_IMPLEMENTATION
#define KTERM_ENABLE_GATEWAY

#include "kterm.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main() {
    printf("Starting Voice Command Verification...\n");

    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    if (!term) return 1;
    KTermSession* session = &term->sessions[0];

    // 1. Enable Voice
    if (KTerm_Voice_Enable(session, true) != SITUATION_SUCCESS) {
        fprintf(stderr, "Voice Enable Failed\n");
        return 1;
    }

    // 2. Mock Audio Data to trigger ProcessCapture (needed to set ctx->term)
    // We feed silence initially
    float silence[256] = {0};
    if (mock_audio_cb) mock_audio_cb(mock_audio_user_data, silence, 256);
    KTerm_Net_Process(term);

    // 3. Test KTerm_Voice_Command Injection
    printf("Testing KTerm_Voice_Command...\n");
    if (KTerm_Voice_Command("ls") != SITUATION_SUCCESS) {
        fprintf(stderr, "KTerm_Voice_Command returned failure\n");
        return 1;
    }

    // 4. Verify Input Buffer
    KTermKeyEvent event;
    char buffer[64] = {0};
    int idx = 0;

    // Consume queue
    while(KTerm_GetKey(term, &event)) {
        char c = '?';
        if (event.key_code == KTERM_KEY_L) c = 'l';
        else if (event.key_code == KTERM_KEY_S) c = 's';

        // Also check raw sequence if set
        if (event.sequence[0]) c = event.sequence[0];

        buffer[idx++] = c;
    }
    buffer[idx] = '\0';

    // Note: KTerm_QueueInputEvent logic I wrote creates Key Events.
    // If I map 'l' to KTERM_KEY_L, the sequence is NOT yet generated because KTerm_TranslateKey generates it in KTerm_QueueInputEvent
    // BUT KTerm_QueueInputEvent calls KTerm_TranslateKey internally if sequence is empty.
    // So sequence should be populated.
    // 'l' -> KTERM_KEY_L -> "l" (if legacy translation works) or just key_code.

    // Let's re-verify my implementation of InjectCommand.
    // It sets key_code. It leaves sequence empty (mostly).
    // KTerm_QueueInputEvent calls KTerm_TranslateKey if sequence is empty.
    // KTerm_TranslateKey maps KTERM_KEY_L to 'l' if no modifiers.
    // So event.sequence should contain "l".

    printf("Input Buffer Content (Raw Chars): '%s'\n", buffer);
    // If we rely on sequence being populated, we should check it.

    // Check if buffer contains "ls"
    // Since my logic above populated buffer based on KEY_CODE or SEQUENCE...
    // If KTerm_TranslateKey worked, sequence[0] is 'l'/'s'.
    if (strcmp(buffer, "ls") != 0) {
        // Fallback check
        if (buffer[0] == 'l' && buffer[1] == 's') {
             // OK
        } else {
             fprintf(stderr, "Command Injection Failed: Expected 'ls', got '%s'\n", buffer);
             return 1;
        }
    }

    // 5. Test VAD Logic
    printf("Testing VAD Logic...\n");
    KTermVoiceContext* ctx = KTerm_Voice_GetContext(session);

    // Feed high energy
    float loud_samples[256];
    for(int i=0; i<256; i++) loud_samples[i] = 1.0f; // Max amplitude

    // Simulate Ring Buffer Fill (via callback)
    if (mock_audio_cb) mock_audio_cb(mock_audio_user_data, loud_samples, 256);

    // Process Capture
    KTerm_Net_Process(term);

    // Check VAD State
    printf("Energy: %f, Active: %d\n", ctx->energy_level, ctx->vad_active);
    if (!ctx->vad_active || ctx->energy_level < 0.9f) {
        fprintf(stderr, "VAD Activation Failed\n");
        return 1;
    }

    // Feed Silence
    if (mock_audio_cb) mock_audio_cb(mock_audio_user_data, silence, 256);

    KTerm_Net_Process(term);
    printf("Energy: %f, Active: %d\n", ctx->energy_level, ctx->vad_active);
    if (ctx->vad_active) {
        fprintf(stderr, "VAD Deactivation Failed\n");
        return 1;
    }

    printf("Voice Commands Verification Passed!\n");
    KTerm_Destroy(term);
    return 0;
}
