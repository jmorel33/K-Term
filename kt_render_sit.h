#ifndef KT_RENDER_SIT_H
#define KT_RENDER_SIT_H

#ifdef KTERM_TESTING
#include "tests/mock_situation.h"
#else
#include "situation.h"
#endif

// =============================================================================
// RENDER ADAPTER (Situation -> KTerm)
// =============================================================================
// This adapter abstracts the underlying platform/engine (Situation) from the K-Term core.
// It allows for easier porting or mocking in the future while currently aliasing
// directly to the Situation library.

// --- Types ---
typedef SituationComputePipeline KTermPipeline;
typedef SituationBuffer KTermBuffer;
typedef SituationTexture KTermTexture;
typedef SituationImage KTermImage;
typedef SituationCommandBuffer KTermCommandBuffer;

typedef Vector2 KTermVector2;
typedef Color KTermColor;

// --- Constants ---
#define KTERM_SUCCESS SITUATION_SUCCESS
#define KTERM_FAILURE SITUATION_FAILURE

#define KTERM_TEXTURE_USAGE_SAMPLED SITUATION_TEXTURE_USAGE_SAMPLED
#define KTERM_TEXTURE_USAGE_STORAGE SITUATION_TEXTURE_USAGE_STORAGE
#define KTERM_TEXTURE_USAGE_TRANSFER_SRC SITUATION_TEXTURE_USAGE_TRANSFER_SRC
#define KTERM_TEXTURE_USAGE_TRANSFER_DST SITUATION_TEXTURE_USAGE_TRANSFER_DST
#define KTERM_TEXTURE_USAGE_COMPUTE_SAMPLED SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED

#define KTERM_BUFFER_USAGE_STORAGE_BUFFER SITUATION_BUFFER_USAGE_STORAGE_BUFFER
#define KTERM_BUFFER_USAGE_TRANSFER_DST SITUATION_BUFFER_USAGE_TRANSFER_DST
#define KTERM_BUFFER_USAGE_STORAGE_COMPUTE SITUATION_BUFFER_USAGE_STORAGE_COMPUTE

#define KTERM_BARRIER_COMPUTE_SHADER_WRITE SITUATION_BARRIER_COMPUTE_SHADER_WRITE
#define KTERM_BARRIER_COMPUTE_SHADER_READ SITUATION_BARRIER_COMPUTE_SHADER_READ
#define KTERM_BARRIER_TRANSFER_READ SITUATION_BARRIER_TRANSFER_READ

#define KTERM_COMPUTE_LAYOUT_TERMINAL SIT_COMPUTE_LAYOUT_TERMINAL
#define KTERM_COMPUTE_LAYOUT_VECTOR SIT_COMPUTE_LAYOUT_VECTOR
#define KTERM_COMPUTE_LAYOUT_SIXEL SIT_COMPUTE_LAYOUT_TERMINAL  // Sixel layout not in current API

#define KTERM_SCALING_INTEGER SITUATION_SCALING_INTEGER
#define KTERM_BLEND_ALPHA SITUATION_BLEND_ALPHA
#define KTERM_WINDOW_STATE_RESIZABLE SITUATION_WINDOW_STATE_RESIZABLE

// --- Functions ---
#define KTerm_CreateBuffer SituationCreateBuffer
#define KTerm_UpdateBuffer SituationUpdateBuffer
#define KTerm_DestroyBuffer SituationDestroyBuffer

#define KTerm_CreateImage SituationCreateImage
#define KTerm_UnloadImage SituationUnloadImage

#define KTerm_CreateTexture SituationCreateTexture
#define KTerm_CreateTextureEx SituationCreateTextureEx
#define KTerm_DestroyTexture SituationDestroyTexture
#define KTerm_GetTextureHandle SituationGetTextureHandle

#define KTerm_CreateComputePipeline SituationCreateComputePipelineFromMemory
#define KTerm_DestroyPipeline SituationDestroyComputePipeline

#define KTerm_GetBufferAddress SituationGetBufferDeviceAddress

#define KTerm_AcquireFrameCommandBuffer SituationAcquireFrameCommandBuffer
#define KTerm_GetCommandBuffer SituationGetMainCommandBuffer
#define KTerm_EndFrame SituationEndFrame

// Wrapper for SituationCmdBindComputePipeline (returns void, but K-Term expects KTERM_SUCCESS)
static inline int KTerm_CmdBindPipeline(SituationCommandBuffer cmd, SituationComputePipeline pipeline) {
    SituationCmdBindComputePipeline(cmd, pipeline);
    return KTERM_SUCCESS;
}

#define KTerm_CmdBindTexture SituationCmdBindComputeTexture
#define KTerm_CmdBindBuffer SituationCmdBindComputeBuffer
#define KTerm_CmdSetPushConstant SituationCmdSetPushConstant
#define KTerm_CmdDispatch SituationCmdDispatch
#define KTerm_CmdPipelineBarrier SituationCmdPipelineBarrier
#define KTerm_CmdPresent SituationCmdPresent

#define KTerm_TimerGetOscillator SituationTimerGetOscillatorState
#define KTerm_TimerGetTime SituationTimerGetTime
#define KTerm_GetFrameTime SituationGetFrameTime

#define KTerm_LoadFileData SituationLoadFileData

#define KTerm_CreateVirtualDisplay SituationCreateVirtualDisplay
#define KTerm_SetWindowTitlePlatform SituationSetWindowTitle

// Window Management & Clipboard
#define KTerm_SetClipboardText SituationSetClipboardText
#define KTerm_GetClipboardText SituationGetClipboardText
#define KTerm_FreeString SituationFreeString

#define KTerm_RestoreWindow SituationRestoreWindow
#define KTerm_MinimizeWindow SituationMinimizeWindow
#define KTerm_SetWindowPosition SituationSetWindowPosition
#define KTerm_SetWindowSize SituationSetWindowSize
#define KTerm_SetWindowFocused SituationSetWindowFocused
#define KTerm_MaximizeWindow SituationMaximizeWindow
#define KTerm_IsWindowFullscreen SituationIsWindowFullscreen
#define KTerm_ToggleFullscreen SituationToggleFullscreen
#define KTerm_GetScreenHeight SituationGetScreenHeight
#define KTerm_GetScreenWidth SituationGetScreenWidth

#define KTerm_Platform_Init SituationInit
#define KTerm_SetTargetFPS SituationSetTargetFPS
#define KTerm_BeginFrame SituationBeginFrame
#define KTerm_Platform_Shutdown SituationShutdown
typedef SituationInitInfo KTermInitInfo;

// Memory
#ifdef SIT_FREE
  #define KTERM_FREE SIT_FREE
#else
  #define KTERM_FREE free
#endif

#endif // KTERM_RENDER_SIT_H
