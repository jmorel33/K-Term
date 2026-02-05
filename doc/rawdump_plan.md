# Gateway "RAWDUMP" – Mirror Raw Input Bytes to Target Session (No Interpretation)

**PR Title:** feat(gateway): add RAWDUMP extension – mirror raw input bytes as literal chars to target session (no interpretation)

## Description

New Gateway command to copy incoming raw bytes (from host/PTY or `KTerm_PushInput`) to a specified session's grid as literal single-byte characters (0–255 → one cell each).

*   **Non-intrusive:** The main input path remains completely unaffected—parser runs normally, sequences execute, modes apply, etc.
*   **Bypass Parser:** Only a copy of the raw bytes is fed into the target session via direct cell writes (bypass parser for the dump copy only).
*   **Literal Rendering:** Bytes are rendered exactly as-is using the current font atlas mapping—no caret notation, no hex encoding, no forced symbols. Non-printables show whatever glyph the font provides (boxes, ? , control pictures, Latin-1, etc.).
*   **Visuals:** White-on-black enforced only at start of dump (or after clear/resize): one-time reset + explicit FG white / BG black via queued ops. No per-byte attribute changes—keeps rendering clean and efficient.
*   **Control:** Toggle via `START`/`STOP`/`TOGGLE`; target via `SESSION=n` (or current/active if omitted).
*   **Performance:** Optional rate-limiting to avoid overwhelming the target session during high-throughput bursts.

## Command Examples

```
DCS GATE KTERM;RAWDUMP;START;SESSION=3 ST
DCS GATE KTERM;RAWDUMP;STOP;SESSION=3 ST
DCS GATE KTERM;RAWDUMP;TOGGLE ST   # toggles on current session
```

## Implementation Changes

### 1. Data Structures

Add per-session flags (add to `KTermSession.options` or new struct):

```c
struct {
    bool raw_dump_mirror_active;
    int raw_dump_target_session_id;  // -1 = none
    bool raw_dump_force_wob;         // default true
} raw_dump;
```

### 2. Gateway Handler

Implement `RAWDUMP` handler in `kt_gateway.h` (registered extension):

```c
if (strcmp(command, "RAWDUMP") == 0) {
    int target_id = parse_param_int("SESSION", params, session->id);  // default to current
    bool start = strstr(params, "START") || (strstr(params, "TOGGLE") && !sess->raw_dump.raw_dump_mirror_active);
    bool stop  = strstr(params, "STOP") || (strstr(params, "TOGGLE") && sess->raw_dump.raw_dump_mirror_active);

    KTermSession *target_sess = KTerm_GetSession(term, target_id);
    if (!target_sess) {
        // error ACK
        return;
    }

    target_sess->raw_dump.raw_dump_mirror_active = start && !stop;
    target_sess->raw_dump.raw_dump_target_session_id = target_id;
    target_sess->raw_dump.raw_dump_force_wob = parse_bool("FORCE_WOB", params, true);

    if (start && target_sess->raw_dump.raw_dump_mirror_active) {
        KTerm_QueueClearScreen(term, target_sess);
        KTerm_QueueResetAttr(term, target_sess);
        KTerm_QueueSetFGColor(term, target_sess, 15);  // white palette or RGB 255,255,255
        KTerm_QueueSetBGColor(term, target_sess, 0);   // black
        KTerm_QueuePrint(term, target_sess, "=== RAW INPUT MIRROR ACTIVE (literal bytes) ===\r\n");
    }

    // ACK
    KTerm_QueueResponseFormat(term, "\033]9;RAWDUMP;%s;SESSION=%d;OK\007",
                              target_sess->raw_dump.raw_dump_mirror_active ? "ACTIVE" : "STOPPED", target_id);
}
```

### 3. Input Pipeline Integration

In `KTerm_ProcessEventsInternal` (in `kterm.h`), inject logic after popping chunk from `input_queue`:

```c
// After popping len bytes into chunk[]
if (session->raw_dump.raw_dump_mirror_active) {
    KTermSession *dump_sess = KTerm_GetSession(term, session->raw_dump.raw_dump_target_session_id);
    if (dump_sess) {
        // Optional: enforce WOB if needed (e.g., after resize/clear flag)
        if (/* reset needed */) {
            KTerm_QueueResetAttr(term, dump_sess);
            KTerm_QueueSetFGColor(term, dump_sess, 15);
            KTerm_QueueSetBGColor(term, dump_sess, 0);
        }

        for (size_t i = 0; i < len; i++) {
            uint8_t byte = chunk[i];

            // Direct write: one byte → one cell, no parser, no attr change per byte
            KTerm_SetCellDirect(term, dump_sess,
                                dump_sess->cursor.x, dump_sess->cursor.y,
                                byte,  // raw byte as char code
                                /* no new attrs */);
            KTerm_AdvanceCursor(term, dump_sess);  // handle wrap/scroll
        }

        // Mark dirty for redraw
        KTerm_MarkSessionDirty(term, dump_sess);
    }
}

// Continue normal processing: feed chunk to parser as usual
KTerm_FeedParser(term, session, chunk, len);
```

## Benefits

*   **Isolation:** Main session/host flow is untouched (full parsing, execution).
*   **Fidelity:** Target session gets a faithful copy of the raw bytes as literal chars.
*   **Purity:** No interpretation/transformation on the mirrored stream.
*   **Clarity:** Clean white-on-black start banner/reset.
