#ifndef KT_VOICE_H
#define KT_VOICE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#ifdef KTERM_TESTING
#include "tests/mock_situation.h"
#else
#include "situation.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct KTermSession_T KTermSession;
typedef struct KTermVoiceContext KTermVoiceContext;

// Callback for sending packets
typedef void (*KTermVoiceSendCallback)(void* user_data, const void* data, size_t len);

// --- API ---

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

// Integration functions (called by KTerm/Net)
void KTerm_Voice_ProcessCapture(KTermSession* session, KTermVoiceSendCallback send_cb, void* user_data);
void KTerm_Voice_ProcessPlayback(KTermSession* session, const void* data, size_t len);

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
    // Capture Ring Buffer (Mic -> Network)
    float capture_buffer[VOICE_BUFFER_SIZE];
    atomic_uint_fast32_t capture_head; // Write index (Audio Thread)
    atomic_uint_fast32_t capture_tail; // Read index (Main Thread)

    // Playback Ring Buffer (Network -> Speaker)
    float playback_buffer[VOICE_BUFFER_SIZE];
    atomic_uint_fast32_t playback_head; // Write index (Main Thread)
    atomic_uint_fast32_t playback_tail; // Read index (Audio Thread)

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

// Audio Capture Callback (Audio Thread)
static void KTerm_Voice_CaptureCallback(void* user_data, float* buffer, int frames) {
    KTermVoiceContext* ctx = (KTermVoiceContext*)user_data;
    if (!ctx || !ctx->enabled || ctx->muted || atomic_load_explicit(&g_voice_global_mute, memory_order_relaxed)) return;

    int samples = frames * ctx->channels;
    uint32_t head = atomic_load_explicit(&ctx->capture_head, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&ctx->capture_tail, memory_order_acquire);

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
        memcpy(&ctx->capture_buffer[head], buffer, samples * sizeof(float));
    } else {
        memcpy(&ctx->capture_buffer[head], buffer, chunk1 * sizeof(float));
        memcpy(&ctx->capture_buffer[0], buffer + chunk1, (samples - chunk1) * sizeof(float));
    }

    atomic_store_explicit(&ctx->capture_head, (head + samples) % VOICE_BUFFER_SIZE, memory_order_release);
}

// Audio Playback Callback (Audio Thread)
static void KTerm_Voice_PlaybackCallback(void* user_data, float* buffer, int frames) {
    KTermVoiceContext* ctx = (KTermVoiceContext*)user_data;
    int samples = frames * ctx->channels;

    if (!ctx || !ctx->enabled) {
        memset(buffer, 0, samples * sizeof(float));
        return;
    }

    uint32_t tail = atomic_load_explicit(&ctx->playback_tail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&ctx->playback_head, memory_order_acquire);

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
        memcpy(buffer, &ctx->playback_buffer[tail], samples * sizeof(float));
    } else {
        memcpy(buffer, &ctx->playback_buffer[tail], chunk1 * sizeof(float));
        memcpy(buffer + chunk1, &ctx->playback_buffer[0], (samples - chunk1) * sizeof(float));
    }

    atomic_store_explicit(&ctx->playback_tail, (tail + samples) % VOICE_BUFFER_SIZE, memory_order_release);
}

int SituationVoiceEnable(KTermSession* session, bool enable) {
    KTermVoiceContext* ctx = KTerm_Voice_GetContext(session);
    if (!ctx) return SITUATION_FAILURE;

    if (enable) {
        if (!ctx->enabled) {
            ctx->sample_rate = 48000;
            ctx->channels = 1;
            atomic_store(&ctx->capture_head, 0);
            atomic_store(&ctx->capture_tail, 0);
            atomic_store(&ctx->playback_head, 0);
            atomic_store(&ctx->playback_tail, 0);
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

// Process Capture (Main Thread)
void KTerm_Voice_ProcessCapture(KTermSession* session, KTermVoiceSendCallback send_cb, void* user_data) {
    KTermVoiceContext* ctx = KTerm_Voice_GetContext(session);
    if (!ctx || !ctx->enabled) return;

    uint32_t head = atomic_load_explicit(&ctx->capture_head, memory_order_acquire);
    uint32_t tail = atomic_load_explicit(&ctx->capture_tail, memory_order_relaxed);

    uint32_t available;
    if (head >= tail) available = head - tail;
    else available = VOICE_BUFFER_SIZE - (tail - head);

    const int CHUNK_SIZE = 256;

    while (available >= CHUNK_SIZE) {
        float chunk[CHUNK_SIZE];
        uint32_t chunk1 = VOICE_BUFFER_SIZE - tail;
        if ((uint32_t)CHUNK_SIZE <= chunk1) {
            memcpy(chunk, &ctx->capture_buffer[tail], CHUNK_SIZE * sizeof(float));
        } else {
            memcpy(chunk, &ctx->capture_buffer[tail], chunk1 * sizeof(float));
            memcpy(chunk + chunk1, &ctx->capture_buffer[0], (CHUNK_SIZE - chunk1) * sizeof(float));
        }

        if (send_cb) send_cb(user_data, chunk, CHUNK_SIZE * sizeof(float));

        tail = (tail + CHUNK_SIZE) % VOICE_BUFFER_SIZE;
        atomic_store_explicit(&ctx->capture_tail, tail, memory_order_release);
        available -= CHUNK_SIZE;
    }
}

// Process Playback (Main Thread)
void KTerm_Voice_ProcessPlayback(KTermSession* session, const void* data, size_t len) {
    KTermVoiceContext* ctx = KTerm_Voice_GetContext(session);
    if (!ctx || !ctx->enabled) return;

    int samples = len / sizeof(float);
    if (samples <= 0) return;

    const float* buffer = (const float*)data;

    uint32_t head = atomic_load_explicit(&ctx->playback_head, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&ctx->playback_tail, memory_order_acquire);

    uint32_t free_space;
    if (head >= tail) free_space = VOICE_BUFFER_SIZE - (head - tail) - 1;
    else free_space = (tail - head) - 1;

    if ((uint32_t)samples > free_space) {
        // Drop/Overrun
        return;
    }

    uint32_t chunk1 = VOICE_BUFFER_SIZE - head;
    if ((uint32_t)samples <= chunk1) {
        memcpy(&ctx->playback_buffer[head], buffer, samples * sizeof(float));
    } else {
        memcpy(&ctx->playback_buffer[head], buffer, chunk1 * sizeof(float));
        memcpy(&ctx->playback_buffer[0], buffer + chunk1, (samples - chunk1) * sizeof(float));
    }

    atomic_store_explicit(&ctx->playback_head, (head + samples) % VOICE_BUFFER_SIZE, memory_order_release);
}

#endif // KTERM_VOICE_IMPLEMENTATION_GUARD
#endif // KTERM_VOICE_IMPLEMENTATION
