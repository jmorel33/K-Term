#ifndef KT_VOICE_H
#define KT_VOICE_H

#include "kterm.h"
#include <stdatomic.h>

#ifdef KTERM_TESTING
#include "tests/mock_situation.h"
#else
#include "situation.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// --- API ---

// Context for Voice Reactor
typedef struct KTermVoiceContext KTermVoiceContext;

// Enable/Disable voice capture/playback on a specific session
int SituationVoiceEnable(KTermSession* session, bool enable);

// Set the target for voice data (another session index, or remote IP for P2P)
int SituationVoiceSetTarget(KTermSession* session, const char* remote_id_or_ip);

// Execute a voice command programmatically (simulating speech input)
int SituationVoiceCommand(const char* command_text);

// Global controls (Push-to-Talk / Mute)
void SituationVoiceSetGlobalMute(bool mute);

// Helper for testing/debugging
KTermVoiceContext* KTerm_Voice_GetContext(KTermSession* session);

#ifdef __cplusplus
}
#endif

#endif // KT_VOICE_H

#ifdef KTERM_VOICE_IMPLEMENTATION
#ifndef KTERM_VOICE_IMPLEMENTATION_GUARD
#define KTERM_VOICE_IMPLEMENTATION_GUARD

#include <stdlib.h>
#include <string.h>

#define VOICE_BUFFER_SIZE 65536 // Power of 2

struct KTermVoiceContext {
    // Ring Buffer (Interleaved Float)
    float buffer[VOICE_BUFFER_SIZE];
    atomic_uint_fast32_t head; // Write index
    atomic_uint_fast32_t tail; // Read index

    bool enabled;
    bool muted;

    // Capture settings
    int sample_rate;
    int channels;

    KTermSession* session;
};

// Global Mute
static atomic_bool g_voice_global_mute = false;

// Static context storage
static KTermVoiceContext g_voice_contexts[MAX_SESSIONS];

KTermVoiceContext* KTerm_Voice_GetContext(KTermSession* session) {
    // Find existing
    for(int i=0; i<MAX_SESSIONS; i++) {
        if (g_voice_contexts[i].session == session) return &g_voice_contexts[i];
    }
    // Bind new
    for(int i=0; i<MAX_SESSIONS; i++) {
        if (g_voice_contexts[i].session == NULL) {
            g_voice_contexts[i].session = session;
            return &g_voice_contexts[i];
        }
    }
    return NULL;
}

// Audio Capture Callback
static void KTerm_Voice_CaptureCallback(void* user_data, float* buffer, int frames) {
    KTermVoiceContext* ctx = (KTermVoiceContext*)user_data;
    if (!ctx || !ctx->enabled || ctx->muted || atomic_load_explicit(&g_voice_global_mute, memory_order_relaxed)) return;

    int samples = frames * ctx->channels;
    uint32_t head = atomic_load_explicit(&ctx->head, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&ctx->tail, memory_order_acquire);

    // Check available space
    uint32_t free_space;
    if (head >= tail) free_space = VOICE_BUFFER_SIZE - (head - tail) - 1;
    else free_space = (tail - head) - 1;

    if ((uint32_t)samples > free_space) {
        // Drop/Overrun
        return;
    }

    uint32_t chunk1 = VOICE_BUFFER_SIZE - head;
    if ((uint32_t)samples <= chunk1) {
        memcpy(&ctx->buffer[head], buffer, samples * sizeof(float));
    } else {
        memcpy(&ctx->buffer[head], buffer, chunk1 * sizeof(float));
        memcpy(&ctx->buffer[0], buffer + chunk1, (samples - chunk1) * sizeof(float));
    }

    atomic_store_explicit(&ctx->head, (head + samples) % VOICE_BUFFER_SIZE, memory_order_release);
}

// Audio Playback Callback (Loopback)
static void KTerm_Voice_PlaybackCallback(void* user_data, float* buffer, int frames) {
    KTermVoiceContext* ctx = (KTermVoiceContext*)user_data;
    int samples = frames * ctx->channels;

    if (!ctx || !ctx->enabled) {
        memset(buffer, 0, samples * sizeof(float));
        return;
    }

    uint32_t tail = atomic_load_explicit(&ctx->tail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&ctx->head, memory_order_acquire);

    uint32_t available;
    if (head >= tail) available = head - tail;
    else available = VOICE_BUFFER_SIZE - (tail - head);

    if (available < (uint32_t)samples) {
        // Underrun
        memset(buffer, 0, samples * sizeof(float));
        return;
    }

    uint32_t chunk1 = VOICE_BUFFER_SIZE - tail;
    if ((uint32_t)samples <= chunk1) {
        memcpy(buffer, &ctx->buffer[tail], samples * sizeof(float));
    } else {
        memcpy(buffer, &ctx->buffer[tail], chunk1 * sizeof(float));
        memcpy(buffer + chunk1, &ctx->buffer[0], (samples - chunk1) * sizeof(float));
    }

    atomic_store_explicit(&ctx->tail, (tail + samples) % VOICE_BUFFER_SIZE, memory_order_release);
}

int SituationVoiceEnable(KTermSession* session, bool enable) {
    KTermVoiceContext* ctx = KTerm_Voice_GetContext(session);
    if (!ctx) return SITUATION_FAILURE;

    if (enable) {
        if (!ctx->enabled) {
            ctx->sample_rate = 48000;
            ctx->channels = 1;
            atomic_store(&ctx->head, 0);
            atomic_store(&ctx->tail, 0);
            ctx->enabled = true;
            ctx->muted = false;

            SituationStartAudioCaptureEx(KTerm_Voice_CaptureCallback, ctx, ctx->sample_rate, ctx->channels);
            SituationStartAudioPlayback(KTerm_Voice_PlaybackCallback, ctx, ctx->sample_rate, ctx->channels);
        }
    } else {
        if (ctx->enabled) {
            SituationStopAudioCapture();
            SituationStopAudioPlayback();
            ctx->enabled = false;
        }
    }
    return SITUATION_SUCCESS;
}

int SituationVoiceSetTarget(KTermSession* session, const char* remote_id_or_ip) {
    (void)session; (void)remote_id_or_ip;
    return SITUATION_SUCCESS;
}

int SituationVoiceCommand(const char* command_text) {
    (void)command_text;
    return SITUATION_SUCCESS;
}

void SituationVoiceSetGlobalMute(bool mute) {
    atomic_store_explicit(&g_voice_global_mute, mute, memory_order_relaxed);
}

#endif // KTERM_VOICE_IMPLEMENTATION_GUARD
#endif // KTERM_VOICE_IMPLEMENTATION
