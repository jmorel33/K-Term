# K-Term VoIP Implementation Roadmap (PJSIP)

This document outlines the phased implementation plan for integrating full SIP/VoIP capabilities into K-Term using the PJSIP library (PJSUA2/C API). This integration leverages the existing "Voice Reactor" architecture (`kt_voice.h`) to provide a seamless, GPU-accelerated voice experience.

## Architecture Overview

*   **Signaling:** PJSIP (PJSUA API) handles SIP registration, call state, and NAT traversal.
*   **Media Transport:** PJSIP (PJMEDIA) handles RTP/SRTP, jitter buffering, and codec encoding/decoding.
*   **Audio I/O (The Bridge):** Instead of PJSIP opening the host audio device directly, we implement a **custom media bridge**.
    *   **Capture:** `Situation` (Mic) -> `kt_voice.h` (Ring Buffer) -> PJSIP Bridge -> Network.
    *   **Playback:** Network -> PJSIP Bridge -> `kt_voice.h` (Ring Buffer) -> `Situation` (Speaker).
    *   **Reasoning:** This allows K-Term to visualize audio (VU meters), inject commands, and record streams using its existing "Voice Reactor" infrastructure without device conflicts.

## Phase 1: Foundation & Lifecycle

**Goal:** Link PJSIP, initialize the stack, and manage the thread lifecycle.

*   [ ] **Dependency Management:**
    *   Ensure `libpjsip` headers and libraries are available in the build environment.
    *   Add `KTERM_USE_PJSIP` macro to `kterm_api.h` (commented out by default).
*   **Initialization (`KTerm_VoIP_Init`):**
    *   Create `pj_caching_pool` and `pjsua_config`.
    *   Initialize `pjsua_create()` and `pjsua_init()`.
    *   Configure UDP/TCP/TLS transports (bind to random or fixed ports).
    *   Disable PJSIP's default sound device (`snd_dev`) to prepare for the custom bridge.
*   **Threading:**
    *   PJSIP creates its own worker threads. Ensure thread safety when calling `pjsua_*` functions from K-Term's main loop.
    *   Implement `KTerm_VoIP_Cleanup` to properly destroy the stack and join threads on exit.
*   **Logging:**
    *   Redirect PJSIP logs to K-Term's debug output or a dedicated log buffer.

## Phase 2: Signaling Core

**Goal:** Enable SIP registration and basic call setup/teardown.

*   [ ] **Registration (`KTerm_VoIP_Register`):**
    *   Map `user`, `pass`, `domain` to `pjsua_acc_config`.
    *   Call `pjsua_acc_add()` to register with the SIP server.
    *   Handle `on_reg_state` callback to report status via Gateway (`EXT;voip;status;REGISTERED`).
*   [ ] **Outbound Calls (`KTerm_VoIP_Dial`):**
    *   Validate SIP URI (`sip:user@domain`).
    *   Call `pjsua_call_make_call()`.
    *   Handle `on_call_state` to track ringing/connected/disconnected states.
*   [ ] **Inbound Calls:**
    *   Implement `on_incoming_call` callback.
    *   **Auto-Answer logic:** For now, or expose `EXT;voip;answer`.
    *   Send `EXT;voip;incoming;caller=...` Gateway event to the host.
*   [ ] **Hangup (`KTerm_VoIP_Hangup`):**
    *   Call `pjsua_call_hangup_all()` or specific call ID.

## Phase 3: Media Bridge (Voice Reactor Integration)

**Goal:** Route audio between PJSIP and `kt_voice.h` ring buffers. This is the most critical and complex phase.

*   [ ] **Custom Media Port (`KTermMediaPort`):**
    *   Create a `pjmedia_port` subclass/struct.
    *   **`get_frame` (Mic -> Net):**
        *   Read float samples from `session->voice_ctx.capture_buffer`.
        *   Convert Float -> Int16 (if PJSIP uses 16-bit internal clock).
        *   Return frame to PJSIP.
    *   **`put_frame` (Net -> Speaker):**
        *   Receive Int16/Float frame from PJSIP.
        *   Convert -> Float.
        *   Write to `session->voice_ctx.playback_buffer`.
*   [ ] **Conference Bridge:**
    *   Connect the Call's media stream (slot 0) to the Custom Media Port (slot 1) in the PJSIP conference bridge (`pjsua_conf_connect`).
*   [ ] **Clock Synchronization:**
    *   Ensure `kt_voice.h` sampling rate matches PJSIP's clock (usually 48kHz or 16kHz). Resampling might be needed if they mismatch, though PJSIP handles this well.

## Phase 4: Data & Advanced Features

**Goal:** Enhance the calling experience.

*   [ ] **DTMF Injection:**
    *   Implement `KTerm_VoIP_DTMF` using `pjsua_call_dial_dtmf()`.
*   [ ] **Codecs:**
    *   Prioritize **Opus** for high quality/low latency.
    *   Fallback to PCMU/PCMA (G.711).
    *   Configure via `pjsua_codec_set_priority`.
*   [ ] **NAT Traversal:**
    *   Enable ICE/STUN/TURN in `pjsua_config` if needed for WAN operation.

## Phase 5: Gateway Events & API Refinement

**Goal:** Expose full control to the host application via escape sequences.

*   [ ] **Async Events:**
    *   `EXT;voip;status;STATE=REGISTERING`
    *   `EXT;voip;call;ID=1;STATE=RINGING;REMOTE=...`
    *   `EXT;voip;audio;RX=-20dB;TX=-15dB` (Optional stats).
*   [ ] **Commands:**
    *   `answer`: Accept incoming call.
    *   `hold` / `resume`.
    *   `transfer`.

## Dependencies

*   `pkg-config` for `libpjproject`.
*   `libsituation` (already present).

## Future Work

*   **Video:** PJSIP supports video. K-Term could potentially render video frames into a Kitty Graphics image buffer or a Sixel stream!
