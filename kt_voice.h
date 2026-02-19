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
typedef struct KTerm_T KTerm;
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
void KTerm_Voice_ProcessCapture(KTerm* term, KTermSession* session, KTermVoiceSendCallback send_cb, void* user_data);
void KTerm_Voice_ProcessPlayback(KTermSession* session, const void* data, size_t len);

// Internal Helper (exposed for Net integration)
void KTerm_Voice_InjectCommand(KTerm* term, const char* cmd);

#ifdef __cplusplus
}
#endif

#endif // KT_VOICE_H

#ifdef KTERM_VOICE_IMPLEMENTATION
#ifndef KTERM_VOICE_IMPLEMENTATION_GUARD
#define KTERM_VOICE_IMPLEMENTATION_GUARD

#include <stdlib.h>
#include <string.h>
#include <math.h>

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

    uint16_t sequence;

    KTermSession* session;
    KTerm* term;

    // VAD State
    float energy_level;
    bool vad_active;
    float vad_threshold;
    uint64_t vad_start_time;
};

// Timestamp Helper
#ifdef _WIN32
#include <windows.h>
static uint64_t KTerm_Voice_GetMicroseconds() {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    uint64_t sec = counter.QuadPart / freq.QuadPart;
    uint64_t usec = (counter.QuadPart % freq.QuadPart) * 1000000 / freq.QuadPart;
    return sec * 1000000 + usec;
}
#else
#include <sys/time.h>
static uint64_t KTerm_Voice_GetMicroseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif

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
            ctx->vad_active = false;
            ctx->energy_level = 0.0f;
            ctx->vad_threshold = 0.05f; // Default threshold
            ctx->vad_start_time = 0;

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

// Inject command helper
void KTerm_Voice_InjectCommand(KTerm* term, const char* cmd) {
    if (!term || !cmd) return;
    // Iterate string and inject key events
    for (int i = 0; cmd[i]; i++) {
        KTermKeyEvent event = {0};
        event.key_code = (unsigned char)cmd[i];

        // Simple mapping for basic chars
        char c = cmd[i];
        if (c >= 'A' && c <= 'Z') {
            event.key_code = KTERM_KEY_A + (c - 'A');
            event.shift = true;
        } else if (c >= 'a' && c <= 'z') {
            event.key_code = KTERM_KEY_A + (c - 'a');
        } else if (c >= '0' && c <= '9') {
            event.key_code = KTERM_KEY_0 + (c - '0');
        } else if (c == ' ') {
            event.key_code = KTERM_KEY_SPACE;
        } else if (c == '\n' || c == '\r') {
            event.key_code = KTERM_KEY_ENTER;
        } else {
            // Fallback: Just pass the char code as key_code and hope KTerm handles it or use sequence
            // KTerm_QueueInputEvent handles sequence[0] check.
            event.key_code = (unsigned char)c;
            event.sequence[0] = c;
            event.sequence[1] = '\0';
        }

        KTerm_QueueInputEvent(term, event);
    }

    // Append Enter if not present?
    // Usually voice commands are "Execute this", so appending Enter is helpful.
    // But let's assume the command string includes it or the user wants it raw.
    // For now, raw injection.
}

int SituationVoiceCommand(const char* command_text) {
    if (!command_text) return SITUATION_FAILURE;

    int injected_count = 0;
    for(int i=0; i<MAX_SESSIONS; i++) {
        if (g_voice_contexts[i].enabled && g_voice_contexts[i].term) {
            KTerm_Voice_InjectCommand(g_voice_contexts[i].term, command_text);
            injected_count++;
        }
    }
    return (injected_count > 0) ? SITUATION_SUCCESS : SITUATION_FAILURE;
}

void SituationVoiceSetGlobalMute(bool mute) {
    atomic_store_explicit(&g_voice_global_mute, mute, memory_order_relaxed);
}

// Process Capture (Main Thread)
void KTerm_Voice_ProcessCapture(KTerm* term, KTermSession* session, KTermVoiceSendCallback send_cb, void* user_data) {
    KTermVoiceContext* ctx = KTerm_Voice_GetContext(session);
    if (!ctx || !ctx->enabled) return;

    ctx->term = term;

    uint32_t head = atomic_load_explicit(&ctx->capture_head, memory_order_acquire);
    uint32_t tail = atomic_load_explicit(&ctx->capture_tail, memory_order_relaxed);

    uint32_t available;
    if (head >= tail) available = head - tail;
    else available = VOICE_BUFFER_SIZE - (tail - head);

    const int CHUNK_SIZE = 256;
    // Header size: Format(1) + Channels(1) + Rate(1) + Seq(2) + TS(8) + Pad(3) = 16 bytes
    const int HEADER_SIZE = 16;
    const int PACKET_SIZE = HEADER_SIZE + (CHUNK_SIZE * sizeof(float));

    while (available >= CHUNK_SIZE) {
        uint8_t packet[PACKET_SIZE];

        // 1. Fill Header
        packet[0] = 0; // Format: PCM Float
        packet[1] = (uint8_t)ctx->channels;
        packet[2] = (ctx->sample_rate == 48000) ? 1 : 0;

        uint16_t seq = ctx->sequence++;
        packet[3] = (seq >> 8) & 0xFF;
        packet[4] = seq & 0xFF;

        uint64_t ts = KTerm_Voice_GetMicroseconds();
        packet[5] = (ts >> 56) & 0xFF;
        packet[6] = (ts >> 48) & 0xFF;
        packet[7] = (ts >> 40) & 0xFF;
        packet[8] = (ts >> 32) & 0xFF;
        packet[9] = (ts >> 24) & 0xFF;
        packet[10] = (ts >> 16) & 0xFF;
        packet[11] = (ts >> 8) & 0xFF;
        packet[12] = ts & 0xFF;

        // Padding
        packet[13] = 0; packet[14] = 0; packet[15] = 0;

        // 2. Fill Audio Data
        float* audio_payload = (float*)(packet + HEADER_SIZE);
        uint32_t chunk1 = VOICE_BUFFER_SIZE - tail;
        if ((uint32_t)CHUNK_SIZE <= chunk1) {
            memcpy(audio_payload, &ctx->capture_buffer[tail], CHUNK_SIZE * sizeof(float));
        } else {
            memcpy(audio_payload, &ctx->capture_buffer[tail], chunk1 * sizeof(float));
            memcpy(audio_payload + chunk1, &ctx->capture_buffer[0], (CHUNK_SIZE - chunk1) * sizeof(float));
        }

        // VAD Analysis
        float sum_sq = 0.0f;
        for(int i=0; i<CHUNK_SIZE; i++) {
            float s = audio_payload[i];
            sum_sq += s * s;
        }
        float rms = sqrtf(sum_sq / CHUNK_SIZE);
        ctx->energy_level = rms;

        if (rms > ctx->vad_threshold) {
            if (!ctx->vad_active) {
                ctx->vad_active = true;
                ctx->vad_start_time = ts;
            }
        } else {
            if (ctx->vad_active) {
                // Hangover or just stop
                ctx->vad_active = false;
                // Here we could check duration (ts - start_time) and trigger a command
                // For now, we leave the infrastructure ready for the Keyword Spotter integration
            }
        }

        if (send_cb) send_cb(user_data, packet, PACKET_SIZE);

        tail = (tail + CHUNK_SIZE) % VOICE_BUFFER_SIZE;
        atomic_store_explicit(&ctx->capture_tail, tail, memory_order_release);
        available -= CHUNK_SIZE;
    }
}

// Process Playback (Main Thread)
void KTerm_Voice_ProcessPlayback(KTermSession* session, const void* data, size_t len) {
    KTermVoiceContext* ctx = KTerm_Voice_GetContext(session);
    if (!ctx || !ctx->enabled) return;

    if (len < 16) return; // Header size check

    const uint8_t* packet = (const uint8_t*)data;
    uint8_t format = packet[0];
    // uint8_t channels = packet[1]; // TODO: Validate/Resample if needed
    // uint8_t rate = packet[2];     // TODO: Validate/Resample if needed
    // uint16_t seq = (packet[3] << 8) | packet[4];
    // uint64_t ts = ...;

    if (format != 0) return; // Only PCM Float supported for now

    const float* buffer = (const float*)(packet + 16);
    int samples = (len - 16) / sizeof(float);

    if (samples <= 0) return;

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
