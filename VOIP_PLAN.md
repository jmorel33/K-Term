# K-Term "Voice Reactor" — Design Specification

**Codename:** Voice Reactor
**Goal:** Turn K-Term into a full real-time voice + creative command nervous system.

This is not just "adding voice chat".
This is making the console feel alive — like you’re sitting in a real studio with a patchbay, where you can talk, command, and stream audio between playgrounds as naturally as you type.

We want the same feeling you get when a Fairlight Page R locks in, or when a TX816 stack starts breathing. Voice should feel like part of the instrument, not an add-on.

**Philosophy:** Maximum control, zero hand-holding. We provide the infrastructure; the user decides the workflow.

## 1. Vision: Creative Violence

The console should let you shatter the limits of what a single thread can do. It's about raw power and direct connection.

*   **The 4-Playground Studio**: Imagine 4 independent K-Term sessions running in parallel.
    *   **Session 1 (Music):** Sequencing drums and bass.
    *   **Session 2 (Graphics):** Live-coding a shader visualizer.
    *   **Session 3 (Docs):** Reference manual.
    *   **Session 4 (Comms):** Talking to your collaborator.
    *   **The magic:** Routing the *output* of Session 1 directly into the *input* of Session 2 to drive the visuals, while shouting commands at Session 4 to "deploy to production."

*   **Single-Thread Power**: We don't spawn a hundred threads. We use one, perfectly optimized audio thread and one networking thread, running lock-free ring buffers to move data faster than you can blink. It's about efficiency, not bloat.

## 2. Core Architecture

We extend K-Term’s existing framed packet protocol with audio support. No new transport layer — we ride on what already works.

### 2.1 Packet Extension (`kt_net.h`)

```c
typedef enum {
    // Existing types...
    KTERM_PKT_AUDIO_VOICE   = 0x10,   // Compressed or raw PCM voice data
    KTERM_PKT_AUDIO_COMMAND = 0x11,   // Voice command (recognized text or raw waveform)
    KTERM_PKT_AUDIO_STREAM  = 0x12    // High-quality audio stream between sessions
} KTermAudioPacketType;
```

### 2.2 Byte-Level Protocol (`KTERM_PKT_AUDIO_VOICE`)

To ensure maximum performance and minimal overhead, the packet structure is strictly defined:

| Offset | Field | Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | `Type` | `uint8_t` | `0x10` (KTERM_PKT_AUDIO_VOICE) |
| 1 | `Length` | `uint32_t` | Total payload length (Big Endian) |
| 5 | `Format` | `uint8_t` | `0x00`=PCM Float, `0x01`=Opus |
| 6 | `Channels` | `uint8_t` | `1`=Mono, `2`=Stereo |
| 7 | `SampleRate` | `uint8_t` | `0`=44.1k, `1`=48k |
| 8 | `Sequence` | `uint16_t` | Rolling sequence ID for jitter buffer (Big Endian) |
| 10 | `Timestamp` | `uint64_t` | Capture timestamp (microseconds) |
| 18 | `Data` | `bytes` | Raw audio payload |

### 2.3 Data Flow (Low Latency Focus)

1.  **Capture:**
    *   **Source:** `SituationStartAudioCaptureEx(callback, user_data, sample_rate, channels)`
    *   **Processing:** Interleaved float samples captured in the high-priority audio thread.
2.  **Buffering:**
    *   **Mechanism:** Lock-free ring buffer (miniaudio based) to decouple the audio thread from the networking thread. Zero allocations during runtime.
3.  **Packetization:**
    *   **Compression:** Optional Opus or custom compression applied before transmission.
    *   **Timestamping:** Critical for jitter correction and sync.
4.  **Transmission:**
    *   **Transport:** Existing `KTerm_Net_SendPacket` (async, non-blocking TCP).
5.  **Reception:**
    *   **Handling:** `KTerm_Net_ProcessFrame` decodes and routes payload.
    *   **Routing:** Audio payload is routed to the target session (or global mixer).
6.  **Voice Commands:**
    *   **Input:** Local VAD (Voice Activity Detection) + keyword spotting.
    *   **Execution:** Recognized commands injected as console commands into the active session.

## 3. Public API Specification

The API will be exposed to developers via `kt_api.h` or `kt_voice.h`, maintaining clear ownership within the K-Term context but leveraging Situation's capabilities.

```c
// Enable/Disable voice capture/playback on a specific session
SituationError SituationVoiceEnable(SituationSession* session, bool enable);

// Set the target for voice data (another session index, or remote IP for P2P)
SituationError SituationVoiceSetTarget(SituationSession* local, const char* remote_id_or_ip);

// Execute a voice command programmatically (simulating speech input)
SituationError SituationVoiceCommand(const char* command_text);

// Global controls (Push-to-Talk / Mute)
void SituationVoiceSetGlobalMute(bool mute);
```

## 4. Phased Implementation Plan

### Phase 1: Foundation
- [x] Define `KTERM_PKT_AUDIO_*` packet types in `kt_net.h`.
- [x] Create `kt_voice.h` with the public API definitions.
- [x] Implement ring buffer + capture callback integration (`SituationStartAudioCaptureEx`).
- [x] Local loopback test (Mic -> Ring Buffer -> Playback) to verify audio chain.

### Phase 2: Network Voice
- [x] Implement packetization of audio samples in `KTerm_Net_Process` according to the byte-level spec (Updated with 16-byte aligned header).
- [x] Send packets via `KTerm_Net_SendPacket`.
- [x] Implement receiver logic in `KTerm_Net_ProcessFrame` to extract and play back audio.
- [x] Basic multi-user support (routing audio to correct session/mixer channel).
- [x] Verified via `tests/verify_voice.c` (Capture -> Network -> Loopback -> Playback).

### Phase 3: Voice Commands
-   [x] Implement Voice Activity Detection (VAD).
-   [x] Simple keyword spotting -> console command injection.
-   [ ] (Future) Full local STT option.

### Phase 4: Advanced Streaming & Polish
-   [x] High-quality inter-session audio streaming (`KTERM_PKT_AUDIO_STREAM`).
-   [x] Visual voice activity indicators (VU meter in status bar).
-   [x] Latency optimization (Opus integration, jitter buffer tuning).
