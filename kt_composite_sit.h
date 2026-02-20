#ifndef KT_COMPOSITE_SIT_H
#define KT_COMPOSITE_SIT_H

#include "kt_render_sit.h"
#ifndef KTERM_DISABLE_VOICE
#include "kt_voice.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Forward Declarations
typedef struct KTerm_T KTerm;
typedef struct KTermSession_T KTermSession;
typedef struct KTermPane_T KTermPane;

// =============================================================================
// GPU STRUCTURES
// =============================================================================

typedef struct {
    uint32_t char_code;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t flags;
    uint32_t ul_color;
    uint32_t st_color;
} GPUCell;

typedef struct {
    float x0, y0;
    float x1, y1;
    uint32_t color;
    float intensity;
    uint32_t mode;
    float padding;
} GPUVectorLine;

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t pattern;
    uint32_t color_index;
} GPUSixelStrip;

typedef struct {
    float crt_curvature;
    float scanline_intensity;
    float glow_intensity;
    float noise_intensity;
    float visual_bell_intensity;
    float voice_energy;
    uint32_t flags; // 1=CRT, 2=Scanline, 4=Glow, 8=Noise
    
    // Font dimensions
    uint32_t font_cell_width;   // Full cell size (e.g., 10 for IBM font)
    uint32_t font_cell_height;  // Full cell size (e.g., 10 for IBM font)
    uint32_t font_data_width;   // Actual glyph data size (e.g., 8 for IBM font)
    uint32_t font_data_height;  // Actual glyph data size (e.g., 8 for IBM font)
    uint32_t atlas_cols;        // Number of columns in font atlas (e.g., 256 for IBM font)
    uint32_t padding_reserved;  // Reserved for alignment
} GPUShaderConfig;

typedef struct {
    KTermVector2 screen_size;
    KTermVector2 char_size;
    KTermVector2 grid_size;
    float time;
    union {
        uint32_t cursor_index;
    };
    uint32_t cursor_blink_state;
    uint32_t text_blink_state;
    uint32_t sel_start;
    uint32_t sel_end;
    uint32_t sel_active;

    // Moved to GPUShaderConfig (via shader_config_addr)
    // float scanline_intensity;
    // float crt_curvature;

    uint32_t mouse_cursor_index;
    uint64_t terminal_buffer_addr;
    uint64_t vector_buffer_addr;
    uint64_t font_texture_handle;
    uint64_t sixel_texture_handle;
    uint64_t vector_texture_handle;

    uint64_t shader_config_addr; // New: Address of GPUShaderConfig buffer

    uint32_t atlas_cols;
    uint32_t vector_count;

    // float visual_bell_intensity; // Moved to GPUShaderConfig
    int sixel_y_offset;
    uint32_t grid_color;
    uint32_t conceal_char_code;
} KTermPushConstants;

// GPU Attribute Flags (Must match shaders/terminal.comp)
#define GPU_ATTR_BOLD       (1 << 0)
#define GPU_ATTR_FAINT      (1 << 1)
#define GPU_ATTR_ITALIC     (1 << 2)
#define GPU_ATTR_UNDERLINE  (1 << 3)
#define GPU_ATTR_BLINK      (1 << 4)
#define GPU_ATTR_REVERSE    (1 << 5)
#define GPU_ATTR_STRIKE     (1 << 6)
#define GPU_ATTR_DOUBLE_WIDTH       (1 << 7)
#define GPU_ATTR_DOUBLE_HEIGHT_TOP  (1 << 8)
#define GPU_ATTR_DOUBLE_HEIGHT_BOT  (1 << 9)
#define GPU_ATTR_CONCEAL            (1 << 10)

// Shader Variant Flags
#define SHADER_FLAG_CRT      (1 << 0)
#define SHADER_FLAG_SCANLINE (1 << 1)
#define SHADER_FLAG_GLOW     (1 << 2)
#define SHADER_FLAG_NOISE    (1 << 3)

typedef struct {
    int x, y, width, height;
    int z_index;
    int clip_x, clip_y, clip_mx, clip_my;
    KTermTexture texture;
} KittyRenderOp;

typedef struct {
    GPUCell* cells;
    size_t cell_count;
    size_t cell_capacity;

    KTermPushConstants constants;

    // Sixel Data
    GPUSixelStrip* sixel_strips;
    size_t sixel_count;
    size_t sixel_capacity;
    uint32_t sixel_palette[256];
    bool sixel_active;
    int sixel_width;
    int sixel_height;
    int sixel_y_offset;

    // Vector Data
    GPUVectorLine* vectors;
    size_t vector_count;
    size_t vector_capacity;

    // Kitty Graphics
    KittyRenderOp* kitty_ops;
    size_t kitty_count;
    size_t kitty_capacity;

    // Garbage Collection
    KTermTexture garbage[8];
    int garbage_count;

} KTermRenderBuffer;

typedef struct {
    KTermRenderBuffer render_buffers[2];
    int rb_front;
    int rb_back;
    kterm_mutex_t render_lock;
} KTermCompositor;

// API
bool KTermCompositor_Init(KTermCompositor* comp, int width, int height);
void KTermCompositor_Cleanup(KTermCompositor* comp);
void KTermCompositor_Prepare(KTermCompositor* comp, KTerm* term);
void KTermCompositor_Render(KTermCompositor* comp, KTerm* term);
void KTermCompositor_Resize(KTermCompositor* comp, int width, int height);

#ifdef __cplusplus
}
#endif

#endif // KT_COMPOSITE_SIT_H

#ifdef KTERM_COMPOSITE_IMPLEMENTATION

bool KTermCompositor_Init(KTermCompositor* comp, int width, int height) {
    comp->rb_front = 0;
    comp->rb_back = 1;
    KTERM_MUTEX_INIT(comp->render_lock);

    for (int i = 0; i < 2; i++) {
        // Cells
        size_t cell_count = width * height;
        comp->render_buffers[i].cell_count = cell_count;
        comp->render_buffers[i].cell_capacity = cell_count;
        comp->render_buffers[i].cells = (GPUCell*)KTerm_Calloc(cell_count, sizeof(GPUCell));
        if (!comp->render_buffers[i].cells) return false;

        // Vectors
        comp->render_buffers[i].vector_capacity = 1024;
        comp->render_buffers[i].vector_count = 0;
        comp->render_buffers[i].vectors = (GPUVectorLine*)KTerm_Calloc(comp->render_buffers[i].vector_capacity, sizeof(GPUVectorLine));

        // Sixel
        comp->render_buffers[i].sixel_capacity = 1024;
        comp->render_buffers[i].sixel_count = 0;
        comp->render_buffers[i].sixel_strips = (GPUSixelStrip*)KTerm_Calloc(comp->render_buffers[i].sixel_capacity, sizeof(GPUSixelStrip));

        // Kitty
        comp->render_buffers[i].kitty_capacity = 64;
        comp->render_buffers[i].kitty_count = 0;
        comp->render_buffers[i].kitty_ops = (KittyRenderOp*)KTerm_Calloc(comp->render_buffers[i].kitty_capacity, sizeof(KittyRenderOp));
    }
    return true;
}

void KTermCompositor_Cleanup(KTermCompositor* comp) {
    KTERM_MUTEX_DESTROY(comp->render_lock);
    for (int i = 0; i < 2; i++) {
        if (comp->render_buffers[i].cells) KTerm_Free(comp->render_buffers[i].cells);
        if (comp->render_buffers[i].vectors) KTerm_Free(comp->render_buffers[i].vectors);
        if (comp->render_buffers[i].sixel_strips) KTerm_Free(comp->render_buffers[i].sixel_strips);
        if (comp->render_buffers[i].kitty_ops) KTerm_Free(comp->render_buffers[i].kitty_ops);

        for (int g = 0; g < comp->render_buffers[i].garbage_count; g++) {
            if (comp->render_buffers[i].garbage[g].slot_index != 0) {
                KTerm_DestroyTexture(&comp->render_buffers[i].garbage[g]);
            }
        }
        memset(&comp->render_buffers[i], 0, sizeof(KTermRenderBuffer));
    }
}

void KTermCompositor_Resize(KTermCompositor* comp, int width, int height) {
    KTERM_MUTEX_LOCK(comp->render_lock);
    for (int i = 0; i < 2; i++) {
        size_t new_cell_count = width * height;
        // Only realloc if size changed significantly or grew
        if (new_cell_count != comp->render_buffers[i].cell_capacity) {
             void* new_ptr = KTerm_Realloc(comp->render_buffers[i].cells, new_cell_count * sizeof(GPUCell));
             if (new_ptr) {
                 comp->render_buffers[i].cells = (GPUCell*)new_ptr;
                 comp->render_buffers[i].cell_capacity = new_cell_count;
             }
             // else handle error? legacy logged error but continued.
        }
        // Safely set cell_count
        if (new_cell_count <= comp->render_buffers[i].cell_capacity) {
            comp->render_buffers[i].cell_count = new_cell_count;
        } else {
             comp->render_buffers[i].cell_count = comp->render_buffers[i].cell_capacity;
        }
        // Clear buffer
        if (comp->render_buffers[i].cells) {
            memset(comp->render_buffers[i].cells, 0, new_cell_count * sizeof(GPUCell));
        }
    }

    KTERM_MUTEX_UNLOCK(comp->render_lock);
}

// JIT Run Builder
static KTermTextRun KTerm_BuildRun(EnhancedTermChar* row, int start_idx, int max_idx) {
    KTermTextRun run = {0};
    run.start_index = start_idx;
    if (start_idx >= max_idx) return run;

    EnhancedTermChar* base = &row[start_idx];
    run.codepoints[run.codepoint_count++] = base->ch;
    run.length = 1;

    // Visual Width: Check double width flag
    // Assume 1 if flag not set. Wide chars might have width 2.
    run.visual_width = (base->flags & KTERM_ATTR_DOUBLE_WIDTH) ? 2 : 1;

    // Combining chars
    int next_idx = start_idx + 1;
    while (next_idx < max_idx) {
        EnhancedTermChar* next = &row[next_idx];
        if (next->flags & KTERM_FLAG_COMBINING) {
            if (run.codepoint_count < 8) {
                run.codepoints[run.codepoint_count++] = next->ch;
            }
            run.length++;
            next_idx++;
        } else {
            break;
        }
    }
    return run;
}

// Helper for UpdatePaneRow
static void KTerm_UpdatePaneRow(KTerm* term, KTermSession* source_session, KTermRenderBuffer* rb, int global_x, int global_y, int width, int source_y, int source_x) {
    if (source_y >= source_session->rows || source_y < 0) return;

    EnhancedTermChar* src_row_ptr = GetScreenRow(source_session, source_y);
    int cols = source_session->cols;

    int current_visual_x = 0;
    int current_source_idx = source_x;

    while (current_source_idx > 0 && (src_row_ptr[current_source_idx].flags & KTERM_FLAG_COMBINING)) {
        current_source_idx--;
    }

    int backtrack_dist = source_x - current_source_idx;
    int effective_global_x = global_x - backtrack_dist;
    int effective_width = width + backtrack_dist;

    while (current_visual_x < effective_width && current_source_idx < cols) {
        KTermTextRun run = KTerm_BuildRun(src_row_ptr, current_source_idx, cols);

        uint32_t char_code;
        if (run.codepoint_count == 1 && run.codepoints[0] < 256) {
            char_code = run.codepoints[0];
        } else {
            char_code = KTerm_AllocateGlyph(term, run.codepoints[0]);
        }

        for (int v = 0; v < run.visual_width; v++) {
            int draw_visual_x = effective_global_x + current_visual_x + v;

            if (draw_visual_x >= 0 && draw_visual_x < term->width) {
                int gy = global_y;
                if (gy >= 0 && gy < term->height) {
                    size_t offset = gy * term->width + draw_visual_x;
                    if (offset < rb->cell_capacity) {
                        GPUCell* gpu_cell = &rb->cells[offset];
                        EnhancedTermChar* cell = &src_row_ptr[current_source_idx];

                        gpu_cell->char_code = (v == 0) ? char_code : 0;

                        KTermColor fg = {255, 255, 255, 255};
                        if (cell->fg_color.color_mode == 0) {
                             RGB_KTermColor c = term->color_palette[cell->fg_color.value.index];
                             fg = (KTermColor){c.r, c.g, c.b, 255};
                        } else {
                            fg = (KTermColor){cell->fg_color.value.rgb.r, cell->fg_color.value.rgb.g, cell->fg_color.value.rgb.b, 255};
                        }
                        gpu_cell->fg_color = (uint32_t)fg.r | ((uint32_t)fg.g << 8) | ((uint32_t)fg.b << 16) | ((uint32_t)fg.a << 24);

                        KTermColor bg = {0, 0, 0, 255};
                        if (cell->bg_color.color_mode == 0) {
                             RGB_KTermColor c = term->color_palette[cell->bg_color.value.index];
                             bg = (KTermColor){c.r, c.g, c.b, 255};
                             if (cell->bg_color.value.index == 0) bg.a = 0;
                        } else {
                            bg = (KTermColor){cell->bg_color.value.rgb.r, cell->bg_color.value.rgb.g, cell->bg_color.value.rgb.b, 255};
                        }
                        gpu_cell->bg_color = (uint32_t)bg.r | ((uint32_t)bg.g << 8) | ((uint32_t)bg.b << 16) | ((uint32_t)bg.a << 24);

                        KTermColor ul = fg;
                        if (cell->ul_color.color_mode != 2) {
                             if (cell->ul_color.color_mode == 0) {
                                 RGB_KTermColor c = term->color_palette[cell->ul_color.value.index];
                                 ul = (KTermColor){c.r, c.g, c.b, 255};
                             } else {
                                 ul = (KTermColor){cell->ul_color.value.rgb.r, cell->ul_color.value.rgb.g, cell->ul_color.value.rgb.b, 255};
                             }
                        }
                        gpu_cell->ul_color = (uint32_t)ul.r | ((uint32_t)ul.g << 8) | ((uint32_t)ul.b << 16) | ((uint32_t)ul.a << 24);

                        KTermColor st = fg;
                        if (cell->st_color.color_mode != 2) {
                             if (cell->st_color.color_mode == 0) {
                                 RGB_KTermColor c = term->color_palette[cell->st_color.value.index];
                                 st = (KTermColor){c.r, c.g, c.b, 255};
                             } else {
                                 st = (KTermColor){cell->st_color.value.rgb.r, cell->st_color.value.rgb.g, cell->st_color.value.rgb.b, 255};
                             }
                        }
                        gpu_cell->st_color = (uint32_t)st.r | ((uint32_t)st.g << 8) | ((uint32_t)st.b << 16) | ((uint32_t)st.a << 24);

                        gpu_cell->flags = cell->flags & 0x3FFFFFFF;
                        if ((source_session->dec_modes & KTERM_MODE_DECSCNM)) {
                            gpu_cell->flags ^= KTERM_ATTR_REVERSE;
                        }
                        if (source_session->grid_enabled) {
                            gpu_cell->flags |= KTERM_ATTR_GRID;
                        }
                    }
                }
            }
        }

        current_source_idx += run.length;
        current_visual_x += run.visual_width;
    }

    if (source_session->row_dirty[source_y] > 0) {
        source_session->row_dirty[source_y]--;
    }
}

static void KTerm_UpdateAtlasWithSoftFont(KTerm* term) {
    if (!term->font_atlas_pixels) return;

    int char_w = term->char_width;
    int char_h = term->char_height;
    if (GET_SESSION(term)->soft_font.active) {
        char_w = GET_SESSION(term)->soft_font.char_width;
        char_h = GET_SESSION(term)->soft_font.char_height;
    }
    int dynamic_chars_per_row = term->atlas_width / char_w;
    unsigned char* pixels = term->font_atlas_pixels;

    for (int i = 0; i < 256; i++) {
        int glyph_col = i % dynamic_chars_per_row;
        int glyph_row = i / dynamic_chars_per_row;
        int dest_x_start = glyph_col * char_w;
        int dest_y_start = glyph_row * char_h;

        bool use_soft = (GET_SESSION(term)->soft_font.active && GET_SESSION(term)->soft_font.loaded[i]);
        int glyph_data_w = use_soft ? GET_SESSION(term)->soft_font.char_width : term->font_data_width;
        int glyph_data_h = use_soft ? GET_SESSION(term)->soft_font.char_height : term->font_data_height;
        int pad_x = (char_w - glyph_data_w) / 2;
        int pad_y = (char_h - glyph_data_h) / 2;

        // Clear the cell first (in case switching fonts)
        for (int y = 0; y < char_h; y++) {
            for (int x = 0; x < char_w; x++) {
                 int px_idx = ((dest_y_start + y) * term->atlas_width + (dest_x_start + x)) * 4;
                 *((uint32_t*)&pixels[px_idx]) = 0;
            }
        }

        for (int y = 0; y < char_h; y++) {
            uint16_t row_data = 0;
            bool in_glyph_y = (y >= pad_y && y < (pad_y + glyph_data_h));

            if (in_glyph_y) {
                int src_y = y - pad_y;
                if (use_soft) {
                    row_data = GET_SESSION(term)->soft_font.font_data[i][src_y];
                } else if (term->current_font_data) {
                    if (term->current_font_is_16bit) {
                         row_data = ((const uint16_t*)term->current_font_data)[i * term->font_data_height + src_y];
                    } else {
                         row_data = ((const uint8_t*)term->current_font_data)[i * term->font_data_height + src_y];
                    }
                }
            }

            for (int x = 0; x < char_w; x++) {
                int px_idx = ((dest_y_start + y) * term->atlas_width + (dest_x_start + x)) * 4;
                bool pixel_on = false;
                bool in_glyph_x = (x >= pad_x && x < (pad_x + glyph_data_w));

                if (in_glyph_y && in_glyph_x) {
                    int src_x = x - pad_x;
                    pixel_on = (row_data >> (glyph_data_w - 1 - src_x)) & 1;
                }

                if (pixel_on) {
                    pixels[px_idx + 0] = 255;
                    pixels[px_idx + 1] = 255;
                    pixels[px_idx + 2] = 255;
                    pixels[px_idx + 3] = 255;
                }
            }
        }
    }
}

static bool KTerm_RecursiveUpdateSSBO(KTerm* term, KTermPane* pane, KTermRenderBuffer* rb) {
    if (!pane) return false;
    bool any_update = false;

    if (pane->type == PANE_LEAF) {
        if (pane->session_index >= 0 && pane->session_index < MAX_SESSIONS) {
            KTermSession* session = &term->sessions[pane->session_index];
            if (session->session_open) {
                if (session->synchronized_update) return false;

                for (int y = 0; y < pane->height; y++) {
                    if (y < session->rows && session->row_dirty[y]) {
                        int sx = 0;
                        int sw = pane->width;

                        if (session->dirty_rect.w > 0) {
                            int dr_x = session->dirty_rect.x;
                            int dr_w = session->dirty_rect.w;
                            int dr_end = dr_x + dr_w;

                            int start_x = 0;
                            int end_x = pane->width;

                            if (start_x < dr_x) start_x = dr_x;
                            if (end_x > dr_end) end_x = dr_end;

                            if (start_x < end_x) {
                                sx = start_x;
                                sw = end_x - start_x;
                            }
                        }
                        KTerm_UpdatePaneRow(term, session, rb, pane->x + sx, pane->y + y, sw, y, sx);
                        any_update = true;
                    }
                }
            }
        }
    } else {
        if (KTerm_RecursiveUpdateSSBO(term, pane->child_a, rb)) any_update = true;
        if (KTerm_RecursiveUpdateSSBO(term, pane->child_b, rb)) any_update = true;
    }
    return any_update;
}

void KTermCompositor_Prepare(KTermCompositor* comp, KTerm* term) {
    KTermSession* session = GET_SESSION(term);
    if (term->terminal_buffer.id == 0) return;

    KTermRenderBuffer* rb = &comp->render_buffers[comp->rb_back];

    // Cleanup leftover garbage
    for (int g = 0; g < rb->garbage_count; g++) {
        if (rb->garbage[g].slot_index != 0) KTerm_DestroyTexture(&rb->garbage[g]);
    }
    rb->garbage_count = 0;

    // Handle Soft Font Update
    if (GET_SESSION(term)->soft_font.dirty || term->font_atlas_dirty) {
        if (GET_SESSION(term)->soft_font.dirty) {
            KTerm_UpdateAtlasWithSoftFont(term);
        }

        if (term->font_atlas_pixels) {
            KTermImage img = {0};
            img.width = term->atlas_width;
            img.height = term->atlas_height;
            img.channels = 4;
            img.data = term->font_atlas_pixels;

            KTermTexture new_texture = {0};
            // Font texture will be sampled in compute shader and has initial data
            KTerm_CreateTextureEx(img, false, SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED | SITUATION_TEXTURE_USAGE_TRANSFER_DST, &new_texture);

            if (new_texture.slot_index != 0) {
                if (term->font_texture.slot_index != 0) {
                    if (rb->garbage_count < 8) rb->garbage[rb->garbage_count++] = term->font_texture;
                    else KTerm_DestroyTexture(&term->font_texture);
                }
                term->font_texture = new_texture;
            }
        }
        GET_SESSION(term)->soft_font.dirty = false;
        term->font_atlas_dirty = false;
    }

    // Vector Clear Request
    if (term->vector_clear_request) {
        KTermImage clear_img = {0};
        if (KTerm_CreateImage(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, 4, &clear_img) == KTERM_SUCCESS) {
            memset(clear_img.data, 0, DEFAULT_WINDOW_WIDTH * DEFAULT_WINDOW_HEIGHT * 4);

            if (term->vector_layer_texture.slot_index != 0) {
                if (rb->garbage_count < 8) rb->garbage[rb->garbage_count++] = term->vector_layer_texture;
                else KTerm_DestroyTexture(&term->vector_layer_texture);
            }

            KTerm_CreateTextureEx(clear_img, false, KTERM_TEXTURE_USAGE_SAMPLED | KTERM_TEXTURE_USAGE_STORAGE | KTERM_TEXTURE_USAGE_TRANSFER_DST, &term->vector_layer_texture);
            KTerm_UnloadImage(clear_img);
        }
        term->vector_clear_request = false;
    }

    term->frame_count++;

    // Update Layout
    if (term->layout && term->layout->root) {
        KTerm_RecursiveUpdateSSBO(term, term->layout->root, rb);
    } else {
        if (term->active_session >= 0) {
            KTermSession* s = GET_SESSION(term);
            for(int y=0; y<term->height; y++) {
                 if (y < s->rows && s->row_dirty[y]) {
                     int sx = 0;
                     int sw = term->width;
                     if (s->dirty_rect.w > 0) {
                        int dr_x = s->dirty_rect.x;
                        int dr_w = s->dirty_rect.w;
                        int dr_end = dr_x + dr_w;
                        int start_x = 0;
                        int end_x = term->width;
                        if (start_x < dr_x) start_x = dr_x;
                        if (end_x > dr_end) end_x = dr_end;
                        if (start_x < end_x) {
                            sx = start_x;
                            sw = end_x - start_x;
                        }
                     }
                     KTerm_UpdatePaneRow(term, s, rb, 0 + sx, y, sw, y, sx);
                 }
            }
        }
    }

    // Copy Sixel Data
    KTermSession* sixel_session = GET_SESSION(term);
    int sixel_y_shift = 0;
    if (sixel_session->sixel.active && sixel_session->sixel.strip_count > 0) {
        bool recreate = false;
        if (term->sixel_texture.slot_index == 0 || term->sixel_texture.width != sixel_session->sixel.width || term->sixel_texture.height != sixel_session->sixel.height) {
            recreate = true;
        }
        if (sixel_session->sixel.dirty || recreate) {
             KTermImage img = {0};
             KTerm_CreateImage(sixel_session->sixel.width, sixel_session->sixel.height, 4, &img);
             if (img.data) memset(img.data, 0, img.width * img.height * 4);
             KTermTexture new_tex = {0};
             KTerm_CreateTextureEx(img, false, SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED | KTERM_TEXTURE_USAGE_STORAGE | KTERM_TEXTURE_USAGE_TRANSFER_DST, &new_tex);
             KTerm_UnloadImage(img);

             if (new_tex.slot_index != 0) {
                 if (term->sixel_texture.slot_index != 0) {
                     if (rb->garbage_count < 8) rb->garbage[rb->garbage_count++] = term->sixel_texture;
                     else KTerm_DestroyTexture(&term->sixel_texture);
                 }
                 term->sixel_texture = new_tex;
             }
             sixel_session->sixel.dirty = false;
        }

        if (rb->sixel_capacity < sixel_session->sixel.strip_count) {
            rb->sixel_capacity = sixel_session->sixel.strip_count + 1024;
            rb->sixel_strips = (GPUSixelStrip*)KTerm_Realloc(rb->sixel_strips, rb->sixel_capacity * sizeof(GPUSixelStrip));
        }
        if (rb->sixel_strips) {
            memcpy(rb->sixel_strips, sixel_session->sixel.strips, sixel_session->sixel.strip_count * sizeof(GPUSixelStrip));
            rb->sixel_count = sixel_session->sixel.strip_count;
            for(int i=0; i<256; i++) {
                RGB_KTermColor c = sixel_session->sixel.palette[i];
                rb->sixel_palette[i] = (uint32_t)c.r | ((uint32_t)c.g << 8) | ((uint32_t)c.b << 16) | ((uint32_t)c.a << 24);
            }
            rb->sixel_width = sixel_session->sixel.width;
            rb->sixel_height = sixel_session->sixel.height;
            rb->sixel_active = true;

            if (sixel_session->sixel.scrolling) {
                int height = sixel_session->buffer_height;
                int dist = (sixel_session->screen_head - sixel_session->sixel.logical_start_row);
                if (dist < 0) dist += height;
                dist %= height;
                sixel_y_shift = (dist * term->char_height) - (sixel_session->view_offset * term->char_height);
            }
            rb->sixel_y_offset = sixel_y_shift;
        }
    } else {
        rb->sixel_count = 0;
        rb->sixel_active = false;
    }

    // Populate Push Constants
    KTERM_MUTEX_LOCK(comp->render_lock);
    KTermPushConstants* pc = &rb->constants;
    memset(pc, 0, sizeof(KTermPushConstants));

    pc->terminal_buffer_addr = KTerm_GetBufferAddress(term->terminal_buffer);
    pc->font_texture_handle = KTerm_GetTextureHandle(term->font_texture);
    
    if (session->sixel.active && term->sixel_texture.generation != 0) {
        pc->sixel_texture_handle = KTerm_GetTextureHandle(term->sixel_texture);
    } else {
        pc->sixel_texture_handle = KTerm_GetTextureHandle(term->dummy_sixel_texture);
    }
    pc->sixel_y_offset = sixel_y_shift;

    pc->vector_texture_handle = KTerm_GetTextureHandle(term->vector_layer_texture);
    pc->atlas_cols = term->atlas_cols;
    pc->screen_size = (KTermVector2){{(float)(term->width * term->char_width * DEFAULT_WINDOW_SCALE), (float)(term->height * term->char_height * DEFAULT_WINDOW_SCALE)}};
    int char_w = term->char_width;
    int char_h = term->char_height;
    if (GET_SESSION(term)->soft_font.active) {
        char_w = GET_SESSION(term)->soft_font.char_width;
        char_h = GET_SESSION(term)->soft_font.char_height;
    }
    pc->char_size = (KTermVector2){{(float)char_w, (float)char_h}};
    pc->grid_size = (KTermVector2){{(float)term->width, (float)term->height}};
    pc->time = (float)KTerm_TimerGetTime();
    
    // Debug: Verify push constants (AFTER all values are set)
    static int debug_frame_count = 0;
    if (debug_frame_count < 5) {  // Only first 5 frames
        fprintf(stderr, "[KTerm_CmdPresent] Push Constants (frame %d):\n", debug_frame_count);
        fprintf(stderr, "  - terminal_buffer_addr: 0x%llx\n", (unsigned long long)pc->terminal_buffer_addr);
        fprintf(stderr, "  - font_texture_handle: %llu\n", (unsigned long long)pc->font_texture_handle);
        fprintf(stderr, "  - atlas_cols: %d\n", pc->atlas_cols);
        fprintf(stderr, "  - grid_size: %.0fx%.0f\n", pc->grid_size.x, pc->grid_size.y);
        fprintf(stderr, "  - char_size: %.0fx%.0f\n", pc->char_size.x, pc->char_size.y);
        fprintf(stderr, "  - screen_size: %.0fx%.0f\n", pc->screen_size.x, pc->screen_size.y);
        fflush(stderr);
        debug_frame_count++;
    }

    uint32_t cursor_idx = 0xFFFFFFFF;
    KTermSession* focused_session = NULL;
    if (term->layout && term->layout->focused && term->layout->focused->type == PANE_LEAF) {
        if (term->layout->focused->session_index >= 0) focused_session = &term->sessions[term->layout->focused->session_index];
    }
    if (!focused_session) focused_session = GET_SESSION(term);

    if (focused_session && focused_session->session_open && focused_session->cursor.visible) {
        int origin_x = (term->layout && term->layout->focused) ? term->layout->focused->x : 0;
        int origin_y = (term->layout && term->layout->focused) ? term->layout->focused->y : 0;
        int gx = origin_x + focused_session->cursor.x;
        int gy = origin_y + focused_session->cursor.y;
        if (gx >= 0 && gx < term->width && gy >= 0 && gy < term->height) cursor_idx = gy * term->width + gx;
    }
    pc->cursor_index = cursor_idx;

    if (focused_session && focused_session->mouse.enabled && focused_session->mouse.cursor_x > 0) {
         int mx = focused_session->mouse.cursor_x - 1;
         int my = focused_session->mouse.cursor_y - 1;
         if (term->layout && term->layout->focused) { mx += term->layout->focused->x; my += term->layout->focused->y; }
         if (mx >= 0 && mx < term->width && my >= 0 && my < term->height) pc->mouse_cursor_index = my * term->width + mx;
         else pc->mouse_cursor_index = 0xFFFFFFFF;
    } else pc->mouse_cursor_index = 0xFFFFFFFF;

    pc->cursor_blink_state = focused_session ? (focused_session->cursor.blink_state ? 1 : 0) : 0;
    pc->text_blink_state = focused_session ? focused_session->text_blink_state : 0;

    if (GET_SESSION(term)->selection.active) {
         uint32_t start_idx = GET_SESSION(term)->selection.start_y * term->width + GET_SESSION(term)->selection.start_x;
         uint32_t end_idx = GET_SESSION(term)->selection.end_y * term->width + GET_SESSION(term)->selection.end_x;
         if (start_idx > end_idx) { uint32_t t = start_idx; start_idx = end_idx; end_idx = t; }
         pc->sel_start = start_idx; pc->sel_end = end_idx; pc->sel_active = 1;
    }

    // Shader Config Buffer Update
    // Allocate if needed (Fixed small size)
    if (term->shader_config_buffer.id == 0) {
        KTerm_CreateBuffer(sizeof(GPUShaderConfig), NULL, KTERM_BUFFER_USAGE_STORAGE_BUFFER | KTERM_BUFFER_USAGE_TRANSFER_DST, &term->shader_config_buffer);
    }
    if (term->shader_config_buffer.id != 0) {
        GPUShaderConfig config = {0};
        config.scanline_intensity = term->visual_effects.scanline_intensity;
        config.crt_curvature = term->visual_effects.curvature;
        config.glow_intensity = term->visual_effects.glow_intensity;
        config.noise_intensity = term->visual_effects.noise_intensity;
        config.flags = term->visual_effects.flags;

        // Font dimensions
        config.font_cell_width = term->char_width;
        config.font_cell_height = term->char_height;
        config.font_data_width = term->font_data_width;
        config.font_data_height = term->font_data_height;
        config.atlas_cols = term->atlas_cols;

        if (GET_SESSION(term)->visual_bell_timer > 0.0) {
            float intensity = (float)(GET_SESSION(term)->visual_bell_timer / 0.2);
            if (intensity > 1.0f) intensity = 1.0f; else if (intensity < 0.0f) intensity = 0.0f;
            config.visual_bell_intensity = intensity;
        } else {
            config.visual_bell_intensity = 0.0f;
        }

        // Voice Energy
        float max_energy = 0.0f;
#ifndef KTERM_DISABLE_VOICE
        for (int i = 0; i < MAX_SESSIONS; i++) {
            KTermVoiceContext* vctx = KTerm_Voice_GetContext(&term->sessions[i]);
            if (vctx && vctx->enabled) {
                if (vctx->energy_level > max_energy) max_energy = vctx->energy_level;
            }
        }
#endif
        config.voice_energy = max_energy;

        KTerm_UpdateBuffer(term->shader_config_buffer, 0, sizeof(GPUShaderConfig), &config);
        pc->shader_config_addr = KTerm_GetBufferAddress(term->shader_config_buffer);
    }

    if (focused_session) {
        RGB_KTermColor gc = focused_session->grid_color;
        pc->grid_color = (uint32_t)gc.r | ((uint32_t)gc.g << 8) | ((uint32_t)gc.b << 16) | ((uint32_t)gc.a << 24);
        pc->conceal_char_code = focused_session->conceal_char_code;
    }

    // Copy Vector Data
    if (term->vector_count > 0) {
        if (rb->vector_capacity < term->vector_count) {
             rb->vector_capacity = term->vector_count + 1024;
             rb->vectors = (GPUVectorLine*)KTerm_Realloc(rb->vectors, rb->vector_capacity * sizeof(GPUVectorLine));
        }
        if (rb->vectors && term->vector_staging_buffer) {
            memcpy(rb->vectors, term->vector_staging_buffer, term->vector_count * sizeof(GPUVectorLine));
            rb->vector_count = term->vector_count;
            pc->vector_count = term->vector_count;
        }
    } else {
        rb->vector_count = 0;
        pc->vector_count = 0;
    }

    // Copy Kitty Graphics Ops
    rb->kitty_count = 0;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        KTermSession* session = &term->sessions[i];
        if (!session->session_open || !session->kitty.images) continue;

        KTermPane* pane = NULL;
        KTermPane* stack[32];
        int stack_top = 0;
        if (term->layout && term->layout->root) stack[stack_top++] = term->layout->root;
        while (stack_top > 0) {
            KTermPane* p = stack[--stack_top];
            if (p->type == PANE_LEAF) {
                if (p->session_index == i) { pane = p; break; }
            } else {
                if (p->child_b) stack[stack_top++] = p->child_b;
                if (p->child_a) stack[stack_top++] = p->child_a;
            }
        }
        if (!pane) continue;

        for (int k = 0; k < session->kitty.image_count; k++) {
            KittyImageBuffer* img = &session->kitty.images[k];
            if (!img->visible || !img->frames || img->frame_count == 0 || !img->complete) continue;

            if (img->current_frame >= img->frame_count) img->current_frame = 0;
            KittyFrame* frame = &img->frames[img->current_frame];

            if (frame->texture.slot_index == 0 && frame->data) {
                KTermImage kimg = {0};
                kimg.width = frame->width;
                kimg.height = frame->height;
                kimg.channels = 4;
                kimg.data = frame->data;
                KTerm_CreateTextureEx(kimg, false, KTERM_TEXTURE_USAGE_SAMPLED, &frame->texture);
            }

            if (frame->texture.slot_index == 0) continue;

            if (rb->kitty_count >= rb->kitty_capacity) {
                rb->kitty_capacity = (rb->kitty_capacity == 0) ? 64 : rb->kitty_capacity * 2;
                rb->kitty_ops = (KittyRenderOp*)KTerm_Realloc(rb->kitty_ops, rb->kitty_capacity * sizeof(KittyRenderOp));
            }

            if (rb->kitty_ops) {
                KittyRenderOp* op = &rb->kitty_ops[rb->kitty_count++];
                op->texture = frame->texture;
                op->width = frame->width;
                op->height = frame->height;
                op->z_index = img->z_index;

                int dist = (session->screen_head - img->start_row + session->buffer_height) % session->buffer_height;
                int y_shift = (dist * term->char_height) - (session->view_offset * term->char_height);

                op->x = (pane->x * term->char_width) + img->x;
                op->y = (pane->y * term->char_height) + img->y - y_shift;

                op->clip_x = pane->x * term->char_width;
                op->clip_y = pane->y * term->char_height;
                op->clip_mx = op->clip_x + pane->width * term->char_width - 1;
                op->clip_my = op->clip_y + pane->height * term->char_height - 1;
            }
        }
    }

    // Swap Buffers
    int temp = comp->rb_front;
    comp->rb_front = comp->rb_back;
    comp->rb_back = temp;

    KTERM_MUTEX_UNLOCK(comp->render_lock);
}

void KTermCompositor_Render(KTermCompositor* comp, KTerm* term) {
    if (!term->compute_initialized) return;

    KTERM_MUTEX_LOCK(comp->render_lock);
    KTermRenderBuffer* rb = &comp->render_buffers[comp->rb_front];

    for (int g = 0; g < rb->garbage_count; g++) {
        if (rb->garbage[g].slot_index != 0) KTerm_DestroyTexture(&rb->garbage[g]);
    }
    rb->garbage_count = 0;

    bool acquired = KTerm_AcquireFrameCommandBuffer();
    if (!acquired) {
        // Get the error from Situation
        char* err_msg = NULL;
        SituationGetLastErrorMsg(&err_msg);
        fprintf(stderr, "[KTerm] AcquireFrameCommandBuffer FAILED: %s\n", err_msg ? err_msg : "Unknown error"); 
        fflush(stderr);
        if (err_msg) free(err_msg);
    } else {
        KTermCommandBuffer cmd = KTerm_GetCommandBuffer();
        fprintf(stderr, "[KTerm] AcquireFrameCommandBuffer SUCCESS, cmd=%llu\n", (unsigned long long)cmd.id); fflush(stderr);

        // 1. Sixel Graphics
        if (rb->sixel_active && rb->sixel_count > 0) {
            KTerm_UpdateBuffer(term->sixel_buffer, 0, rb->sixel_count * sizeof(GPUSixelStrip), rb->sixel_strips);
            KTerm_UpdateBuffer(term->sixel_palette_buffer, 0, 256 * sizeof(uint32_t), rb->sixel_palette);

            if (KTerm_CmdBindPipeline(cmd, term->sixel_pipeline) == KTERM_SUCCESS &&
                KTerm_CmdBindTexture(cmd, 0, term->sixel_texture) == KTERM_SUCCESS) {

                KTermPushConstants pc = {0};
                pc.screen_size = (KTermVector2){{(float)rb->sixel_width, (float)rb->sixel_height}};
                pc.vector_count = rb->sixel_count;
                pc.vector_buffer_addr = KTerm_GetBufferAddress(term->sixel_buffer);
                pc.terminal_buffer_addr = KTerm_GetBufferAddress(term->sixel_palette_buffer);
                pc.sixel_y_offset = rb->sixel_y_offset;

                KTerm_CmdSetPushConstant(cmd, 0, &pc, sizeof(pc));
                KTerm_CmdDispatch(cmd, (rb->sixel_count + 63) / 64, 1, 1);
                KTerm_CmdPipelineBarrier(cmd, KTERM_BARRIER_COMPUTE_SHADER_WRITE, KTERM_BARRIER_COMPUTE_SHADER_READ);
            }
        }

        // 2. Clear Screen
        if (term->texture_blit_pipeline.id != 0 && term->clear_texture.slot_index != 0) {
            if (KTerm_CmdBindPipeline(cmd, term->texture_blit_pipeline) == KTERM_SUCCESS &&
                KTerm_CmdBindTexture(cmd, 1, term->output_texture) == KTERM_SUCCESS) {

                struct { int dst_x, dst_y, src_w, src_h; uint64_t handle, _p; int cx, cy, cmx, cmy; } blit_pc;
                blit_pc.dst_x = 0; blit_pc.dst_y = 0;
                blit_pc.src_w = term->width * term->char_width * DEFAULT_WINDOW_SCALE;
                blit_pc.src_h = term->height * term->char_height * DEFAULT_WINDOW_SCALE;
                blit_pc.handle = KTerm_GetTextureHandle(term->clear_texture); blit_pc._p = 0;
                blit_pc.cx = 0; blit_pc.cy = 0; blit_pc.cmx = blit_pc.src_w; blit_pc.cmy = blit_pc.src_h;

                KTerm_CmdSetPushConstant(cmd, 0, &blit_pc, sizeof(blit_pc));
                KTerm_CmdDispatch(cmd, (blit_pc.src_w + 15) / 16, (blit_pc.src_h + 15) / 16, 1);
                KTerm_CmdPipelineBarrier(cmd, KTERM_BARRIER_COMPUTE_SHADER_WRITE, KTERM_BARRIER_COMPUTE_SHADER_READ);
            }
        }

        // 3. Kitty Graphics (Background)
        if (term->texture_blit_pipeline.id != 0 && rb->kitty_count > 0) {
            for (size_t k = 0; k < rb->kitty_count; k++) {
                KittyRenderOp* op = &rb->kitty_ops[k];
                if (op->z_index >= 0) continue;

                if (KTerm_CmdBindPipeline(cmd, term->texture_blit_pipeline) == KTERM_SUCCESS &&
                    KTerm_CmdBindTexture(cmd, 1, term->output_texture) == KTERM_SUCCESS) {
                     struct { int dst_x, dst_y, src_w, src_h; uint64_t handle, _p; int cx, cy, cmx, cmy; } blit_pc;
                     blit_pc.dst_x = op->x; blit_pc.dst_y = op->y;
                     blit_pc.src_w = op->width; blit_pc.src_h = op->height;
                     blit_pc.handle = KTerm_GetTextureHandle(op->texture); blit_pc._p = 0;
                     blit_pc.cx = op->clip_x; blit_pc.cy = op->clip_y;
                     blit_pc.cmx = op->clip_mx; blit_pc.cmy = op->clip_my;
                     KTerm_CmdSetPushConstant(cmd, 0, &blit_pc, sizeof(blit_pc));
                     KTerm_CmdDispatch(cmd, (op->width + 15) / 16, (op->height + 15) / 16, 1);
                     KTerm_CmdPipelineBarrier(cmd, KTERM_BARRIER_COMPUTE_SHADER_WRITE, KTERM_BARRIER_COMPUTE_SHADER_READ);
                }
            }
        }

        // 4. Terminal Text
        size_t required_size = rb->cell_count * sizeof(GPUCell);
        fprintf(stderr, "[KTerm] Rendering terminal: %zu cells\n", rb->cell_count); fflush(stderr);
        KTerm_UpdateBuffer(term->terminal_buffer, 0, required_size, rb->cells);
        
        // Debug: Check terminal buffer content (first frame only)
        static bool buffer_checked = false;
        if (!buffer_checked) {
            GPUCell* cells = rb->cells;
            if (cells) {
                fprintf(stderr, "[KTerm_CmdPresent] Terminal buffer sample (first 10 cells):\n");
                for (int i = 0; i < 10 && i < term->width * term->height; i++) {
                    fprintf(stderr, "  Cell[%d]: char=0x%04x fg=0x%08x bg=0x%08x flags=0x%08x\n",
                            i, cells[i].char_code, cells[i].fg_color, cells[i].bg_color, cells[i].flags);
                }
                fflush(stderr);
            } else {
                fprintf(stderr, "[KTerm_CmdPresent] WARNING: rb->cells is NULL!\n");
                fflush(stderr);
            }
            buffer_checked = true;
        }

        if (KTerm_CmdBindPipeline(cmd, term->compute_pipeline) == KTERM_SUCCESS &&
            KTerm_CmdBindTexture(cmd, 1, term->output_texture) == KTERM_SUCCESS) {
            
            // CRITICAL: Bind terminal buffer to set 0 (required by pipeline layout even though shader uses BDA)
            // The SIT_COMPUTE_LAYOUT_TERMINAL layout expects set 0 to be bound
            KTerm_CmdBindBuffer(cmd, 0, term->terminal_buffer);
            
            // Bind font texture to set 2
            KTerm_CmdBindTexture(cmd, 2, term->font_texture);
            
            // Bind sixel texture to set 3
            KTermSession* active_session = GET_SESSION(term);
            if (active_session->sixel.active && term->sixel_texture.generation != 0) {
                KTerm_CmdBindTexture(cmd, 3, term->sixel_texture);
            } else {
                KTerm_CmdBindTexture(cmd, 3, term->dummy_sixel_texture);
            }
            
            // Debug: Verify bindings (first frame only)
            static bool bind_checked = false;
            if (!bind_checked) {
                fprintf(stderr, "[KTerm] Pipeline and all resources bound successfully\n");
                fprintf(stderr, "[KTerm] terminal_buffer.id=%llu\n", (unsigned long long)term->terminal_buffer.id);
                fprintf(stderr, "[KTerm] output_texture.slot_index=%u, generation=%u\n", 
                        term->output_texture.slot_index, term->output_texture.generation);
                fprintf(stderr, "[KTerm] font_texture.slot_index=%u, generation=%u\n",
                        term->font_texture.slot_index, term->font_texture.generation);
                fflush(stderr);
                bind_checked = true;
            }

            KTerm_CmdSetPushConstant(cmd, 0, &rb->constants, sizeof(KTermPushConstants));
            
            // Calculate workgroups based on screen size and workgroup size (8x16)
            uint32_t screen_w = (uint32_t)rb->constants.screen_size.x;
            uint32_t screen_h = (uint32_t)rb->constants.screen_size.y;
            uint32_t groups_x = (screen_w + 7) / 8;
            uint32_t groups_y = (screen_h + 15) / 16;
            
            KTerm_CmdDispatch(cmd, groups_x, groups_y, 1);
            fprintf(stderr, "[KTerm] Dispatched terminal compute: %dx%d workgroups (screen %dx%d pixels)\n", 
                    groups_x, groups_y, screen_w, screen_h); 
            fflush(stderr);
            KTerm_CmdPipelineBarrier(cmd, KTERM_BARRIER_COMPUTE_SHADER_WRITE, KTERM_BARRIER_COMPUTE_SHADER_READ);
        } else {
            fprintf(stderr, "[KTerm] ERROR: Failed to bind pipeline or texture!\n"); fflush(stderr);
        }

        // 5. Kitty Graphics (Foreground)
        if (term->texture_blit_pipeline.id != 0 && rb->kitty_count > 0) {
            for (size_t k = 0; k < rb->kitty_count; k++) {
                KittyRenderOp* op = &rb->kitty_ops[k];
                if (op->z_index < 0) continue;

                if (KTerm_CmdBindPipeline(cmd, term->texture_blit_pipeline) == KTERM_SUCCESS &&
                    KTerm_CmdBindTexture(cmd, 1, term->output_texture) == KTERM_SUCCESS) {
                     struct { int dst_x, dst_y, src_w, src_h; uint64_t handle, _p; int cx, cy, cmx, cmy; } blit_pc;
                     blit_pc.dst_x = op->x; blit_pc.dst_y = op->y;
                     blit_pc.src_w = op->width; blit_pc.src_h = op->height;
                     blit_pc.handle = KTerm_GetTextureHandle(op->texture); blit_pc._p = 0;
                     blit_pc.cx = op->clip_x; blit_pc.cy = op->clip_y;
                     blit_pc.cmx = op->clip_mx; blit_pc.cmy = op->clip_my;
                     KTerm_CmdSetPushConstant(cmd, 0, &blit_pc, sizeof(blit_pc));
                     KTerm_CmdDispatch(cmd, (op->width + 15) / 16, (op->height + 15) / 16, 1);
                     KTerm_CmdPipelineBarrier(cmd, KTERM_BARRIER_COMPUTE_SHADER_WRITE, KTERM_BARRIER_COMPUTE_SHADER_READ);
                }
            }
        }

        // 6. Vectors
        if (rb->vector_count > 0) {
            KTerm_UpdateBuffer(term->vector_buffer, 0, rb->vector_count * sizeof(GPUVectorLine), rb->vectors);
            if (KTerm_CmdBindPipeline(cmd, term->vector_pipeline) == KTERM_SUCCESS &&
                KTerm_CmdBindTexture(cmd, 1, term->vector_layer_texture) == KTERM_SUCCESS) {

                KTermPushConstants vector_pc = {0};
                vector_pc.vector_count = rb->vector_count;
                vector_pc.vector_buffer_addr = KTerm_GetBufferAddress(term->vector_buffer);
                KTerm_CmdSetPushConstant(cmd, 0, &vector_pc, sizeof(vector_pc));
                KTerm_CmdDispatch(cmd, (rb->vector_count + 63) / 64, 1, 1);
                KTerm_CmdPipelineBarrier(cmd, KTERM_BARRIER_COMPUTE_SHADER_WRITE, KTERM_BARRIER_COMPUTE_SHADER_READ);
            }
        }

        KTerm_CmdPipelineBarrier(cmd, KTERM_BARRIER_COMPUTE_SHADER_WRITE, KTERM_BARRIER_TRANSFER_READ);
        fprintf(stderr, "[KTerm] About to present output_texture (slot_index=%u)\n", term->output_texture.slot_index); fflush(stderr);
        SituationError present_result = KTerm_CmdPresent(cmd, term->output_texture);
        fprintf(stderr, "[KTerm] Present result: %d (%s)\n", present_result, present_result == SITUATION_SUCCESS ? "SUCCESS" : "FAILED"); fflush(stderr);
        if (present_result != SITUATION_SUCCESS) {
             if (GET_SESSION(term)->options.debug_sequences) KTerm_LogUnsupportedSequence(term, "Present failed");
        }
    }
    KTerm_EndFrame();
    KTERM_MUTEX_UNLOCK(comp->render_lock);
}

#endif
