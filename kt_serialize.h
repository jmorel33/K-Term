#ifndef KT_SERIALIZE_H
#define KT_SERIALIZE_H

#include "kterm.h"

#ifdef __cplusplus
extern "C" {
#endif

// Serialize the session state (grid, cursor, scrollback) to a newly allocated buffer.
// Returns true on success, false on failure.
// The caller is responsible for freeing *out_buf using KTerm_Free.
bool KTerm_SerializeSession(KTermSession* session, void** out_buf, size_t* out_len);

// Restore session state from a buffer.
// Returns true on success, false on failure.
bool KTerm_DeserializeSession(KTermSession* session, const void* buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // KT_SERIALIZE_H

#ifdef KTERM_SERIALIZE_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KTERM_SERIALIZE_MAGIC "KTERM_SES_V1"
#define KTERM_SERIALIZE_MAGIC_LEN 12

typedef struct {
    char magic[KTERM_SERIALIZE_MAGIC_LEN];
    int cols;
    int rows;
    int buffer_height;
    int screen_head;
    int view_offset;
    int cursor_x;
    int cursor_y;
    int scroll_top;
    int scroll_bottom;
    // Add more metadata as needed
} KTermSessionHeader;

bool KTerm_SerializeSession(KTermSession* session, void** out_buf, size_t* out_len) {
    if (!session || !out_buf || !out_len) return false;

    // Calculate size
    size_t header_size = sizeof(KTermSessionHeader);
    size_t screen_size = session->buffer_height * session->cols * sizeof(EnhancedTermChar);
    size_t alt_size = session->rows * session->cols * sizeof(EnhancedTermChar);
    // Tab stops could be added here

    size_t total_size = header_size + screen_size + alt_size;

    *out_buf = KTerm_Malloc(total_size);
    if (!*out_buf) return false;

    *out_len = total_size;
    unsigned char* ptr = (unsigned char*)*out_buf;

    // Header
    KTermSessionHeader header;
    memset(&header, 0, sizeof(header));
    strncpy(header.magic, KTERM_SERIALIZE_MAGIC, KTERM_SERIALIZE_MAGIC_LEN);
    header.cols = session->cols;
    header.rows = session->rows;
    header.buffer_height = session->buffer_height;
    header.screen_head = session->screen_head;
    header.view_offset = session->view_offset;
    header.cursor_x = session->cursor.x;
    header.cursor_y = session->cursor.y;
    header.scroll_top = session->scroll_top;
    header.scroll_bottom = session->scroll_bottom;

    memcpy(ptr, &header, header_size);
    ptr += header_size;

    // Screen Buffer
    memcpy(ptr, session->screen_buffer, screen_size);
    ptr += screen_size;

    // Alt Buffer
    memcpy(ptr, session->alt_buffer, alt_size);
    ptr += alt_size;

    return true;
}

bool KTerm_DeserializeSession(KTermSession* session, const void* buf, size_t len) {
    if (!session || !buf) return false;

    const unsigned char* ptr = (const unsigned char*)buf;
    size_t offset = 0;

    // Header Check
    if (len < sizeof(KTermSessionHeader)) return false;
    KTermSessionHeader header;
    memcpy(&header, ptr, sizeof(KTermSessionHeader));

    if (strncmp(header.magic, KTERM_SERIALIZE_MAGIC, KTERM_SERIALIZE_MAGIC_LEN) != 0) {
        return false; // Invalid magic
    }

    offset += sizeof(KTermSessionHeader);

    // Validate dimensions match session? Or resize session?
    // For now, assume session is already initialized or resized to match.
    // Ideally, we should resize the session to match the serialized state.
    // Let's call QueueResize? No, we are restoring state directly.
    // If dimensions differ, we can't easily map the buffer directly.
    // The simplest approach for Phase 1 is to require dimensions match or fail,
    // OR resize the session if possible.
    // Since KTermSession allocation depends on cols/rows, we should probably check.

    if (session->cols != header.cols || session->rows != header.rows) {
        // We could call KTerm_QueueResize but that's async.
        // We probably want synchronous restoration.
        // Let's just fail for now if dimensions mismatch, or implement resize logic.
        // Given we are deserializing "into" a session, the user should probably ensure it's sized correctly
        // or we force it.
        // Let's try to update metadata.
        // But buffer sizes are critical.
        // If we just overwrite pointers, we leak old buffers if we don't free them.
        // Let's assume for this "hook" implementation that we just overwrite content
        // IF sizes match. If not, fail.
        if (session->cols != header.cols || session->rows != header.rows) {
             // For robustness, let's just return false and let caller handle resize.
             // Or, we can try to realloc.
             return false;
        }
    }

    size_t screen_size = header.buffer_height * header.cols * sizeof(EnhancedTermChar);
    size_t alt_size = header.rows * header.cols * sizeof(EnhancedTermChar);

    if (offset + screen_size + alt_size > len) return false; // Buffer too small

    // Restore Metadata
    session->buffer_height = header.buffer_height; // Should match if cols/rows match?
    // Wait, buffer_height includes scrollback. If serialized session had different scrollback depth?
    // We should be careful.
    // If buffer_height differs, we can't direct copy if we treat it as ring buffer.
    // Let's check buffer_height.
    if (session->buffer_height != header.buffer_height) {
        return false;
    }

    session->screen_head = header.screen_head;
    session->view_offset = header.view_offset;
    session->cursor.x = header.cursor_x;
    session->cursor.y = header.cursor_y;
    session->scroll_top = header.scroll_top;
    session->scroll_bottom = header.scroll_bottom;

    // Restore Screen Buffer
    memcpy(session->screen_buffer, ptr + offset, screen_size);
    offset += screen_size;

    // Restore Alt Buffer
    memcpy(session->alt_buffer, ptr + offset, alt_size);
    offset += alt_size;

    // Mark as dirty
    if (session->row_dirty) {
        for(int i=0; i<session->rows; i++) session->row_dirty[i] = KTERM_DIRTY_FRAMES;
    }
    session->dirty_rect = (KTermRect){0, 0, session->cols, session->rows};

    return true;
}

#endif // KTERM_SERIALIZE_IMPLEMENTATION
