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

    // Skip printable characters - GetCharPressed handles them with correct case
    // UNLESS Ctrl or Alt is held (for shortcuts like Ctrl+C)
    if (rk >= 32 && rk <= 126 && !event.key.ctrl && !event.key.alt) {
        return;
    }

    // Special Scrollback Handling (Shift + PageUp/Down)
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
    int rk;
    double now = KTerm_TimerGetTime();
    KTermSession* session = GET_SESSION(term);

    // Process virtual key presses (for special keys and tracking)
    while ((rk = SituationGetKeyPressed()) != 0) {
        // Process immediately (First Press) - only for special keys
        KTermSit_ProcessSingleKey(term, session, rk);
    }

    // Process Unicode Characters - correct case and keyboard layout
    // This handles ALL printable text input
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

        fprintf(stderr, "[IO] CharPressed: unicode=%d(0x%02X) seq[0]=0x%02X('%c')\n",
                ch_unicode, ch_unicode, (unsigned char)sequence[0], sequence[0] >= 32 ? sequence[0] : '?');

        if (sequence[0] != '\0') {
            strncpy(event.key.sequence, sequence, sizeof(event.key.sequence));
            KTerm_ProcessEvent(term, session, &event);
        } else {
            fprintf(stderr, "[IO] WARNING: sequence[0] is NULL for unicode=%d!\n", ch_unicode);
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
    KTermSession* session = GET_SESSION(term);

    // Calculate Cell Dimensions
    float cell_w = (float)term->char_width * DEFAULT_WINDOW_SCALE;
    float cell_h = (float)term->char_height * DEFAULT_WINDOW_SCALE;
    if (cell_w < 1.0f) cell_w = 1.0f;
    if (cell_h < 1.0f) cell_h = 1.0f;

    // Map to Grid
    int global_cell_x = (int)floorf(mouse_pos.x / cell_w);
    int global_cell_y = (int)floorf(mouse_pos.y / cell_h);

    // Handle Split Screen targeting
    int target_session_idx = term->active_session;
    int local_cell_y = global_cell_y;

    if (term->split_screen_active) {
        if (global_cell_y <= term->split_row) {
            target_session_idx = term->session_top;
        } else {
            target_session_idx = term->session_bottom;
            local_cell_y = global_cell_y - (term->split_row + 1);
        }
    }

    // Auto-Focus on Click
    if (SituationIsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        if (term->active_session != target_session_idx) {
            KTerm_SetActiveSession(term, target_session_idx);
        }
    }

    // Switch context to target session
    KTermSession* target_session = &term->sessions[target_session_idx];

    // Clamp
    if (global_cell_x < 0) global_cell_x = 0;
    if (global_cell_x >= term->width) global_cell_x = term->width - 1;
    if (local_cell_y < 0) local_cell_y = 0;
    if (local_cell_y >= target_session->rows) local_cell_y = target_session->rows - 1;

    // Prepare Event
    KTermEvent event = {0};
    event.type = KTERM_EVENT_MOUSE;
    event.mouse.x = global_cell_x;
    event.mouse.y = local_cell_y;
    event.mouse.ctrl = SituationIsKeyDown(SIT_KEY_LEFT_CONTROL) || SituationIsKeyDown(SIT_KEY_RIGHT_CONTROL);
    event.mouse.alt = SituationIsKeyDown(SIT_KEY_LEFT_ALT) || SituationIsKeyDown(SIT_KEY_RIGHT_ALT);
    event.mouse.shift = SituationIsKeyDown(SIT_KEY_LEFT_SHIFT) || SituationIsKeyDown(SIT_KEY_RIGHT_SHIFT);
    event.mouse.wheel_delta = SituationGetMouseWheelMove();

    // Check Buttons
    bool current_buttons[3] = {
        SituationIsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT),
        SituationIsMouseButtonDown(GLFW_MOUSE_BUTTON_MIDDLE),
        SituationIsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT)
    };

    bool event_sent = false;

    // 1. Wheel
    if (event.mouse.wheel_delta != 0) {
        KTerm_ProcessEvent(term, target_session, &event);
        event_sent = true;
    }

    // 2. Buttons
    for (int i = 0; i < 3; i++) {
        if (current_buttons[i] != target_session->mouse.buttons[i]) {
            target_session->mouse.buttons[i] = current_buttons[i];
            event.mouse.button = i;
            event.mouse.is_release = !current_buttons[i];
            event.mouse.is_drag = false;
            event.mouse.wheel_delta = 0; // Clear wheel for button event
            KTerm_ProcessEvent(term, target_session, &event);
            event_sent = true;
        }
    }

    // 3. Drag / Motion
    if (!event_sent) {
        bool any_down = current_buttons[0] || current_buttons[1] || current_buttons[2];
        if (any_down && (global_cell_x != target_session->mouse.last_x || local_cell_y != target_session->mouse.last_y)) {
            event.mouse.is_drag = true;
            event.mouse.button = current_buttons[0] ? 0 : (current_buttons[1] ? 1 : 2);
            event.mouse.wheel_delta = 0;
            KTerm_ProcessEvent(term, target_session, &event);
        } else if (target_session->mouse.mode == MOUSE_TRACKING_ANY_EVENT &&
                   (global_cell_x != target_session->mouse.last_x || local_cell_y != target_session->mouse.last_y)) {
            event.mouse.is_drag = false;
            KTerm_ProcessEvent(term, target_session, &event);
        }
    }

    target_session->mouse.last_x = global_cell_x;
    target_session->mouse.last_y = local_cell_y;

    // 4. Selection Logic (Client side)
    if (SituationIsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        target_session->selection.active = true;
        target_session->selection.dragging = true;
        target_session->selection.start_x = global_cell_x;
        target_session->selection.start_y = local_cell_y;
        target_session->selection.end_x = global_cell_x;
        target_session->selection.end_y = local_cell_y;
    } else if (SituationIsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT) && target_session->selection.dragging) {
        target_session->selection.end_x = global_cell_x;
        target_session->selection.end_y = local_cell_y;
    } else if (SituationIsMouseButtonReleased(GLFW_MOUSE_BUTTON_LEFT) && target_session->selection.dragging) {
        target_session->selection.dragging = false;
        KTerm_CopySelectionToClipboard(term);
    }
}

#endif // KTERM_IO_SIT_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // KT_IO_SIT_H
