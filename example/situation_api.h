/***************************************************************************************************
*
*   -- The "Situation" Advanced Platform Awareness, Control, and Timing --
*   Core API library (see version in Version Macros)
*   (c) 2025 Jacques Morel
*   MIT Licensed
*
*   A single-file, cross-platform C/C++ library providing unified, low-level access and control over essential application subsystems. Its purpose is to abstract away platform-specific complexities,
*   offering a lean yet powerful API for building sophisticated, high-performance software.
*
*   The library's philosophy is reflected in its name, granting developers complete situational "Awareness," precise "Control," and fine-grained "Timing."
*
*   **Velocity Module (Hot-Reloading):**
*   This release integrates the **Hot-Reloading Module**, a development-focused toolset that allows Shaders, Compute Pipelines, Textures, and 3D Models to be reloaded from disk at runtime.
*   This eliminates the need to restart the application to see asset changes, significantly increasing iteration speed for visual adjustments and shader programming.
*
*   It provides deep **Awareness** of the host system through APIs for querying hardware and multi-monitor display information, and by handling operating system events like window focus and file drops.
*
*   This foundation enables precise **Control** over the entire application stack, from window management (fullscreen, borderless) and input devices (keyboard, mouse, gamepad) to a comprehensive audio
*   pipeline with playback, capture, and real-time effects. This control extends to the graphics and compute pipeline, abstracting modern OpenGL and Vulkan through a unified command-buffer model.
*   It offers simplified management of GPU resources—such as shaders, meshes, and textures—and includes powerful utilities for high-quality text rendering and robust filesystem I/O.
*
*   Finally, its **Timing** capabilities range from high-resolution performance measurement and frame rate management to an advanced **Temporal Oscillator System** for creating complex, rhythmically
*   synchronized events. By handling the foundational boilerplate of platform interaction, "Situation" empowers developers to focus on core application logic, enabling the creation of responsive and
*   sophisticated software—from games and creative coding projects to data visualization tools—across all major desktop platforms.
*
***************************************************************************************************
*
*   License (MIT)
*   -------------
*   Copyright (c) 2025 Jacques Morel
*
*   Permission is hereby granted, free of charge, to any person obtaining a copy
*   of this software and associated documentation files (the "Software"), to deal
*   in the Software without restriction, including without limitation the rights
*   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*   copies of the Software, and to permit persons to whom the Software is
*   furnished to do so, subject to the following conditions:
*
*   The above copyright notice and this permission notice shall be included in all
*   copies or substantial portions of the Software.
*
*   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*   SOFTWARE.
*
***************************************************************************************************/
// --- Version Macros ---
#define SITUATION_VERSION_MAJOR 2
#define SITUATION_VERSION_MINOR 3
#define SITUATION_VERSION_PATCH 41
#define SITUATION_VERSION_REVISION ""

/*
 *  ---------------------------------------------------------------------------------------------------
 *  COMPILATION INSTRUCTIONS
 *  ---------------------------------------------------------------------------------------------------
 *
 *  GCC / Clang (Linux):
 *      gcc main.c -o situation_demo -std=c11 -I. -I./ext -D_POSIX_C_SOURCE=200809L \
 *      -lglfw -lGL -lm -ldl -lpthread
 *
 *  MinGW (Windows):
 *      gcc main.c -o situation_demo.exe -std=c11 -I. -I./ext \
 *      -lglfw3 -lgdi32 -lopengl32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi \
 *      -ldxgi -lpropsys -lshlwapi -lm
 *
 *  Note: Ensure you link against GLFW3 and your system's OpenGL libraries.
 *        The 'ext' directory should contain the required dependencies (stb, glad, miniaudio).
 */

#ifndef SITUATION_API_H
#define SITUATION_API_H

/*
 * Feature Test Macros (Strict C11 Support)
 * ----------------------------------------
 * When compiling with strict standard flags (e.g., -std=c11), compilers like GCC and Clang disable non-standard extensions by default. This hides common OS-level functions
 * that are part of POSIX but not ISO C.
 *
 * We define these macros to explicitly request POSIX.1-2008 and X/Open 7 support.
 * This exposes necessary APIs required by the implementation, specifically:
 * - Filesystem queries: stat(), S_ISREG, S_ISDIR
 * - System interaction: readlink(), nanosleep()
 *
 * NOTE: These must be defined before ANY system headers are included.
 */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

/**
 * @brief API Declaration Control (for Shared Library / DLL support)
 */
#if defined(_WIN32)
    #if defined(SITUATION_BUILD_SHARED)
        #define SITAPI __declspec(dllexport) // Exporting functions from the DLL
    #elif defined(SITUATION_USE_SHARED)
        #define SITAPI __declspec(dllimport) // Importing functions from the DLL
    #else
        #define SITAPI // Static library or header-only, no special declaration needed
    #endif
#else // On other platforms like Linux or macOS, attribute visibility is preferred
    #if defined(SITUATION_BUILD_SHARED)
        #define SITAPI __attribute__((visibility("default")))
    #else
        #define SITAPI // No special declaration needed for static or header-only
    #endif
#endif

/**
 * @brief The public-facing macro. It now ALWAYS calls the logging function.
 *      The logging function itself will decide whether to print to stderr based on the build type.
 */
#if defined(SITUATION_USE_OPENGL)
SITAPI void _SituationLogGLError(const char* file, int line);
#define SIT_CHECK_GL_ERROR() _SituationLogGLError(__FILE__, __LINE__)
#else
#define SIT_CHECK_GL_ERROR() do {} while(0)
#endif

// --- [IMPORTANT] User must define the backend to use ---
#if !defined(SITUATION_USE_VULKAN) && !defined(SITUATION_USE_OPENGL)
    #error "You must define either SITUATION_USE_VULKAN or SITUATION_USE_OPENGL before including situation.h"
#endif
#if defined(SITUATION_USE_VULKAN) && defined(SITUATION_USE_OPENGL)
    #error "You can only define one renderer backend (SITUATION_USE_VULKAN or SITUATION_USE_OPENGL)"
#endif

// --- [OPTIONAL] Define this to enable runtime GLSL -> SPIR-V compilation for both backends.
// #define SITUATION_ENABLE_SHADER_COMPILER

// --- Check for valid configurations ---
// FIX 2.3.2A: Relaxed requirement. Internal renderers will be disabled if compiler is missing.
#if defined(SITUATION_USE_VULKAN) && !defined(SITUATION_ENABLE_SHADER_COMPILER)
    // Warning: SituationCmdDrawQuad and Virtual Displays will be unavailable.
#endif

#include <stddef.h> // for audio stream
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> 	// For fmodf, fmaxf, fminf
#include <float.h> 	// For FLT_MAX
#ifndef M_PI // Define M_PI if not already defined (common for MSVC)
    #define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
    #define M_PI_2 1.57079632679489661923
#endif
/**
 * @brief Core dependencies
 */
#include <cglm/cglm.h>
#define GLFW_INCLUDE_NONE   // This prevents GLFW from trying to include the system GL/gl.h header, which is missing in some environments (including this one) and is unnecessary when using glad.
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <miniaudio.h>

/**
 * @brief Backend-specific includes
 */
#if defined(SITUATION_USE_OPENGL)
    #include <glad.h>
#elif defined(SITUATION_USE_VULKAN)
    #include <vulkan/vulkan.h>
#endif

/**
 * @brief Optional Shader Compiler (shaderc)
 */
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    #include <shaderc/shaderc.h>
#endif

/**
 * @brief Platform-specific includes
 */
#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#include <windows.h>
    #include <ws2tcpip.h>       // For network adapter info
    #include <iphlpapi.h>       // For GetAdaptersAddresses
    #include <objbase.h>        // For CoInitialize/CoUninitialize
    #include <shellapi.h>       // For ShellExecute
    #ifdef SITUATION_ENABLE_DXGI // Optional DXGI for GPU info
        #include <dxgi.h>
    #endif
    #include <Xinput.h>         // For gamepad vibration
    #include <setupapi.h>       // For SetupDiGetClassDevs
    #include <devguid.h>        // For GUID_DEVCLASS_xxx
    #include <shlobj.h>         // For SHGetKnownFolderPath
#else                           // For non-Windows (e.g., Linux for GetUserDirectory)
    #include <unistd.h>         // For getuid (potentially), sysconf
    #include <sys/types.h>      // For getpwuid (potentially)
    #include <pwd.h>            // For getpwuid (potentially)
    #include <sys/statvfs.h>    // For storage info on Linux/macOS
    #include <sys/wait.h>       // For waitpid (SituationExecuteCommand)
    #include <fcntl.h>          // For open, O_RDWR (SituationExecuteCommand)
    #if defined(__linux__)
        #include <sys/sysinfo.h>    // For RAM info on Linux
    #endif
    #if defined(__APPLE__)
        #include <sys/sysctl.h> // For sysctlbyname
        #include <sys/mount.h>  // For statfs
    #endif
    #if defined(__linux__) || defined(__APPLE__)
        #include <ifaddrs.h>    // For getifaddrs
        #include <netinet/in.h> // For sockaddr_in
        #include <arpa/inet.h>  // For inet_ntoa
        #include <net/if.h>     // For IFF_LOOPBACK
    #endif
#endif


// --- Initialization State Management (v2.3.40) ---
typedef enum SituationInitState {
    SITUATION_STATE_UNINITIALIZED = 0,  // Library not initialized
    SITUATION_STATE_INITIALIZING = 1,    // Init in progress, render thread starting
    SITUATION_STATE_READY = 2,           // Fully initialized, safe to create resources
    SITUATION_STATE_SHUTTING_DOWN = 3    // Cleanup in progress
} SituationInitState;

/**
 * @brief Logs a warning message in debug builds.
 * @details This function is intended for internal library use. It formats a warning message
 *          and, in debug builds (when NDEBUG is not defined), prints it to stderr and sets the
 *          library's last error state. In release builds, this function is compiled out to nothing.
 * @param code The SituationError code associated with the warning.
 * @param fmt The printf-style format string for the message.
 * @param ... Variable arguments for the format string.
 */
//==================================================================================
//  SituationError - Comprehensive, Strictly Ordered Error Code System (Titanium Grade)
//==================================================================================
//
//  Every possible failure in the entire library has its own unique, permanent code.
//  Ranges are sacred and immutable. No gaps. No re-use. Exhaustive switch() possible.
//  All errors are negative. SITUATION_SUCCESS = 0.
//
//  0      → Success
//  1–99   → Core & System
//  100–199→ Platform & Windowing
//  200–299→ Display System
//  300–399→ Filesystem & Hot-Reloading
//  400–499→ Audio Subsystem
//  500–599→ Resource Management & Rendering Core
//  600–699→ OpenGL Backend-Specific (OpenGL)
//  700–799→ Backend-Specific (Vulkan)
//  800–899→ Compute / GPGPU
//  900–999→ Reserved (Debug/Profiler, Network, Future Platforms)

typedef enum {
    SITUATION_SUCCESS                                       =   0,  // Operation completed successfully

    // ── Core & System Errors (1–99) ─────────────────────────────────────
	SITUATION_ERROR_GENERAL                                	=   -1,  // Catch-all for unexpected errors
    SITUATION_ERROR_NOT_IMPLEMENTED                        	=   -2,  // Feature declared but intentionally unimplemented on current backend
    SITUATION_ERROR_NOT_INITIALIZED                        	=   -3,  // API called before SituationInit()
    SITUATION_ERROR_ALREADY_INITIALIZED                    	=   -4,  // SituationInit() called twice
    SITUATION_ERROR_INIT_FAILED                            	=   -5,  // Core initialization sequence failed
    SITUATION_ERROR_SHUTDOWN_FAILED                        	=   -6,  // Resources still alive or backend refused cleanup
    SITUATION_ERROR_INVALID_PARAM                          	=   -7,  // NULL pointer, out-of-range value, invalid enum, etc.
    SITUATION_ERROR_MEMORY_ALLOCATION                      	=   -8,  // SIT_MALLOC/SIT_CALLOC/SIT_REALLOC/VmaAllocation failed
    SITUATION_ERROR_INTERNAL_STATE_CORRUPTED               	=   -9,  // Internal invariant violated — fatal bug, please report
    SITUATION_ERROR_ASSERTION_FAILED                       	=  -10,  // Debug assertion tripped (only in debug builds)
    SITUATION_ERROR_UPDATE_AFTER_DRAW_VIOLATION            	=  -11,  // Critical architectural rule broken (debug builds only)
    SITUATION_ERROR_TIMER_SYSTEM                           	=  -20,  // An error occurred within the internal timer/oscillator system
	SITUATION_ERROR_THREAD_QUEUE_FULL  						=  -80,  // Threading Error: Thread Queue Full
	SITUATION_ERROR_THREAD_VIOLATION   						=  -81,  // Main-thread-only function called from worker thread
    SITUATION_ERROR_THREAD_CYCLE                            =  -82,  // Dependency cycle or depth limit exceeded
    SITUATION_ERROR_THREAD_CREATION_FAILED                  =  -83,  // Failed to spawn a new thread (thrd_create)
    SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT             =  -84,  // Render thread join timeout
    SITUATION_ERROR_RENDER_LIST_INCOMPLETE                  =  -85,  // Render list incomplete (Momentum)
    SITUATION_ERROR_ARM_INTRINSICS_FAILED                   =  -86,  // ARM-specific WFE/SEV intrinsic failure
	SITUATION_ERROR_COMMAND_EXECUTION_FAILED                =  -90,  // External system command execution failed


    // ── Platform & Windowing Errors (100–199) ───────────────────────────
    SITUATION_ERROR_GLFW_FAILED                             = -100,  // Any GLFW function returned an error
    SITUATION_ERROR_WINDOW_CREATION_FAILED                  = -101,  // Failed to create GLFW window
    SITUATION_ERROR_WINDOW_FOCUS_FAILED                     = -102,  // Focus/minimize/restore operation failed
    SITUATION_ERROR_CLIPBOARD_FAILED                        = -103,  // Clipboard get/set failed
    SITUATION_ERROR_CURSOR_CREATION_FAILED                  = -104,  // Custom cursor creation failed
    SITUATION_ERROR_COM_INITIALIZATION_FAILED               = -110,  // CoInitializeEx failed (Windows only)
    SITUATION_ERROR_DXGI_QUERY_FAILED                       = -111,  // DXGI GPU query failed (Windows only)
    SITUATION_ERROR_WINDOW_FOCUS							= -120,  // An operation related to window focus failed.
    SITUATION_ERROR_DEVICE_QUERY							= -121,  // Failed to query system hardware or device information.
    SITUATION_ERROR_COM_FAILED								= -123,  // [Win32] Failed to initialize the COM library.
    SITUATION_ERROR_DXGI_FAILED								= -124,  // [Win32] A call to the DXGI library failed (e.g., for GPU info).

	// ── Display & Virtual Display Errors (-200 to -299) ─────────────────
    SITUATION_ERROR_DISPLAY_QUERY                           = -200,  // Failed to query physical monitor information
    SITUATION_ERROR_DISPLAY_SET                             = -201,  // Failed to set a display mode on a physical monitor
    SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT                   = -202,  // The maximum number of virtual displays has been reached
    SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID              = -203,  // Invalid virtual display ID supplied
    SITUATION_ERROR_DISPLAY_QUERY_FAILED                    = -210,  // glfwGetMonitors / mode query failed (detailed variant)
    SITUATION_ERROR_DISPLAY_MODE_UNSUPPORTED                = -211,  // Requested resolution/refresh rate not available
    SITUATION_ERROR_DISPLAY_MODE_SET_FAILED                 = -212,  // Failed to apply fullscreen mode
    SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT_REACHED           = -213,  // Max virtual displays (32) already created (detailed variant)
    SITUATION_ERROR_VIRTUAL_DISPLAY_NOT_FOUND               = -214,  // Virtual display ID not found in active list

	// ── Filesystem & Hot-Reloading Errors (-300 to -399) ────────────────
    SITUATION_ERROR_FILE_ACCESS                             = -300,  // A generic file or directory access error occurred
    SITUATION_ERROR_PATH_NOT_FOUND                          = -301,  // The specified file or directory was not found
    SITUATION_ERROR_PATH_INVALID                            = -302,  // The specified path is invalid or contains illegal characters
    SITUATION_ERROR_PERMISSION_DENIED                       = -303,  // Permission was denied for the requested file operation
    SITUATION_ERROR_DISK_FULL                               = -304,  // The disk is full; cannot complete a write operation
    SITUATION_ERROR_FILE_LOCKED                             = -305,  // The file is locked or currently in use by another process
    SITUATION_ERROR_DIR_NOT_EMPTY                           = -306,  // A directory is not empty and cannot be deleted non-recursively
    SITUATION_ERROR_FILE_ALREADY_EXISTS                     = -307,  // The specified file already exists where it shouldn't
    SITUATION_ERROR_PATH_IS_DIRECTORY                       = -308,  // A file operation was attempted on a path that is a directory
    SITUATION_ERROR_PATH_IS_FILE                            = -309,  // A directory operation was attempted on a path that is a file
    SITUATION_ERROR_FILE_NOT_FOUND                          = -310,  // File does not exist (detailed variant)
    SITUATION_ERROR_FILE_ACCESS_DENIED                      = -311,  // Permission denied (detailed variant)
    SITUATION_ERROR_FILE_OPEN_FAILED                        = -312,  // fopen() or equivalent failed
    SITUATION_ERROR_FILE_READ_FAILED                        = -313,  // Read operation failed
    SITUATION_ERROR_FILE_WRITE_FAILED                       = -314,  // Write operation failed
    SITUATION_ERROR_FILE_TOO_LARGE                          = -315,  // File exceeds internal limits (>2 GB typically)
    SITUATION_ERROR_DIRECTORY_CREATION_FAILED               = -316,  // Failed to create directory
    SITUATION_ERROR_HOTRELOAD_WATCHER_FAILED                = -320,  // inotify / ReadDirectoryChangesW failed
    SITUATION_ERROR_HOTRELOAD_FILE_CHANGED_TOO_FAST         = -321,  // File changed faster than debounce window
    SITUATION_ERROR_HOTRELOAD_GPU_SYNC_FAILED               = -322,  // vkDeviceWaitIdle / glFinish failed during reload

	// ── Audio Subsystem Errors (-400 to -499) ───────────────────────────
    SITUATION_ERROR_AUDIO_CONTEXT                           = -400,  // Failed to initialize the audio context (MiniAudio)
    SITUATION_ERROR_AUDIO_DEVICE                            = -401,  // Failed to initialize, start, or stop an audio device
    SITUATION_ERROR_AUDIO_SOUND_LIMIT                       = -402,  // The sound playback queue limit was reached
    SITUATION_ERROR_AUDIO_CONVERTER                         = -403,  // Failed to configure a data format/rate converter for a sound
    SITUATION_ERROR_AUDIO_DECODING                          = -404,  // Failed to decode an audio file
    SITUATION_ERROR_AUDIO_INVALID_OPERATION                 = -405,  // An invalid operation was attempted on a sound (e.g., cropping a stream)
    SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED               = -410,  // MiniAudio context failed (detailed variant)
    SITUATION_ERROR_AUDIO_DEVICE_INIT_FAILED                = -411,  // Device startup failed
    SITUATION_ERROR_AUDIO_DEVICE_START_FAILED               = -412,  // ma_device_start() failed
    SITUATION_ERROR_AUDIO_DECODER_INIT_FAILED               = -413,  // ma_decoder_init failed
    SITUATION_ERROR_AUDIO_DECODER_FORMAT_UNSUPPORTED        = -414,  // Codec/container not supported
    SITUATION_ERROR_AUDIO_STREAM_ENDED                      = -415,  // Internal: stream reached EOF (not fatal)
    SITUATION_ERROR_AUDIO_SOUND_LIMIT_REACHED               = -420,  // Max concurrent sounds exceeded (detailed variant)
    SITUATION_ERROR_AUDIO_CAPTURE_NOT_AVAILABLE             = -430,  // No microphone or capture device found

	// ── Resource Management & Rendering Core Errors (-500 to -599) ──────
    SITUATION_ERROR_RESOURCE_INVALID                        = -500,  // An invalid handle (shader, mesh, texture, buffer) was passed to a function
    SITUATION_ERROR_BUFFER_INVALID_SIZE                     = -501,  // A buffer operation was attempted with an out-of-bounds offset or size
    SITUATION_ERROR_RENDER_COMMAND_FAILED                   = -502,  // A command failed to be recorded to a command buffer
    SITUATION_ERROR_RENDER_PASS_ACTIVE                      = -503,  // An operation was attempted that is illegal during a render pass
    SITUATION_ERROR_INVALID_RESOURCE_HANDLE                 = -510,  // Null or corrupted handle passed (detailed variant)
    SITUATION_ERROR_RESOURCE_ALREADY_DESTROYED              = -511,  // Use-after-free attempt
    SITUATION_ERROR_BUFFER_MAP_FAILED                       = -512,  // vkMapMemory / glMapBuffer failed
    SITUATION_ERROR_BUFFER_OVERFLOW                         = -513,  // Write beyond buffer bounds
    SITUATION_ERROR_BUFFER_INVALID_USAGE                    = -514,  // Wrong usage flags for operation
    SITUATION_ERROR_TEXTURE_UPLOAD_FAILED                   = -520,  // vkImage upload / glTexImage failed
    SITUATION_ERROR_NO_ACTIVE_COMMAND_BUFFER                = -530,  // No frame acquired
    SITUATION_ERROR_COMMAND_BUFFER_FULL                     = -531,  // Command limit reached (extremely rare)
    SITUATION_ERROR_NO_RENDER_PASS_ACTIVE                   = -540,  // Draw call outside render pass
    SITUATION_ERROR_RENDER_PASS_ALREADY_ACTIVE              = -541,  // Nested render pass attempted
    SITUATION_ERROR_BACKEND_MISMATCH                        = -550,  // Operation requested on wrong backend (e.g., GL call on Vulkan)
    SITUATION_ERROR_PIPELINE_BIND_FAIL                      = -552,  // Failed to bind pipeline (incompatible layout or invalid handle)

	// ── OpenGL Backend Errors (-600 to -699) ────────────────────────────
    SITUATION_ERROR_OPENGL_GENERAL                          = -600,  // A generic OpenGL error occurred (glGetError)
    SITUATION_ERROR_OPENGL_LOADER_FAILED                    = -601,  // Failed to load OpenGL functions (GLAD)
    SITUATION_ERROR_OPENGL_UNSUPPORTED                      = -602,  // A required OpenGL version or extension is not supported by the driver
    SITUATION_ERROR_OPENGL_SHADER_COMPILE                   = -610,  // GLSL shader compilation failed
    SITUATION_ERROR_OPENGL_SHADER_LINK                      = -611,  // GLSL shader program linking failed
    SITUATION_ERROR_OPENGL_FBO_INCOMPLETE                   = -620,  // A Framebuffer Object is not complete and cannot be used for rendering
    SITUATION_ERROR_OPENGL_CONTEXT_CREATION_FAILED          = -630,  // OpenGL context creation failed
    SITUATION_ERROR_OPENGL_UNSUPPORTED_VERSION              = -631,  // < GL 4.6 Core
    SITUATION_ERROR_OPENGL_SHADER_COMPILE_FAILED            = -632,  // Detailed shader compile error
    SITUATION_ERROR_OPENGL_SHADER_LINK_FAILED               = -633,  // Detailed link error
    SITUATION_ERROR_OPENGL_PROGRAM_VALIDATION_FAILED        = -634,  // Program validation failed
    SITUATION_ERROR_OPENGL_UNIFORM_NOT_FOUND                = -635,  // Uniform location query failed

	// ── Vulkan Backend Errors (-700 to -799) ────────────────────────────
    SITUATION_ERROR_VULKAN_INIT_FAILED                      = -700,  // General Vulkan initialization failed
    SITUATION_ERROR_VULKAN_INSTANCE_FAILED                  = -701,  // Failed to create a VkInstance
    SITUATION_ERROR_VULKAN_DEVICE_FAILED                    = -702,  // Failed to select a physical or create a logical device
    SITUATION_ERROR_VULKAN_UNSUPPORTED                      = -703,  // A required Vulkan layer, extension, or feature is unsupported
    SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED                 = -710,  // A swapchain operation failed (creation, acquire, present)
    SITUATION_ERROR_VULKAN_COMMAND_FAILED                   = -720,  // A command pool or buffer operation failed
    SITUATION_ERROR_VULKAN_RENDERPASS_FAILED                = -730,  // Failed to create a VkRenderPass
    SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED               = -731,  // Failed to create a VkFramebuffer
    SITUATION_ERROR_VULKAN_PIPELINE_FAILED                  = -732,  // Failed to create a graphics or compute pipeline
    SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED               = -733,  // Failed to create a fence or semaphore
    SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED              = -734,  // A GPU memory allocation failed (VMA)
    SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED                = -735,  // A descriptor set or pool operation failed
    SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED         = -740,  // Detailed instance creation error
    SITUATION_ERROR_VULKAN_PHYSICAL_DEVICE_UNSUITABLE       = -741,  // Physical device unsuitable
    SITUATION_ERROR_VULKAN_DEVICE_CREATION_FAILED           = -742,  // Logical device creation failed
    SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED        = -743,  // Detailed swapchain creation error
    SITUATION_ERROR_VULKAN_SWAPCHAIN_INVALID                = -744,  // Invalid swapchain state
    SITUATION_ERROR_VULKAN_IMAGE_ACQUIRE_FAILED             = -745,  // Image acquire failed
    SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED              = -746,  // Queue submit failed
    SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED         = -747,  // Detailed pipeline creation error
    SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED             = -748,  // Shader module creation failed
    SITUATION_ERROR_VULKAN_DESCRIPTOR_POOL_EXHAUSTED        = -749,  // Descriptor pool exhausted
    SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED         = -750,  // Detailed memory allocation error
    SITUATION_ERROR_VULKAN_VALIDATION_LAYER_ERROR           = -751,  // Validation layer error (debug only)
    SITUATION_ERROR_SHADER_COMPILATION_FAILED               = -752,  // Shader compilation failed (shaderc)

	// ── Compute / GPGPU Errors (-800 to -899) ───────────────────────────
    SITUATION_ERROR_COMPUTE_PIPELINE_CREATION_FAILED        = -800,  // Compute pipeline creation failed
    SITUATION_ERROR_COMPUTE_DISPATCH_FAILED                 = -801,  // Dispatch command failed
    SITUATION_ERROR_COMPUTE_BUFFER_BINDING_MISSING          = -802,  // Missing storage buffer binding

    // ── Network Errors (-900 to -949) ───────────────────────────────────
    SITUATION_ERROR_NETWORK_INIT_FAILED                     = -900,  // Failed to initialize network subsystem
    SITUATION_ERROR_NETWORK_SOCKET_CREATION_FAILED          = -901,  // Failed to create socket
    SITUATION_ERROR_NETWORK_CONNECTION_FAILED               = -902,  // Failed to connect to remote host
    SITUATION_ERROR_NETWORK_SEND_FAILED                     = -903,  // Failed to send data
    SITUATION_ERROR_NETWORK_RECEIVE_FAILED                  = -904,  // Failed to receive data
    SITUATION_ERROR_NETWORK_BIND_FAILED                     = -905,  // Failed to bind socket
    SITUATION_ERROR_NETWORK_LISTEN_FAILED                   = -906,  // Failed to listen on socket
    SITUATION_ERROR_NETWORK_ACCEPT_FAILED                   = -907,  // Failed to accept connection

    // ── Unknown / Catch-All ─────────────────────────────────────────────
    SITUATION_ERROR_UNKNOWN_ERROR                           = -999,  // Cosmic rays.

} SituationError;

/**
 * @brief Logs a warning message in debug builds.
 * @details This function is intended for internal library use. It formats a warning message
 *          and, in debug builds (when NDEBUG is not defined), prints it to stderr and sets the
 *          library's last error state. In release builds, this function is compiled out to nothing.
 * @param code The SituationError code associated with the warning.
 * @param fmt The printf-style format string for the message.
 * @param ... Variable arguments for the format string.
 */
typedef enum {
    SIT_LOG_ALL = 0,
    SIT_LOG_TRACE,
    SIT_LOG_DEBUG,
    SIT_LOG_INFO,
    SIT_LOG_WARNING,
    SIT_LOG_ERROR,
    SIT_LOG_FATAL,
    SIT_LOG_NONE
} SituationLogLevel;

SITAPI void SituationLog(int msgType, const char* text, ...);
SITAPI void SituationSetTraceLogLevel(int logType);

SITAPI void SituationLogWarning(SituationError code, const char* fmt, ...);
#define SITUATION_LOG_WARNING SituationLogWarning

// Enable runtime main-thread asserts (debug only)
#ifdef SITUATION_ENABLE_MT_ASSERTS
    #define SIT_ASSERT_MAIN_THREAD() _SituationAssertMainThread(__FILE__, __LINE__)
#else
    #define SIT_ASSERT_MAIN_THREAD() do {} while(0)
#endif

// --- Threading Support Configuration ---
// Use native C11 threads if available, otherwise fall back to tinycthread (Windows MinGW)
#ifdef __cplusplus
    // C++ mode: Use C++11 <atomic> instead of C11 <stdatomic.h>
    #include <atomic>
    #include <thread>
    #include <mutex>
    #include <condition_variable>
    
    // Map C11 atomic types to C++ std::atomic
    #define _Atomic(T) std::atomic<T>
    #define atomic_load(ptr) ((ptr)->load())
    #define atomic_store(ptr, val) ((ptr)->store(val))
    #define atomic_fetch_add(ptr, val) ((ptr)->fetch_add(val))
    #define atomic_fetch_sub(ptr, val) ((ptr)->fetch_sub(val))
    #define atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail) \
        ((ptr)->compare_exchange_strong(*(expected), desired, succ, fail))
    #define atomic_init(ptr, val) ((ptr)->store(val))
    #define memory_order_seq_cst std::memory_order_seq_cst
    #define memory_order_relaxed std::memory_order_relaxed
    
    // C++ atomic type aliases
    using atomic_int = std::atomic<int>;
    using atomic_bool = std::atomic<bool>;
    using atomic_uint = std::atomic<unsigned int>;
    using atomic_size_t = std::atomic<size_t>;
    using atomic_uint16_t = std::atomic<uint16_t>;
    using atomic_uint32_t = std::atomic<uint32_t>;
    using atomic_ushort = std::atomic<unsigned short>;
    using atomic_uint_least32_t = std::atomic<uint_least32_t>;
    using atomic_uint_least64_t = std::atomic<uint_least64_t>;
    using atomic_float = std::atomic<float>;
    
    // Note: C11 threads (mtx_*, cnd_*, thrd_*) are NOT available in C++ mode
    // The library will need to use tinycthread which provides these on Windows
    // For now, we'll use tinycthread even in C++ mode
    #define TINYCTHREAD_IMPLEMENTATION
    #include "ext/glfw/deps/tinycthread.h"
    
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
        #include <stdalign.h>
    #endif
#else
    // C mode: Use C11 atomics
    #if !defined(__STDC_NO_THREADS__)
        #include <threads.h>
        #include <stdatomic.h>
        #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
            #include <stdalign.h> // For alignas/_Alignas
        #endif
    #else
        // Fallback: Use tinycthread for platforms without native C11 threads (e.g., MinGW)
        #if defined(SITUATION_ENABLE_THREADING)
            #define TINYCTHREAD_IMPLEMENTATION
            #include "ext/glfw/deps/tinycthread.h"
            #include <stdatomic.h>
            #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
                #include <stdalign.h>
            #endif
        #endif
    #endif
    
    // Define atomic_float (not in C11 standard but needed for audio)
    typedef _Atomic(float) atomic_float;
#endif


// Shim for timespec_get on Windows (not available in MinGW)
#if defined(_WIN32) && !defined(timespec_get)
static inline int timespec_get(struct timespec *ts, int base) {
    if (base != TIME_UTC) return 0;
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    ts->tv_sec = (time_t)(counter.QuadPart / frequency.QuadPart);
    ts->tv_nsec = (long)(((counter.QuadPart % frequency.QuadPart) * 1000000000) / frequency.QuadPart);
    return base;
}
#endif

#ifdef SITUATION_ENABLE_THREADING
    #if defined(__STDC_NO_THREADS__) && !defined(TINYCTHREAD_IMPLEMENTATION)
        #error "SITUATION_ENABLE_THREADING requires C11 <threads.h> support or tinycthread fallback."
    #endif

// ==================================================================================
//  Thread Pool & Task System (Generational)
// ==================================================================================
//  A high-performance, lock-free (for reading) job system with two priority queues.
//  Features:
//   - Dual Priority Queues: High (Physics/Logic) and Low (Assets/IO).
//   - O(1) Job Tracking: Uses a generational index to safely track job completion.
//   - Small Object Optimization (SOO): Embeds 64 bytes of data directly in the job struct.
//   - Fork-Join Parallelism: DispatchParallel for batched workloads.
//   - Backpressure Handling: Run-Inline fallback when queues are full.

// -- Constants --
#define SITUATION_MAX_THREADS 32
#define SITUATION_JOB_PAYLOAD_MAX 64

// -- Types --
typedef uint32_t SituationJobId;

// -- Submission Flags --
typedef enum {
    SIT_SUBMIT_DEFAULT       = 0,       // Low Priority, Return 0 if full
    SIT_SUBMIT_HIGH_PRIORITY = 1 << 0,  // Use High Priority Queue (Physics, Audio)
    SIT_SUBMIT_BLOCK_IF_FULL = 1 << 1,  // Spin/Sleep until a slot opens
    SIT_SUBMIT_RUN_IF_FULL   = 1 << 2,  // Execute immediately on current thread if full
    SIT_SUBMIT_POINTER_ONLY  = 1 << 3   // Do not copy large data; user guarantees lifetime
} SituationJobFlags;

// -- Job Definition (Generational) --
// Aligned to cache line boundaries to prevent false sharing
typedef struct SituationJob {
    // Generation counter for O(1) validation (prevents ABA problems)
    atomic_ushort generation;

    // Callback function: func(payload_ptr, user_context_ptr)
    void (*func)(void*, void*);

    // Dependency Graph Support
    atomic_int dependency_count;        // Wait counter: Job runs when this hits 0
    atomic_uint_least32_t continuation_id; // ID of job to trigger when this finishes (CAS target)
    uint8_t dep_depth;                  // Cycle detection depth counter (max 32)
    bool uses_large_data;               // Flag for data location
    bool owns_memory;                   // [New] If true, large_data_ptr must be freed

    // Small Object Optimization (SOO)
    // 64 bytes avoids malloc for matrices, config structs, etc.
    #if defined(__stdalign_h)
    alignas(16) uint8_t storage[SITUATION_JOB_PAYLOAD_MAX];
    #else
    uint8_t storage[SITUATION_JOB_PAYLOAD_MAX]; // Fallback alignment
    #endif

    // Fallback for large data (>64 bytes)
    void* large_data_ptr;

    // Synchronization & Status
    atomic_bool is_completed;
} SituationJob;

// -- Thread Pool Handle --
typedef struct SituationThreadPool {
    bool is_active;
    thrd_t threads[SITUATION_MAX_THREADS];
    size_t thread_count;

    // -- Dual Ring Buffers --
    // Index 0 = Low Priority (Assets/IO), Index 1 = High Priority (Physics/Logic)
    struct {
        SituationJob* jobs;
        size_t capacity;
        size_t mask;        // capacity - 1 (for fast bitwise wrapping)
        atomic_size_t head; // Write index
        atomic_size_t tail; // Read index
        mtx_t lock;         // Fine-grained lock per queue
    } queues[2];

    // Signaling
    cnd_t wake_condition;   // Wakes workers when work is added
    cnd_t idle_condition;   // Wakes main thread when all jobs complete

    // Dedicated I/O Thread
    thrd_t io_thread;
    atomic_bool io_active;
    double hot_reload_rate; // [v2.3.37] Store rate

    atomic_int active_jobs; // Total jobs currently running or pending
    atomic_bool shutdown;
    char _padding[64];      // Prevent false sharing on the shutdown flag
} SituationThreadPool;

// Note: API Prototypes are located in the main API section below (around line ~2240)
// to keep header structure clean and consistent with other modules.
// See: SituationCreateThreadPool, SituationSubmitJobEx, etc.

#endif // SITUATION_ENABLE_THREADING

// ---------------------------------------------------------------------------------
//  Buffer Usage Flags (Critical for backend memory optimisation)
//  These flags are translated directly to VkBufferUsageFlags / GL buffer usage hints.
//  Always specify the minimal set required - the backend will place the buffer in the fastest
//  memory type possible based on these hints.
// ---------------------------------------------------------------------------------
typedef enum {
    SITUATION_BUFFER_USAGE_VERTEX_BUFFER     = 1 << 0,   // Source of vertex data
    SITUATION_BUFFER_USAGE_INDEX_BUFFER      = 1 << 1,   // Source of index data
    SITUATION_BUFFER_USAGE_UNIFORM_BUFFER    = 1 << 2,   // Uniform Buffer Object (constant data, frequently updated)
    SITUATION_BUFFER_USAGE_STORAGE_BUFFER    = 1 << 3,   // Shader Storage Buffer Object (read/write in shaders)
    SITUATION_BUFFER_USAGE_INDIRECT_BUFFER   = 1 << 4,   // Indirect draw/dispatch command buffer
    SITUATION_BUFFER_USAGE_TRANSFER_SRC      = 1 << 5,   // Source for copy operations (CPU → GPU staging)
    SITUATION_BUFFER_USAGE_TRANSFER_DST      = 1 << 6,   // Destination for copy operations (GPU → CPU readback)
    SITUATION_BUFFER_USAGE_DEVICE_ADDRESS    = 1 << 7,   // Buffer can be accessed via device address (for buffer references)

    // Common combination presets (use these for convenience and maximum performance)
    SITUATION_BUFFER_USAGE_VERTEX_AND_STORAGE = SITUATION_BUFFER_USAGE_VERTEX_BUFFER | SITUATION_BUFFER_USAGE_STORAGE_BUFFER,
    SITUATION_BUFFER_USAGE_DYNAMIC_VERTEX = SITUATION_BUFFER_USAGE_VERTEX_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_DST,
    SITUATION_BUFFER_USAGE_DYNAMIC_UNIFORM = SITUATION_BUFFER_USAGE_UNIFORM_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_DST,
    SITUATION_BUFFER_USAGE_STORAGE_COMPUTE = SITUATION_BUFFER_USAGE_STORAGE_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_SRC | SITUATION_BUFFER_USAGE_TRANSFER_DST | SITUATION_BUFFER_USAGE_DEVICE_ADDRESS,
} SituationBufferUsageFlags;
// --- New Convenience Macro for Safe Main Loop ---
// Ensures inputs are polled and timers updated at the exact start of the frame.
#define SITUATION_BEGIN_FRAME() \
    SituationPollInputEvents(); \
    SituationUpdateTimers();

// --- Config Flags ---
#define SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD 0x00000001 // Process audio capture callbacks on main thread


/**
  * @brief Configuration Defines
  *
  * These macros define compile-time constants and limits for the library's internal structures and behaviors.
  * They control resource capacities, buffer sizes, and default thresholds across subsystems. Adjust via
  * preprocessor overrides before including `situation.h` if needed (e.g., for embedded targets).
  * Defaults are tuned for desktop performance; lower for mobile/constrained environments.
  *
  * @note Increasing limits may raise memory usage; decreasing may cap features (e.g., concurrent sounds).
  * @see SituationInitInfo for runtime tunables.
  */

/* === General System Limits === */
#define SITUATION_VK_STAGING_BUFFER_SIZE 		(128 * 1024 * 1024) /* 128 MB — great for large texture/model uploads in Vulkan */
#define SITUATION_GL_RING_SIZE                  (64 * 1024 * 1024) /* 64MB Persistent Ring Buffer for OpenGL Zero-Copy updates */
#define SITUATION_MAX_FRAMES_IN_FLIGHT          2    /* Max overlapping frames for VK/GL swapchains (2-3 typical for V-Sync). */
#define SITUATION_MAX_STORAGE_DEVICES           8    /* Max detected storage volumes (e.g., drives, mounts). */
#define SITUATION_MAX_NETWORK_ADAPTERS          8    /* Max network interfaces (e.g., Ethernet/Wi-Fi). */
#define SITUATION_MAX_DEVICE_NAME_LEN           128  /* Max length for device strings (e.g., GPU/CPU names). */
#define SITUATION_MAX_CPU_NAME_LEN              64   /* Max CPU model string length (e.g., "Intel i9-13900K"). */
#define SITUATION_MAX_GPU_NAME_LEN              128  /* Max GPU model string length (e.g., "NVIDIA RTX 4090"). */
#define SITUATION_MAX_MONITORS                  8    /* Max physical displays to track in device snapshot. */
#define SITUATION_MAX_MONITOR_NAME_LEN          128  /* Max monitor EDID name length (e.g., "Dell UltraSharp"). */
#define SITUATION_MAX_ERROR_MSG_LEN             2048 /* Max length for error messages and logs. */
#define SITUATION_MAX_SHADER_LOG_LEN            2048 /* Max length for shader compilation logs. */

/* === Graphics & Rendering Limits === */
#define SITUATION_MAX_VIRTUAL_DISPLAYS          16   /* Max offscreen render targets (e.g., for UI/post-fx). */
#define SITUATION_MAX_TEXTURES                  4096

/* === Audio Subsystem Limits === */
#define SITUATION_MAX_AUDIO_SOUNDS_QUEUED       32   /* Max concurrent sounds in mixing queue (e.g., SFX layers). */
#define SITUATION_MAX_TONES                     64   /* 64-voice polyphony for procedural synthesis. */
#define SITUATION_AUDIO_CALLBACK_TEMP_BUFFER_FRAMES 2048 /* Scratch frames for decode/effects/conversion (48kHz ~40ms). */

/* === Input Subsystem Limits === */
#define SITUATION_MAX_INPUT_DEVICES             16   /* Max tracked input devices (keyboards, mice, gamepads, etc.). */
#define SITUATION_KEY_QUEUE_MAX                 64   /* Max buffered keyboard events per frame (anti-loss ring). */
#define SITUATION_CHAR_QUEUE_MAX                64   /* Max buffered char inputs per frame (IME/text entry). */
#define SITUATION_MAX_SCANCODES                 512  /* Max number of scancodes to track (safe for most OS). */
#define SITUATION_MAX_JOYSTICKS                 2    /* Max tracked gamepads/joysticks (local co-op). */
#define SITUATION_MAX_JOYSTICK_BUTTONS          15   /* Standard gamepad buttons (A/B/X/Y, D-pad, bumpers, etc.). */
#define SITUATION_MAX_JOYSTICK_AXES             6    /* Standard gamepad axes (left/right sticks + triggers). */
#define SITUATION_JOYSTICK_DEADZONE_L           0.10f /* Default deadzone for left analog (anti-drift). */
#define SITUATION_JOYSTICK_DEADZONE_R           0.10f /* Default deadzone for right analog (anti-drift). */


/**
 * @brief Key Codes (from GLFW, re-defined for API stability)
 */
#define SIT_KEY_SPACE              32
#define SIT_KEY_APOSTROPHE         39  /* ' */
#define SIT_KEY_COMMA              44  /* , */
#define SIT_KEY_MINUS              45  /* - */
#define SIT_KEY_PERIOD             46  /* . */
#define SIT_KEY_SLASH              47  /* / */
#define SIT_KEY_0                  48
#define SIT_KEY_1                  49
#define SIT_KEY_2                  50
#define SIT_KEY_3                  51
#define SIT_KEY_4                  52
#define SIT_KEY_5                  53
#define SIT_KEY_6                  54
#define SIT_KEY_7                  55
#define SIT_KEY_8                  56
#define SIT_KEY_9                  57
#define SIT_KEY_SEMICOLON          59  /* ; */
#define SIT_KEY_EQUAL              61  /* = */
#define SIT_KEY_A                  65
#define SIT_KEY_B                  66
#define SIT_KEY_C                  67
#define SIT_KEY_D                  68
#define SIT_KEY_E                  69
#define SIT_KEY_F                  70
#define SIT_KEY_G                  71
#define SIT_KEY_H                  72
#define SIT_KEY_I                  73
#define SIT_KEY_J                  74
#define SIT_KEY_K                  75
#define SIT_KEY_L                  76
#define SIT_KEY_M                  77
#define SIT_KEY_N                  78
#define SIT_KEY_O                  79
#define SIT_KEY_P                  80
#define SIT_KEY_Q                  81
#define SIT_KEY_R                  82
#define SIT_KEY_S                  83
#define SIT_KEY_T                  84
#define SIT_KEY_U                  85
#define SIT_KEY_V                  86
#define SIT_KEY_W                  87
#define SIT_KEY_X                  88
#define SIT_KEY_Y                  89
#define SIT_KEY_Z                  90
#define SIT_KEY_LEFT_BRACKET       91  /* [ */
#define SIT_KEY_BACKSLASH          92  /* \ */
#define SIT_KEY_RIGHT_BRACKET      93  /* ] */
#define SIT_KEY_GRAVE_ACCENT       96  /* ` */
#define SIT_KEY_WORLD_1            161 /* non-US #1 */
#define SIT_KEY_WORLD_2            162 /* non-US #2 */

// --- Function keys ---
#define SIT_KEY_ESCAPE             256
#define SIT_KEY_ENTER              257
#define SIT_KEY_TAB                258
#define SIT_KEY_BACKSPACE          259
#define SIT_KEY_INSERT             260
#define SIT_KEY_DELETE             261
#define SIT_KEY_RIGHT              262
#define SIT_KEY_LEFT               263
#define SIT_KEY_DOWN               264
#define SIT_KEY_UP                 265
#define SIT_KEY_PAGE_UP            266
#define SIT_KEY_PAGE_DOWN          267
#define SIT_KEY_HOME               268
#define SIT_KEY_END                269
#define SIT_KEY_CAPS_LOCK          280
#define SIT_KEY_SCROLL_LOCK        281
#define SIT_KEY_NUM_LOCK           282
#define SIT_KEY_PRINT_SCREEN       283
#define SIT_KEY_PAUSE              284
#define SIT_KEY_F1                 290
#define SIT_KEY_F2                 291
#define SIT_KEY_F3                 292
#define SIT_KEY_F4                 293
#define SIT_KEY_F5                 294
#define SIT_KEY_F6                 295
#define SIT_KEY_F7                 296
#define SIT_KEY_F8                 297
#define SIT_KEY_F9                 298
#define SIT_KEY_F10                299
#define SIT_KEY_F11                300
#define SIT_KEY_F12                301
#define SIT_KEY_F13                302
#define SIT_KEY_F14                303
#define SIT_KEY_F15                304
#define SIT_KEY_F16                305
#define SIT_KEY_F17                306
#define SIT_KEY_F18                307
#define SIT_KEY_F19                308
#define SIT_KEY_F20                309
#define SIT_KEY_F21                310
#define SIT_KEY_F22                311
#define SIT_KEY_F23                312
#define SIT_KEY_F24                313
#define SIT_KEY_F25                314

// --- Keypad keys ---
#define SIT_KEY_KP_0               320
#define SIT_KEY_KP_1               321
#define SIT_KEY_KP_2               322
#define SIT_KEY_KP_3               323
#define SIT_KEY_KP_4               324
#define SIT_KEY_KP_5               325
#define SIT_KEY_KP_6               326
#define SIT_KEY_KP_7               327
#define SIT_KEY_KP_8               328
#define SIT_KEY_KP_9               329
#define SIT_KEY_KP_DECIMAL         330
#define SIT_KEY_KP_DIVIDE          331
#define SIT_KEY_KP_MULTIPLY        332
#define SIT_KEY_KP_SUBTRACT        333
#define SIT_KEY_KP_ADD             334
#define SIT_KEY_KP_ENTER           335
#define SIT_KEY_KP_EQUAL           336

// --- Modifier keys (positional) ---
#define SIT_KEY_LEFT_SHIFT         340
#define SIT_KEY_LEFT_CONTROL       341
#define SIT_KEY_LEFT_ALT           342
#define SIT_KEY_LEFT_SUPER         343 // Windows/Command/Meta key
#define SIT_KEY_RIGHT_SHIFT        344
#define SIT_KEY_RIGHT_CONTROL      345
#define SIT_KEY_RIGHT_ALT          346
#define SIT_KEY_RIGHT_SUPER        347
#define SIT_KEY_MENU               348

// --- Modifier Bitmasks ---
#define SIT_MOD_SHIFT              0x0001
#define SIT_MOD_CONTROL            0x0002
#define SIT_MOD_ALT                0x0004
#define SIT_MOD_SUPER              0x0008
#define SIT_MOD_CAPS_LOCK          0x0010
#define SIT_MOD_NUM_LOCK           0x0020

/**
 * @brief Basic Math Types
 */
typedef struct ColorHSV { float h, s, v; } ColorHSV; // Hue = 0.0f to 360.0f degrees, Saturation = 0.0f grayscale to 1.0f color, Value/Brightness = 0.0f to 1.0f
typedef struct ColorYPQA { unsigned char y, p, q, a; } ColorYPQA; // Luminance (0-255), Phase (0-255), Quadrature (0-255), Alpha (0-255)
typedef struct ColorRGBA { unsigned char r, g, b, a; } ColorRGBA;
typedef ColorRGBA Color;

typedef union Vector2 {
    struct { float x, y; };
    float raw[2];
} Vector2;

typedef union Vector3 {
    struct { float x, y, z; };
    struct { float r, g, b; };
    float raw[3];
} Vector3;

typedef union Vector4 {
    struct { float x, y, z, w; };
    struct { float r, g, b, a; };
    float raw[4];
} Vector4;

typedef struct SitRectangle { float x, y, width, height; } SitRectangle;

/**
 * @brief MIDI note to Frequency hz table
 */
static const float SITUATION_MIDI_NOTE_FREQUENCY[128] = {
    8.1758f,   8.66196f,  9.17702f,  9.72272f,  10.3009f,  10.9134f,  11.5623f,  12.2499f,  12.9783f,  13.75f,    14.5676f,  15.4339f,
    16.3516f,  17.3239f,  18.3540f,  19.4454f,  20.6017f,  21.8268f,  23.1247f,  24.4997f,  25.9565f,  27.5f,     29.1352f,  30.8677f,
    32.7032f,  34.6478f,  36.7081f,  38.8909f,  41.2034f,  43.6535f,  46.2493f,  48.9994f,  51.9131f,  55.0f,     58.2705f,  61.7354f,
    65.4064f,  69.2957f,  73.4162f,  77.7817f,  82.4069f,  87.3071f,  92.4986f,  97.9989f,  103.826f,  110.0f,    116.541f,  123.471f,
    130.813f,  138.591f,  146.832f,  155.563f,  164.814f,  174.614f,  184.997f,  195.998f,  207.652f,  220.0f,    233.082f,  246.942f,
    261.626f,  277.183f,  293.665f,  311.127f,  329.628f,  349.228f,  369.994f,  391.995f,  415.305f,  440.0f,    466.164f,  493.883f,
    523.251f,  554.365f,  587.330f,  622.254f,  659.255f,  698.456f,  739.989f,  783.991f,  830.609f,  880.0f,    932.328f,  987.767f,
    1046.50f,  1108.73f,  1174.66f,  1244.51f,  1318.51f,  1396.91f,  1479.98f,  1567.98f,  1661.22f,  1760.0f,   1864.66f,  1975.53f,
    2093.00f,  2217.46f,  2349.32f,  2489.02f,  2637.02f,  2793.83f,  2959.96f,  3135.96f,  3322.44f,  3520.0f,   3729.31f,  3951.07f,
    4186.01f,  4434.92f,  4698.64f,  4978.03f,  5274.04f,  5587.65f,  5919.91f,  6271.93f,  6644.88f,  7040.0f,   7458.62f,  7902.13f,
    8372.02f,  8869.84f,  9397.27f,  9956.06f,  10548.1f,  11175.3f,  11839.8f,  12543.9f
};
//==================================================================================
//  Callback Type Definitions - v2.3.4 "Velocity" Standard
//==================================================================================
//
//  All Situation callbacks are designed to be:
//   • Invoked exclusively from the main thread (except audio capture when the SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD flag is NOT set — then it runs on the audio thread)
//   • Real-time safe where applicable (especially audio-related callbacks)
//   • Zero-overhead (no virtual calls, no hidden allocations)
//   • Fully user-data driven (last parameter is always the pointer you supplied)
//
//  Never call other SITAPI functions from inside a callback unless the documentation for that specific callback explicitly declares it safe.
//
//  These signatures are frozen — they will never change in any future version.
//

// ── Window / OS Events ─────────────────────────────────────────────────────
typedef void (*SituationFileDropCallback)(
    int          count,       // Number of files dropped
    const char** paths,       // Array of UTF-8 file paths (valid only for duration of callback)
    void*        user_data
); // Files/folders dragged from OS onto the window

typedef void (*SituationFileLoadCallback)(
    void*        data,        // The loaded file data (or NULL on failure). OWNED BY CALLER (must free).
    size_t       size,        // Size of the loaded data in bytes.
    void*        user_data    // User data passed to the async function.
); // Callback for asynchronous file loading

typedef void (*SituationFileSaveCallback)(
    bool         success,     // true if the file was written successfully.
    void*        user_data    // User data passed to the async function.
); // Callback for asynchronous file saving

typedef void (*SituationFileTextLoadCallback)(
    char*        text,        // The loaded null-terminated string (or NULL on failure). OWNED BY CALLER (must free).
    void*        user_data    // User data passed to the async function.
); // Callback for asynchronous text file loading

typedef void (*SituationFocusCallback)(
    bool   gained_focus,      // true = window gained focus, false = lost focus
    void*  user_data
); // Window focus change (alt-tab, click, etc.)

typedef void (*SituationWindowCloseCallback)(
    void* user_data
); // User clicked the OS close button. Rarely used — most code just polls SituationWindowShouldClose()

// ── Input Events (Optional Event-Driven API — polling API is always available) ──
typedef void (*SituationKeyCallback)(
    int   key,       // SIT_KEY_xxx code
    int   scancode,  // Platform-specific scancode (useful for non-QWERTY layouts)
    int   action,    // SIT_PRESS, SIT_RELEASE, or SIT_REPEAT
    int   mods,      // Bitfield of SIT_MOD_xxx
    void* user_data
); // Exact GLFW key callback signature

typedef void (*SituationCharCallback)(
    unsigned int codepoint,   // UTF-32 codepoint (valid Unicode character)
    void*        user_data
); // Text input (separate from key events — handles IME, dead keys, etc.)

typedef void (*SituationMouseButtonCallback)(
    int   button,    // SIT_MOUSE_BUTTON_1 to 8
    int   action,    // SIT_PRESS or SIT_RELEASE
    int   mods,      // Modifier bitfield
    void* user_data
); // Mouse button events

typedef void (*SituationCursorPosCallback)(
    Vector2 position, // Cursor position in screen coordinates (HiDPI-aware, sub-pixel precision)
    void*   user_data
); // Called every time the mouse moves (can be very frequent)

typedef void (*SituationScrollCallback)(
    Vector2 offset,   // x/y scroll amount (y is usually ±1.0 per notch)
    void*   user_data
); // Mouse wheel / trackpad scroll

typedef void (*SituationJoystickCallback)(
    int  jid,        // Joystick ID (0 to SITUATION_MAX_JOYSTICKS-1)
    int  event,      // GLFW_CONNECTED or GLFW_DISCONNECTED
    void* user_data
); // Gamepad/controller hotplug events

// ── Custom Audio Streaming (Exact MiniAudio vtable signatures — required for perfect compatibility) ──
typedef ma_uint64 (*SituationStreamReadCallback)(
    void*      pUserData,    // Your custom stream context pointer
    void*      pBufferOut,   // Buffer to fill with PCM data
    ma_uint64  bytesToRead   // Maximum bytes to write
); // Return number of bytes actually written

typedef ma_result (*SituationStreamSeekCallback)(
    void*       pUserData,   // Your custom stream context pointer
    ma_int64    byteOffset,  // Offset in bytes
    ma_seek_origin origin    // MA_SEEK_WHENCE_SOF/COF/EOF
); // Return MA_SUCCESS on success

// ── Audio Capture (Microphone / Line-In) ─────────────────────────────────────
typedef void (*SituationAudioCaptureCallback)(
    const float* input_buffer,   // Interleaved 32-bit float samples (read-only!)
    uint32_t     frame_count,    // Number of frames in this block (typically 256–1024)
    void*        user_data
); // Called from audio thread unless SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD is set
   // Format is always engine native: 48 kHz (or custom, stereo (or mono), interleaved floats

// ── Custom DSP Processor Chain (Per-Sound Post-Effects) ─────────────────────
typedef void (*SituationAudioProcessorCallback)(
    float*       buffer,         // Interleaved float samples — read/write in-place
    uint32_t     frames,         // Number of frames in this block
    uint32_t     channels,       // 1 (mono) or 2 (stereo)
    uint32_t     sampleRate,     // Current engine sample rate in Hz
    void*        user_data 		 // Pointer supplied when the processor was added
); // Applied after built-in effects (filter → echo → reverb) but before final volume/pan
   // Must be real-time safe — no SIT_MALLOC, no locking, no system calls

// ── Internal GLFW Error Callback (Exposed only for extremely advanced users) ──
typedef void (*GLFWerrorfun)(int error_code, const char* description);

SITAPI void SituationFreeString(char* str);

#if defined(__cplusplus)
extern "C++" {
/**
 * @brief RAII Wrapper for Situation Strings (C++ only).
 * @details Automatically calls SituationFreeString() when it goes out of scope.
 */
struct SituationScopedString {
    char* str;
    SituationScopedString(char* s) : str(s) {}
    ~SituationScopedString() { if(str) SituationFreeString(str); }
    operator const char*() const { return str; }
    const char* get() const { return str; }
    // Prevent copy
    SituationScopedString(const SituationScopedString&) = delete;
    SituationScopedString& operator=(const SituationScopedString&) = delete;
    // Allow move
    SituationScopedString(SituationScopedString&& other) noexcept : str(other.str) { other.str = NULL; }
    SituationScopedString& operator=(SituationScopedString&& other) noexcept {
        if (this != &other) {
            if (str) SituationFreeString(str);
            str = other.str;
            other.str = NULL;
        }
        return *this;
    }
};
}
#endif


/**
 * @brief Specifies the color encoding of image data.
 * 
 * This enum describes whether pixel data is stored in linear or SRGB color space.
 * The encoding affects how the data should be interpreted when creating GPU textures.
 * 
 * **SITUATION_COLOR_LINEAR:**
 * - Data is in linear color space with no gamma encoding
 * - Required for storage images (textures writable by compute shaders)
 * - No automatic gamma correction applied during sampling
 * - Maps to UNORM formats:
 *   - Vulkan: VK_FORMAT_R8G8B8A8_UNORM
 *   - OpenGL: GL_RGBA8
 * 
 * **SITUATION_COLOR_SRGB:**
 * - Data is in SRGB color space with gamma 2.2 encoding
 * - Preferred for sampled-only textures (photos, UI elements, etc.)
 * - Automatic gamma correction applied when sampled in shaders
 * - Cannot be used with storage images on most GPUs
 * - Maps to SRGB formats:
 *   - Vulkan: VK_FORMAT_R8G8B8A8_SRGB
 *   - OpenGL: GL_SRGB8_ALPHA8
 * 
 * @note When creating textures with SITUATION_TEXTURE_USAGE_STORAGE flag, LINEAR encoding
 *       must be used. SRGB formats typically don't support storage image operations.
 * @note For sampled-only textures, SRGB encoding is preferred for proper gamma correction
 *       and color accuracy on standard displays.
 * 
 * @since v2.3.40
 */
typedef enum SituationColorEncoding {
    SITUATION_COLOR_LINEAR = 0,     // Linear color space - required for storage images (both OpenGL and Vulkan)
    SITUATION_COLOR_SRGB = 1        // SRGB color space with gamma encoding - for sampled textures (both OpenGL and Vulkan)
} SituationColorEncoding;
typedef struct SituationImage {
    void *data;                                     // Image raw data
    int width;                                      // Image width
    int height;                                     // Image height
    int channels;                                   // Number of channels (e.g., 4 for RGBA, 1 for Grayscale)
    SituationColorEncoding color_encoding;      // Color space encoding (LINEAR or SRGB)
} SituationImage;

/**
 * @brief Specifies the type of flip operation to perform on an image.
 */
typedef enum SituationImageFlipMode {
    SIT_FLIP_VERTICAL,                              // Flips the image top-to-bottom.
    SIT_FLIP_HORIZONTAL,                            // Flips the image left-to-right.
    SIT_FLIP_BOTH                                   // Flips both vertically and horizontally (180-degree rotation).
} SituationImageFlipMode;


/**
 * @brief Renderer Abstraction
 */
typedef enum {
    SIT_RENDERER_OPENGL,
    SIT_RENDERER_VULKAN
} SituationRendererType;

/**
 * @brief Defines a set of common, pre-configured layouts for compute pipelines.
 * @details This enum is passed to SituationCreateComputePipeline* to select an appropriate
 *          VkPipelineLayout that matches the resources declared in the compute shader.
 */
typedef enum {
    SIT_COMPUTE_LAYOUT_ONE_SSBO,                    // A layout for shaders that use one Shader Storage Buffer Object (SSBO) at set 0.
    SIT_COMPUTE_LAYOUT_TWO_SSBOS,                   // A layout for shaders that use two SSBOs at sets 0 and 1.
    SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO,              // A layout for shaders that use one Storage Image at set 0 and one SSBO at set 1.
    SIT_COMPUTE_LAYOUT_PUSH_CONSTANT,               // A layout for shaders that use a 64-byte push constant for small data.
    SIT_COMPUTE_LAYOUT_EMPTY,                       // A layout for simple shaders that take no external resources.
    SIT_COMPUTE_LAYOUT_BUFFER_IMAGE,                // A layout for shaders that use one SSBO (Set 0) and one Storage Image (Set 1).
    SIT_COMPUTE_LAYOUT_TERMINAL,
    SIT_COMPUTE_LAYOUT_VECTOR,
} SituationComputeLayoutType;

/**
 * @brief Flags for texture creation (used in SituationCreateTextureEx)
 */
typedef enum {
    SITUATION_TEXTURE_USAGE_SAMPLED         = 1 << 0, // Standard texture
    SITUATION_TEXTURE_USAGE_STORAGE         = 1 << 1, // Can be used with imageStore/Compute
    SITUATION_TEXTURE_USAGE_TRANSFER_SRC    = 1 << 2, // Can be copied from
    SITUATION_TEXTURE_USAGE_TRANSFER_DST    = 1 << 3, // Can be copied to
    SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED = 1 << 4  // Will be sampled (read-only) in compute shaders
} SituationTextureUsageFlags;

/**
 * @brief Opaque handle for a command buffer
 */
#ifdef SITUATION_USE_VULKAN
typedef VkCommandBuffer SituationCommandBuffer;
#else
typedef struct SituationCommandBuffer_t* SituationCommandBuffer;
#endif


/**
 * @brief Device Information Structures
 */
typedef struct {
    char cpu_name[SITUATION_MAX_CPU_NAME_LEN];
    int cpu_cores;
    float cpu_clock_speed_ghz;
    char gpu_name[SITUATION_MAX_GPU_NAME_LEN];
    uint64_t gpu_dedicated_memory_bytes;            // Primarily via DXGI on Windows
    uint64_t total_ram_bytes;
    uint64_t available_ram_bytes;
    int storage_device_count;
    char storage_device_names[SITUATION_MAX_STORAGE_DEVICES][SITUATION_MAX_DEVICE_NAME_LEN];
    uint64_t storage_capacity_bytes[SITUATION_MAX_STORAGE_DEVICES];
    uint64_t storage_free_bytes[SITUATION_MAX_STORAGE_DEVICES];
    int network_adapter_count;
    char network_adapter_names[SITUATION_MAX_NETWORK_ADAPTERS][SITUATION_MAX_DEVICE_NAME_LEN];
    int input_device_count;
    char input_device_names[SITUATION_MAX_INPUT_DEVICES][SITUATION_MAX_DEVICE_NAME_LEN];
    int display_count;
    char display_names[SITUATION_MAX_MONITORS][SITUATION_MAX_MONITOR_NAME_LEN];
    int display_widths[SITUATION_MAX_MONITORS];
    int display_heights[SITUATION_MAX_MONITORS];
    int display_refresh_rates[SITUATION_MAX_MONITORS];
} SituationDeviceInfo;

/**
 * @brief Physical Display Management Structures
 */
typedef struct {
    int width;
    int height;
    int refresh_rate;
    int color_depth;                                // Can be tricky to get reliably for all modes/APIs
} SituationDisplayMode;

typedef struct {
    char name[SITUATION_MAX_MONITOR_NAME_LEN];      // Win32 device name
    int situation_monitor_id;                       // Internal ID, corresponds to index in cached_physical_displays_array
    GLFWmonitor* glfw_monitor_handle;               // Corresponding GLFW monitor handle, if matched
    bool is_primary;
    SituationDisplayMode current_mode;
    SituationDisplayMode* available_modes;          // Caller must free
    int available_mode_count;
} SituationDisplayInfo;

/**
 * @brief Defines standard system cursor shapes.
 */
typedef enum {
    SIT_CURSOR_DEFAULT = 0,                         // The default, platform-specific arrow
    SIT_CURSOR_ARROW,                               // A standard arrow cursor
    SIT_CURSOR_IBEAM,                               // The text input I-beam
    SIT_CURSOR_CROSSHAIR,                           // A crosshair for targeting
    SIT_CURSOR_HAND,                                // A pointing hand, for links or buttons
    SIT_CURSOR_HRESIZE,                             // Horizontal resize arrow (e.g., <->)
    SIT_CURSOR_VRESIZE                              // Vertical resize arrow (e.g., ^ v)
} SituationCursor;

/**
 * @brief Defines the color blending mode for a virtual display during compositing.
 * @details These modes determine how a virtual display's texture is drawn onto the main framebuffer.
 */
typedef enum {
    // --- Standard & Simple Modes ---
    SITUATION_BLEND_ALPHA,                          // Default alpha blending. Final = Src * SrcAlpha + Dst * (1 - SrcAlpha). Ideal for UI.
    SITUATION_BLEND_ADDITIVE,                       // Brightening blend (Src + Dst). Black is transparent. Good for glows, sparks.
    SITUATION_BLEND_MULTIPLY,                       // Darkening blend (Src * Dst). White is transparent. Good for shadows, tinting.
    SITUATION_BLEND_SCREEN,                         // Brightening blend, less harsh than additive. Inverts, multiplies, and inverts again.
    SITUATION_BLEND_NONE,                           // Opaque blend (Final = Src). Ignores alpha and overwrites destination.

    // --- Photoshop-Style Blend Modes (require custom shader) ---
    SITUATION_BLEND_OVERLAY,                        // Combines Multiply and Screen. Preserves highlights and shadows of the destination.
    SITUATION_BLEND_SOFT_LIGHT,                     // Darkens or lightens, depending on source color. A softer version of Hard Light.
    SITUATION_BLEND_HARD_LIGHT,                     // Combines Multiply and Screen based on source color. A harsher version of Overlay.
    SITUATION_BLEND_COLOR_DODGE,                    // Brightens the destination color to reflect the source color.
    SITUATION_BLEND_COLOR_BURN,                     // Darkens the destination color to reflect the source color.
    SITUATION_BLEND_DARKEN,                         // Selects the darker of the source and destination pixels.
    SITUATION_BLEND_LIGHTEN,                        // Selects the lighter of the source and destination pixels.
    SITUATION_BLEND_DIFFERENCE,                     // Subtracts the darker color from the lighter color. Black shows no change.
    SITUATION_BLEND_EXCLUSION,                      // Similar to Difference but with lower contrast.
} SituationBlendMode;

/**
 * @brief Defines the scaling and filtering method for a virtual display.
 */
typedef enum {
    // @brief Smoothly stretches the VD to fill its defined rectangle (via offset/resolution).
    // Ignores aspect ratio. Uses GL_LINEAR filtering (blurry). Good for high-res UI.
    SITUATION_SCALING_STRETCH,

    // @brief Sharp, aspect-correct scaling that fills the screen as much as possible.
    // Uses GL_NEAREST filtering (sharp). This is your requested "sharp stretch" mode.
    // This will leave minimal black bars (letterbox/pillarbox).
    SITUATION_SCALING_FIT,

    // @brief Sharp, aspect-correct, integer-only scaling.
    // Guarantees all game pixels are perfect squares on screen, but may leave larger black bars.
    // Uses GL_NEAREST filtering (sharp). This is the "pixel perfect" purist mode.
    SITUATION_SCALING_INTEGER

} SituationScalingMode;

/** @brief Specifies how an attachment's contents should be treated at the start of a render pass. */
typedef enum {
    SIT_LOAD_OP_LOAD,       // Preserve the existing contents of the attachment.
    SIT_LOAD_OP_CLEAR,      // Clear the attachment to a specified value.
    SIT_LOAD_OP_DONT_CARE   // The existing contents are undefined and can be discarded.
} SituationAttachmentLoadOp;

/** @brief Specifies how an attachment's contents should be treated at the end of a render pass. */
typedef enum {
    SIT_STORE_OP_STORE,     // The rendered contents will be stored in memory for later access.
    SIT_STORE_OP_DONT_CARE  // The rendered contents are not needed after the pass and can be discarded.
} SituationAttachmentStoreOp;

/** @brief Defines the clear values for color and depth/stencil attachments. */
typedef struct {
    ColorRGBA color;
    float     depth;
    uint32_t  stencil;
} SituationClearValue;

/** @brief Configuration for a single attachment (color or depth) in a render pass. */
typedef struct {
    SituationAttachmentLoadOp  loadOp;
    SituationAttachmentStoreOp storeOp;
    SituationClearValue        clear;
} SituationAttachmentInfo;

/** @brief Complete configuration for beginning a render pass. */
typedef struct {
    int                     display_id;     // The render target (-1 for main window, >= 0 for a Virtual Display).
    SituationAttachmentInfo color_attachment;
    SituationAttachmentInfo depth_attachment;
} SituationRenderPassInfo;

/**
 * @brief Window State Management
 * @details These flags are now custom defines, their values are arbitrary but must be unique bits. Their functionality will be mapped to GLFW operations.
 */
#define SITUATION_FLAG_WINDOW_TOPMOST           0x00000001  // GLFW_FLOATING
#define SITUATION_FLAG_WINDOW_HIDDEN            0x00000002  // glfwHideWindow/ShowWindow
#define SITUATION_FLAG_WINDOW_FROZEN            0x00000004  // Conceptual, app-defined
#define SITUATION_FLAG_FULLSCREEN_MODE          0x00000008  // glfwSetWindowMonitor
#define SITUATION_FLAG_WINDOW_UNDECORATED       0x00000010  // GLFW_DECORATED attribute
#define SITUATION_FLAG_WINDOW_ALWAYS_RUN        0x00000020  // No direct GLFW equivalent, typically always runs when visible
#define SITUATION_FLAG_WINDOW_MINIMIZED         0x00000040  // glfwIconifyWindow, GLFW_ICONIFIED attribute
#define SITUATION_FLAG_WINDOW_MAXIMIZED         0x00000080  // glfwMaximizeWindow, GLFW_MAXIMIZED attribute
#define SITUATION_FLAG_WINDOW_UNFOCUSED         0x00000100  // Queryable state (GLFW_FOCUSED), not settable
#define SITUATION_FLAG_WINDOW_RESIZABLE         0x00000200  // GLFW_RESIZABLE attribute
#define SITUATION_FLAG_BORDERLESS_WINDOWED_MODE 0x00000400  // Achieved via undecorated + specific size/pos
#define SITUATION_FLAG_MSAA_4X_HINT             0x00000800  // glfwWindowHint(GLFW_SAMPLES, 4)
#define SITUATION_FLAG_VSYNC_HINT               0x00001000  // glfwSwapInterval(1)

typedef enum { // Enum can still use the defines for clarity in API
    SITUATION_WINDOW_STATE_ON_TOP         = SITUATION_FLAG_WINDOW_TOPMOST,
    SITUATION_WINDOW_STATE_HIDDEN         = SITUATION_FLAG_WINDOW_HIDDEN,
    SITUATION_WINDOW_STATE_FROZEN         = SITUATION_FLAG_WINDOW_FROZEN,
    SITUATION_WINDOW_STATE_FULLSCREEN     = SITUATION_FLAG_FULLSCREEN_MODE,
    SITUATION_WINDOW_STATE_UNDECORATED    = SITUATION_FLAG_WINDOW_UNDECORATED,
    SITUATION_WINDOW_STATE_ALWAYS_RUN     = SITUATION_FLAG_WINDOW_ALWAYS_RUN,
    SITUATION_WINDOW_STATE_MINIMIZED      = SITUATION_FLAG_WINDOW_MINIMIZED,
    SITUATION_WINDOW_STATE_MAXIMIZED      = SITUATION_FLAG_WINDOW_MAXIMIZED,
    // SITUATION_WINDOW_STATE_UNFOCUSED   // Removed as it's not a settable state directly
    SITUATION_WINDOW_STATE_RESIZABLE      = SITUATION_FLAG_WINDOW_RESIZABLE,
    SITUATION_WINDOW_STATE_BORDERLESS     = SITUATION_FLAG_BORDERLESS_WINDOWED_MODE,
    SITUATION_WINDOW_STATE_MSAA_4X_HINT   = SITUATION_FLAG_MSAA_4X_HINT,
    SITUATION_WINDOW_STATE_VSYNC_HINT     = SITUATION_FLAG_VSYNC_HINT
} SituationWindowStateFlags;

// In your global state or a new rendering state struct
typedef struct {
    mat4 view;
    mat4 projection;
    // Add other per-view data here like camera position
} ViewDataUBO;

#ifdef SITUATION_IMPLEMENTATION
#if defined(SITUATION_USE_VULKAN)
// Forward declare VMA types used in public-facing structs
struct VmaAllocation_T;
typedef struct VmaAllocation_T* VmaAllocation;
#endif
#endif

/**
 * @brief Opaque handle for a compute pipeline.
 * @details In OpenGL, this represents a linked shader program containing only a compute shader.
 */
typedef struct {
    uint64_t id;

#if defined(SITUATION_IMPLEMENTATION)
#if defined(SITUATION_USE_VULKAN)
    VkPipeline vk_pipeline;
    VkPipelineLayout vk_pipeline_layout;
#elif defined(SITUATION_USE_OPENGL)
    GLuint gl_program_id;
    uint64_t _pad[1]; 
#endif
#else
    // OPAQUE PADDING: Space for 2 handles
    uint64_t _internal_padding[2];
#endif
} SituationComputePipeline;



/**
 * @brief Opaque handle for a generic GPU data buffer (e.g., an SSBO).
 */
typedef struct {
    uint64_t id;
    size_t size_in_bytes;
    SituationBufferUsageFlags usage_flags;

#if defined(SITUATION_IMPLEMENTATION)
#if defined(SITUATION_USE_VULKAN)
    VkBuffer vk_buffer;
    VmaAllocation vma_allocation;
    VkBufferUsageFlags vk_usage_flags;
    VkDescriptorSet descriptor_set;
#elif defined(SITUATION_USE_OPENGL)
    GLuint gl_buffer_id;
    // [Phase 1] Ring Buffer Tracking
    uint64_t dynamic_offset;      // Offset in ring buffer for current frame
    uint32_t dynamic_frame_index; // Frame index when this offset was assigned
    uint32_t _pad[3];             // Adjusted padding (Total 24 bytes: 8+4+12)
#endif
#else
    // OPAQUE PADDING: Space for VkBuffer, VmaAllocation, Flags, DescriptorSet
    uint64_t _internal_padding[4]; 
#endif
} SituationBuffer;


// --- Cross-Platform Resource Validation Helpers ---


/**
 * @brief Represents a mesh of vertices and indices stored on the GPU.
 * @details This is an opaque handle to the underlying graphics resources (VBO/EBO/VAO for OpenGL, VkBuffers for Vulkan).
        The library manages the creation and destruction of these resources.
 */
typedef struct {
    uint64_t id; 
    int index_count;
    int vertex_count;       
    size_t vertex_stride;   

#if defined(SITUATION_IMPLEMENTATION)
#if defined(SITUATION_USE_VULKAN)
    VkBuffer vertex_buffer;
    VmaAllocation vertex_buffer_memory;
    VkBuffer index_buffer;
    VmaAllocation index_buffer_memory;
#elif defined(SITUATION_USE_OPENGL)
    GLuint vbo_id; 
    GLuint ebo_id; 
    uint64_t _pad[2];
#endif
#else
    // OPAQUE PADDING: Space for 4 Vulkan handles
    uint64_t _internal_padding[4];
#endif
} SituationMesh;

// Forward-declaration for the internal uniform map implementation struct.
// The full definition is hidden inside the SITUATION_IMPLEMENTATION block.
struct _SituationUniformMap;

// Enums for the 'type' parameter
typedef enum {
    SIT_UNIFORM_FLOAT,
    SIT_UNIFORM_VEC2,
    SIT_UNIFORM_VEC3,
    SIT_UNIFORM_VEC4,
    SIT_UNIFORM_INT,
    SIT_UNIFORM_IVEC2,
    SIT_UNIFORM_IVEC3,
    SIT_UNIFORM_IVEC4,
    SIT_UNIFORM_MAT4
} SituationUniformType;

// --- Shader Handle ---
typedef struct {
    uint64_t id; 

#if defined(SITUATION_IMPLEMENTATION)
#if defined(SITUATION_USE_OPENGL)
    GLuint gl_program_id; 
    struct _SituationUniformMap* uniform_map;
    uint64_t _pad[1]; // Pad to match Vulkan size
#elif defined(SITUATION_USE_VULKAN)
    VkPipeline vk_pipeline;
    VkPipeline vk_pipeline_legacy;
    VkPipelineLayout vk_pipeline_layout;
#endif
#else
    // OPAQUE PADDING: Space for 3 handles
    uint64_t _internal_padding[3];
#endif
} SituationShader;

/**
 * @brief Opaque handle for a generic GPU texture resource.
 * @details Uses an Indirect Handle system (Index + Generation) to allow safe hot-reloading.
 */
typedef struct {
    uint32_t slot_index;    // Index into the internal texture registry
    uint32_t generation;    // Validation ID to detect use-after-free
    int width;              // Cached metadata
    int height;             // Cached metadata
} SituationTexture;

/**
 * @brief Represents a single drawable part of a larger 3D model.
 * @details A model can be composed of multiple sub-meshes, each with its own material properties and GPU mesh resource.
 */
typedef struct SituationModelMesh {
    char name[SITUATION_MAX_DEVICE_NAME_LEN]; // Name of the mesh from the model file
    SituationMesh gpu_mesh;                   // The handle to the GPU vertex/index data

    // --- PBR Material Properties ---
    // These are loaded directly from the GLTF material definition.
    Vector4 base_color_factor;                // The base color tint (RGBA)
    float metallic_factor;                    // How metallic the surface is [0-1]
    float roughness_factor;                   // How rough the surface is [0-1]
    Vector3 emissive_factor;                  // The color of light emitted by the surface

    // --- Texture Handles ---
    // These point to textures that are also part of the model.
    SituationTexture base_color_texture;      // Albedo/Diffuse map
    SituationTexture metallic_roughness_texture; // Packed Metal (R), Rough (G) map
    SituationTexture normal_texture;          // Normal map
    SituationTexture occlusion_texture;       // Ambient Occlusion map
    SituationTexture emissive_texture;        // Emissive/Glow map
} SituationModelMesh;

/**
 * @brief Represents a complete 3D model, loaded from a file.
 * @details This is a container for all the meshes and materials that make up a model.
 *          It is the result of a call to SituationLoadModel.
 */
typedef struct SituationModel {
    uint64_t id;                              // A unique ID for the model object
    int mesh_count;                           // The number of sub-meshes in this model
    SituationModelMesh* meshes;               // A pointer to an array of this model's meshes

    // --- Resource Management (Internal) ---
#if defined(SITUATION_IMPLEMENTATION)
    // We need to track all textures loaded with this model so we can unload them properly.
    int texture_count;
    SituationTexture* all_model_textures;
#endif
} SituationModel;

// --- Virtual Display Structures ---
typedef struct {
    int      id;                     // Unique sequential ID assigned at creation (used internally for tracking)
    Vector2  resolution;             // Render resolution of this virtual display (width, height in pixels)
    Vector2  offset;                 // Top-left screen position when composited to the main window (in screen pixels)
    float    opacity;                // Global alpha multiplier for the entire display (0.0f = fully transparent, 1.0f = opaque)
    bool     visible;                // If false, the display is skipped entirely during compositing
    int      z_order;                // Sorting key for compositing order — lower values are drawn first (background → foreground)

    // ── Independent Timing & Animation System (allows retro slowdown, bullet-time, UI-independent speed, etc.) ──
    uint64_t frame_count;                // Number of frames this virtual display has advanced (independent of main window)
    double   frame_time_multiplier;      // Speed multiplier (1.0 = normal, 0.5 = half speed, 2.0 = double speed, etc.)
    double   elapsed_time_seconds;       // Total time this display has been running (affected by frame_time_multiplier)
    float    cycle_animation_value;      // Oscillating value 0.0..1.0..0.0 useful for cheap pulsing/shake effects
    double   last_update_time_seconds;   // Timestamp of the last frame advance (used for delta calculation)
    double   frame_delta_time_seconds;   // Delta time for this virtual display's last frame (affected by multiplier)

    // ── Optimization & Compositing Controls ──
    bool                    is_dirty;       // Set to true when content changed → forces re-render of the off-screen buffer
    SituationScalingMode    scaling_mode;   // How the VD is scaled when composited (Integer, Fit, Stretch, etc.)
    SituationBlendMode      blend_mode;     // Blending style when compositing (Alpha, Additive, Overlay, Soft Light, Screen Grab, etc.)

    // ── Backend-Specific GPU Resources (only compiled in the implementation file) ──
#if defined(SITUATION_IMPLEMENTATION)
#if defined(SITUATION_USE_VULKAN)
    struct {
        VkImage         image;               // Device-local color image
        VmaAllocation   image_memory;        // VMA allocation handle for the color image
        VkImageView     image_view;          // Color attachment view
        VkImage         depth_image;         // Depth-stencil image (if enabled)
        VmaAllocation   depth_image_memory;  // VMA allocation for depth image
        VkImageView     depth_image_view;    // Depth attachment view
        VkFramebuffer   framebuffer;         // Framebuffer that references the above images
        VkSampler       sampler;             // Sampler used when sampling this VD as a texture
        VkRenderPass    render_pass;         // Dedicated render pass (one per VD for maximum compatibility/flexibility)
        VkDescriptorSet descriptor_set;      // Pre-allocated descriptor set for ultra-fast compositing (Velocity era)
        VkDescriptorPool descriptor_pool;    // [FIX v2.3.27B]
    } vk;
#elif defined(SITUATION_USE_OPENGL)
    struct {
        GLuint fbo_id;          // Framebuffer Object ID
        GLuint texture_id;      // Color attachment texture (GL_TEXTURE_2D)
        GLuint depth_rbo_id;    // Renderbuffer Object for depth/stencil (optional but usually present)
    } gl;
#endif
#else
    // Opaque placeholder for backend data to ensure vd->gl.texture_id compiles
    // for code outside the implementation block, even if it can't be used.
    struct {
        uint64_t _internal_handle_1;
        uint64_t _internal_handle_2;
        uint64_t _internal_handle_3;
    } gl;
#endif
} SituationVirtualDisplay;

/**
 * @brief manage a loaded font
 */
typedef struct SituationFont {
    void *fontData;                                 // The raw data buffer of the .ttf file
    void *stbFontInfo;                              // A pointer to the stbtt_fontinfo struct

    // [NEW] GPU-side data for real-time rendering
    SituationTexture atlas_texture;
    void* glyph_info; // Pointer to stbtt_bakedchar array
    int atlas_width;
    int atlas_height;
    float font_height_pixels; // The size this atlas was baked at

    // [v2.3.38] Bitmap Font Support
    bool is_bitmap;
    const unsigned char* bitmap_data;
    int bitmap_width;   // Width of one character (e.g. 8)
    int bitmap_height;  // Height of one character (e.g. 8)
    int bitmap_count;   // Number of characters (e.g. 256)
} SituationFont;

// --- Audio Control Structures ---

// --- Audio Handle System ---
typedef uint64_t SituationSoundHandle;
#define SITUATION_NULL_HANDLE 0
#define SITUATION_MAX_LOADED_SOUNDS 1024

typedef struct {
    int sample_rate;
    int channels;
    int bit_depth;
} SituationAudioFormat;

typedef struct {
    char name[SITUATION_MAX_DEVICE_NAME_LEN];
    ma_device_id id;
    int situation_internal_id;
    bool is_default_playback;
    bool is_default_capture;
} SituationAudioDeviceInfo;

/**
 * @brief Strategy for loading audio data from disk.
 * @details This enum allows the user to control the trade-off between RAM usage and CPU/Disk latency.
 *
 * - **SITUATION_AUDIO_LOAD_AUTO:** The recommended default. Automatically selects the strategy based on file duration.
 *   Files shorter than ~10 seconds are fully decoded to RAM (safest for SFX). Longer files are streamed.
 * - **SITUATION_AUDIO_LOAD_FULL:** Forces the entire audio file to be decoded into a raw PCM buffer in RAM upon load.
 *   - *Pros:* Zero disk I/O during playback; impossible to stutter during gameplay; perfectly thread-safe.
 *   - *Cons:* Higher RAM usage. High load times for long music tracks.
 * - **SITUATION_AUDIO_LOAD_STREAM:** Forces the audio engine to read from the file on disk during playback.
 *   - *Pros:* Minimal RAM usage; instant load times.
 *   - *Cons:* Risk of audio stuttering if the OS disk cache misses or if the drive is busy (e.g., loading textures).
 */
typedef enum {
    SITUATION_AUDIO_LOAD_AUTO,   // Library decides based on file size (<10 sec -> RAM)
    SITUATION_AUDIO_LOAD_FULL,   // Force full decode to RAM (Safest, best for SFX)
    SITUATION_AUDIO_LOAD_STREAM  // Force disk streaming (Best for long Music)
} SituationAudioLoadMode;

typedef enum {
    SIT_WAVE_SINE,      // Pure tone
    SIT_WAVE_SQUARE,    // Retro/8-bit sound (has harmonics)
    SIT_WAVE_TRIANGLE,  // Mellow, flute-like
    SIT_WAVE_SAW,       // Harsh, string-like
    SIT_WAVE_NOISE      // White noise (for percussion/explosions)
} SituationWaveType;

typedef enum {
    SITUATION_FILTER_NONE,
    SITUATION_FILTER_LOWPASS,
    SITUATION_FILTER_HIGHPASS
} SituationFilterType;

// --- Sound Instance Structure (Hardened Audio Engine - v2.3.3C+) ---
typedef struct {
    ma_decoder                  decoder;                // Internal MiniAudio decoder (handles WAV, MP3, FLAC, OGG, etc.)
    // ── Preloaded RAM Buffer (Critical for stutter-free SFX playback) ──
    void*                       preloaded_data;         // Fully decoded PCM data in RAM when using SITUATION_AUDIO_LOAD_FULL/AUTO
    bool                        is_preloaded;           // True if sound is fully decoded to RAM (zero audio-thread disk I/O)

    // ── Data Conversion & Format Normalization ──
    ma_data_converter           converter;              // Converts source format → engine format (always f32, 48kHz stereo)
    bool                        is_initialized;         // True if decoder was successfully initialized
    bool                        converter_initialized; // True if data converter was successfully set up

    // ── Playback Behaviour ──
    bool                        is_looping;             // If true, sound restarts automatically when reaching end
    bool                        is_streamed;            // True if sound is disk-streamed (music) rather than fully preloaded (SFX)
    uint64_t                    cursor_frames;          // Current playback position in frames (updated by audio thread)
    uint64_t                    total_frames;           // Total length in frames (0 if streamed/unknown length)

    // ── Mixer Controls (per-instance) ──
    atomic_float               volume;                 // Linear volume multiplier (0.0f = silent, 1.0f = normal, >1.0f allowed)
    atomic_float               pan;                    // Stereo panning (-1.0f = full left, 0.0f = center, +1.0f = full right)
    atomic_float               pitch;                  // Playback speed/pitch shift (1.0f normal, 0.5f half-speed, 2.0f double-speed)
    float                       _internal_pitch_tracker; // Internal tracker to detect pitch changes on the audio thread

    // ── Custom Streaming Support (Instance-Specific Callbacks - Thread-Safe Design) ──
    // These are stored directly in the instance so each streamed sound can have its own callbacks/userdata.
    // The audio thread uses static thunks + pointer arithmetic to safely invoke them without global state.
    SituationStreamReadCallback stream_read_cb;         // User-provided read callback for custom streaming sources
    SituationStreamSeekCallback stream_seek_cb;         // User-provided seek callback (optional but recommended)
    void*                       stream_user_data;       // User pointer passed to the stream callbacks

    // ── Built-in Effects Chain (Applied in processing order: Filter → Filter → Echo → Reverb → Volume/Pan) ──
    struct {
        // Biquad Filter (Low-pass, High-pass, Band-pass, etc.)
        bool                    filter_enabled;         // Master enable for filter stage
        ma_biquad               biquad;                 // MiniAudio biquad instance
        SituationFilterType     filter_type;            // Current filter mode (LPF12, HPF12, etc.)
        float                   filter_cutoff_hz;       // Cutoff frequency in Hz
        float                   filter_q;               // Resonance/Q factor

        // Echo / Delay Effect
        bool                    echo_enabled;           // Master enable for echo stage
        ma_delay                delay;                  // MiniAudio delay line
        float                   echo_delay_sec;         // Delay time in seconds (typical range 0.1–1.0)
        float                   echo_feedback;          // Feedback amount (0.0–1.0, >0.9 gets intense)
        float                   echo_wet_mix;           // Wet/dry mix for delayed signal (0.0 = dry only, 1.0 = wet only)

        // Simple Plate Reverb
        bool                    reverb_enabled;         // Master enable for reverb stage
        void*                   reverb_state;           // Internal custom reverb state (opaque)
        float                   reverb_room_size;       // Simulated room size (0.0 small → 1.0 large hall)
        float                   reverb_damping;         // High-frequency damping (0.0.0 bright → 1.0 very damped)
        float                   reverb_wet_mix;         // Wet amount (0.0 = dry only)
        float                   reverb_dry_mix;         // Dry amount (usually kept at 1.0f)
    } effects;

    // ── Custom DSP Processor Chain (User-defined audio processing callbacks) ──
    SituationAudioProcessorCallback* processors;        // Dynamic array of user callbacks (applied in order)
    void**                      processor_user_data;    // Parallel array of user data pointers for each processor
    int                         processor_count;        // Number of active custom processors
} SituationSound;

// --- Resonance (Procedural Synthesis) ---
/**
 * @brief Handle for an actively playing procedural tone.
 *        Invalid/expired handle is 0.
 */
typedef uint32_t SituationToneHandle;  // 0 = invalid

/**
 * @brief Plays an extended procedural tone with full control.
 *
 * @param type          Waveform type (Sine, Square, Triangle, Saw, Noise)
 * @param frequency     Frequency in Hz (e.g., 440.0f). For noise: ignored (use 0.0f)
 * @param volume        Peak volume (0.0 to 1.0)
 * @param pan           Stereo panning (-1.0 left, 0.0 center, +1.0 right)
 * @param attack_sec    Attack time in seconds
 * @param decay_sec     Decay time in seconds
 * @param sustain_level Sustain volume level (0.0 to 1.0)
 * @param release_sec   Release time in seconds
 * @param hold_sec      Hold duration in seconds. Use -1.0f for infinite sustain (key down)
 *
 * @return Handle to the playing tone, or 0 if no voice available (polyphony limit)
 */
SITAPI SituationToneHandle SituationPlayToneEx(
    SituationWaveType type,
    float frequency,
    float volume,
    float pan,
    float attack_sec,
    float decay_sec,
    float sustain_level,
    float release_sec,
    float hold_sec
);

// --- Temporal Oscillator System (Global High-Precision Timing & Rhythm Engine) ---
// This subsystem powers the advanced "Temporal Oscillator" feature set — a deterministic,
// high-resolution metronome/beat-sync system capable of driving music-reactive events,
// gameplay rhythms, animation pulses, procedural sequencing, etc.
// All oscillators run on the same global timebase but can have independent periods and phases.

// --- Timer System Structures ---
#define SITUATION_MAX_OSCILLATORS 256
#define SITUATION_TIMER_GRID_PERIOD_EDGES 60.0
#define SITUATION_TIMER_GRIDILON 1.182940076

typedef struct {
    // ── Oscillator Configuration ──
    double   period_seconds[SITUATION_MAX_OSCILLATORS];     // Period of each oscillator in seconds (e.g. 0.5 = 120 BPM, 1/4 note)

    // ── Deterministic Pseudo-Random State (xoshiro256** derived - 256-bit state) ──
    // Used for repeatable "random" pulses, shakes, or procedural events that must stay perfectly in sync across runs/recordings
    uint64_t state_current[4];      // Current 256-bit RNG state (xoshiro256** algorithm)
    uint64_t state_previous[4];     // Previous frame state — enables perfect reverse playback or rewind debugging

    // ── Per-Oscillator Runtime State ──
    uint64_t trigger_count[SITUATION_MAX_OSCILLATORS];          // How many times this oscillator has fired since init (rolls over safely)
    double   next_trigger_time_seconds[SITUATION_MAX_OSCILLATORS]; // Absolute time when the next trigger is scheduled
    double   last_ping_time_seconds[SITUATION_MAX_OSCILLATORS];    // Time of the most recent trigger (for phase/duty queries)

    // ── Global Timebase ──
    double   current_system_time_seconds;   // Monotonically increasing high-resolution time (updated every frame via SituationUpdateTimers())

    // ── Initialization Guard ──
    bool     is_initialized;                // True after first call to SituationUpdateTimers()
} SituationTimerSystem;

// --- Initialization Configuration Structure (Passed to SituationInit) ---
typedef struct {
    // ── Window Creation Parameters ──
    int          window_width;              // Initial window width in screen coordinates
    int          window_height;             // Initial window height in screen coordinates
    const char*  window_title;              // Window title bar text (UTF-8)

    // ── Window State Flags (Applied via GLFW window hints or direct state changes) ──
    uint32_t     initial_active_window_flags;    // Flags when window has focus (e.g. SIT_WINDOW_BORDERLESS | SIT_WINDOW_VSYNC)
    uint32_t     initial_inactive_window_flags;  // Flags when window is unfocused (e.g. pause rendering or reduce refresh rate)

    // ── Vulkan-Specific Options ──
    bool         enable_vulkan_validation;       // Enable VK_LAYER_KHRONOS_validation (debug builds only - auto-disabled in release)
    bool         force_single_queue;             // Force shared compute/graphics queue (debug/compatibility)
    uint32_t     max_frames_in_flight;           // Override SITUATION_MAX_FRAMES_IN_FLIGHT (usually 2 or 3)

    // Optional: Provide custom Vulkan instance extensions (e.g. for VR, ray tracing, etc.)
    const char** required_vulkan_extensions;     // Array of extension names (null or empty = use defaults)
    uint32_t     required_vulkan_extension_count;// Length of the above array

    // ── Engine Feature Flags ──
    uint32_t     flags;  // Bitfield:
                         //   SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD → route mic capture callbacks to main thread
                         //   (future-proof expansion slot)

    // ── Audio Configuration ──
    uint32_t     max_audio_voices; // Max concurrent audio voices. 0 = Unlimited (Dynamic).

#if defined(SITUATION_ENABLE_RENDER_THREAD)
    int          render_thread_count; // Number of render threads to spawn (0 = Single Threaded)
    // [v2.3.22] Backpressure Policy
    // Determines behavior when the render queue is full (Depth >= Max Frames)
    // 0: Spin (Low Latency, High CPU), 1: Yield (Balanced), 2: Sleep (Low CPU)
    int          backpressure_policy;
#endif

    // [v2.3.34] Async I/O
    uint32_t     io_queue_capacity; // Size of the IO queue (Low Priority). Default: 1024.

    // [v2.3.37] I/O Configuration
    bool disable_io_thread;         // If true, runs I/O tasks on main thread (fallback)
    double hot_reload_poll_rate;    // Seconds between checks (default 0.5). 0 = disable.
} SituationInitInfo;

// [v2.3.22] Render Queue Backpressure Policies
typedef enum {
    SIT_RENDER_BACKPRESSURE_SPIN  = 0, // Busy-wait loop (Highest responsiveness, uses CPU)
    SIT_RENDER_BACKPRESSURE_YIELD = 1, // Yield thread slice (OS decides, good balance)
    SIT_RENDER_BACKPRESSURE_SLEEP = 2  // Sleep 1ms (Low CPU usage, worst latency)
} SituationRenderBackpressurePolicy;

// [v2.3.22] Opaque Render List Handle (Momentum)
typedef struct SituationRenderList_t* SituationRenderList;

/**
 * @brief Flags representing optional GPU capabilities and advanced feature sets.
 * @details Used with SituationIsFeatureSupported() to check runtime availability. These flags cover core
 *          rasterization features, compute capabilities, and next-generation rendering techniques.
 */
typedef enum {
    // ── Core Rasterization ──
    SIT_FEATURE_GEOMETRY_SHADER        = 1 << 0,  // Geometry shader support
    SIT_FEATURE_TESSELLATION_SHADER    = 1 << 1,  // Tessellation control/eval shaders
    SIT_FEATURE_WIDE_LINES             = 1 << 2,  // Lines with width > 1.0
    SIT_FEATURE_FILL_MODE_NON_SOLID    = 1 << 3,  // Wireframe/Point rendering (PolygonMode)
    SIT_FEATURE_SAMPLER_ANISOTROPY     = 1 << 4,  // Anisotropic texture filtering
    SIT_FEATURE_MULTI_VIEWPORT         = 1 << 5,  // Multiple viewports/scissors (e.g. for VR/Split-screen without multiple passes)

    // ── Compute & Precision ──
    SIT_FEATURE_COMPUTE_SHADER         = 1 << 6,  // Compute shader support (Standard in Vulkan, GL 4.3+)
    SIT_FEATURE_INT64                  = 1 << 7,  // 64-bit integer support in shaders (int64_t)
    SIT_FEATURE_FLOAT64                = 1 << 8,  // 64-bit float (double) support in shaders
    SIT_FEATURE_FLOAT16                = 1 << 9,  // 16-bit float (half) support for storage/arithmetic (performance/bandwidth optimization)
    SIT_FEATURE_SUBGROUP_OPERATIONS    = 1 << 10, // Subgroup/Wave intrinsics (ballot, shuffle, arithmetic)

    // ── Modern Memory Model (Bindless) ──
    SIT_FEATURE_BINDLESS_BUFFERS       = 1 << 11, // Buffer Device Address / GL_EXT_buffer_reference (Pointers in shaders)
    SIT_FEATURE_BINDLESS_TEXTURES      = 1 << 12, // Descriptor Indexing / Bindless Textures (Arrays of unbounded textures)

    // ── GPU-Driven Rendering ──
    SIT_FEATURE_DRAW_INDIRECT_COUNT    = 1 << 13, // DrawIndirectCount / MultiDrawIndirect with count buffer (GPU culling)
    SIT_FEATURE_MULTI_DRAW_INDIRECT    = 1 << 14, // Standard MultiDrawIndirect support

    // ── Advanced Rendering ──
    SIT_FEATURE_MESH_SHADER            = 1 << 15, // Mesh Shaders (NV/EXT) - Replaces vertex/geometry pipeline
    SIT_FEATURE_RAY_TRACING            = 1 << 16, // Hardware Ray Tracing (KHR_ray_tracing_pipeline / queries)
    SIT_FEATURE_VARIABLE_RATE_SHADING  = 1 << 17, // Variable Rate Shading (VRS) for performance optimization
    SIT_FEATURE_ATOMIC_FLOAT           = 1 << 18, // Atomic operations on floating point images/buffers

    // ── Asset Support ──
    SIT_FEATURE_TEXTURE_COMPRESSION_BC = 1 << 19, // Block Compression (BC1-BC7 / S3TC) support
    SIT_FEATURE_TEXTURE_COMPRESSION_ASTC = 1 << 20, // ASTC Compression support (Mobile/High-end)
    SIT_FEATURE_HDR_OUTPUT             = 1 << 21, // High Dynamic Range display output support (10-bit/16-bit swapchain)

} SituationRenderFeature;

/**
 * @brief Checks if a specific graphics feature is supported and enabled on the current hardware.
 * @param feature The feature flag to check.
 * @return true if the feature is available for use.
 */
SITAPI bool SituationIsFeatureSupported(SituationRenderFeature feature);


// --- Deprecated Barrier Flags (for SituationMemoryBarrier) ---
#define SITUATION_BARRIER_VERTEX_ATTRIB_ARRAY_BIT   0x00000001
#define SITUATION_BARRIER_ELEMENT_ARRAY_BIT         0x00000002
#define SITUATION_BARRIER_UNIFORM_BARRIER_BIT       0x00000004
#define SITUATION_BARRIER_TEXTURE_FETCH_BARRIER_BIT 0x00000008
#define SITUATION_BARRIER_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#define SITUATION_BARRIER_COMMAND_BARRIER_BIT       0x00000040
#define SITUATION_BARRIER_PIXEL_BUFFER_BARRIER_BIT  0x00000080
#define SITUATION_BARRIER_TEXTURE_UPDATE_BARRIER_BIT 0x00000100
#define SITUATION_BARRIER_BUFFER_UPDATE_BARRIER_BIT 0x00000200
#define SITUATION_BARRIER_FRAMEBUFFER_BARRIER_BIT   0x00000400
#define SITUATION_BARRIER_TRANSFORM_FEEDBACK_BARRIER_BIT 0x00000800
#define SITUATION_BARRIER_ATOMIC_COUNTER_BARRIER_BIT 0x00001000
#define SITUATION_BARRIER_SHADER_STORAGE_BARRIER_BIT 0x00002000
#define SITUATION_BARRIER_ALL_BARRIER_BITS          0xFFFFFFFF

// Aliases to match implementation usage
#define SITUATION_BARRIER_INDEX_BUFFER_BIT          SITUATION_BARRIER_ELEMENT_ARRAY_BIT
#define SITUATION_BARRIER_UNIFORM_BUFFER_BIT        SITUATION_BARRIER_UNIFORM_BARRIER_BIT
#define SITUATION_BARRIER_TEXTURE_FETCH_BIT         SITUATION_BARRIER_TEXTURE_FETCH_BARRIER_BIT
#define SITUATION_BARRIER_SHADER_IMAGE_ACCESS_BIT   SITUATION_BARRIER_SHADER_IMAGE_ACCESS_BARRIER_BIT
#define SITUATION_BARRIER_COMMAND_BIT               SITUATION_BARRIER_COMMAND_BARRIER_BIT
#define SITUATION_BARRIER_SHADER_STORAGE_BIT        SITUATION_BARRIER_SHADER_STORAGE_BARRIER_BIT

//==================================================================================
//  Core Data Types & GPU Resource Semantics - v2.3.4 "Velocity" Standard
//==================================================================================

// ---------------------------------------------------------------------------------
//  Vertex Attribute Data Types (used in SituationVertexAttribute layout descriptions)
// ---------------------------------------------------------------------------------
typedef enum {
    SIT_DATA_BYTE           = 0,  // 8-bit signed integer   (normalized possible)
    SIT_DATA_UNSIGNED_BYTE  = 1,  // 8-bit unsigned integer (normalized possible)
    SIT_DATA_SHORT          = 2,  // 16-bit signed integer
    SIT_DATA_UNSIGNED_SHORT = 3,  // 16-bit unsigned integer
    SIT_DATA_INT            = 4,  // 32-bit signed integer
    SIT_DATA_UNSIGNED_INT   = 5,  // 32-bit unsigned integer
    SIT_DATA_FLOAT          = 6,  // 32-bit IEEE floating point (default for most attributes)
    SIT_DATA_DOUBLE         = 7,  // 64-bit IEEE floating point (rare, only when explicitly needed)
} SituationDataType;

// ---------------------------------------------------------------------------------
//  Memory Allocation Macros (overridable for custom allocators)
// ---------------------------------------------------------------------------------
#ifndef SIT_MALLOC
    #define SIT_MALLOC(sz) malloc(sz)
#endif
#ifndef SIT_CALLOC
    #define SIT_CALLOC(n, sz) calloc(n, sz)
#endif
#ifndef SIT_REALLOC
#ifdef __cplusplus
    #define SIT_REALLOC(p, sz) reinterpret_cast<std::remove_reference<decltype(p)>::type>(realloc(p, sz))
#else
    #define SIT_REALLOC(p, sz) realloc(p, sz)
#endif
#endif
#ifndef SIT_FREE
    #define SIT_FREE(p) do { if (p) free(p); p = NULL; } while(0)
#endif

// ---------------------------------------------------------------------------------
//  Compute Shader Source Format Specification
//  Used when creating compute pipelines on backends that support multiple input formats.
// ---------------------------------------------------------------------------------
typedef enum {
    SITUATION_GL_SHADER_SOURCE_TYPE_GLSL   = 0,  // Null-terminated GLSL source string (compiled at runtime via shaderc when enabled)
    SITUATION_GL_SHADER_SOURCE_TYPE_SPIRV  = 1,  // Raw SPIR-V bytecode blob (uint32_t array) - used when pre-compiling offline
} SituationGLShaderSourceType;

// ---------------------------------------------------------------------------------
//  Pipeline Barrier Source Access Flags
//  Describes which previous pipeline stages have written to memory that later stages need to read.
//  Combine with bitwise OR.
// ---------------------------------------------------------------------------------
typedef enum {
    SITUATION_BARRIER_VERTEX_SHADER_WRITE   = 1 << 0,   // Vertex shader wrote to SSBO / image
    SITUATION_BARRIER_FRAGMENT_SHADER_WRITE  = 1 << 1,   // Fragment shader wrote to SSBO / image / color attachment
    SITUATION_BARRIER_COMPUTE_SHADER_WRITE   = 1 << 2,   // Compute shader wrote to storage buffer / image
    SITUATION_BARRIER_TRANSFER_WRITE         = 1 << 3,   // Copy/blit/fill operations wrote to buffer/image
} SituationBarrierSrcFlags;

// ---------------------------------------------------------------------------------
//  Pipeline Barrier Destination Access Flags
//  Describes which subsequent pipeline stages will read memory written by earlier stages.
//  Combine with bitwise OR.
// ---------------------------------------------------------------------------------
typedef enum {
    SITUATION_BARRIER_VERTEX_SHADER_READ     = 1 << 0,   // Vertex shader will read SSBO/image
    SITUATION_BARRIER_FRAGMENT_SHADER_READ    = 1 << 1,   // Fragment shader will read SSBO/image/color attachment
    SITUATION_BARRIER_COMPUTE_SHADER_READ     = 1 << 2,   // Compute shader will read storage buffer/image
    SITUATION_BARRIER_TRANSFER_READ           = 1 << 3,   // Copy/blit operations will read from buffer/image
    SITUATION_BARRIER_INDIRECT_COMMAND_READ   = 1 << 4,   // Indirect draw/dispatch buffer will be read by command processor
} SituationBarrierDstFlags;


//==================================================================================================
//
//  SITUATION API USAGE GUIDE - v2.3.4 "Velocity"
//
//  This is the canonical reference for correct usage of the Situation library.
//  Every rule here is deliberate and enforced for maximum performance, identical cross-backend
//  behaviour, and long-term stability in production applications.
//
//  Read this once. Then read it again. Then keep it open while you code.
//
//==================================================================================================

/**
 * @section Core Principles (Non-Negotiable)
 *
 * 1. Single-Threaded API
 *    All SITAPI functions (windowing, input polling, rendering, resource creation)
 *    MUST be called from the main thread that called SituationInit().
 *    Background threads may perform pure CPU work or prepare data, but never call the API directly.
 *
 * 2. Update-Before-Draw Contract (CRITICAL FOR BACKEND PARITY)
 *    You MUST update buffers / push constants / textures
 *    THEN you record draw commands.
 *    Never the other way around.
 *    In debug builds the library actively detects violations and aborts with a clear error.
 *    This guarantees pixel-identical results between OpenGL (immediate) and Vulkan (deferred).
 *
 * 3. Explicit Resource Ownership
 *    SituationCreate*  → must be paired with SituationDestroy*
 *    SituationLoad*    → must be paired with SituationUnload*
 *    SituationTakeScreenshot(), SituationGetLastErrorMsg(), SituationGetBasePath(), etc.
 *      → return heap-allocated data → caller must SIT_FREE() or SituationFreeString().
 *    SituationShutdown() performs leak detection and prints warnings for any GPU resource still alive.
 *
 * 4. Handle Pattern (by value) vs Modification (by pointer)
 *    - Use:    SituationCmdDrawMesh(mesh_handle);          // pass by value
 *    - Destroy: SituationDestroyMesh(&mesh_handle);        // pass by pointer → handle is zeroed
 *    This pattern is used everywhere and prevents use-after-free bugs.
 */

/**
 * @section Recommended Main Loop (Velocity-Era Standard)
 *
 * while (!SituationWindowShouldClose()) {
 *     SITUATION_BEGIN_FRAME();           // Polls input + updates timers in correct order
 *
 *     // ── Your Update Logic Here (physics, gameplay, audio triggers, hot-reload checks) ──
 *     SituationCheckHotReloads();        // Optional but highly recommended in development
 *
 *     if (SituationAcquireFrameCommandBuffer()) {
 *         SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
 *
 *         // ── Update GPU data first (buffers, push constants, descriptor binds) ──
 *
 *         // ── Then record draw commands ──
 *         SituationCmdBeginRenderPass(cmd, &main_pass_info);
 *         SituationCmdDrawMesh(cmd, my_mesh);
 *         SituationCmdDrawText(cmd, font, "Situation v2.3.4F", vec2(10,10), WHITE);
 *         SituationCmdEndRenderPass(cmd);
 *
 *         SituationEndFrame();               // Presents + GPU submit
 *     }
 * }
 *
 * This structure is now the official recommended pattern as of v2.3.4.
 */

/**
 * @section Hot-Reloading Workflow (The "Velocity" Killer Feature)
 *
 * In development builds:
 *   - Call SituationCheckHotReloads() once per frame (usually right after SITUATION_BEGIN_FRAME()).
 *   - Any shader, compute pipeline, texture, or GLTF model that was loaded with the normal
 *     SituationLoad* functions will automatically reload when the file on disk changes.
 *   - Original handles remain valid → no need to rebuild materials, UI, or scene graphs.
 *
 * This feature alone typically doubles or triples artist/programmer iteration speed.
 */

/**
 * @section Zero-Friction Features (They Just Work™)
 *
 *  SituationLoadSoundFromFile("music.mp3", SITUATION_AUDIO_LOAD_AUTO, true, &snd);
 *  SituationLoadTexture("tex.png", true, &tex);           // mips + hot-reload ready
 *  SituationTakeScreenshot("shot.png");            // always PNG, always works
 *  SituationLoadFont("font.ttf") → SituationBakeFontAtlas() → SituationCmdDrawText()
 *
 * No extra defines, no manual stb includes, no custom writers required.
 */

/**
 * @section Final Checklist Before Shipping
 *
 * [ ] Remove or #ifdef out SituationCheckHotReloads() in release builds
 * [ ] Disable validation layers (SituationInitInfo::enable_vulkan_validation = false)
 * [ ] Verify SituationShutdown() prints "No resource leaks detected"
 * [ ] Confirm no "update-after-draw" assertions in debug builds
 *
 * If you can tick all boxes, you have achieved Situation mastery.
 *
 * Welcome to the Titanium tier.
 */

//==================================================================================
// Core Module: Application Lifecycle and System
//==================================================================================

// --- Application Lifecycle & State ---
SITAPI const char* SituationGetVersionString(void); 									// Returns a read-only static string (e.g., "2.3.3A"). Do not free.
SITAPI SituationError SituationInit(int argc, char** argv, const SituationInitInfo* init_info); // Initialize the library, create window and graphics context.
SITAPI void SituationPollInputEvents(void);                                             // Poll for all input events (keyboard, mouse, joystick). Call once per frame.
SITAPI void SituationUpdateTimers(void);                                                // Update all internal timers (frame timer, temporal system). Call after polling events.
SITAPI void SituationUpdate(void);                                                      // DEPRECATED: Use SituationPollInputEvents() and SituationUpdateTimers().
SITAPI void SituationShutdown(void);                                                    // Shut down the library and release all resources.
SITAPI bool SituationIsInitialized(void);                                               // Check if the library has been successfully initialized.

// Query the current initialization state (thread-safe)
SITAPI SituationInitState SituationGetInitState(void);

SITAPI bool SituationWindowShouldClose(void);                                           // Check if the application should close (e.g., user clicked X).
SITAPI void SituationPauseApp(void);                                                    // Pause the application's internal state (e.g., audio).
SITAPI void SituationResumeApp(void);                                                   // Resume a paused application.
SITAPI bool SituationIsAppPaused(void);                                                 // Check if the application is currently paused.

// --- Frame Timing & FPS Management ---
SITAPI void SituationSetTargetFPS(int fps);                                             // Set a desired frame rate cap (0 for uncapped).
SITAPI float SituationGetFrameTime(void);                                               // Get the time in seconds for the last frame to complete (deltaTime).
SITAPI int SituationGetFPS(void);                                                       // Get the current frames-per-second value.

// --- Callbacks and Event Handling ---
SITAPI SituationError SituationGetLastErrorMsg(char** out_msg);                         // Get the last error message as a string (caller must free).
SITAPI void SituationSetExitCallback(void (*callback)(void* user_data), void* user_data); // Set a callback to run just before shutdown.
SITAPI void SituationSetResizeCallback(void (*callback)(int width, int height, void* user_data), void* user_data); // Set a callback for window framebuffer resize events.
SITAPI void SituationSetFocusCallback(SituationFocusCallback callback, void* user_data); // Set a callback for window focus events.
SITAPI void SituationSetFileDropCallback(SituationFileDropCallback callback, void* user_data); // Set a callback for file drop events.

// --- Command-Line Argument Queries ---
SITAPI bool SituationIsArgumentPresent(const char* arg_name);                           // Check if a command-line argument (e.g., "-server") was provided.
SITAPI const char* SituationGetArgumentValue(const char* arg_name);                     // Get the value of an argument (e.g., "jungle" from "-level:jungle").

// --- System & Hardware Information ---
SITAPI SituationDeviceInfo SituationGetDeviceInfo(void);                                // Get detailed information about system hardware (CPU, GPU, RAM, etc.).
SITAPI uint32_t SituationGetCPUThreadCount(void);                                       // Get the number of logical CPU cores.
SITAPI const char* SituationGetGPUName(void);											// Get the name of the active GPU.
SITAPI char* SituationGetUserDirectory(void);                                           // Get the full path to the current user's home directory (caller must free).
#if defined(_WIN32)
SITAPI char SituationGetCurrentDriveLetter(void);                                       // Get the drive letter of the running executable (Windows only).
SITAPI bool SituationGetDriveInfo(char drive_letter, uint64_t* out_total_capacity_bytes, uint64_t* out_free_space_bytes, char* out_volume_name, int volume_name_len); // Get info for a specific drive (Windows only).
#endif // _WIN32

SITAPI void SituationOpenFile(const char* filePath);                                    // Open a file or folder with its default application.
SITAPI int SituationExecuteCommand(const char *cmd, char **output);                     // Execute a shell command hidden, return exit code & combined output.

//==================================================================================
// Window and Display Module
//==================================================================================
// --- Window State Management ---
SITAPI void SituationSetWindowState(uint32_t flags);                                    // Set window configuration state using flags (additive).
SITAPI void SituationClearWindowState(uint32_t flags);                                  // Clear window configuration state flags.
SITAPI void SituationSetVSync(bool enable);                                             // Enable or disable VSync (vertical synchronization).
SITAPI void SituationToggleFullscreen(void);                                            // Toggle window between fullscreen and windowed mode.
SITAPI void SituationToggleBorderlessWindowed(void);                                    // Toggle window between borderless and decorated mode.
SITAPI void SituationMaximizeWindow(void);                                              // Maximize the window if it's resizable.
SITAPI void SituationMinimizeWindow(void);                                              // Minimize the window (iconify).
SITAPI void SituationRestoreWindow(void);                                               // Restore a minimized or maximized window.
SITAPI void SituationSetWindowFocused(void);                                            // Set the window to be focused.

// --- Window Property Management ---
SITAPI void SituationSetWindowTitle(const char *title);                                 // Set the title for the window.
SITAPI void SituationSetWindowIcon(SituationImage image);                               // Set the icon for the window (single image).
SITAPI void SituationSetWindowIcons(SituationImage *images, int count);                 // Set the icon for the window (multiple sizes).
SITAPI void SituationSetWindowPosition(int x, int y);                                   // Set the window position on the screen.
SITAPI void SituationSetWindowSize(int width, int height);                              // Set the window dimensions.
SITAPI void SituationSetWindowMinSize(int width, int height);                           // Set the window minimum dimensions.
SITAPI void SituationSetWindowMaxSize(int width, int height);                           // Set the window maximum dimensions.
SITAPI void SituationSetWindowOpacity(float opacity);                                   // Set window opacity [0.0f to 1.0f].

// --- Window State Queries ---
SITAPI bool SituationIsWindowState(uint32_t flag);                                      // Check if a specific window state flag is set.
SITAPI bool SituationIsWindowFullscreen(void);                                          // Check if the window is currently in fullscreen mode.
SITAPI bool SituationIsWindowHidden(void);                                              // Check if the window is currently hidden.
SITAPI bool SituationIsWindowMinimized(void);                                           // Check if the window is currently minimized.
SITAPI bool SituationIsWindowMaximized(void);                                           // Check if the window is currently maximized.
SITAPI bool SituationHasWindowFocus(void);                                              // Check if the window is currently focused.
SITAPI bool SituationIsWindowResized(void);                                             // Check if the window was resized in the last frame.

// --- Window & Screen Dimension Queries ---
SITAPI int SituationGetScreenWidth(void);                                               // Get the current logical width of the window.
SITAPI int SituationGetScreenHeight(void);                                              // Get the current logical height of the window.
SITAPI int SituationGetRenderWidth(void);                                               // Get the current render width (backbuffer size, considers HiDPI).
SITAPI int SituationGetRenderHeight(void);                                              // Get the current render height (backbuffer size, considers HiDPI).
SITAPI void SituationGetWindowSize(int* width, int* height);                            // Get the current logical window size.
SITAPI Vector2 SituationGetWindowPosition(void);                                        // Get the window's top-left position on the screen.
SITAPI Vector2 SituationGetWindowScaleDPI(void);                                        // Get the DPI scaling factor for the window.

// --- Physical Display (Monitor) Management ---
SITAPI int SituationGetMonitorCount(void);                                              // Get the number of connected monitors.
SITAPI int SituationGetCurrentMonitor(void);                                            // Get the index of the monitor the window is on.
SITAPI SituationError SituationGetDisplays(SituationDisplayInfo** out_displays, int* out_count); // Get information for all displays (caller must free).
SITAPI void SituationFreeDisplays(SituationDisplayInfo* displays, int count);
SITAPI void SituationRefreshDisplays(void);                                             // Force a refresh of the cached display information.
SITAPI SituationError SituationSetDisplayMode(int monitor_id, const SituationDisplayMode* mode, bool fullscreen); // Set the display mode for a monitor.
SITAPI void SituationSetWindowMonitor(int monitor_id);                                  // Set the window to be fullscreen on a specific monitor.
SITAPI const char* SituationGetMonitorName(int monitor_id);                             // Get the human-readable name of a monitor.
SITAPI int SituationGetMonitorWidth(int monitor_id);                                    // Get the width of a monitor's current video mode.
SITAPI int SituationGetMonitorHeight(int monitor_id);                                   // Get the height of a monitor's current video mode.
SITAPI int SituationGetMonitorPhysicalWidth(int monitor_id);                            // Get the physical width of a monitor in millimeters.
SITAPI int SituationGetMonitorPhysicalHeight(int monitor_id);                           // Get the physical height of a monitor in millimeters.
SITAPI int SituationGetMonitorRefreshRate(int monitor_id);                              // Get the refresh rate of a monitor.
SITAPI Vector2 SituationGetMonitorPosition(int monitor_id);                             // Get the top-left position of a monitor on the desktop.

// --- Cursor, Clipboard and File Drops ---
SITAPI void SituationSetCursor(SituationCursor cursor);                                 // Set the mouse cursor to a standard shape.
SITAPI void SituationShowCursor(void);                                                  // Show the mouse cursor.
SITAPI void SituationHideCursor(void);                                                  // Hide the mouse cursor.
SITAPI void SituationDisableCursor(void);                                               // Hide and lock the cursor, providing raw mouse motion.
SITAPI SituationError SituationGetClipboardText(const char** out_text);                 // Get text from the system clipboard.
SITAPI SituationError SituationSetClipboardText(const char* text);                      // Set text in the system clipboard.
SITAPI bool SituationIsFileDropped(void);                                               // Check if a file was dropped into the window this frame.
SITAPI char** SituationLoadDroppedFiles(int* count);                                    // Get the paths of dropped files (returns a copy, caller must free).
SITAPI void SituationUnloadDroppedFiles(char** paths, int count);                       // Unload the file path list returned by SituationLoadDroppedFiles.

// --- Advanced Window Profile Management ---
SITAPI SituationError SituationSetWindowStateProfiles(uint32_t active_flags, uint32_t inactive_flags); // Set the flag profiles for when the window is focused vs. unfocused.
SITAPI SituationError SituationApplyCurrentProfileWindowState(void);                    // Manually apply the appropriate window state profile based on current focus.
SITAPI SituationError SituationToggleWindowStateFlags(SituationWindowStateFlags flags_to_toggle); // Toggle flags in the current profile and apply the result.
SITAPI uint32_t SituationGetCurrentActualWindowStateFlags(void);                        // Gets flags based on current GLFW window state

//==================================================================================
// Image Module: CPU-side Image and Font Loading and Manipulation
//==================================================================================
// --- Image Loading and Unloading ---
SITAPI SituationError SituationLoadImage(const char *fileName, SituationImage* out_image);                         // Load an image from a file into CPU memory (RAM).
SITAPI SituationError SituationLoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, SituationImage* out_image); // Load an image from a memory buffer.
SITAPI void SituationUnloadImage(SituationImage image);                                 // Unload an image's pixel data from memory.
SITAPI bool SituationIsImageValid(SituationImage image);                                // Check if an image has been loaded successfully.

// --- Image Exporting ---
SITAPI SituationError SituationExportImage(SituationImage image, const char *fileName);           // Export image data to a file (PNG, BMP supported).

// --- Image Generation & Copying ---
SITAPI SituationError SituationImageCopy(SituationImage image, SituationImage* out_image);                         // Create a new image by copying another.

SITAPI SituationError SituationCreateImage(int width, int height, int channels, SituationImage* out_image);        // Allocates a new SituationImage container with UNINITIALIZED data.
SITAPI void SituationSetPixelColor(SituationImage *img, int x, int y, ColorRGBA col);   // Helper to set a specific pixel color (CPU-side).
SITAPI void SituationBlitRawDataToImage(SituationImage *dst, const void* data, int x, int y, int width, int height, int src_channels); // Copies raw byte data into a specific region of an image.

SITAPI void SituationImageDraw(SituationImage *dst, SituationImage src, SitRectangle srcRect, Vector2 dstPos); // Copying portion of one image into another image at destination placement
SITAPI void SituationImageDrawAlpha(SituationImage *dst, SituationImage src, SitRectangle srcRect, Vector2 dstPos, ColorRGBA tint);
SITAPI SituationError SituationGenImageColor(int width, int height, ColorRGBA color, SituationImage* out_image);   // Generate a new image of a solid color.
SITAPI SituationError SituationGenImageGradient(int width, int height, ColorRGBA tl, ColorRGBA tr, ColorRGBA bl, ColorRGBA br, SituationImage* out_image); // Generate a new image with a gradient.

// --- Image Manipulation (Modifies image in-place) ---
SITAPI void SituationImageCrop(SituationImage *image, SitRectangle crop);                  // Crop an image to a specific rectangle.
SITAPI void SituationImageResize(SituationImage *image, int newWidth, int newHeight);   // Resize an image using default bicubic scaling.
SITAPI void SituationImageFlip(SituationImage *image, SituationImageFlipMode mode);     // Flip an image.
SITAPI void SituationImageAdjustHSV(SituationImage *image, float hue_shift, float sat_factor, float val_factor, float mix);   // Control an image by Hue Saturation and Brightness.

// --- Font Management ---
SITAPI SituationError SituationLoadFont(const char *fileName, SituationFont* out_font);                         // Load a font from a TTF/OTF file for CPU rendering.
SITAPI SituationError SituationLoadFontFromMemory(const void* data, int dataSize, SituationFont* out_font);		// Loads a font directly from a memory buffer (e.g., embedded resource).
SITAPI SituationError SituationLoadBitmapFontFromMemory(const unsigned char* data, int char_width, int char_height, int num_chars, SituationFont* out_font); // Loads a raw bitmap font (e.g. 8x8 array).
SITAPI SituationError SituationBakeFontAtlas(SituationFont* font, float fontSizePixels);
SITAPI void SituationUnloadFont(SituationFont font);                                    // Unload a CPU-side font and free its memory.
SITAPI SitRectangle SituationMeasureText(SituationFont font, const char *text, float fontSize); // Measure the pixel dimensions of a string before drawing.
SITAPI void SituationImageDrawCodepoint(SituationImage *dst, SituationFont font, int codepoint, Vector2 position, float fontSize, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness); // Draw a single Unicode character with advanced styling onto an image.
SITAPI void SituationImageDrawText(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, ColorRGBA tint ); // Draw a simple, tinted text string onto an image.
SITAPI void SituationImageDrawTextEx(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness); // Draw a text string with advanced styling (rotation, outline) onto an image.
SITAPI void SituationImageDrawTextFormatted(SituationImage *dst, SituationFont font, Vector2 position, float fontSize, float spacing, ColorRGBA tint, const char* fmt, ...);

//==================================================================================
// Graphics Module: Rendering, Shaders, and GPU Resources
//==================================================================================

// --- Profiling & Diagnostics ---
SITAPI uint32_t SituationGetDrawCallCount(void); 										// Number of draw commands this frame
SITAPI uint64_t SituationGetVRAMUsage(void);     										// Total GPU memory allocated (Bytes)
SITAPI void SituationExportRenderHistogram(char* buf, size_t buf_size);
#if defined(SITUATION_ENABLE_RENDER_THREAD)
SITAPI size_t SituationGetRenderQueueDepth(void);                                       // Get the current depth of the render queue
SITAPI void SituationGetRenderLatencyStats(uint64_t* avg_ns, uint64_t* max_ns);         // Get render thread latency metrics
#endif

// [v2.3.37] I/O Metrics
SITAPI size_t SituationGetIOQueueDepth(void);                                           // [v2.3.37] Get the current depth of the IO/Low Priority queue

// [v2.3.23] Debug Overlay
SITAPI void SituationDrawMetricsOverlay(SituationCommandBuffer cmd, Vector2 position, ColorRGBA color); // Draws FPS, Latency, and Memory stats

// --- Frame Lifecycle & Command Buffer ---
SITAPI bool SituationAcquireFrameCommandBuffer(void);                                   // Prepare the backend for a new frame of rendering commands.
#if defined(SITUATION_ENABLE_THREADING)
SITAPI SituationJobId SituationSubmitRenderList(SituationThreadPool* pool, SituationRenderList list, void (*func)(void*, void*), void* user_data);
#else
SITAPI void SituationSubmitRenderList(SituationRenderList list, void (*func)(void*, void*), void* user_data);
#endif
SITAPI void SituationReplayRenderList(SituationCommandBuffer cmd, SituationRenderList list);
SITAPI void SituationResetRenderList(SituationRenderList list);
SITAPI SituationCommandBuffer SituationGetMainCommandBuffer(void);                      // Get the primary command buffer for the current frame.
SITAPI SituationCommandBuffer SituationGetComputeCommandBuffer(void);                   // [v2.3.23] Get the compute-specific command buffer (Vulkan only).
SITAPI SituationError SituationEndFrame(void);                                          // Submit all commands for the frame and present the result.

// --- Abstracted Rendering Commands ---
SITAPI SituationError SituationCmdSetViewport(SituationCommandBuffer cmd, float x, float y, float width, float height);                           // Sets the dynamic viewport and scissor for the current render pass.
SITAPI SituationError SituationCmdSetScissor(SituationCommandBuffer cmd, int x, int y, int width, int height);                                    // Sets the dynamic scissor rectangle to clip rendering.
SITAPI SituationError SituationCmdBindPipeline(SituationCommandBuffer cmd, SituationShader shader);                                     // Binds a graphics pipeline (shader program) for subsequent draws.
SITAPI SituationError SituationCmdDrawMesh(SituationCommandBuffer cmd, SituationMesh mesh);                                             // [High-Level] Records a command to draw a complete, pre-configured mesh.
SITAPI SituationError SituationCmdDrawQuad(SituationCommandBuffer cmd, mat4 model, Vector4 color);                                                // [High-Level] Record a command to draw a simple, colored 2D quad.
SITAPI SituationError SituationCmdDrawTexture(SituationCommandBuffer cmd, SituationTexture texture, SitRectangle source, SitRectangle dest, Vector2 origin, float rotation, ColorRGBA tint); // [High-Level] Draw a part of a texture defined by a rectangle.
SITAPI SituationError SituationCmdSetPushConstant(SituationCommandBuffer cmd, uint32_t contract_id, const void* data, size_t size);               // [Core] Set a small block of per-draw uniform data (push constant).
SITAPI SituationError SituationCmdBindDescriptorSet(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer);            // [Core] Binds a buffer's descriptor set (UBO/SSBO) to a set index.
SITAPI SituationError SituationCmdBindDescriptorSetDynamic(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer, uint32_t dynamic_offset); // [Core] Binds a dynamic buffer descriptor set with an offset.
SITAPI SituationError SituationCmdBindTextureSet(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture);             // [Core] Binds a texture's descriptor set (sampler/storage) to a set index.
SITAPI SituationError SituationCmdBindComputeTexture(SituationCommandBuffer cmd, uint32_t binding, SituationTexture texture);           // [Core] Binds a texture as a storage image for compute shaders.
SITAPI SituationError SituationCmdSetVertexAttribute(SituationCommandBuffer cmd, uint32_t location, int size, SituationDataType type, bool normalized, size_t offset); // [OpenGL Only] Define the format of a vertex attribute for the active VAO.
SITAPI SituationError SituationCmdDraw(SituationCommandBuffer cmd, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance); // [Core] Record a non-indexed draw call.
SITAPI SituationError SituationCmdDrawIndexed(SituationCommandBuffer cmd, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance); // [Core] Record an indexed draw call.
SITAPI SituationError SituationCmdBeginRenderPass(SituationCommandBuffer cmd, const SituationRenderPassInfo* info);                     // Begins a render pass with detailed configuration.
SITAPI SituationError SituationCmdEndRenderPass(SituationCommandBuffer cmd);                                                                      // Ends the current render pass.
SITAPI SituationError SituationCmdDrawText(SituationCommandBuffer cmd, SituationFont font, const char* text, Vector2 pos, ColorRGBA color);		// Draws a text string using GPU-accelerated textured quads.
SITAPI SituationError SituationCmdDrawTextEx(SituationCommandBuffer cmd, SituationFont font, const char* text, Vector2 pos, float fontSize, float spacing, ColorRGBA color); // Advanced text drawing (scaling/spacing).
SITAPI SituationError SituationCmdPresent(SituationCommandBuffer cmd, SituationTexture texture);                                                  // Submits a command to copy a texture to the main window's swapchain (Compute-Only).
SITAPI SituationError SituationCmdBindSampledTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture);                // Binds a texture as a sampled image (sampler2D) to a binding point.

// --- Graphics Resource Management ---
SITAPI SituationError SituationCreateMesh(const void* vertex_data, int vertex_count, size_t vertex_stride, const uint32_t* index_data, int index_count, SituationMesh* out_mesh); // Create a mesh from vertex and index data.
SITAPI void SituationDestroyMesh(SituationMesh* mesh);                                  // Unload a mesh from GPU memory.
SITAPI uint64_t SituationGetBufferDeviceAddress(SituationBuffer buffer);                // Retrieves the GPU device address of a buffer for bindless access.
SITAPI uint64_t SituationGetTextureHandle(SituationTexture texture);                    // Retrieves the bindless texture handle (OpenGL Only).

// --- Shader Management ---
SITAPI SituationError SituationLoadShader(const char* vs_path, const char* fs_path, SituationShader* out_shader);   // Load a graphics shader pipeline from vertex and fragment files.
SITAPI SituationError SituationLoadShaderFromMemory(const char* vs_code, const char* fs_code, SituationShader* out_shader); // Create a graphics shader pipeline from in-memory GLSL source.
SITAPI void SituationUnloadShader(SituationShader* shader);                             // Unload a graphics shader pipeline and free its GPU resources.

// --- Shader Interaction & Synchronization ---
SITAPI SituationError SituationSetShaderUniform(SituationShader shader, const char* uniform_name, const void* data, SituationUniformType type); // [OpenGL] Set a standalone uniform value by name (uses a cache).
SITAPI void SituationCmdPipelineBarrier(SituationCommandBuffer cmd, uint32_t src_flags, uint32_t dst_flags); // Insert a fine-grained pipeline barrier for synchronization.

// --- Texture Management ---
SITAPI SituationError SituationLoadTexture(const char* file_path, bool generate_mipmaps, SituationTexture* out_texture);// Loads a texture from disk and registers the path for hot-reloading.
SITAPI SituationError SituationCreateTexture(SituationImage image, bool generate_mipmaps, SituationTexture* out_texture); // Create a texture from a CPU-side image.
SITAPI SituationError SituationCreateTextureEx(SituationImage image, bool generate_mipmaps, SituationTextureUsageFlags flags, SituationTexture* out_texture); // Create a texture with specific usage flags.
SITAPI void SituationDestroyTexture(SituationTexture* texture);                         // Unload a texture from GPU memory.

// --- Compute Shader Pipeline ---
SITAPI SituationError SituationCreateComputePipeline(const char* compute_shader_path, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline); // Create a compute pipeline from a shader file.
SITAPI SituationError SituationCreateComputePipelineFromMemory(const char* compute_shader_source, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline); // Create a compute pipeline from in-memory GLSL source.
SITAPI void SituationDestroyComputePipeline(SituationComputePipeline* pipeline);        // Destroy a compute pipeline and free its GPU resources.
SITAPI void SituationCmdBindComputePipeline(SituationCommandBuffer cmd, SituationComputePipeline pipeline); // Bind a compute pipeline for a subsequent dispatch.
SITAPI void SituationCmdDispatch(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z); // Record a command to dispatch compute shader work groups.
SITAPI void SituationGetMaxComputeWorkGroups(uint32_t* x, uint32_t* y, uint32_t* z); // Query maximum compute work group count per dispatch.

// --- GPU Buffer Management ---
SITAPI SituationError SituationCreateBuffer(size_t size, const void* initial_data, SituationBufferUsageFlags usage_flags, SituationBuffer* out_buffer); // Create a generic GPU data buffer (e.g., SSBO).
SITAPI void SituationDestroyBuffer(SituationBuffer* buffer);                            // Destroy a GPU buffer.
SITAPI SituationError SituationUpdateBuffer(SituationBuffer buffer, size_t offset, size_t size, const void* data); // Update data in a GPU buffer.
SITAPI SituationError SituationGetBufferData(SituationBuffer buffer, size_t offset, size_t size, void* out_data); // Read data from a GPU buffer.

// --- Virtual Displays (Render Targets) ---
SITAPI SituationError SituationCreateVirtualDisplay(Vector2 resolution, double frame_time_mult, int z_order, SituationScalingMode scaling_mode, SituationBlendMode blend_mode, int* out_id); // Create an off-screen render target.
SITAPI SituationError SituationDestroyVirtualDisplay(int display_id);                   // Destroy a virtual display.
SITAPI SituationError SituationRenderVirtualDisplays(SituationCommandBuffer cmd);       // Composite all visible virtual displays to the current target.
SITAPI SituationError SituationConfigureVirtualDisplay(int display_id, Vector2 offset, float opacity, int z_order, bool visible, double frame_time_mult, SituationBlendMode blend_mode); // Configure a virtual display's properties.
SITAPI SituationVirtualDisplay* SituationGetVirtualDisplay(int display_id);             // Get a pointer to a virtual display's state.
SITAPI SituationError SituationSetVirtualDisplayScalingMode(int display_id, SituationScalingMode scaling_mode); // Set the scaling/filtering mode for a virtual display.
SITAPI void SituationSetVirtualDisplayDirty(int display_id, bool is_dirty);             // Mark a virtual display as needing to be re-rendered.
SITAPI bool SituationIsVirtualDisplayDirty(int display_id);                             // Check if a virtual display is marked as dirty.
SITAPI double SituationGetLastVDCompositeTimeMS(void);                                  // Get the time taken for the last virtual display composite pass.
SITAPI void SituationGetVirtualDisplaySize(int display_id, int* width, int* height);    // Get the internal resolution of a virtual display.

// --- 3D Model Utilities ---
SITAPI SituationError SituationLoadModel(const char* file_path, SituationModel* out_model); // Loads a complete 3D model and its textures from a GLTF file.
SITAPI void SituationUnloadModel(SituationModel* model);                                // Frees all GPU and CPU resources associated with a loaded model.
SITAPI void SituationDrawModel(SituationCommandBuffer cmd, SituationModel model, mat4 transform); // Draws all sub-meshes of a model with a single root transformation.
SITAPI bool SituationSaveModelAsGltf(SituationModel model, const char* file_path);      // Exports a model to a human-readable .gltf and a .bin file for debugging.
SITAPI void SituationGetMeshData(SituationMesh mesh, void** vertex_data, int* vertex_count, int* vertex_stride, void** index_data, int* index_count);

// --- Image & Screenshot Utilities ---
SITAPI SituationError SituationLoadImageFromScreen(SituationImage* out_image);          // Get a copy of the current screen backbuffer as an image.
SITAPI SituationError SituationTakeScreenshot(const char *fileName);                    // Take a screenshot and save it to a file (PNG or BMP).

// --- Backend-Specific Accessors ---
SITAPI SituationRendererType SituationGetRendererType(void);                            // Get the current active renderer type (OpenGL or Vulkan).
SITAPI GLFWwindow* SituationGetGLFWwindow(void);                                        // Get the raw GLFW window handle.
#ifdef SITUATION_USE_VULKAN
SITAPI VkInstance SituationGetVulkanInstance(void);                                     // Get the raw Vulkan instance handle.
SITAPI VkDevice SituationGetVulkanDevice(void);                                         // Get the raw Vulkan logical device handle.
SITAPI VkPhysicalDevice SituationGetVulkanPhysicalDevice(void);                         // Get the raw Vulkan physical device handle.
SITAPI VkRenderPass SituationGetMainWindowRenderPass(void);                             // Get the render pass for the main window.
#endif

// --- [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead. ---
SITAPI SituationError SituationCmdBeginRenderToDisplay(SituationCommandBuffer cmd, int display_id, ColorRGBA clear_color);              // [DEPRECATED] Begins a render pass on a target (-1 for main window), clearing it.
SITAPI SituationError SituationCmdEndRender(SituationCommandBuffer cmd);                                                                // [DEPRECATED] End the current render pass.
SITAPI SituationError SituationCmdBindUniformBuffer(SituationCommandBuffer cmd, uint32_t contract_id, SituationBuffer buffer);          // [DEPRECATED] [Core] Bind a Uniform Buffer Object (UBO) to a shader binding point.
SITAPI SituationError SituationCmdBindTexture(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture);                // [DEPRECATED] [Core] Bind a texture and sampler to a shader binding point.
SITAPI SituationError SituationCmdBindComputeBuffer(SituationCommandBuffer cmd, uint32_t binding, SituationBuffer buffer);              // [DEPRECATED] Bind a buffer to a compute shader binding point.
SITAPI SituationError SituationLoadComputeShader(const char* cs_path, SituationShader* out_shader);                                     // [DEPRECATED] Load a compute shader from a file. Use SituationCreateComputePipeline instead.
SITAPI SituationError SituationLoadComputeShaderFromMemory(const char* cs_code, SituationShader* out_shader);                           // [DEPRECATED] Create a compute shader from memory. Use SituationCreateComputePipelineFromMemory instead.
SITAPI void SituationMemoryBarrier(SituationCommandBuffer cmd, uint32_t barrier_bits);                                                  // [DEPRECATED] Insert a coarse-grained memory barrier. Use SituationCmdPipelineBarrier instead.

//==================================================================================
// Hot-Reloading Module (Development Tools)
//==================================================================================
// These functions allow you to reload assets from disk at runtime without restarting.
// They handle GPU synchronization, resource destruction, and re-loading.
// Returns true if the reload was successful. On failure, the old handle is usually invalid.
SITAPI SituationError SituationCheckHotReloads(void);                                   // Checks all tracked resources for file changes and reloads them if necessary.
SITAPI bool SituationReloadShader(SituationShader* shader);                             // Recompiles and links a shader from its original source files (Synchronous/Stalls GPU).
SITAPI bool SituationReloadComputePipeline(SituationComputePipeline* pipeline);         // Recompiles a compute pipeline from its original source file (Synchronous/Stalls GPU).
SITAPI bool SituationReloadTexture(SituationTexture* texture);                          // Re-reads image file and recreates the GPU texture resource (Synchronous/Stalls GPU).
SITAPI bool SituationReloadModel(SituationModel* model);                                // Re-parses GLTF/GLB file and rebuilds all meshes and textures (Synchronous/Stalls GPU).

//==================================================================================
// Input Module: Keyboard, Mouse, and Gamepad
//==================================================================================
// --- Keyboard Input ---
SITAPI int SituationGetCharFromScancode(int window, int scancode, int mods, uint32_t* out_char); // Maps a physical key scancode (plus modifiers) to a Unicode character, respecting the current OS keyboard layout.
SITAPI bool SituationIsKeyDown(int key);                                                // Check if a key is currently held down (a state).
SITAPI bool SituationIsKeyUp(int key);                                                  // Check if a key is currently up (a state).
SITAPI bool SituationIsKeyPressed(int key);                                             // Check if a key was pressed down this frame (an event).
SITAPI bool SituationIsKeyReleased(int key);                                            // Check if a key was released this frame (an event).
SITAPI bool SituationIsScancodeDown(int scancode);                                      // Check if a physical key (scancode) is currently held down.
SITAPI int SituationGetKeyScancode(int key);                                            // Get the platform-specific scancode for a logical key.
SITAPI int SituationGetKeyPressed(void);                                                // Get the next key from the press queue (no repeats).
SITAPI int SituationGetKeyPressedEx(int* out_scancode);                                 // Get the next key and its scancode from the queue.
SITAPI int SituationPeekKeyPressed(void);                                               // Peek at the next key in the press queue without consuming it.
SITAPI int SituationPeekKeyPressedEx(int* out_scancode);                                // Peek at the next key and its scancode.
SITAPI unsigned int SituationGetCharPressed(void);                                      // Get the next character from the text input queue.
SITAPI bool SituationIsLockKeyPressed(int lock_key_mod);                                // Check if a lock key (Caps, Num) is currently active.
SITAPI bool SituationIsScrollLockOn(void);                                              // Check if Scroll Lock is currently toggled on.
SITAPI bool SituationIsModifierPressed(int modifier);                                   // Check if a modifier key (Shift, Ctrl, Alt) is pressed.
SITAPI void SituationSetKeyCallback(SituationKeyCallback callback, void* user_data);    // Set a callback for key events.

// --- Mouse Input ---
SITAPI Vector2 SituationGetMousePosition(void);                                         // Get the mouse position within the window.
SITAPI Vector2 SituationGetMouseDelta(void);                                            // Get the mouse movement since the last frame.
SITAPI float SituationGetMouseWheelMove(void);                                          // Get vertical mouse wheel movement.
SITAPI Vector2 SituationGetMouseWheelMoveV(void);                                       // Get vertical and horizontal mouse wheel movement.
SITAPI bool SituationIsMouseButtonDown(int button);                                     // Check if a mouse button is currently held down (a state).
SITAPI bool SituationIsMouseButtonPressed(int button);                                  // Check if a mouse button was pressed down this frame (an event).
SITAPI bool SituationIsMouseButtonReleased(int button);                                 // Check if a mouse button was released this frame.
SITAPI void SituationSetMousePosition(Vector2 pos);                                     // Set the mouse position within the window.
SITAPI void SituationSetMouseOffset(Vector2 offset);                                    // Set a software offset for the mouse position.
SITAPI void SituationSetMouseScale(Vector2 scale);                                      // Set a software scale for the mouse position and delta.
SITAPI void SituationSetMouseButtonCallback(SituationMouseButtonCallback callback, void* user_data); // Set a callback for mouse button events.
SITAPI void SituationSetCursorPosCallback(SituationCursorPosCallback callback, void* user_data); // Set a callback for mouse movement events.
SITAPI void SituationSetScrollCallback(SituationScrollCallback callback, void* user_data); // Set a callback for mouse scroll events.

// --- Gamepad Input ---
SITAPI bool SituationIsJoystickPresent(int jid);                                        // Check if a joystick/gamepad is connected.
SITAPI bool SituationIsGamepad(int jid);                                                // Check if a connected joystick has a standard gamepad mapping.
SITAPI const char* SituationGetJoystickName(int jid);                                   // Get the human-readable name of a joystick/gamepad.
SITAPI void SituationSetJoystickCallback(SituationJoystickCallback callback, void* user_data); // Set a callback for joystick connection events.
SITAPI int SituationSetGamepadMappings(const char *mappings);                           // Load a new set of gamepad mappings from a string.
SITAPI int SituationGetGamepadButtonPressed(void);                                      // Get the next gamepad button from the press queue.
SITAPI bool SituationIsGamepadButtonDown(int jid, int button);                          // Check if a gamepad button is currently held down (a state).
SITAPI bool SituationIsGamepadButtonPressed(int jid, int button);                       // Check if a gamepad button was pressed down this frame (an event).
SITAPI bool SituationIsGamepadButtonReleased(int jid, int button);                      // Check if a gamepad button was released this frame (an event).
SITAPI int SituationGetGamepadAxisCount(int jid);                                       // Get the number of axes for a gamepad.
SITAPI float SituationGetGamepadAxisValue(int jid, int axis);                           // Get the value of a gamepad axis (deadzone applied).
SITAPI bool SituationSetGamepadVibration(int jid, float left_motor, float right_motor); // Set gamepad vibration/rumble (Windows only).

//==================================================================================
// Audio Module
//==================================================================================

// --- Audio Device Management ---
SITAPI SituationAudioDeviceInfo* SituationGetAudioDevices(int* count);                  // Get a list of available audio playback devices (caller must free).
SITAPI SituationError SituationSetAudioDevice(int internal_id, const SituationAudioFormat* format); // Set the active audio device.
SITAPI int SituationGetAudioPlaybackSampleRate(void);                                   // Get the sample rate of the current audio device.
SITAPI SituationError SituationSetAudioPlaybackSampleRate(int sample_rate);             // Re-initialize the audio device with a new sample rate.
SITAPI float SituationGetAudioMasterVolume(void);                                       // Get the master volume for the audio device.
SITAPI SituationError SituationSetAudioMasterVolume(float volume);                      // Set the master volume for the audio device.
SITAPI bool SituationIsAudioDevicePlaying(void);                                        // Check if the audio device is currently playing.
SITAPI SituationError SituationPauseAudioDevice(void);                                  // Pause audio playback on the device.
SITAPI SituationError SituationResumeAudioDevice(void);                                 // Resume audio playback on the device.

// --- Audio Capture ---
SITAPI SituationError SituationStartAudioCapture(SituationAudioCaptureCallback callback, void* user_data);
SITAPI void SituationStopAudioCapture(void);

// --- Audio Output Monitoring (for visualization) ---
SITAPI void SituationSetAudioOutputMonitor(void (*callback)(const float* samples, uint32_t frame_count, void* user_data), void* user_data);

// --- Sound Loading and Management ---
// --- Audio Handle API ---
SITAPI SituationSoundHandle SituationLoadAudio(const char* file_path, SituationAudioLoadMode mode, bool looping);
SITAPI SituationError SituationPlayAudio(SituationSoundHandle handle);
SITAPI void SituationUnloadAudio(SituationSoundHandle handle);
SITAPI SituationError SituationSetAudioVolume(SituationSoundHandle handle, float volume);
SITAPI SituationError SituationSetAudioPan(SituationSoundHandle handle, float pan);
SITAPI SituationError SituationSetAudioPitch(SituationSoundHandle handle, float pitch);

SITAPI SituationError SituationLoadSoundFromFile(const char* file_path, SituationAudioLoadMode mode, bool looping, SituationSound* out_sound); // Load a sound from a file.
SITAPI SituationError SituationLoadSoundFromStream(SituationStreamReadCallback on_read, SituationStreamSeekCallback on_seek, void* user_data, const SituationAudioFormat* format, bool looping, SituationSound* out_sound); // Load a sound from a custom stream.
SITAPI void SituationUnloadSound(SituationSound* sound);                                // Unload a sound and free its resources.
SITAPI SituationError SituationPlayLoadedSound(SituationSound* sound);                  // Play a loaded sound (restarts if already playing).
SITAPI SituationError SituationStopLoadedSound(SituationSound* sound);                  // Stop a specific sound from playing.
SITAPI SituationError SituationStopAllLoadedSounds(void);                               // Stop all currently playing sounds.

/**
 * @brief Gracefully stops a tone by triggering its release envelope.
 *        If the tone is already released or invalid, does nothing.
 *
 * @param handle The tone handle returned by SituationPlayToneEx()
 */
SITAPI void SituationStopTone(SituationToneHandle handle);

/**
 * @brief Legacy simple blip (kept for backward compatibility and quick UI sounds)
 */
SITAPI void SituationPlayTone(SituationWaveType type, float frequency, float volume, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec);
SITAPI void SituationPlayMidiNote(int note, SituationWaveType type, float volume, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec);
SITAPI void SituationStopAllTones(void);

/**
 * @brief Enable or disable reverb effect for tone generation.
 * @details Applies a global reverb effect to all tones generated by SituationPlayTone/SituationPlayMidiNote.
 *          The reverb creates a spacious, ambient sound by simulating room acoustics.
 * 
 * @param enabled true to enable reverb, false to disable
 */
SITAPI void SituationSetToneReverbEnabled(bool enabled);

/**
 * @brief Configure the tone reverb parameters.
 * @details Adjusts the characteristics of the reverb effect applied to tones.
 * 
 * @param room_size Room size (0.0 to 1.0). Larger values create longer reverb tails.
 * @param damping High frequency damping (0.0 to 1.0). Higher values make the reverb darker.
 * @param wet_level Reverb mix level (0.0 to 1.0). Amount of reverb signal in the output.
 * @param dry_level Dry signal level (0.0 to 1.0). Amount of original signal in the output.
 * @param width Stereo width (0.0 to 1.0). Controls the stereo spread of the reverb.
 */
SITAPI void SituationSetToneReverbParameters(float room_size, float damping, float wet_level, float dry_level, float width);


// --- Sound Data Manipulation (Wave Utilities) ---
SITAPI SituationError SituationSoundCopy(const SituationSound* source, SituationSound* out_destination);    // Create a new sound by copying the raw PCM data from a source.
SITAPI SituationError SituationSoundCrop(SituationSound* sound, uint64_t initFrame, uint64_t finalFrame);   // Crop a sound's PCM data in-place to a new range.
SITAPI bool SituationSoundExportAsWav(const SituationSound* sound, const char* fileName);                   // Export the sound's raw PCM data to a WAV file.

// --- Sound Parameters and Effects ---
SITAPI SituationError SituationSetSoundVolume(SituationSound* sound, float volume);     // Set the volume for a specific sound.
SITAPI float SituationGetSoundVolume(SituationSound* sound);                            // Get the volume of a specific sound.
SITAPI SituationError SituationSetSoundPan(SituationSound* sound, float pan);           // Set the stereo pan for a sound [-1.0 to 1.0].
SITAPI float SituationGetSoundPan(SituationSound* sound);                               // Get the stereo pan of a sound.
SITAPI SituationError SituationSetSoundPitch(SituationSound* sound, float pitch);       // Set the pitch for a sound (resamples).
SITAPI float SituationGetSoundPitch(SituationSound* sound);                             // Get the pitch of a sound.
SITAPI SituationError SituationSetSoundFilter(SituationSound* sound, SituationFilterType type, float cutoff_hz, float q_factor);                    // Apply a low-pass or high-pass filter to a sound.
SITAPI SituationError SituationSetSoundEcho(SituationSound* sound, bool enabled, float delay_sec, float feedback, float wet_mix);                   // Apply an echo effect to a sound.
SITAPI SituationError SituationSetSoundReverb(SituationSound* sound, bool enabled, float room_size, float damping, float wet_mix, float dry_mix);   // Apply a reverb effect to a sound.

// --- Custom Audio Processing ---
SITAPI SituationError SituationAttachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* user_data); // Attach a custom DSP processor to a sound's effect chain.
SITAPI SituationError SituationDetachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* user_data); // Detach a custom DSP processor from a sound.


//==================================================================================
// Filesystem Module
//==================================================================================
// --- Path Management & Special Directories ---
SITAPI char* SituationGetAppSavePath(const char* app_name);                             // Get a safe, persistent path for saving application data (caller must free).
SITAPI char* SituationGetBasePath(void);                                                // Get the path to the directory containing the executable (caller must free).
static char* SituationGetBasePathFromFile(const char* file_path);                       // Internal helper: Extract directory path from file path (caller must free).
SITAPI char* SituationJoinPath(const char* base_path, const char* file_or_dir_name);    // Join two path components with the correct OS separator (caller must free).
SITAPI const char* SituationGetFileName(const char* full_path);                         // Extract the file name (including extension) from a full path.
SITAPI const char* SituationGetFileExtension(const char* file_path);                    // Extract the file extension from a path.

// --- File & Directory Queries ---
SITAPI bool SituationFileExists(const char* file_path);                                 // Check if a file exists at the given path.
SITAPI bool SituationDirectoryExists(const char* dir_path);                             // Check if a directory exists at the given path.
SITAPI long SituationGetFileModTime(const char* file_path);                             // Get the last modification time of a file (Unix timestamp).

// --- File Operations ---
SITAPI SituationError SituationLoadFileData(const char* file_path, unsigned int* out_bytes_read, unsigned char** out_data);           // Load an entire file into a memory buffer (caller must free).
SITAPI SituationError SituationSaveFileData(const char* file_path, const void* data, unsigned int bytes_to_write);    // Save a block of memory to a file.
#ifdef SITUATION_ENABLE_THREADING
SITAPI SituationJobId SituationLoadFileAsync(SituationThreadPool* pool, const char* file_path, SituationFileLoadCallback callback, void* user_data); // Asynchronously load a file.
SITAPI SituationJobId SituationSaveFileAsync(SituationThreadPool* pool, const char* file_path, const void* data, size_t size, SituationFileSaveCallback callback, void* user_data); // Asynchronously save a file.
SITAPI SituationJobId SituationLoadFileTextAsync(SituationThreadPool* pool, const char* file_path, SituationFileTextLoadCallback callback, void* user_data); // Asynchronously load a text file.
SITAPI SituationJobId SituationSaveFileTextAsync(SituationThreadPool* pool, const char* file_path, const char* text, SituationFileSaveCallback callback, void* user_data); // Asynchronously save a text file.
#endif
SITAPI char* SituationLoadFileText(const char* file_path);                                                  // Load a text file into a null-terminated string (caller must free).
SITAPI bool SituationSaveFileText(const char* file_path, const char* text);                                 // Save a null-terminated string to a text file.
SITAPI bool SituationCopyFile(const char* source_path, const char* dest_path);                              // Copy a file.
SITAPI bool SituationDeleteFile(const char* file_path);                                                     // Delete a file.
SITAPI bool SituationMoveFile(const char* old_path, const char* new_path);                                  // Move/rename a file, even across drives on Windows.
SITAPI bool SituationRenameFile(const char* old_path, const char* new_path);                                // Alias for SituationMoveFile.

// --- Directory Operations ---
SITAPI bool SituationCreateDirectory(const char* dir_path, bool create_parents);        // Create a directory, optionally creating parent directories.
SITAPI bool SituationDeleteDirectory(const char* dir_path, bool recursive);             // Delete a directory, optionally deleting all its contents.
SITAPI char** SituationListDirectoryFiles(const char* dir_path, int* out_count);        // List files and subdirectories in a path (caller must free with SituationFreeDirectoryFileList).
SITAPI void SituationFreeDirectoryFileList(char** file_list, int count);                // Free the memory allocated by SituationListDirectoryFiles.

//==================================================================================
// Miscellaneous Module
//==================================================================================
// --- Temporal Oscillator System ---
SITAPI bool SituationTimerGetOscillatorState(int oscillator_id);                        // Get the current binary state (0 or 1) of an oscillator.
SITAPI bool SituationTimerGetPreviousOscillatorState(int oscillator_id);                // Get the previous frame's state of an oscillator.
SITAPI bool SituationTimerHasOscillatorUpdated(int oscillator_id);                      // Check if an oscillator's state has changed this frame.
SITAPI bool SituationTimerPingOscillator(int oscillator_id);                            // Check if an oscillator's period has elapsed since the last ping.
SITAPI uint64_t SituationTimerGetOscillatorTriggerCount(int oscillator_id);             // Get the total number of times an oscillator has triggered.
SITAPI double SituationTimerGetOscillatorPeriod(int oscillator_id);                     // Get the period of an oscillator in seconds.
SITAPI SituationError SituationSetTimerOscillatorPeriod(int oscillator_id, double period_seconds); // Set the period of an oscillator.
SITAPI double SituationTimerGetPingProgress(int oscillator_id);                         // Get progress [0.0 to 1.0+] of the interval since the last successful ping.
SITAPI double SituationTimerGetTime(void);                                              // Get the total time elapsed since initialization.

// --- Color Space Conversions ---
SITAPI void SituationConvertColorToVector4(ColorRGBA c, Vector4* out_normalized_color); // Convert an 8-bit ColorRGBA struct to a normalized Vector4.
SITAPI ColorHSV SituationRgbToHsv(ColorRGBA rgb);                                       // Converts a standard RGBA color to the Hue, Saturation, Value color space.
SITAPI ColorRGBA SituationHsvToRgb(ColorHSV hsv);                                       // Converts a Hue, Saturation, Value color back to the standard RGBA color space.
SITAPI ColorYPQA SituationColorToYPQ(ColorRGBA color);                                  // Converts a standard RGBA color to the YPQA (Luma, Phase, Quadrature) color space.
SITAPI ColorRGBA SituationColorFromYPQ(ColorYPQA ypq_color);                            // Converts a YPQA color back to the standard RGBA color space.

//==================================================================================
// Threading Module
//==================================================================================
#ifdef SITUATION_ENABLE_THREADING
SITAPI bool SituationCreateThreadPool(SituationThreadPool* pool, size_t num_threads, size_t queue_size, double hot_reload_rate, bool disable_io); // Initializes the thread pool with dual-priority queues and worker threads.
SITAPI void SituationDestroyThreadPool(SituationThreadPool* pool); 											// Shuts down the thread pool and releases resources.
SITAPI SituationJobId SituationSubmitJobEx(SituationThreadPool* pool, void (*func)(void*, void*), const void* data, size_t data_size, SituationJobFlags flags); // Submits a job with priority flags and optional data payload.
 // Legacy wrapper for simple pointer passing (Low priority, no copy).
#define SituationSubmitJob(pool, func, user_ptr) \
    SituationSubmitJobEx(pool, (void(*)(void*, void*))func, user_ptr, 0, SIT_SUBMIT_DEFAULT)
SITAPI void SituationDispatchParallel(SituationThreadPool* pool, int count, int min_batch_size, void (*func)(int index, void* user_data), void* user_data); // Executes a loop in parallel across worker threads (Fork-Join).
SITAPI bool SituationWaitForJob(SituationThreadPool* pool, SituationJobId job_id); 							// Waits for a specific job to complete (O(1) check).
SITAPI void SituationWaitForAllJobs(SituationThreadPool* pool); 											// Blocks until all queued jobs are finished.
SITAPI bool SituationAddJobDependency(SituationThreadPool* pool, SituationJobId prerequisite_job, SituationJobId dependent_job); // Adds a dependency between two jobs (prereq -> dependent).
SITAPI bool SituationAddJobDependencies(SituationThreadPool* pool, SituationJobId* prerequisites, int count, SituationJobId dependent_job); // Adds multiple dependencies for a single dependent job.
SITAPI void SituationDumpTaskGraph(SituationThreadPool* pool, FILE* out_stream, bool json_mode); 			// Prints the current task graph state to the stream.

SITAPI SituationJobId SituationLoadSoundFromFileAsync(SituationThreadPool* pool, const char* file_path, bool looping, SituationSound* out_sound); // Asynchronously loads and decodes a sound file.
#endif // SITUATION_ENABLE_THREADING

#endif // SITUATION_API_H
