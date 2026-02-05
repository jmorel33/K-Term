#ifndef KT_IO_SIT_H
#define KT_IO_SIT_H

#include "kterm.h"

#ifdef KTERM_TESTING
#include "mock_situation.h"
#else
#include "situation.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// INPUT ADAPTER (Situation -> KTerm)
// =============================================================================
// This adapter handles keyboard and mouse input using the Situation library
// and translates it into VT sequences for KTerm.

void KTermSit_ProcessInput(KTerm* term);

#ifdef KTERM_IO_SIT_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Helper macro to access active session
#ifndef GET_SESSION
#define GET_SESSION(term) (&(term)->sessions[(term)->active_session])
#endif

// Use Core Key Event Structure
typedef KTermKeyEvent KTermSitKeyEvent;

// Forward declarations of internal helpers
static void KTermSit_UpdateKeyboard(KTerm* term);
static void KTermSit_UpdateMouse(KTerm* term);
static int KTermSit_EncodeUTF8(int codepoint, char* buffer);

void KTermSit_ProcessInput(KTerm* term) {
    KTermSit_UpdateKeyboard(term);
    KTermSit_UpdateMouse(term);
}

static void KTermSit_ProcessSingleKey(KTerm* term, KTermSession* session, int rk) {
    // 1. UDK Handling
    for (size_t i = 0; i < session->programmable_keys.count; i++) {
        if (session->programmable_keys.keys[i].key_code == rk && session->programmable_keys.keys[i].active) {
            const char* seq = session->programmable_keys.keys[i].sequence;
            KTerm_QueueResponse(term, seq);
            if ((session->dec_modes & KTERM_MODE_LOCALECHO)) KTerm_WriteString(term, seq);
            return; // Handled by UDK
        }
    }

    // 2. Standard Key Handling
    KTermEvent event = {0};
    event.type = KTERM_EVENT_KEY;
    event.key.key_code = rk;
    event.key.ctrl = SituationIsKeyDown(SIT_KEY_LEFT_CONTROL) || SituationIsKeyDown(SIT_KEY_RIGHT_CONTROL);
    event.key.alt = SituationIsKeyDown(SIT_KEY_LEFT_ALT) || SituationIsKeyDown(SIT_KEY_RIGHT_ALT);
    event.key.shift = SituationIsKeyDown(SIT_KEY_LEFT_SHIFT) || SituationIsKeyDown(SIT_KEY_RIGHT_SHIFT);

    // Filter printable characters if not modified
    // If it is printable and no modifiers, we inject it directly as a character event (via KEY event with sequence populated)
    // because we are suppressing OS repeats (CharPressed relies on OS repeats).
    if (rk >= 32 && rk <= 126 && !event.key.ctrl && !event.key.alt) {
        event.key.sequence[0] = (char)rk;
        event.key.sequence[1] = '\0';
        KTerm_ProcessEvent(term, session, &event);
        return;
    }

    // Special Scrollback Handling (Shift + PageUp/Down)
    // TODO: This should probably be moved to KTerm_ProcessEvent or generic handler too?
    // For now, keeping it here as it manipulates session state directly.
    if (event.key.shift && (rk == SIT_KEY_PAGE_UP || rk == SIT_KEY_PAGE_DOWN)) {
        if (rk == SIT_KEY_PAGE_UP) session->view_offset += DEFAULT_TERM_HEIGHT / 2;
        else session->view_offset -= DEFAULT_TERM_HEIGHT / 2;

        if (session->view_offset < 0) session->view_offset = 0;
        int max_offset = session->buffer_height - DEFAULT_TERM_HEIGHT;
        if (session->view_offset > max_offset) session->view_offset = max_offset;
        for (int i=0; i<DEFAULT_TERM_HEIGHT; i++) session->row_dirty[i] = true;
    } else {
        // Delegate to Core Translation
        KTerm_ProcessEvent(term, session, &event);
    }
}

static void KTermSit_UpdateKeyboard(KTerm* term) {
    // Process Key Events (Queue based to preserve order)
    // We swallow OS repeats here and implement our own repeater.
    int rk;
    double now = KTerm_TimerGetTime();
    KTermSession* session = GET_SESSION(term);

    while ((rk = SituationGetKeyPressed()) != 0) {
        // Suppress OS repeats if matching the held key
        if (session->input.use_software_repeat && rk == session->input.last_key_code) {
            // It's likely an OS repeat. Ignore it.
            continue;
        }

        // New key press
        session->input.last_key_code = rk;
        session->input.last_key_time = now;
        session->input.repeat_state = 1; // WaitDelay

        // Process immediately (First Press)
        KTermSit_ProcessSingleKey(term, session, rk);
    }

    // Software Repeater (Poller)
    if (session->input.use_software_repeat && session->input.last_key_code != -1 && session->auto_repeat_rate != 31) {
        if (!SituationIsKeyDown(session->input.last_key_code)) {
            session->input.last_key_code = -1;
            session->input.repeat_state = 0;
        } else {
            // Key is held
            double interval;
            if (session->input.repeat_state == 1) { // Waiting for Initial Delay
                interval = session->auto_repeat_delay / 1000.0;
            } else { // Repeating
                // Rate 0=33ms, 30=500ms
                interval = 0.033 + (session->auto_repeat_rate * (0.500 - 0.033) / 30.0);
            }

            if ((now - session->input.last_key_time) >= interval) {
                // Generate Repeat
                session->input.last_key_time = now;
                session->input.repeat_state = 2; // Now repeating

                // Process Repeat
                KTermSit_ProcessSingleKey(term, session, session->input.last_key_code);
            }
        }
    }

    // Process Unicode Characters
    int ch_unicode;
    while ((ch_unicode = SituationGetCharPressed()) != 0) {
        KTermEvent event = {0};
        event.type = KTERM_EVENT_KEY;
        event.key.key_code = ch_unicode;

        // Populate sequence directly
        char sequence[8] = {0};
        bool ctrl = SituationIsKeyDown(SIT_KEY_LEFT_CONTROL) || SituationIsKeyDown(SIT_KEY_RIGHT_CONTROL);
        bool alt = SituationIsKeyDown(SIT_KEY_LEFT_ALT) || SituationIsKeyDown(SIT_KEY_RIGHT_ALT);

        event.key.ctrl = ctrl;
        event.key.alt = alt;

        if (ctrl && ch_unicode >= 'a' && ch_unicode <= 'z') sequence[0] = (char)(ch_unicode - 'a' + 1);
        else if (ctrl && ch_unicode >= 'A' && ch_unicode <= 'Z') sequence[0] = (char)(ch_unicode - 'A' + 1);
        else if (alt && GET_SESSION(term)->input.meta_sends_escape && !ctrl) {
            sequence[0] = '\x1B';
            KTermSit_EncodeUTF8(ch_unicode, sequence + 1);
        } else {
            KTermSit_EncodeUTF8(ch_unicode, sequence);
        }

        if (sequence[0] != '\0') {
            strncpy(event.key.sequence, sequence, sizeof(event.key.sequence));
            KTerm_ProcessEvent(term, session, &event);
        }
    }
}

static int KTermSit_EncodeUTF8(int codepoint, char* buffer) {
    if (codepoint < 0x80) {
        buffer[0] = (char)codepoint;
        buffer[1] = '\0';
        return 1;
    } else if (codepoint < 0x800) {
        buffer[0] = (char)(0xC0 | (codepoint >> 6));
        buffer[1] = (char)(0x80 | (codepoint & 0x3F));
        buffer[2] = '\0';
        return 2;
    } else if (codepoint < 0x10000) {
        buffer[0] = (char)(0xE0 | (codepoint >> 12));
        buffer[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buffer[2] = (char)(0x80 | (codepoint & 0x3F));
        buffer[3] = '\0';
        return 3;
    } else {
        buffer[0] = (char)(0xF0 | (codepoint >> 18));
        buffer[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        buffer[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buffer[3] = (char)(0x80 | (codepoint & 0x3F));
        buffer[4] = '\0';
        return 4;
    }
}

static void KTermSit_UpdateMouse(KTerm* term) {
    Vector2 mouse_pos = SituationGetMousePosition();

    // [FIX] Mouse Tracking Precision
    float cell_w = (float)term->char_width * DEFAULT_WINDOW_SCALE;
    float cell_h = (float)term->char_height * DEFAULT_WINDOW_SCALE;

    // Avoid division by zero
    if (cell_w < 1.0f) cell_w = 8.0f;
    if (cell_h < 1.0f) cell_h = 10.0f;

    // Use floorf for precise cell mapping
    int global_cell_x = (int)floorf(mouse_pos.x / cell_w);
    int global_cell_y = (int)floorf(mouse_pos.y / cell_h);

    int target_session_idx = term->active_session;
    int local_cell_y = global_cell_y;

    if (term->split_screen_active) {
        if (global_cell_y <= term->split_row) {
            target_session_idx = term->session_top;
            local_cell_y = global_cell_y;
        } else {
            target_session_idx = term->session_bottom;
            local_cell_y = global_cell_y - (term->split_row + 1);
        }
    }

    if (SituationIsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        if (term->active_session != target_session_idx) {
            KTerm_SetActiveSession(term, target_session_idx);
        }
    }

    // Temporarily switch to target session for correct context
    int saved_session_idx = term->active_session;
    term->active_session = target_session_idx;
    KTermSession* session = &term->sessions[target_session_idx];

    // Clamp coordinates to valid range
    if (global_cell_x < 0) global_cell_x = 0;
    if (global_cell_x >= term->width) global_cell_x = term->width - 1;

    if (local_cell_y < 0) local_cell_y = 0;
    if (local_cell_y >= session->rows) local_cell_y = session->rows - 1;

    // Build Mouse Event
    KTermEvent event = {0};
    event.type = KTERM_EVENT_MOUSE;
    event.mouse.x = global_cell_x;
    event.mouse.y = local_cell_y;
    event.mouse.ctrl = SituationIsKeyDown(SIT_KEY_LEFT_CONTROL) || SituationIsKeyDown(SIT_KEY_RIGHT_CONTROL);
    event.mouse.alt = SituationIsKeyDown(SIT_KEY_LEFT_ALT) || SituationIsKeyDown(SIT_KEY_RIGHT_ALT);
    event.mouse.shift = SituationIsKeyDown(SIT_KEY_LEFT_SHIFT) || SituationIsKeyDown(SIT_KEY_RIGHT_SHIFT);
    event.mouse.wheel_delta = SituationGetMouseWheelMove();

    // Check buttons
    bool current_buttons[3] = {
        SituationIsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT),
        SituationIsMouseButtonDown(GLFW_MOUSE_BUTTON_MIDDLE),
        SituationIsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT)
    };

    bool event_sent = false;

    // Wheel events always fire
    if (event.mouse.wheel_delta != 0) {
        // Note: For wheel, button is irrelevant for core event but used in legacy report gen?
        // We set button=0 but wheel_delta is set.
        KTerm_ProcessEvent(term, session, &event);
        event_sent = true;
    }

    // Button state changes
    for (int i=0; i<3; i++) {
        if (current_buttons[i] != session->mouse.buttons[i]) {
            session->mouse.buttons[i] = current_buttons[i];
            event.mouse.button = i;
            event.mouse.is_release = !current_buttons[i];
            event.mouse.is_drag = false; // Click/Release
            // Reset wheel for button event
            event.mouse.wheel_delta = 0;
            KTerm_ProcessEvent(term, session, &event);
            event_sent = true;
        }
    }

    // Drag / Motion
    if (!event_sent) {
        // If button down and moved
        bool any_button_down = current_buttons[0] || current_buttons[1] || current_buttons[2];
        if (any_button_down && (global_cell_x != session->mouse.last_x || local_cell_y != session->mouse.last_y)) {
             event.mouse.is_drag = true;
             event.mouse.button = current_buttons[0] ? 0 : (current_buttons[1] ? 1 : 2); // Priority to Left
             event.mouse.wheel_delta = 0;
             KTerm_ProcessEvent(term, session, &event);
        }
        else if (global_cell_x != session->mouse.last_x || local_cell_y != session->mouse.last_y) {
             // Just motion (no button)
             // Only if tracking ANY
             if (session->mouse.mode == MOUSE_TRACKING_ANY_EVENT) {
                 event.mouse.is_drag = false;
                 // Button?
                 KTerm_ProcessEvent(term, session, &event);
             }
        }
    }

    session->mouse.last_x = global_cell_x;
    session->mouse.last_y = local_cell_y;

    // Selection Logic (Local) - Kept here or moved?
    // Moving it to KTerm_ProcessEvent is cleaner but selection interacts with `Situation` inputs directly here?
    // Actually selection state is in session.
    // I can leave selection logic here or move it.
    // Selection is "Client Side" feature, not VT emulation (usually).
    // KTerm is a library.
    // I'll keep selection logic here for now as it uses Situation mouse buttons directly.
    // But wait, I updated `session->mouse.buttons` above.

    if (SituationIsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        session->selection.active = true;
        session->selection.dragging = true;
        session->selection.start_x = global_cell_x;
        session->selection.start_y = local_cell_y;
        session->selection.end_x = global_cell_x;
        session->selection.end_y = local_cell_y;
    } else if (SituationIsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT) && session->selection.dragging) {
        session->selection.end_x = global_cell_x;
        session->selection.end_y = local_cell_y;
    } else if (SituationIsMouseButtonReleased(GLFW_MOUSE_BUTTON_LEFT) && session->selection.dragging) {
        session->selection.dragging = false;
        KTerm_CopySelectionToClipboard(term);
    }

    // Focus Tracking
    bool current_focus = SituationHasWindowFocus();
    if (current_focus != session->mouse.focused) {
        KTermEvent fe = {0};
        fe.type = KTERM_EVENT_FOCUS;
        fe.focused = current_focus;
        KTerm_ProcessEvent(term, session, &fe);
    }

    // Restore active session
    term->active_session = saved_session_idx;
}

#endif // KTERM_IO_SIT_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // KTERM_IO_SIT_H
