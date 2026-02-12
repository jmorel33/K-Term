#ifndef MOCK_SITUATION_H
#define MOCK_SITUATION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Include stb_truetype for font support
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// Types
typedef int SituationError;
typedef struct { uint64_t id; } SituationComputePipeline;
typedef struct { uint64_t id; } SituationBuffer;
typedef struct { uint64_t id; uint32_t width; uint32_t height; uint32_t generation; uint32_t slot_index; } SituationTexture;
typedef struct { int width; int height; int channels; unsigned char* data; } SituationImage;
typedef struct { uint64_t id; } SituationCommandBuffer;

// Mock texture slot for Vulkan
typedef struct {
    uint64_t id;
    void* descriptor_set;
    void* image;
    void* image_view;
    void* sampler;
} _SituationTextureSlot;

typedef struct Vector2 {
    union {
        struct { float x, y; };
        float v[2];
    };
} Vector2;

typedef struct { unsigned char r, g, b, a; } Color;

typedef int SituationTextureUsageFlags;
typedef int SituationRendererType;

// Constants
#define SITUATION_SUCCESS 0
#define SITUATION_FAILURE 1
#define SITUATION_TEXTURE_USAGE_SAMPLED 1
#define SITUATION_TEXTURE_USAGE_STORAGE 2
#define SITUATION_TEXTURE_USAGE_TRANSFER_SRC 4
#define SITUATION_TEXTURE_USAGE_TRANSFER_DST 8
#define SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED 16
#define SITUATION_BUFFER_USAGE_STORAGE_BUFFER 1
#define SITUATION_BUFFER_USAGE_TRANSFER_DST 2
#define SITUATION_BUFFER_USAGE_STORAGE_COMPUTE 4
#define SITUATION_BARRIER_COMPUTE_SHADER_WRITE 1
#define SITUATION_BARRIER_COMPUTE_SHADER_READ 2
#define SITUATION_BARRIER_TRANSFER_READ 4
#define SIT_COMPUTE_LAYOUT_TERMINAL 0
#define SIT_COMPUTE_LAYOUT_VECTOR 1
#define SIT_COMPUTE_LAYOUT_SIXEL 2
#define SITUATION_SCALING_INTEGER 0
#define SITUATION_BLEND_ALPHA 0
#define SITUATION_WINDOW_STATE_RESIZABLE 1

#define SIT_RENDERER_OPENGL 0
#define SIT_RENDERER_VULKAN 1

// Keys
#define SIT_KEY_LEFT_CONTROL 341
#define SIT_KEY_RIGHT_CONTROL 345
#define SIT_KEY_LEFT_ALT 342
#define SIT_KEY_RIGHT_ALT 346
#define SIT_KEY_LEFT_SHIFT 340
#define SIT_KEY_RIGHT_SHIFT 344
#define SIT_KEY_PAGE_UP 266
#define SIT_KEY_PAGE_DOWN 267
#define SIT_KEY_F1 290
#define SIT_KEY_F2 291
#define SIT_KEY_F3 292
#define SIT_KEY_F4 293
#define SIT_KEY_F5 294
#define SIT_KEY_F6 295
#define SIT_KEY_F7 296
#define SIT_KEY_F8 297
#define SIT_KEY_F9 298
#define SIT_KEY_F10 299
#define SIT_KEY_F11 300
#define SIT_KEY_F12 301

// Mouse Buttons
#define GLFW_MOUSE_BUTTON_LEFT 0
#define GLFW_MOUSE_BUTTON_RIGHT 1
#define GLFW_MOUSE_BUTTON_MIDDLE 2

// Globals for tests
static char last_clipboard_text[1024 * 1024]; // 1MB buffer for test
static double mock_time = 0.0;
static inline void MockSetTime(double t) { mock_time = t; }

// Functions
static inline int SituationCreateBuffer(size_t size, void* data, int usage, SituationBuffer* buffer) {
    buffer->id = 1;
    return SITUATION_SUCCESS;
}
static inline int SituationUpdateBuffer(SituationBuffer buffer, size_t offset, size_t size, const void* data) {
    return SITUATION_SUCCESS;
}
static inline void SituationDestroyBuffer(SituationBuffer* buffer) { buffer->id = 0; }

static inline int SituationCreateImage(int width, int height, int channels, SituationImage* image) {
    image->width = width; image->height = height; image->channels = channels;
    image->data = calloc(1, width * height * channels);
    return SITUATION_SUCCESS;
}
static inline void SituationUnloadImage(SituationImage image) {
    if (image.data) free(image.data);
}

static inline int SituationCreateTexture(SituationImage image, bool mipmaps, SituationTexture* texture) {
    texture->id = 1; texture->width = image.width; texture->height = image.height; texture->generation = 1; texture->slot_index = 1;
    return SITUATION_SUCCESS;
}
static inline int SituationCreateTextureEx(SituationImage image, bool mipmaps, int usage, SituationTexture* texture) {
    return SituationCreateTexture(image, mipmaps, texture);
}
static inline void SituationDestroyTexture(SituationTexture* texture) { texture->id = 0; texture->slot_index = 0; }
static inline uint64_t SituationGetTextureHandle(SituationTexture texture) { return texture.id; }

static inline _SituationTextureSlot* _SitGetTextureSlot(SituationTexture handle) {
    static _SituationTextureSlot slot = {0};
    slot.id = handle.id;
    return &slot;
}

static inline int SituationCreateComputePipelineFromMemory(const char* code, int layout_id, SituationComputePipeline* pipeline) {
    pipeline->id = 1;
    return SITUATION_SUCCESS;
}
static inline void SituationDestroyComputePipeline(SituationComputePipeline* pipeline) { pipeline->id = 0; }

static inline uint64_t SituationGetBufferDeviceAddress(SituationBuffer buffer) { return buffer.id; }

static inline bool SituationAcquireFrameCommandBuffer() { return true; }
static inline SituationCommandBuffer SituationGetMainCommandBuffer() { return (SituationCommandBuffer){1}; }
static inline void SituationEndFrame() {}

static inline int SituationCmdBindComputePipeline(SituationCommandBuffer cmd, SituationComputePipeline pipeline) { return SITUATION_SUCCESS; }
static inline int SituationCmdBindComputeTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture) { return SITUATION_SUCCESS; }
static inline void SituationCmdSetPushConstant(SituationCommandBuffer cmd, int offset, const void* data, size_t size) {}
static inline void SituationCmdDispatch(SituationCommandBuffer cmd, int x, int y, int z) {}
static inline void SituationCmdPipelineBarrier(SituationCommandBuffer cmd, int src, int dst) {}
static inline int SituationCmdPresent(SituationCommandBuffer cmd, SituationTexture texture) { return SITUATION_SUCCESS; }
static inline int SituationCmdBindComputeBuffer(SituationCommandBuffer cmd, int binding, SituationBuffer buffer) { return SITUATION_SUCCESS; }

static inline bool SituationTimerGetOscillatorState(int slot) { return true; }
static inline double SituationTimerGetTime() { return mock_time; }
static inline float SituationGetFrameTime() { return 0.016f; }

static inline int SituationLoadFileData(const char* path, int* len, unsigned char** data) {
    if (len) *len = 0;
    if (data) *data = NULL;
    return SITUATION_SUCCESS;
}

static inline int SituationCreateVirtualDisplay(Vector2 size, float scale, int layout, int scaling, int blend, int* id) {
    if (id) *id = 1;
    return SITUATION_SUCCESS;
}
static inline void SituationSetWindowTitle(const char* title) {}

// Clipboard
static inline void SituationSetClipboardText(const char* text) {
    if (text) {
        strncpy(last_clipboard_text, text, sizeof(last_clipboard_text) - 1);
        last_clipboard_text[sizeof(last_clipboard_text) - 1] = '\0';
    } else {
        last_clipboard_text[0] = '\0';
    }
}
static inline int SituationGetClipboardText(const char** text) {
    *text = strdup(last_clipboard_text);
    return SITUATION_SUCCESS;
}
static inline void SituationFreeString(char* str) { free(str); }

static inline SituationRendererType SituationGetRendererType() { return SIT_RENDERER_OPENGL; }
static inline void SituationGetLastErrorMsg(char** msg) { *msg = NULL; }

// Window / Input
static inline void SituationRestoreWindow() {}
static inline void SituationMinimizeWindow() {}
static inline void SituationSetWindowPosition(int x, int y) {}
static inline void SituationSetWindowSize(int w, int h) {}
static inline void SituationSetWindowFocused() {}
static inline void SituationMaximizeWindow() {}
static inline bool SituationIsWindowFullscreen() { return false; }
static inline void SituationToggleFullscreen() {}
static inline int SituationGetScreenHeight() { return 1080; }
static inline int SituationGetScreenWidth() { return 1920; }

// Structs for Init
typedef struct {
    int window_width;
    int window_height;
    const char* window_title;
    int initial_active_window_flags;
} SituationInitInfo;

static inline int SituationInit(int flags, void* ctx, SituationInitInfo* info) { return SITUATION_SUCCESS; }
static inline void SituationSetTargetFPS(int fps) {}
static inline void SituationBeginFrame() {}
static inline void SituationShutdown() {}

// Input
static inline bool SituationIsKeyDown(int key) { return false; }
static inline int SituationGetKeyPressed() { return 0; }
static inline int SituationGetCharPressed() { return 0; }
static inline Vector2 SituationGetMousePosition() { return (Vector2){0,0}; }
static inline bool SituationIsMouseButtonPressed(int button) { return false; }
static inline bool SituationIsMouseButtonDown(int button) { return false; }
static inline bool SituationIsMouseButtonReleased(int button) { return false; }
static inline float SituationGetMouseWheelMove() { return 0.0f; }

// Raylib-like
static inline bool WindowShouldClose() { return false; } // Loop forever in mock
static inline void ClearBackground(Color c) {}
static inline bool SituationIsWindowResized() { return false; }
static inline void SituationGetWindowSize(int* w, int* h) { *w=800; *h=600; }

#endif // MOCK_SITUATION_H
