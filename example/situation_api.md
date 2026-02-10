# The "Situation" Advanced Platform Awareness, Control, and Timing

_Core API library v2.3.41 "Flexible Formats"_

_(c) 2025 Jacques Morel_

_MIT Licenced_

Welcome to "Situation", a public API engineered for high-performance, cross-platform development. "Situation" is a single-file, cross-platform **[Strict C11 (ISO/IEC 9899:2011) Compliant](C11_Compliance_Report.md)** library providing unified, low-level access and control over essential application subsystems. Its purpose is to abstract away platform-specific complexities, offering a lean yet powerful API for building sophisticated, high-performance software. This library is designed as a foundational layer for professional applications, including but not limited to: real-time simulations, game engines, multimedia installations, and scientific visualization tools.

**Current Version: v2.3.41 "Flexible Formats"**

**Version 2.3.41** introduces flexible texture format selection through the new `SituationColorEncoding` enum. Images can now specify whether their data is in linear or SRGB color space, enabling automatic GPU format selection that works identically across both OpenGL and Vulkan backends. This fixes storage image compatibility issues while maintaining proper gamma correction for sampled textures.

> **See the complete changelog:** [UPDATELOG.md](UPDATELOG.md)

Our immediate development roadmap is focused on expanding the library's capability:
*   **Flexible Texture Formats (v2.3.41):** 🎉 **COMPLETE!** Introduced `SituationColorEncoding` enum for automatic format selection. Storage images now work correctly with compute shaders, and sampled textures maintain proper gamma correction.
*   **Vulkan Text Rendering (v2.3.39):** Fixed all 11 critical bugs in the Vulkan text rendering pipeline. Text now renders correctly with proper descriptor set layouts, UV calculations, and coordinate system handling.
*   **Asset Pipeline (v2.3.38):** Added `SituationLoadBitmapFontFromMemory` and enhanced I/O thread controls for smoother background loading.
*   **OpenGL Optimization (v2.3.36):** Completed the "Max Out Core" plan with MDI batching, Zero-Copy Ring Buffers, and Bindless Textures.
*   **Texture Registry (v2.3.31):** Implemented a generational handle system for textures, enabling safe hot-reloading and O(1) validation.
*   **Universal Handles (v2.4):** Transitioning all resources (Buffers, Shaders, Meshes) to the Registry System for uniform, bindless-ready access.
*   **Async Compute:** Exposing dedicated transfer and compute queues in Vulkan for non-blocking background operations.
*   **Built-in Debug Tools**: Leveraging internal profiling counters to render an immediate-mode performance overlay.
*   **Advanced Audio DSP**: Expanding the effects chain with user-definable graph routing.
*   **Cross-Platform Expansion**: Formalizing support for Android and WebAssembly targets.
*   **Web & Reach (Phase 4):** Full **Emscripten** (WASM) support and a **WebGPU (Dawn)** backend to bring Situation apps to the browser with near-native performance.

"Situation" is an ambitious project that aims to become a premier, go-to solution for developers seeking a reliable and powerful platform layer. We encourage you to explore the library, challenge its capabilities, and contribute to its evolution.

The library's philosophy is reflected in its name, granting developers complete situational "Awareness," precise "Control," and fine-grained "Timing."

It provides deep **Awareness** of the host system through APIs for querying hardware and multi-monitor display information, and by handling operating system events like window focus and file drops.

This foundation enables precise **Control** over the entire application stack, from window management (fullscreen, borderless) and input devices (keyboard, mouse, gamepad) to a comprehensive audio pipeline with playback, capture, and real-time effects. This control extends to the graphics and compute pipeline, abstracting modern OpenGL and Vulkan through a unified command-buffer model. It offers simplified management of GPU resources—such as shaders, meshes, and textures—and includes powerful utilities for high-quality text rendering and robust filesystem I/O.

Finally, its **Timing** capabilities range from high-resolution performance measurement and frame rate management to an advanced **Temporal Oscillator System** for creating complex, rhythmically synchronized events. By handling the foundational boilerplate of platform interaction, "Situation" empowers developers to focus on core application logic, enabling the creation of responsive and sophisticated software—from games and creative coding projects to data visualization tools—across all major desktop platforms.

---

# Situation v2.3.41 API Programming Guide

"Situation" is a single-file, cross-platform C/C++ library designed for advanced platform awareness, control, and timing. It provides a comprehensive, immediate-mode API that abstracts the complexities of windowing, graphics (OpenGL/Vulkan), audio, and input. This guide serves as the primary technical manual for the library, detailing its architecture, usage patterns, and the complete Application Programming Interface (API).

---

## What's New in v2.3.41

### 🎉 Flexible Texture Formats - Color Encoding & Format Selection

Version 2.3.41 introduces flexible texture format selection through the new `SituationColorEncoding` enum. This critical update fixes storage image compatibility issues while maintaining proper gamma correction for sampled textures.

**Key Features:**
- **`SituationColorEncoding` enum** - Specify LINEAR or SRGB color space
- **Automatic format selection** - GPU format chosen based on color encoding
- **Backend-neutral API** - Same enum works for both OpenGL and Vulkan
- **Storage image fix** - Compute-writable textures now work correctly

**Format Mappings:**

| Color Encoding | Vulkan Format | OpenGL Format | Use Case |
|----------------|---------------|---------------|----------|
| `SITUATION_COLOR_LINEAR` | `VK_FORMAT_R8G8B8A8_UNORM` | `GL_RGBA8` | Storage images, compute writes |
| `SITUATION_COLOR_SRGB` | `VK_FORMAT_R8G8B8A8_SRGB` | `GL_SRGB8_ALPHA8` | Sampled textures, photos, UI |

**Usage Example:**
```c
// Storage image for compute shader
SituationImage img;
SituationCreateImage(1024, 768, 4, &img);
img.color_encoding = SITUATION_COLOR_LINEAR;  // Required for storage!

SituationTexture tex;
SituationCreateTextureEx(img, false, SITUATION_TEXTURE_USAGE_STORAGE, &tex);
// Result: Uses VK_FORMAT_R8G8B8A8_UNORM (Vulkan) or GL_RGBA8 (OpenGL)

// Sampled texture with gamma correction
SituationImage photo;
SituationLoadImage("photo.png", &photo);
photo.color_encoding = SITUATION_COLOR_SRGB;  // Gamma correction

SituationTexture display_tex;
SituationCreateTexture(photo, false, &display_tex);
// Result: Uses VK_FORMAT_R8G8B8A8_SRGB (Vulkan) or GL_SRGB8_ALPHA8 (OpenGL)
```

**Critical Fix:** Storage images (textures writable by compute shaders) were previously hardcoded to SRGB format, which is incompatible with storage operations on most GPUs. This caused validation errors and black screens in applications using compute shaders. The new system automatically selects LINEAR format for storage images while maintaining SRGB for sampled textures.

---

## What's New in v2.3.39

### Vulkan Text Rendering - COMPLETE!
All 11 critical bugs in the Vulkan text rendering pipeline have been fixed:

1. **Internal Renderer Return Value Check** - Fixed treating SUCCESS (0) as failure
2. **Texture Generation** - Textures now properly initialized with generation=1
3. **Descriptor Set Binding** - UBO descriptor set now bound in text pipeline
4. **Projection Matrix** - Matrix now updated in `SituationCmdBeginRenderToDisplay`
5. **UBO Memory Type** - Changed from GPU_ONLY to CPU_TO_GPU for dynamic updates
6. **Vertex Attribute Offset** - Fixed texcoord offset from 0 to 8 bytes
7. **Backface Culling** - Disabled culling for text quads
8. **Viewport/Scissor** - Now properly set in render pass begin
9. **Fragment Shader Descriptor Set** - Added `set = 1` qualifier for texture sampler
10. **Font Atlas Descriptor Layout** - Font atlas now uses correct text_sampler_layout (binding 0)
11. **UV Calculation Bug** - Fixed grid font UV calculation (row/16 instead of row/8, swapped v0/v1 for Vulkan Y-down)

**Text rendering now works perfectly in Vulkan!** The system correctly handles descriptor set layouts, UV coordinates, and Vulkan's coordinate system.

### I/O Thread Metrics (v2.3.37)
Added `SituationGetIOQueueDepth()` to monitor pending background asset loading tasks, enabling better load balancing and progress tracking.

### Enhanced Thread Pool Configuration (v2.3.37)
`SituationCreateThreadPool()` now accepts `hot_reload_rate` and `disable_io` parameters for fine-grained control over the I/O subsystem.

**For complete details, see:** [UPDATELOG.md](UPDATELOG.md)

---

## Table of Contents

### Getting Started
1. [Introduction and Core Concepts](#introduction-and-core-concepts)
   - Core Philosophy and Design
   - Application Structure
   - Core API Patterns
   - Rendering and Graphics
   - Other Key Systems
2. [Building the Library](#building-the-library)
   - Integration Models
   - Project Structure
   - Compilation Requirements
   - Build & Feature Defines
3. [Getting Started](#getting-started)
   - Quick Start Guide
   - Basic Application Template

### API Reference
4. [Detailed API Reference](#detailed-api-reference)

#### Core Systems
- [Core Module](#core-module) - Initialization, lifecycle, system info
- [Window and Display Module](#window-and-display-module) - Window management, monitors, displays
- [Input Module](#input-module) - Keyboard, mouse, gamepad input

#### Graphics & Rendering
- [Graphics Module](#graphics-module) - Command buffers, pipelines, rendering
- [Image Module](#image-module) - CPU-side image manipulation
- [Compute Shaders](#compute-shaders) - GPU compute operations
- [Text Rendering](#text-rendering) - Font loading and text drawing
- [2D Rendering & Drawing](#2d-rendering--drawing) - 2D graphics primitives

#### Media & I/O
- [Audio Module](#audio-module) - Sound playback, capture, effects
- [Filesystem Module](#filesystem-module) - File operations, paths

#### Utilities
- [Threading Module](#threading-module) - Thread pools, async operations
- [Miscellaneous Module](#miscellaneous-module) - Utility functions
- [Hot-Reloading Module](#hot-reloading-module) - Live asset reloading
- [Logging Module](#logging-module) - Debug logging and tracing

### Learning & Support
5. [Examples & Tutorials](#examples--tutorials)
   - Terminal Module
   - Basic Triangle Rendering
   - 3D Model Loading
   - Audio Playback
   - Input Handling
   - Compute Shaders
   - GPU Particle Systems
6. [FAQ & Troubleshooting](#faq--troubleshooting)
   - Common Issues
   - Performance Tips
   - Debugging Guide

### Legal
7. [License](#license-mit) - MIT License

---

<details open>
<summary><h2>Introduction and Core Concepts</h2></summary>

This section explains the fundamental concepts, design philosophy, and core architectural patterns of the "Situation" library. A solid understanding of these ideas is crucial for using the library effectively.

### 1. Core Philosophy and Design
#### Immediate Mode and Explicit Control
The library favors a mostly **"immediate mode"** style API. This means that for many operations, you call a function and it takes effect immediately within the current frame. For example, `SituationCmdDrawQuad()` directly records a draw command into the current frame's command buffer. This approach is designed to be simple, intuitive, and easy to debug, contrasting with "retained mode" systems where you would build a scene graph of objects that is then rendered by the engine.

Complementing this is a philosophy of **explicit resource management**. Any resource you create (a texture, a mesh, a sound) must be explicitly destroyed by you using its corresponding `SituationDestroy...()` or `SituationUnload...()` function. This design choice avoids the complexities and performance overhead of automatic garbage collection and puts you in full control of resource lifecycles. To aid in debugging, the library will warn you at shutdown if you've leaked any GPU resources.

#### C-Style, Single-File, and Backend-Agnostic Architecture
- **C-Style, Data-Oriented API:** The API is pure C, promoting maximum portability and interoperability. It uses handles (structs passed by value) to represent opaque resources and pointers for functions that need to modify or destroy those resources. This approach is data-oriented, focusing on transforming data (e.g., vertex data into a mesh, image data into a texture) rather than on object-oriented hierarchies.
- **Single-File, Header-Only Distribution:** "Situation" is distributed as a single header file (`situation.h`), making it incredibly easy to integrate into your projects. To use it, you simply `#include "situation.h"` in your source files. In exactly one C or C++ file, you must first define `SITUATION_IMPLEMENTATION` before the include to create the implementation.
- **Backend Abstraction:** The library provides a unified API that works over different graphics backends (currently OpenGL and Vulkan). You choose the backend at compile time by defining either `SITUATION_USE_OPENGL` or `SITUATION_USE_VULKAN`.

#### Strictly Single-Threaded Model
The library is **strictly single-threaded**. All API functions must be called from the same thread that called `SituationInit()`. Asynchronous operations, such as asset loading on a worker thread, must be handled by the user with care, ensuring that no `SITAPI` calls are made from outside the main thread.

### 2. Application Structure
#### The Application Lifecycle
The library enforces a strict and predictable lifecycle:
1.  **Initialization:** Call `SituationInit()` exactly once at the beginning. This sets up all subsystems. No other library functions should be called before this.
2.  **Main Loop:** After initialization, your application enters a loop that continues as long as `SituationWindowShouldClose()` returns `false`. This is where all real-time processing occurs.
3.  **Shutdown:** When the main loop terminates, you must call `SituationShutdown()` exactly once to gracefully tear down all subsystems and free resources.

#### The Three-Phase Frame
To ensure stable and predictable behavior, every iteration of the main loop must be divided into three distinct phases, executed in a specific order:
1.  **Input Phase:** At the very beginning of the frame, call `SituationPollInputEvents()`. This gathers all pending input from the operating system, ensuring that all logic in the frame operates on a consistent snapshot of input.
2.  **Update Phase:** Next, call `SituationUpdateTimers()` to calculate `deltaTime`. Immediately after, execute all of your application's logic (physics, AI, state changes). Using `deltaTime` is crucial for creating frame-rate-independent behavior.
3.  **Render Phase:** Finally, perform all rendering. This phase begins with `SituationAcquireFrameCommandBuffer()`, followed by recording all drawing commands, and concludes with `SituationEndFrame()`, which submits the work to the GPU.

### 3. Core API Patterns
#### Handles vs. Pointers
The API uses two patterns for interacting with objects:
- **Handles (by value):** Opaque structs like `SituationMesh` or `SituationShader` are typically passed by value to drawing or binding functions (e.g., `SituationCmdDrawMesh(my_mesh)`). These are lightweight identifiers for GPU resources.
- **Pointers (for modification):** When a function needs to modify or destroy a resource, you must pass a pointer to its handle (e.g., `SituationDestroyMesh(&my_mesh)`). This allows the function to invalidate the handle by setting its internal ID to 0, preventing accidental use after destruction.

#### Input Handling: Polling vs. Callbacks
The library offers two complementary models for handling input:
1.  **State Polling (`SituationIs...Down`, `SituationIs...Pressed`)**: This is the most common approach for real-time applications. Within your main `Update` phase, you can query the current state of any key or button. This is ideal for continuous actions (character movement) or single-trigger game events (jumping, shooting).
2.  **Event-Driven Callbacks (`SituationSet...Callback`)**: This model allows you to register callback functions that are invoked the moment an input event occurs. This is more efficient for UI interactions, text input, or other event-driven logic, as it avoids the need to check for input every single frame.

### 4. Rendering and Graphics
#### The Command Buffer
At the core of the rendering system is the **command buffer**. Rather than telling the GPU to "draw this now," you record a series of commands (prefixed with `SituationCmd...`) into a buffer. Once all commands for a frame are recorded, `SituationEndFrame()` submits the entire buffer to the GPU for execution. This batching approach is far more efficient and is central to how modern graphics APIs operate.

#### CPU-Side vs. GPU-Side Resources
The library makes a clear distinction between resources in system memory (CPU) and video memory (GPU).
- **`SituationImage` (CPU):** A block of pixel data in RAM. The Image module functions operate on this data, allowing for flexible manipulation (resizing, drawing text, etc.) without GPU overhead.
- **`SituationTexture` (GPU):** A GPU resource created by uploading a `SituationImage`. This is the object used by shaders for rendering.
The typical workflow is to load/generate a `SituationImage`, perform all desired manipulations, and then upload it once to a `SituationTexture` for efficient rendering.

#### Logical vs. Physical Coordinates (High-DPI)
Modern displays often have a high pixel density (High-DPI). The library abstracts this complexity:
-   **Logical Size (Screen Coordinates):** Dimensions used by the OS for window sizing and positioning. Functions like `SituationGetScreenWidth()` and `SituationGetMousePosition()` operate in this space. Use this for UI layout and logic.
-   **Physical Size (Render Pixels):** The actual number of pixels in the framebuffer (`SituationGetRenderWidth()`). This is the resolution the GPU renders to.
The library automatically handles this scaling. You can query the scaling factor using `SituationGetWindowScaleDPI()`.

#### The Virtual Display System
A "Virtual Display" is an **off-screen render target**. Instead of drawing directly to the main window, you can render a scene into a virtual display. This is incredibly powerful for post-processing effects (bloom, blur), UI layering (rendering UI at a fixed resolution), and caching parts of a scene that don't change frequently.

### 5. Other Key Systems
#### Audio: Sounds vs. Streams
The audio module can handle audio in two ways:
-   **Loaded Sounds (`SituationLoadSoundFromFile`):** Decodes the entire audio file into memory. Ideal for short, low-latency sound effects.
-   **Streamed Sounds (`SituationLoadSoundFromStream`):** Decodes the audio in small chunks as it's playing. Uses significantly less memory, making it perfect for long background music tracks.

#### Filesystem: Cross-Platform and Special Paths
The filesystem module abstracts away OS-specific differences. All paths are UTF-8. To ensure your application is portable, use the provided helper functions instead of hardcoding paths:
-   `SituationGetBasePath()`: Returns the directory containing your executable. Use this for loading application assets.
-   `SituationGetAppSavePath()`: Returns a platform-appropriate, user-specific directory for saving configuration files and user data.

#### The Temporal Oscillator System
This is a high-level timing utility for creating rhythmic, periodic events. You can initialize oscillators with specific periods (e.g., 0.5 seconds for 120 BPM). The library updates these timers independent of the frame rate, allowing you to easily synchronize animations, game logic, or visual effects to a steady, musical beat using functions like `SituationTimerHasOscillatorUpdated()`.

</details>

---

<details>
<summary><h2>Building the Library</h2></summary>

### 2.1 Integration Models (Header-Only vs. Shared Library)
**A) Header-Only:**
- Add `situation.h` to your project.
- In *one* C/C++ source file (e.g., `sit_lib.c`), define `SITUATION_IMPLEMENTATION` *before* including `situation.h`.
```c
#define SITUATION_IMPLEMENTATION
#include "situation.h"
```
- Compile this source file with your project.

**B) Shared Library (DLL):**
- Create a separate source file (e.g., `sit_dll.c`).
- Define `SITUATION_IMPLEMENTATION` and `SITUATION_BUILD_SHARED`.
```c
#define SITUATION_IMPLEMENTATION
#define SITUATION_BUILD_SHARED
#include "situation.h"
```
- Compile this into a shared library (DLL/.so).
- In your main application, define `SITUATION_USE_SHARED` and include `situation.h`.
```c
#define SITUATION_USE_SHARED
#include "situation.h"
```
- Link your application against the generated library.

### 2.2 Project Structure Recommendations
```
your_project/
├── src/
│   ├── main.c              // Your application entry point
│   └── (other .c files)    // Your application logic
├── lib/
│   └── situation.h         // This library header
├── ext/                    // External dependencies (if not system-installed)
│   ├── glad/               // For OpenGL (if SITUATION_USE_OPENGL)
│   │   ├── glad.c
│   │   └── glad.h
│   ├── cglm/               // For math (if used)
│   │   └── ...             // cglm headers
│   ├── stb/                // For image loading (stb_image.h, etc.)
│   │   └── ...             // stb headers (define STB_IMAGE_IMPLEMENTATION in one .c file)
│   └── miniaudio/          // Audio library (miniaudio.h)
│       └── miniaudio.h     // (define MINIAUDIO_IMPLEMENTATION in one .c file)
├── assets/                 // Your application's assets
│   ├── models/
│   │   └── cube.obj
│   ├── textures/
│   │   └── diffuse.png
│   ├── shaders/
│   │   ├── basic.vert
│   │   ├── basic.frag
│   │   └── compute_filter.comp
│   └── audio/
│       └── background_music.wav
└── build/                  // Build output directory
```

### 2.3 Compilation Requirements & Dependencies
- A C99 or C++ compiler.
- **Required Dependencies (provided or system-installed):**
    - **GLFW3:** For windowing and input. Headers and library linking required.
    - **OpenGL Context Loader (e.g., GLAD):** If using `SITUATION_USE_OPENGL`. `glad.c` must be compiled.
    - **Vulkan SDK:** If using `SITUATION_USE_VULKAN`. Headers and linking required. Includes shaderc, VMA.
    - **cglm:** For math types and functions (vec3, mat4, etc.). Headers needed.
- **Optional Dependencies (for extra features):**
    - **stb_image.h, stb_image_write.h, stb_image_resize.h:** For image loading/saving/resizing. Define `STB_IMAGE_IMPLEMENTATION` etc. in one .c file.
    - **stb_truetype.h:** For styled text rendering (SDF generation). Define `STB_TRUETYPE_IMPLEMENTATION`.
    - **miniaudio.h:** For audio. Define `MINIAUDIO_IMPLEMENTATION` in one .c file.

### 2.4 Build & Feature Defines
This section details the preprocessor defines that control the library's features and build configuration.

#### Backend Selection
- **`SITUATION_USE_OPENGL`**: Enables the modern OpenGL backend. Must be defined before including `situation.h`.
- **`SITUATION_USE_VULKAN`**: Enables the explicit Vulkan backend. Must be defined before including `situation.h`.

#### Shared Library Support
- **`SITUATION_BUILD_SHARED`**: Must be defined when compiling the library as a shared object (DLL). This controls symbol visibility for export.
- **`SITUATION_USE_SHARED`**: Must be defined in the application code when linking against the shared library to control symbol import.

#### Feature Enablement
- **`SITUATION_ENABLE_SHADER_COMPILER`**: Mandatory for using compute shaders with the Vulkan backend as it enables runtime compilation of GLSL to SPIR-V.

</details>

---

<details>
<summary><h2>Getting Started</h2></summary>

Here is a minimal, complete example of a "Situation" application that opens a window, clears it to a blue color, and runs until the user closes it.

### Step 1: Include the Library
First, make sure `situation.h` is in your project's include path. In your main C file, define `SITUATION_IMPLEMENTATION` and include the header.

```c
#define SITUATION_IMPLEMENTATION
// Define a graphics backend before including the library
#define SITUATION_USE_OPENGL // or SITUATION_USE_VULKAN
#include "situation.h"

#include <stdio.h> // For printf
```

### Step 2: Initialize the Library
In your `main` function, you need to initialize the library. Create a `SituationInitInfo` struct to configure your application's startup properties, such as the window title and initial dimensions. Then, call `SituationInit()`.

```c
int main(int argc, char** argv) {
    SituationInitInfo init_info = {
        .app_name = "My First Situation App",
        .app_version = "1.0",
        .initial_width = 1280,
        .initial_height = 720,
        .window_flags = SITUATION_FLAG_WINDOW_RESIZABLE | SITUATION_FLAG_VSYNC_HINT,
        .target_fps = 60,
        .headless = false
    };

    if (SituationInit(argc, argv, &init_info) != SIT_SUCCESS) {
        printf("Failed to initialize Situation: %s\n", SituationGetLastErrorMsg());
        return -1;
    }
```

### Step 3: The Main Loop
The heart of your application is the main loop. This loop continues as long as the user has not tried to close the window (`!SituationWindowShouldClose()`). Inside the loop, you follow a strict three-phase structure: Input, Update, and Render.

```c
    while (!SituationWindowShouldClose()) {
        // --- 1. Input ---
        SituationPollInputEvents();

        // --- 2. Update ---
        SituationUpdateTimers();
        // Your application logic, physics, etc. would go here.
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            break; // Exit the loop
        }

        // --- 3. Render ---
        if (SituationAcquireFrameCommandBuffer()) {
            SituationRenderPassInfo pass_info = {
                .color_load_action = SIT_LOAD_ACTION_CLEAR,
                .clear_color = { .r = 0, .g = 12, .b = 24, .a = 255 }, // A dark blue
                .color_store_action = SIT_STORE_ACTION_STORE,
            };
            SituationCmdBeginRenderPass(SituationGetMainCommandBuffer(), &pass_info);
            // ... Drawing commands go here ...
            SituationCmdEndRenderPass(SituationGetMainCommandBuffer());
            SituationEndFrame();
        }
    }
```

### Step 4: Shutdown
After the main loop finishes, it is critical to call `SituationShutdown()` to clean up all resources.

```c
    SituationShutdown();
    return 0;
}
```

### Full Example Code

```c
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#include "situation.h"

#include <stdio.h>

int main(int argc, char** argv) {
    // 1. Configure and Initialize
    SituationInitInfo init_info = {
        .app_name = "My First Situation App",
        .app_version = "1.0",
        .initial_width = 1280,
        .initial_height = 720,
        .window_flags = SITUATION_FLAG_WINDOW_RESIZABLE | SITUATION_FLAG_VSYNC_HINT,
    };
    if (SituationInit(argc, argv, &init_info) != SIT_SUCCESS) {
        printf("Failed to initialize Situation: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // 2. Main Loop
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) break;

        if (SituationAcquireFrameCommandBuffer()) {
            SituationRenderPassInfo pass_info = {
                .color_load_action = SIT_LOAD_ACTION_CLEAR,
                .clear_color = {0, 12, 24, 255},
                .color_store_action = SIT_STORE_ACTION_STORE,
            };
            SituationCmdBeginRenderPass(SituationGetMainCommandBuffer(), &pass_info);
            SituationCmdEndRenderPass(SituationGetMainCommandBuffer());
            SituationEndFrame();
        }
    }

    // 3. Shutdown
    SituationShutdown();
    return 0;
}
```

---
</details>

---
---

# API Reference

## Detailed API Reference

This section provides a complete list of all functions available in the "Situation" library, organized by module.

<details>
<summary><h3>Core Module</h3></summary>

**Overview:** The Core module is the heart of the "Situation" library, providing the essential functions for application lifecycle management. It handles initialization (`SituationInit`) and shutdown (`SituationShutdown`), processes the main event loop, and manages frame timing and rate control. This module also serves as a gateway to crucial system information, offering functions to query hardware details, manage command-line arguments, and set up critical application-wide callbacks.

### Core Structs

#### `SituationInitInfo`
This struct is passed to `SituationInit()` to configure the application at startup. It allows for detailed control over the initial state of the window, rendering backend, and timing systems.

> **Titanium Tip:** Field names use strictly `snake_case`. Ensure you are not using legacy `camelCase` names (e.g. `windowWidth`) from older versions.

```c
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
    uint32_t     flags;  // Bitfield: SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD

    // ── Audio Configuration ──
    uint32_t     max_audio_voices; // Max concurrent audio voices. 0 = Unlimited (Dynamic).

    int          render_thread_count; // Number of render threads to spawn (0 = Single Threaded)
    // [v2.3.22] Backpressure Policy: 0: Spin (Low Latency), 1: Yield (Balanced), 2: Sleep (Low CPU)
    int          backpressure_policy;

    // [v2.3.34] Async I/O
    uint32_t     io_queue_capacity; // Size of the IO queue (Low Priority). Default: 1024.
} SituationInitInfo;
```
-   `window_width`, `window_height`: The desired initial dimensions for the main window's client area.
-   `window_title`: The text to display in the window title bar.
-   `initial_active_window_flags`: A bitmask of `SituationWindowStateFlags` to set the initial state of the window when focused.
    -   **VSync Control:** To enable VSync, include the `SITUATION_FLAG_VSYNC_HINT` (or `SITUATION_WINDOW_STATE_VSYNC_HINT`) flag here. There is no separate boolean for VSync.
-   `initial_inactive_window_flags`: Flags to apply when the window loses focus (e.g., lower framerate).
-   `enable_vulkan_validation`: Enables Vulkan validation layers for debugging.
-   `io_queue_capacity`: The size of the low-priority IO queue for the dedicated IO thread.

**Note on Backend Selection:**
You do **not** select the graphics backend (OpenGL vs Vulkan) inside this struct. Instead, you must define either `SITUATION_USE_VULKAN` or `SITUATION_USE_OPENGL` in your code *before* including `situation.h`.

---
#### `SituationTimerSystem`
Manages all timing-related functionality for the application, including frame time (`deltaTime`), total elapsed time, and the Temporal Oscillator System. This struct is managed internally by the library; you interact with it through functions like `SituationGetFrameTime()` and `SituationTimerHasOscillatorUpdated()`.
```c
typedef struct SituationTimerSystem {
    double current_time;
    double previous_time;
    float frame_time;
    int target_fps;
    int oscillator_count;
    SituationTimerOscillator* oscillators;
} SituationTimerSystem;
```
-   `current_time`, `previous_time`: Internal timestamps used to calculate `frame_time`.
-   `frame_time`: The duration of the last frame in seconds (`deltaTime`).
-   `target_fps`: The current target frame rate.
-   `oscillator_count`: The number of active oscillators.
-   `oscillators`: A pointer to the internal array of oscillator states.

---
#### `SituationTimerOscillator`
Represents the internal state of a single temporal oscillator. This struct is managed by the library as part of the `SituationTimerSystem` and is not typically interacted with directly. Its properties are exposed through functions like `SituationTimerHasOscillatorUpdated()` and `SituationTimerGetOscillatorValue()`.
```c
typedef struct SituationTimerOscillator {
    double period;
    bool state;
    bool previous_state;
    double last_ping_time;
    uint64_t trigger_count;
} SituationTimerOscillator;
```
-   `period`: The duration of one full cycle of the oscillator in seconds.
-   `state`: The current binary state of the oscillator (`true` or `false`). This flips each time half of the `period` elapses.
-   `previous_state`: The state of the oscillator in the previous frame. Used to detect when the state has changed.
-   `last_ping_time`: An internal timestamp used by `SituationTimerPingOscillator()` to track time since the last successful "ping".
-   `trigger_count`: The total number of times the oscillator has flipped its state since initialization.

---
### Functions

#### `SituationInit`
Initializes all library subsystems. This is the entry point of the "Situation" library and **must be the first function you call**. It sets up the window, initializes the selected graphics backend (OpenGL or Vulkan), prepares the audio device, and processes any command-line arguments.

The function takes a pointer to a `SituationInitInfo` struct, which allows you to configure initial properties like window size, title, and desired frame rate. Passing `NULL` will initialize the library with default settings.
```c
SituationError SituationInit(int argc, char** argv, const SituationInitInfo* init_info);
```
**Usage Example:**
```c
int main(int argc, char* argv[]) {
    // Define the initial configuration for your application.
    SituationInitInfo init_info = {
        .app_name = "Core Example",
        .app_version = "1.0.0",
        .initial_width = 1280,
        .initial_height = 720,
        .window_flags = SITUATION_FLAG_WINDOW_RESIZABLE | SITUATION_FLAG_VSYNC_HINT,
        .target_fps = 60,
        .headless = false
    };

    // Initialize the library with the specified configuration.
    if (SituationInit(argc, argv, &init_info) != SIT_SUCCESS) {
        // If initialization fails, retrieve and print the error message.
        char* error_msg = SituationGetLastErrorMsg();
        fprintf(stderr, "Failed to initialize Situation: %s\n", error_msg);
        SituationFreeString(error_msg); // Free the error message string
        return -1;
    }

    // ... main application loop ...

    // Shutdown the library to release all resources.
    SituationShutdown();
    return 0;
}
```

---
#### `SituationPollInputEvents`
Polls for all pending input and window events from the operating system. This function is the first part of the mandatory three-phase frame structure and **must be called exactly once at the beginning of every frame**. It gathers all keyboard, mouse, gamepad, and window events (like resizing or closing) and stores them in an internal state buffer. All other input and window functions in the frame will operate on this consistent snapshot of the state.
```c
void SituationPollInputEvents(void);
```
**Usage Example:**
```c
while (!SituationWindowShouldClose()) {
    // --- 1. Input Phase ---
    SituationPollInputEvents(); // First call in the loop

    // --- 2. Update Phase ---
    SituationUpdateTimers();
    // ... game logic that relies on input ...

    // --- 3. Render Phase ---
    // ... rendering code ...
}
```

---
#### `SituationUpdateTimers`
Updates all internal timers. This is the second part of the mandatory three-phase frame structure and **must be called exactly once per frame**, immediately after `SituationPollInputEvents()`. This function calculates the `deltaTime` for the previous frame (accessible via `SituationGetFrameTime()`), updates the total elapsed time, and advances the state of the Temporal Oscillator System.
```c
void SituationUpdateTimers(void);
```
**Usage Example:**
```c
while (!SituationWindowShouldClose()) {
    // --- 1. Input Phase ---
    SituationPollInputEvents();

    // --- 2. Update Phase ---
    SituationUpdateTimers(); // Second call in the loop

    // Now it's safe to get the frame time for physics and logic.
    float dt = SituationGetFrameTime();
    player_position.x += player_velocity.x * dt;

    // --- 3. Render Phase ---
    // ... rendering code ...
}
```

---
#### `SituationShutdown`
Shuts down all library subsystems, releases all GPU and CPU resources, and closes the application window. This **must be the last function called** before your application exits. It ensures a graceful cleanup and will report any leaked GPU resources (in debug mode) if you forgot to destroy them.
```c
void SituationShutdown(void);
```
**Usage Example:**
```c
int main(int argc, char* argv[]) {
    // It's good practice to pair Init and Shutdown in the same scope.
    if (SituationInit(argc, argv, NULL) != SIT_SUCCESS) {
        return -1;
    }

    while (!SituationWindowShouldClose()) {
        // Main application loop
    }

    SituationShutdown(); // The very last call.
    return 0;
}
```

---
#### `SituationIsInitialized`
Checks if the library has been successfully initialized. Returns `true` if `SituationInit()` has been called and completed without errors, `false` otherwise.
```c
bool SituationIsInitialized(void);
```
**Usage Example:**
```c
// You might use this in a helper function to ensure it's safe to call library functions.
void UpdatePlayer() {
    if (!SituationIsInitialized()) {
        printf("Error: Cannot update player before the library is initialized.\n");
        return;
    }
    // ... proceed with player update logic ...
}
```

---
#### `SituationWindowShouldClose`
Returns `true` if the user has attempted to close the window (e.g., by clicking the 'X' button, pressing Alt+F4, or sending a quit signal). This is the canonical way to control your main application loop.
```c
bool SituationWindowShouldClose(void);
```
**Usage Example:**
```c
// The main loop should continue as long as this function returns false.
while (!SituationWindowShouldClose()) {
    // Poll events, update logic, render frame
}
// Loop terminates, and the application proceeds to shutdown.
```

---
#### `SituationPauseApp`
Pauses the application's internal state, including audio playback. This is useful when the application loses focus or when implementing a pause menu.
```c
void SituationPauseApp(void);
```
**Usage Example:**
```c
// Pause the application when the window loses focus
void on_focus_changed(bool focused, void* user_data) {
    if (!focused) {
        SituationPauseApp();
    } else {
        SituationResumeApp();
    }
}
```

---
#### `SituationResumeApp`
Resumes a paused application, restoring audio playback and other internal systems.
```c
void SituationResumeApp(void);
```
**Usage Example:**
```c
// Resume the application when the player unpauses
if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
    if (SituationIsAppPaused()) {
        SituationResumeApp();
    } else {
        SituationPauseApp();
    }
}
```

---
#### `SituationIsAppPaused`
Checks if the application is currently paused.
```c
bool SituationIsAppPaused(void);
```
**Usage Example:**
```c
// Skip game logic updates when paused
if (!SituationIsAppPaused()) {
    UpdateGameLogic(deltaTime);
}
```

---
#### `SituationSetTargetFPS`
Sets a target frame rate for the application. The main loop will sleep to avoid exceeding this rate, reducing CPU usage.
```c
void SituationSetTargetFPS(int fps);
```
**Usage Example:**
```c
// Cap the application at 60 FPS.
SituationSetTargetFPS(60);
// To uncap the frame rate, use 0.
// SituationSetTargetFPS(0);
```

---
#### `SituationGetFrameTime`
Gets the time in seconds that the previous frame took to complete, commonly known as `deltaTime`. This value is calculated by `SituationUpdateTimers()` and is essential for creating frame-rate-independent logic for movement, physics, and animations.
```c
float SituationGetFrameTime(void);
```
**Usage Example:**
```c
// Inside the main loop, after SituationUpdateTimers()
float dt = SituationGetFrameTime();

// Update player position based on time, not frames, ensuring smooth movement
// regardless of the frame rate.
player_position.x += player_speed * dt;
```

---
#### `SituationGetFPS`
Gets the current frames-per-second, calculated periodically by the library. This is useful for displaying performance metrics to the user.
```c
int SituationGetFPS(void);
```
**Usage Example:**
```c
// Inside the main loop, update the window title with the current FPS.
int current_fps = SituationGetFPS();
char window_title[128];
sprintf(window_title, "My App | FPS: %d", current_fps);
SituationSetWindowTitle(window_title);
```

---
#### `SituationGetLastErrorMsg`
Retrieves a copy of the last error message generated by the library. This is useful for debugging initialization failures or other runtime errors. The caller is responsible for freeing the returned string with `SituationFreeString()`.
```c
SituationError SituationGetLastErrorMsg(char** out_msg);
```
**Usage Example:**
```c
if (SituationInit(argc, argv, &init_info) != SITUATION_SUCCESS) {
    char* error_msg = NULL;
    if (SituationGetLastErrorMsg(&error_msg) == SITUATION_SUCCESS) {
        fprintf(stderr, "Initialization failed: %s\n", error_msg);
        SituationFreeString(error_msg); // IMPORTANT: Free the memory
    }
    return -1;
}
```

---
#### `SituationSetExitCallback`
Registers a callback function to be executed just before the library shuts down. This is useful for performing final cleanup tasks, such as saving application state.
```c
typedef void (*SituationExitCallback)(void* user_data);
void SituationSetExitCallback(SituationExitCallback callback, void* user_data);
```
**Usage Example:**
```c
void on_exit_cleanup(void* user_data) {
    const char* message = (const char*)user_data;
    printf("Shutting down. Custom data: %s\n", message);
    // You could save game settings or user progress here.
}

// In main, after Init
const char* my_data = "Saving settings...";
SituationSetExitCallback(on_exit_cleanup, (void*)my_data);
```

---
#### `SituationIsArgumentPresent`
Checks if a specific command-line argument flag (e.g., `"-server"`) was provided when the application was launched. The library automatically parses `argv` during `SituationInit`.
```c
bool SituationIsArgumentPresent(const char* arg_name);
```
**Usage Example:**
```c
// Run as: ./my_app -fullscreen -debug
int main(int argc, char* argv[]) {
    SituationInit(argc, argv, NULL);

    // Check for a simple flag to enable fullscreen mode at startup.
    if (SituationIsArgumentPresent("-fullscreen")) {
        SituationToggleFullscreen();
    }
    // Check for a debug flag.
    if (SituationIsArgumentPresent("-debug")) {
        g_enable_debug_mode = true;
    }
}
```

---
#### `SituationGetArgumentValue`
Gets the value associated with a command-line argument. It supports both colon-separated (`-level:jungle`) and space-separated (`-level jungle`) formats. Returns `NULL` if the argument is not found.
```c
const char* SituationGetArgumentValue(const char* arg_name);
```
**Usage Example:**
```c
// Run as: ./my_app -level:forest -player "Jules"
int main(int argc, char* argv[]) {
    SituationInit(argc, argv, NULL);

    // Get the level name to load.
    const char* level_name = SituationGetArgumentValue("-level");
    if (level_name) {
        printf("Loading level: %s\n", level_name);
    } else {
        printf("Loading default level.\n");
    }

    // Get the player's name.
    const char* player_name = SituationGetArgumentValue("-player");
    if (player_name) {
        printf("Welcome, %s!\n", player_name);
    }
}
```

---
#### `SituationGetVersionString`
Gets the version of the Situation library as a string.
```c
SITAPI const char* SituationGetVersionString(void);
```
**Usage Example:**
```c
const char* version = SituationGetVersionString();
printf("Situation library version: %s\n", version);
```

---
#### `SituationGetGPUName`
Gets the human-readable name of the active GPU.
```c
SITAPI const char* SituationGetGPUName(void);
```
**Usage Example:**
```c
const char* gpu_name = SituationGetGPUName();
printf("GPU: %s\n", gpu_name);
```

---
#### `SituationGetVRAMUsage`
Gets the estimated total Video RAM (VRAM) usage in bytes. This is a best-effort query and may not be perfectly accurate on all platforms.
```c
SITAPI uint64_t SituationGetVRAMUsage(void);
```
**Usage Example:**
```c
uint64_t vram_usage = SituationGetVRAMUsage();
printf("VRAM Usage: %.2f MB\n", (double)vram_usage / (1024.0 * 1024.0));
```

---
#### `SituationGetDrawCallCount`
Gets the number of draw calls submitted in the last completed frame. This is a key performance metric for identifying rendering bottlenecks.
```c
SITAPI uint32_t SituationGetDrawCallCount(void);
```
**Usage Example:**
```c
// In the update loop, display the draw call count in the window title.
char title[256];
sprintf(title, "My App | FPS: %d | Draw Calls: %u",
        SituationGetFPS(), SituationGetDrawCallCount());
SituationSetWindowTitle(title);
```

---
#### `SituationExecuteCommand`
Executes a system shell command in a hidden process and captures the output (stdout/stderr).
```c
SITAPI int SituationExecuteCommand(const char *cmd, char **output);
```
**Usage Example:**
```c
char* output = NULL;
int exit_code = SituationExecuteCommand("ls -la", &output);
if (exit_code == 0 && output) {
    printf("Command output:\n%s\n", output);
    SituationFreeString(output);
}
```

---
#### `SituationGetCPUThreadCount`
Gets the number of logical CPU cores available on the system. This is useful for determining optimal thread pool sizes or parallelization strategies.

```c
uint32_t SituationGetCPUThreadCount(void);
```

**Returns:** Number of logical CPU cores (includes hyperthreading/SMT cores)

**Usage Example:**
```c
// Create a thread pool sized for the system
uint32_t core_count = SituationGetCPUThreadCount();
printf("System has %u logical cores\n", core_count);

// Use 75% of cores for worker threads, leaving some for OS/other apps
uint32_t worker_threads = (core_count * 3) / 4;
if (worker_threads < 1) worker_threads = 1;

SituationThreadPool* pool = SituationCreateThreadPool(worker_threads);

// Or use for parallel processing decisions
if (core_count >= 8) {
    // Use multi-threaded asset loading
    LoadAssetsParallel();
} else {
    // Use single-threaded loading on low-core systems
    LoadAssetsSequential();
}
```

**Notes:**
- Returns logical cores, not physical cores (includes hyperthreading)
- On systems with heterogeneous cores (e.g., ARM big.LITTLE), returns total count
- Useful for sizing thread pools and making parallelization decisions

---
#### `SituationGetMaxComputeWorkGroups`
Queries the maximum supported compute shader work group count for each dimension (X, Y, Z). This tells you the hardware limits for `SituationCmdDispatch()` calls.

```c
void SituationGetMaxComputeWorkGroups(uint32_t* x, uint32_t* y, uint32_t* z);
```

**Parameters:**
- `x` - Pointer to receive maximum work groups in X dimension
- `y` - Pointer to receive maximum work groups in Y dimension
- `z` - Pointer to receive maximum work groups in Z dimension

**Usage Example:**
```c
// Query hardware limits
uint32_t max_x, max_y, max_z;
SituationGetMaxComputeWorkGroups(&max_x, &max_y, &max_z);
printf("Max compute work groups: %u x %u x %u\n", max_x, max_y, max_z);

// Ensure dispatch doesn't exceed limits
uint32_t required_groups_x = (particle_count + 255) / 256;
if (required_groups_x > max_x) {
    printf("Error: Too many particles for single dispatch\n");
    // Split into multiple dispatches or use 2D/3D layout
}

// Safe dispatch
SituationCmdDispatch(cmd, 
    min(required_groups_x, max_x), 
    1, 
    1);
```

**Advanced Example (2D Image Processing):**
```c
// Process a large image with compute shader
int image_width = 4096, image_height = 4096;
int local_size_x = 16, local_size_y = 16;

// Calculate required work groups
uint32_t groups_x = (image_width + local_size_x - 1) / local_size_x;
uint32_t groups_y = (image_height + local_size_y - 1) / local_size_y;

// Check against hardware limits
uint32_t max_x, max_y, max_z;
SituationGetMaxComputeWorkGroups(&max_x, &max_y, &max_z);

if (groups_x <= max_x && groups_y <= max_y) {
    // Single dispatch covers entire image
    SituationCmdDispatch(cmd, groups_x, groups_y, 1);
} else {
    // Need to tile the dispatch
    printf("Image too large, using tiled dispatch\n");
    // Implement tiling logic...
}
```

**Notes:**
- Typical values: 65535 x 65535 x 65535 (Vulkan minimum guarantee)
- OpenGL may have lower limits on older hardware
- Total invocations = groups_x × groups_y × groups_z × local_size_x × local_size_y × local_size_z
- Query once at initialization and cache the values
- Use 2D or 3D dispatches for large datasets that exceed 1D limits

---
#### `SituationLogWarning`
Logs a warning message in debug builds.
```c
SITAPI void SituationLogWarning(SituationError code, const char* fmt, ...);
```
**Usage Example:**
```c
if (score > 9000) {
    SituationLogWarning(SITUATION_ERROR_GENERAL, "Score is over 9000!");
}
```

---
#### `SituationGetDeviceInfo`
Gathers and returns a comprehensive snapshot of the host system's hardware and operating system. This is useful for logging, debugging, or adjusting application settings based on the user's hardware.
```c
SituationDeviceInfo SituationGetDeviceInfo(void);
```
**Usage Example:**
```c
// Log device information at startup.
SituationInit(argc, argv, NULL);
SituationDeviceInfo device = SituationGetDeviceInfo();
printf("--- System Information ---\n");
printf("OS: %s %s\n", device.os_name, device.os_version);
printf("CPU: %s (%d cores, %d threads)\n", device.cpu_brand, device.cpu_core_count, device.cpu_thread_count);
printf("RAM: %.2f GB\n", (double)device.system_ram_bytes / (1024.0*1024.0*1024.0));
printf("GPU: %s\n", device.gpu_brand);
printf("VRAM: %.2f GB\n", (double)device.gpu_vram_bytes / (1024.0*1024.0*1024.0));
printf("--------------------------\n");
```

---
#### `SituationGetTime`
Gets the total elapsed time in seconds since `SituationInit()` was called. This is a high-precision monotonic timer.
```c
double SituationGetTime(void);
```
**Usage Example:**
```c
// Use the total elapsed time to drive a continuous animation, like a rotation.
double current_time = SituationGetTime();
mat4 rotation_matrix;
glm_rotate_y(model_matrix, (float)current_time * 0.5f, rotation_matrix); // Rotate over time
```

---
#### `SituationGetFrameCount`
Gets the total number of frames that have been rendered since the application started.
```c
uint64_t SituationGetFrameCount(void);
```
**Usage Example:**
```c
// Use the frame count for simple, periodic logic that doesn't need to be tied to real time.
if (SituationGetFrameCount() % 120 == 0) {
    printf("120 frames have passed.\n");
}
```

---
#### `SituationWaitTime`
Pauses the application thread for a specified duration in seconds. This is a simple wrapper over the system's sleep function and can be useful for debugging or simple timing.
```c
void SituationWaitTime(double seconds);
```
**Usage Example:**
```c
printf("Preparing to load assets...\n");
// Wait for 500 milliseconds before proceeding to give the user time to read the message.
SituationWaitTime(0.5);
printf("Loading...\n");
```

---
#### `SituationEnableEventWaiting`
Enables event waiting. When enabled, `SituationPollInputEvents()` will wait for new events instead of immediately returning, putting the application to sleep and saving CPU cycles when idle. This is ideal for tools and non-game applications.
```c
void SituationEnableEventWaiting(void);
```
**Usage Example:**
```c
// In an editor or tool, enable event waiting to reduce resource usage.
SituationEnableEventWaiting();
while (!SituationWindowShouldClose()) {
    SituationPollInputEvents(); // This will now block until an event occurs.
    // ... update UI or process data only when there are new events ...
    // ... render ...
}
```

---
#### `SituationDisableEventWaiting`
Disables event waiting, restoring the default behavior where `SituationPollInputEvents()` returns immediately. This is necessary for real-time applications like games that need to run the update loop continuously.
```c
void SituationDisableEventWaiting(void);
```
**Usage Example:**
```c
// When switching from an editor mode to a real-time game simulation.
SituationDisableEventWaiting();
```

---
#### `SituationOpenFile`
Asks the operating system to open a file or folder with its default application.
```c
void SituationOpenFile(const char* filePath);
```
**Usage Example:**
```c
// This will open the specified file in its default application (e.g., Notepad).
SituationOpenFile("C:/path/to/your/log.txt");

// This will open the specified directory in the file explorer.
SituationOpenFile("C:/Users/Default/Documents");
```

---
#### `SituationOpenURL`
Asks the operating system to open a URL in the default web browser.
```c
void SituationOpenURL(const char* url);
```
**Usage Example:**
```c
// This will open the user's web browser to the specified URL.
SituationOpenURL("https://www.github.com");
```

---
#### `SituationSetErrorCallback`
Sets a callback for handling library errors.
```c
void SituationSetErrorCallback(SituationErrorCallback callback);
```
**Usage Example:**
```c
void my_error_logger(int error_code, const char* message) {
    fprintf(stderr, "Situation Error [%d]: %s\n", error_code, message);
}

// In main, after Init
SituationSetErrorCallback(my_error_logger);
```

---
#### `SituationSetVSync`
Enables or disables VSync.
```c
void SituationSetVSync(bool enabled);
```
**Usage Example:**
```c
// Disable VSync for performance testing
SituationSetVSync(false);
```

---
#### `SituationGetPlatform`
Gets the current platform.
```c
int SituationGetPlatform(void);
```
**Usage Example:**
```c
int platform = SituationGetPlatform();
#if defined(PLATFORM_DESKTOP)
    if (platform == PLATFORM_DESKTOP) printf("Running on a desktop platform.\n");
#endif
```

---
#### `SituationUpdate`
**[DEPRECATED]** This function has been split into separate calls for better control. Use `SituationPollInputEvents()` and `SituationUpdateTimers()` instead.

```c
void SituationUpdate(void);  // DEPRECATED
```

**Migration Guide:**

**Old Code:**
```c
while (!SituationShouldClose()) {
    SituationUpdate();  // DEPRECATED
    
    // Game logic
    UpdateGame();
    RenderGame();
}
```

**New Code:**
```c
while (!SituationShouldClose()) {
    SituationPollInputEvents();  // Process input events
    SituationUpdateTimers();     // Update oscillators and timers
    
    // Game logic
    UpdateGame();
    RenderGame();
}
```

**Why the change?**
- Better control over update order
- Can skip timer updates if needed
- More explicit about what's being updated
- Allows custom timing logic

**Notes:**
- `SituationUpdate()` still works but is deprecated
- Internally calls `SituationPollInputEvents()` and `SituationUpdateTimers()`
- Will be removed in a future version

---
#### `SituationSetResizeCallback`
Sets a callback function for window framebuffer resize events.
```c
void SituationSetResizeCallback(void (*callback)(int width, int height, void* user_data), void* user_data);
```

---

#### `SituationSetFocusCallback`

Sets a callback function to be called when the window gains or loses focus.

```c
SITAPI void SituationSetFocusCallback(SituationFocusCallback callback, void* user_data);
```

**Usage Example:**
```c
void OnFocusChanged(bool focused, void* user_data) {
    printf("Window focus changed: %d\n", focused);
}

SituationSetFocusCallback(OnFocusChanged, NULL);
```

---
#### `SituationSetFocusCallback`
Sets a callback for window focus events.
```c
void SituationSetFocusCallback(SituationFocusCallback callback, void* user_data);
```

---
#### `SituationSetFileDropCallback`
Sets a callback for file drop events.
```c
void SituationSetFileDropCallback(SituationFileDropCallback callback, void* user_data);
```

---
#### `SituationGetUserDirectory`
Gets the full path to the current user's home directory. The returned string must be freed with `SituationFreeString()`.

```c
char* SituationGetUserDirectory(void);
```

**Returns:** Dynamically allocated string containing the user's home directory path (must be freed)

**Usage Example:**
```c
// Get user's home directory for storing user-specific files
char* home_dir = SituationGetUserDirectory();
printf("User home directory: %s\n", home_dir);

// Build a path to a user-specific config file
char config_path[512];
snprintf(config_path, sizeof(config_path), "%s/.mygame/config.ini", home_dir);

// Load user config
if (SituationFileExists(config_path)) {
    // Load existing config
    char* config_data = SituationLoadFileText(config_path);
    ParseConfig(config_data);
    SituationFreeString(config_data);
}

// Always free the returned string
SituationFreeString(home_dir);
```

**Notes:**
- Returns platform-specific home directory:
  - Windows: `C:\Users\Username`
  - Linux: `/home/username`
  - macOS: `/Users/username`
- The returned string must be freed with `SituationFreeString()`
- For application-specific save data, consider using `SituationGetAppSavePath()` instead

---
#### `SituationGetCurrentDriveLetter`
Gets the drive letter of the running executable (Windows only). Returns '\0' on non-Windows platforms.

```c
char SituationGetCurrentDriveLetter(void);
```

**Returns:** Drive letter (e.g., 'C', 'D') or '\0' on error/non-Windows

**Usage Example:**
```c
// Get current drive
char drive = SituationGetCurrentDriveLetter();
if (drive != '\0') {
    printf("Running on drive: %c:\n", drive);
}

// Check if running on specific drive
char current_drive = SituationGetCurrentDriveLetter();
if (current_drive == 'C') {
    printf("Running on system drive\n");
} else {
    printf("Running on external drive: %c:\n", current_drive);
}

// Build path on current drive
char drive = SituationGetCurrentDriveLetter();
char save_path[256];
snprintf(save_path, sizeof(save_path), "%c:/GameSaves/save.dat", drive);

// Cross-platform check
#ifdef _WIN32
    char drive = SituationGetCurrentDriveLetter();
    printf("Windows drive: %c:\n", drive);
#else
    printf("Not on Windows, no drive letters\n");
#endif
```

**Notes:**
- **Windows only** - returns '\0' on Linux/macOS
- Returns uppercase letter (A-Z)
- Useful for portable app installations
- Use `SituationGetBasePath()` for cross-platform paths

---
#### `SituationGetDriveInfo`
**[Windows Only]** Gets information about a specific drive including total capacity, free space, and volume name.

```c
bool SituationGetDriveInfo(char drive_letter, uint64_t* out_total_capacity_bytes, uint64_t* out_free_space_bytes, char* out_volume_name, int volume_name_len);
```

**Parameters:**
- `drive_letter` - Drive letter to query (e.g., 'C', 'D')
- `out_total_capacity_bytes` - Pointer to receive total drive capacity
- `out_free_space_bytes` - Pointer to receive free space available
- `out_volume_name` - Buffer to receive volume name (can be NULL)
- `volume_name_len` - Size of volume name buffer

**Returns:** `true` if drive exists and info retrieved, `false` otherwise

**Usage Example:**
```c
// Check C: drive space
uint64_t total_bytes, free_bytes;
char volume_name[256];

if (SituationGetDriveInfo('C', &total_bytes, &free_bytes, volume_name, sizeof(volume_name))) {
    float total_gb = total_bytes / (1024.0f * 1024.0f * 1024.0f);
    float free_gb = free_bytes / (1024.0f * 1024.0f * 1024.0f);
    float used_percent = ((total_bytes - free_bytes) / (float)total_bytes) * 100.0f;
    
    printf("Drive C: [%s]\n", volume_name);
    printf("Total: %.2f GB\n", total_gb);
    printf("Free: %.2f GB\n", free_gb);
    printf("Used: %.1f%%\n", used_percent);
    
    // Warn if low on space
    if (free_gb < 10.0f) {
        printf("WARNING: Low disk space!\n");
    }
}

// Check if save location has enough space
uint64_t required_space = 500 * 1024 * 1024;  // 500 MB
if (SituationGetDriveInfo('D', NULL, &free_bytes, NULL, 0)) {
    if (free_bytes < required_space) {
        printf("Not enough space to save game\n");
    }
}
```

**Notes:**
- Windows only - returns `false` on other platforms
- Drive letter is case-insensitive
- Volume name can be NULL if not needed
- Useful for save game space checks and installation validation

---
#### `SituationFreeDisplays`
Frees the memory allocated for the display list returned by `SituationGetDisplays()`. Always call this after you're done using the display information.

```c
void SituationFreeDisplays(SituationDisplayInfo* displays, int count);
```

**Parameters:**
- `displays` - Pointer to the display array to free
- `count` - Number of displays in the array

**Usage Example:**
```c
// Get display information
int display_count;
SituationDisplayInfo* displays = SituationGetDisplays(&display_count);

// Use the display info
for (int i = 0; i < display_count; i++) {
    printf("Display %d: %dx%d @ %dHz\n",
        i,
        displays[i].width,
        displays[i].height,
        displays[i].refresh_rate);
}

// Always free when done
SituationFreeDisplays(displays, display_count);
```

**Notes:**
- Must be called to avoid memory leaks
- Pass the same count returned by `SituationGetDisplays()`
- Safe to call with NULL pointer (does nothing)

---
#### `SituationGetWindowSize`
Gets the current logical window size in screen coordinates. This is the size of the window's client area, excluding decorations.

```c
void SituationGetWindowSize(int* width, int* height);
```

**Parameters:**
- `width` - Pointer to receive window width
- `height` - Pointer to receive window height

**Usage Example:**
```c
// Get current window size
int window_width, window_height;
SituationGetWindowSize(&window_width, &window_height);
printf("Window size: %dx%d\n", window_width, window_height);

// Calculate aspect ratio
float aspect_ratio = (float)window_width / (float)window_height;
printf("Aspect ratio: %.2f:1\n", aspect_ratio);

// Adjust viewport to match window
SituationCmdSetViewport(cmd, 0, 0, window_width, window_height);

// Center UI element
int ui_width = 400, ui_height = 300;
int ui_x = (window_width - ui_width) / 2;
int ui_y = (window_height - ui_height) / 2;
DrawUIPanel(ui_x, ui_y, ui_width, ui_height);

// Check if window was resized
static int last_width = 0, last_height = 0;
if (window_width != last_width || window_height != last_height) {
    printf("Window resized from %dx%d to %dx%d\n",
        last_width, last_height, window_width, window_height);
    OnWindowResize(window_width, window_height);
    last_width = window_width;
    last_height = window_height;
}
```

**Notes:**
- Returns logical size, not physical pixels (important for high-DPI displays)
- For render target size, use `SituationGetRenderWidth()` and `SituationGetRenderHeight()`
- Window size may differ from render size due to DPI scaling
- Excludes window decorations (title bar, borders)

---
#### `SituationGetRendererType`
Gets the current active graphics renderer backend (OpenGL or Vulkan). This is determined at initialization time based on system capabilities and user preferences.

```c
SituationRendererType SituationGetRendererType(void);
```

**Returns:** `SITUATION_RENDERER_OPENGL` or `SITUATION_RENDERER_VULKAN`

**Usage Example:**
```c
// Check which renderer is active
SituationRendererType renderer = SituationGetRendererType();

if (renderer == SITUATION_RENDERER_VULKAN) {
    printf("Running on Vulkan backend\n");
    // Enable Vulkan-specific optimizations
    EnableBindlessTextures();
    EnableAsyncCompute();
} else if (renderer == SITUATION_RENDERER_OPENGL) {
    printf("Running on OpenGL backend\n");
    // Use OpenGL-compatible code paths
    DisableBindlessTextures();
}

// Adjust quality settings based on renderer
if (renderer == SITUATION_RENDERER_VULKAN) {
    // Vulkan can handle more complex effects
    SetShadowQuality(QUALITY_ULTRA);
    SetParticleCount(10000);
} else {
    // OpenGL may need lower settings
    SetShadowQuality(QUALITY_HIGH);
    SetParticleCount(5000);
}
```

**Notes:**
- The renderer is selected at `SituationInit()` time and cannot be changed at runtime
- Vulkan is preferred when available for better performance
- OpenGL is used as a fallback on older systems or when Vulkan is unavailable
- Some features (like bindless textures) may only be available on Vulkan

---
#### `SituationGetGLFWwindow`
Gets the raw GLFW window handle.
```c
GLFWwindow* SituationGetGLFWwindow(void);
```

---
#### `SituationGetVulkanInstance`
Gets the raw Vulkan instance handle for advanced Vulkan interop. This allows direct access to Vulkan API for custom extensions or advanced features.

```c
VkInstance SituationGetVulkanInstance(void);
```

**Returns:** Vulkan instance handle, or NULL if using OpenGL backend

**Usage Example:**
```c
// Query Vulkan extension support
VkInstance instance = SituationGetVulkanInstance();
if (instance != NULL) {
    // Use Vulkan API directly
    uint32_t extension_count;
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
    printf("Vulkan extensions available: %u\n", extension_count);
}
```

**Notes:**
- Returns NULL on OpenGL backend
- Use for advanced Vulkan features not exposed by Situation API
- Be careful not to interfere with Situation's internal state

---
#### `SituationGetVulkanDevice`
Gets the raw Vulkan logical device handle for advanced Vulkan interop.

```c
VkDevice SituationGetVulkanDevice(void);
```

**Returns:** Vulkan device handle, or NULL if using OpenGL backend

**Usage Example:**
```c
// Create custom Vulkan resources
VkDevice device = SituationGetVulkanDevice();
if (device != NULL) {
    // Use Vulkan API for custom operations
    VkFence custom_fence;
    VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(device, &fence_info, NULL, &custom_fence);
}
```

**Notes:**
- Returns NULL on OpenGL backend
- Useful for creating custom Vulkan objects
- Ensure compatibility with Situation's resource management

---
#### `SituationGetVulkanPhysicalDevice`
Gets the raw Vulkan physical device handle for querying hardware capabilities.

```c
VkPhysicalDevice SituationGetVulkanPhysicalDevice(void);
```

**Returns:** Vulkan physical device handle, or NULL if using OpenGL backend

**Usage Example:**
```c
// Query hardware properties
VkPhysicalDevice physical_device = SituationGetVulkanPhysicalDevice();
if (physical_device != NULL) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device, &props);
    printf("GPU: %s\n", props.deviceName);
    printf("Vulkan API: %u.%u.%u\n",
        VK_VERSION_MAJOR(props.apiVersion),
        VK_VERSION_MINOR(props.apiVersion),
        VK_VERSION_PATCH(props.apiVersion));
}
```

**Notes:**
- Returns NULL on OpenGL backend
- Use for querying hardware limits and features
- Read-only queries are safe

---
#### `SituationGetMainWindowRenderPass`
**[Vulkan Only]** Gets the Vulkan render pass handle for the main window's swapchain. This is useful for advanced Vulkan interop and custom rendering.

```c
VkRenderPass SituationGetMainWindowRenderPass(void);
```

**Returns:** Vulkan render pass handle, or NULL if using OpenGL backend

**Usage Example:**
```c
// Get main window render pass for custom Vulkan operations
VkRenderPass main_render_pass = SituationGetMainWindowRenderPass();
if (main_render_pass != VK_NULL_HANDLE) {
    // Use for creating compatible framebuffers or pipelines
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .renderPass = main_render_pass,
        // ... other pipeline settings ...
    };
}
```

**Notes:**
- Returns VK_NULL_HANDLE on OpenGL backend
- The render pass is managed by Situation - do not destroy it
- Useful for creating custom Vulkan pipelines compatible with the main window
- Render pass format matches the swapchain format

</details>
<details>
<summary><h3>Window and Display Module</h3></summary>

**Overview:** This module provides an exhaustive suite of tools for managing the application's window and querying the properties of physical display devices. It handles everything from basic window state changes (fullscreen, minimized, borderless) to more advanced features like DPI scaling, opacity, and clipboard access.

### Structs and Flags

#### `SituationDeviceInfo`
This struct, returned by `SituationGetDeviceInfo()`, provides a snapshot of the host system's hardware.
```c
typedef struct SituationDeviceInfo {
    char cpu_brand[49];
    int cpu_core_count;
    int cpu_thread_count;
    uint64_t system_ram_bytes;
    char gpu_brand[128];
    uint64_t gpu_vram_bytes;
    int display_count;
    char os_name[32];
    char os_version[32];
    uint64_t total_storage_bytes;
    uint64_t free_storage_bytes;
} SituationDeviceInfo;
```
-   `cpu_brand`: The brand and model of the CPU.
-   `cpu_core_count`, `cpu_thread_count`: The number of physical cores and logical threads.
-   `system_ram_bytes`: Total system RAM in bytes.
-   `gpu_brand`: The brand and model of the GPU.
-   `gpu_vram_bytes`: Total dedicated video RAM in bytes.
-   `display_count`: The number of connected displays.
-   `os_name`, `os_version`: The name and version of the operating system.
-   `total_storage_bytes`, `free_storage_bytes`: The total and free space on the primary storage device.

---
#### `SituationDisplayInfo`
Returned by `SituationGetDisplays()`, this struct contains detailed information about a connected monitor.
```c
typedef struct SituationDisplayInfo {
    int id;
    char name[128];
    int current_mode;
    int mode_count;
    SituationDisplayMode* modes;
    vec2 position;
    vec2 physical_size;
} SituationDisplayInfo;
```
-   `id`: The internal ID of the monitor.
-   `name`: The human-readable name of the monitor.
-   `current_mode`: The index of the display's current mode in the `modes` array.
-   `mode_count`: The number of available display modes.
-   `modes`: A pointer to an array of `SituationDisplayMode` structs.
-   `position`: The physical position of the monitor's top-left corner on the virtual desktop.
-   `physical_size`: The physical size of the display in millimeters.

---
#### `SituationDisplayMode`
Represents a single supported display mode (resolution, refresh rate, etc.) for a monitor.
```c
typedef struct SituationDisplayMode {
    int width;
    int height;
    int refresh_rate;
    int red_bits;
    int green_bits;
    int blue_bits;
} SituationDisplayMode;
```
-   `width`, `height`: The resolution of the display mode in pixels.
-   `refresh_rate`: The refresh rate in Hertz (Hz).
-   `red_bits`, `green_bits`, `blue_bits`: The color depth for each channel.

---
#### `SituationWindowStateFlags`
These flags are used with `SituationSetWindowState()` and `SituationClearWindowState()` to control the window's appearance and behavior. They can be combined using the bitwise `|` operator.
| Flag | Description |
|---|---|
| `SITUATION_FLAG_VSYNC_HINT` | Suggests that the graphics backend should wait for vertical sync, reducing screen tearing. |
| `SITUATION_FLAG_FULLSCREEN_MODE` | Enables exclusive fullscreen mode. |
| `SITUATION_FLAG_WINDOW_RESIZABLE` | Allows the user to resize the window. |
| `SITUATION_FLAG_WINDOW_UNDECORATED` | Removes the window's border, title bar, and other decorations. |
| `SITUATION_FLAG_WINDOW_HIDDEN` | Hides the window from view. |
| `SITUATION_FLAG_WINDOW_MINIMIZED` | Minimizes the window to the taskbar. |
| `SITUATION_FLAG_WINDOW_MAXIMIZED` | Maximizes the window to fill the work area. |
| `SITUATION_FLAG_WINDOW_UNFOCUSED` | Prevents the window from gaining focus when created. |
| `SITUATION_FLAG_WINDOW_TOPMOST` | Keeps the window on top of all other windows. |
| `SITUATION_FLAG_WINDOW_ALWAYS_RUN` | Allows the application to continue running even when the window is minimized. |
| `SITUATION_FLAG_WINDOW_TRANSPARENT` | Enables a transparent framebuffer for non-rectangular window shapes. |
| `SITUATION_FLAG_HIGHDPI_HINT` | Requests a high-DPI framebuffer on platforms that support it (e.g., macOS Retina). |
| `SITUATION_FLAG_MSAA_4X_HINT` | Suggests that the graphics backend should use 4x multisample anti-aliasing. |

#### Functions
### Functions

#### `SituationSetWindowState`
Sets one or more window state flags, such as `SITUATION_FLAG_WINDOW_RESIZABLE` or `SITUATION_FLAG_WINDOW_TOPMOST`. You can combine multiple flags using the bitwise OR `|` operator.
```c
void SituationSetWindowState(uint32_t flags);
```
**Usage Example:**
```c
// Make the window resizable and always on top.
SituationSetWindowState(SITUATION_FLAG_WINDOW_RESIZABLE | SITUATION_FLAG_WINDOW_TOPMOST);
```

---
#### `SituationClearWindowState`
Clears one or more window state flags, reverting them to their default behavior.
```c
void SituationClearWindowState(uint32_t flags);
```
**Usage Example:**
```c
// The window was previously set to be always on top.
// This will remove that state, allowing it to behave like a normal window again.
SituationClearWindowState(SITUATION_FLAG_WINDOW_TOPMOST);
```

---
#### `SituationToggleFullscreen`
Toggles the window between exclusive fullscreen and windowed mode. This is a convenient wrapper around `SituationSetWindowState` and `SituationClearWindowState` for the `SITUATION_FLAG_FULLSCREEN_MODE` flag.
```c
void SituationToggleFullscreen(void);
```
**Usage Example:**
```c
// In the update loop, toggle fullscreen when F11 is pressed.
if (SituationIsKeyPressed(SIT_KEY_F11)) {
    SituationToggleFullscreen();
}
```

---
#### `SituationSetWindowTitle`
Sets the text that appears in the window's title bar.
```c
void SituationSetWindowTitle(const char *title);
```
**Usage Example:**
```c
// You can dynamically update the title to show game state or performance info.
char title[128];
sprintf(title, "My Game | Score: %d | FPS: %d", g_player_score, SituationGetFPS());
SituationSetWindowTitle(title);
```

---
#### `SituationSetWindowPosition`
Sets the screen-space position of the top-left corner of the window's client area.
```c
void SituationSetWindowPosition(int x, int y);
```
**Usage Example:**
```c
// Center the window on the primary monitor.
int monitor_width = SituationGetMonitorWidth(0);
int monitor_height = SituationGetMonitorHeight(0);
int window_width = SituationGetScreenWidth();
int window_height = SituationGetScreenHeight();
int x_pos = (monitor_width / 2) - (window_width / 2);
int y_pos = (monitor_height / 2) - (window_height / 2);
SituationSetWindowPosition(x_pos, y_pos);
```

---
#### `SituationGetWindowPosition`
Gets the screen-space position of the top-left corner of the window's client area.
```c
vec2 SituationGetWindowPosition(void);
```
**Usage Example:**
```c
// Get the current window position.
vec2 current_pos;
glm_vec2_copy(SituationGetWindowPosition(), current_pos);
printf("Window is at: (%.0f, %.0f)\n", current_pos[0], current_pos[1]);
```

---
#### `SituationSetWindowSize` / `SituationGetWindowSize`
Sets or gets the dimensions of the window's client area in screen coordinates.
```c
void SituationSetWindowSize(int width, int height);
vec2 SituationGetWindowSize(void);
```
**Usage Example:**
```c
// Set the window to a fixed 800x600 size
SituationSetWindowSize(800, 600);

// Verify the size
vec2 size;
glm_vec2_copy(SituationGetWindowSize(), size);
printf("Window size is now: %.0fx%.0f\n", size[0], size[1]);
```

---
#### `SituationSetWindowSizeLimits`
Sets the minimum and maximum size for a resizable window.
```c
void SituationSetWindowSizeLimits(int min_width, int min_height, int max_width, int max_height);
```
**Usage Example:**
```c
// Ensure the window is never smaller than 640x480 or larger than the primary monitor's resolution.
int max_w = SituationGetMonitorWidth(0);
int max_h = SituationGetMonitorHeight(0);
SituationSetWindowSizeLimits(640, 480, max_w, max_h);
```

---
#### `SituationSetWindowOpacity`
Sets the opacity of the entire window, where `1.0` is fully opaque and `0.0` is fully transparent.
```c
void SituationSetWindowOpacity(float opacity);
```
**Usage Example:**
```c
// Make the window semi-transparent.
SituationSetWindowOpacity(0.5f);
```

---
#### `SituationSetWindowIcon`
Sets a custom icon for the window from a `SituationImage`. The library handles the conversion to the platform's native icon format. For best results, provide a square image (e.g., 256x256).
```c
void SituationSetWindowIcon(SituationImage image);
```
**Usage Example:**
```c
// During initialization, load an icon from a file.
SituationImage icon;
if (SituationLoadImage("assets/icon.png", &icon) == SITUATION_SUCCESS) {
    SituationSetWindowIcon(icon);
    // The icon data is copied by the system, so we can unload the CPU image.
    SituationUnloadImage(icon);
}
```

---
#### `SituationRequestWindowAttention`
Requests the user's attention by making the window's icon flash or bounce in the taskbar. This is a non-intrusive way to signal that a long-running task has completed or that user input is required.
```c
void SituationRequestWindowAttention(void);
```
**Usage Example:**
```c
// When a file export or level loading process is complete:
printf("Export complete!\n");
SituationRequestWindowAttention();
```

---
#### `SituationIsWindowState`
Checks if a specific window state (e.g., `SITUATION_FLAG_WINDOW_MAXIMIZED`) is currently active.
```c
bool SituationIsWindowState(uint32_t flag);
```
**Usage Example:**
```c
if (SituationIsWindowState(SITUATION_FLAG_FULLSCREEN_MODE)) {
    printf("Application is in fullscreen mode.\n");
}
if (SituationIsWindowState(SITUATION_FLAG_WINDOW_MINIMIZED)) {
    printf("Application is minimized.\n");
}
```

---
#### `SituationGetScreenWidth`
Gets the current width of the window in screen coordinates (logical size). This may differ from the render width on high-DPI displays. Use this for UI layout and mouse coordinate calculations.
```c
int SituationGetScreenWidth(void);
```
**Usage Example:**
```c
// Get the logical width for UI layout calculations.
int screen_width = SituationGetScreenWidth();
// Position a UI element 10 pixels from the right edge of the window.
float element_x = screen_width - element_width - 10;
```

---
#### `SituationGetScreenHeight`
Gets the current height of the window in screen coordinates (logical size).
```c
int SituationGetScreenHeight(void);
```
**Usage Example:**
```c
// Center an element vertically based on the logical screen height.
int screen_height = SituationGetScreenHeight();
float element_y = (screen_height / 2.0f) - (element_height / 2.0f);
```

---
#### `SituationGetRenderWidth`
Gets the current width of the rendering framebuffer in pixels. This is the physical size, which may be larger than the logical window size on high-DPI displays. Use this for graphics calculations like setting the viewport.
```c
int SituationGetRenderWidth(void);
```
**Usage Example:**
```c
// Use the render width to set the graphics viewport.
int framebuffer_width = SituationGetRenderWidth();
int framebuffer_height = SituationGetRenderHeight();
SituationCmdSetViewport(SituationGetMainCommandBuffer(), 0, 0, framebuffer_width, framebuffer_height);
```

---
#### `SituationGetRenderHeight`
Gets the current height of the rendering framebuffer in pixels.
```c
int SituationGetRenderHeight(void);
```
**Usage Example:**
```c
// This value is often needed for aspect ratio calculations in projection matrices.
float aspect_ratio = (float)SituationGetRenderWidth() / (float)SituationGetRenderHeight();
```

---
#### `SituationGetWindowScaleDPI`
Gets the ratio of physical pixels to logical screen coordinates.
```c
vec2 SituationGetWindowScaleDPI(void);
```
**Usage Example:**
```c
// On a Retina display, this might return (2.0, 2.0). On standard displays, (1.0, 1.0).
vec2 dpi_scale;
glm_vec2_copy(SituationGetWindowScaleDPI(), dpi_scale);
printf("DPI Scale: %.1fx, %.1fy\n", dpi_scale[0], dpi_scale[1]);
```

---
#### `SituationGetCurrentMonitor`
Gets the index of the monitor that the window is currently on.
```c
int SituationGetCurrentMonitor(void);
```
**Usage Example:**
```c
int current_monitor_id = SituationGetCurrentMonitor();
printf("Window is on monitor %d.\n", current_monitor_id);
```

---
#### `SituationGetMonitorName`
Gets the human-readable name of a monitor by its index.
```c
const char* SituationGetMonitorName(int monitor);
```
**Usage Example:**
```c
int monitor_count;
SituationGetDisplays(&monitor_count);
for (int i = 0; i < monitor_count; i++) {
    printf("Monitor %d is called: %s\n", i, SituationGetMonitorName(i));
}
```

---
#### `SituationGetMonitorWidth`
Gets the current width of a monitor's resolution in pixels.
```c
int SituationGetMonitorWidth(int monitor);
```
**Usage Example:**
```c
// Get the width of the primary monitor (index 0).
int primary_monitor_width = SituationGetMonitorWidth(0);
printf("Primary monitor width: %dpx\n", primary_monitor_width);
```

---
#### `SituationGetMonitorHeight`
Gets the current height of a monitor's resolution in pixels.
```c
int SituationGetMonitorHeight(int monitor);
```
**Usage Example:**
```c
// Get the height of the primary monitor (index 0).
int primary_monitor_height = SituationGetMonitorHeight(0);
printf("Primary monitor height: %dpx\n", primary_monitor_height);
```

---
#### `SituationGetDisplays`
Retrieves detailed information for all connected displays. The caller is responsible for freeing the returned array using `SituationFreeDisplays`.
```c
SituationError SituationGetDisplays(SituationDisplayInfo** out_displays, int* out_count);
```
**Usage Example:**
```c
int display_count = 0;
SituationDisplayInfo* displays = NULL;
if (SituationGetDisplays(&displays, &display_count) == SITUATION_SUCCESS) {
    for (int i = 0; i < display_count; i++) {
        printf("Display %d: %s\n", i, displays[i].name);
    }
    SituationFreeDisplays(displays, display_count); // Don't forget to free!
}
```

---
#### `SituationFreeDisplays`
Frees the memory allocated for the display list returned by `SituationGetDisplays`.
```c
void SituationFreeDisplays(SituationDisplayInfo* displays, int count);
```
**Usage Example:**
```c
int display_count;
SituationDisplayInfo* displays = SituationGetDisplays(&display_count);
// ... use the display info ...
SituationFreeDisplays(displays, display_count); // Free the memory.
```

---
#### `SituationShowCursor`

Shows the cursor.

```c
SITAPI void SituationShowCursor(void);
```

**Usage Example:**
```c
SituationShowCursor();
```

---

#### `SituationHideCursor`

Hides the cursor.

```c
SITAPI void SituationHideCursor(void);
```

**Usage Example:**
```c
SituationHideCursor();
```

---

#### `SituationDisableCursor`

Disables the cursor, hiding it and providing unlimited mouse movement.

```c
SITAPI void SituationDisableCursor(void);
```

**Usage Example:**
```c
SituationDisableCursor();
```

---
#### `SituationShowCursor`
Makes the mouse cursor visible and restores its normal behavior.
```c
void SituationShowCursor(void);
```
**Usage Example:**
```c
// When returning to the main menu from gameplay, show the cursor.
SituationShowCursor();
```

---
#### `SituationHideCursor`
Makes the mouse cursor invisible when it is over the application's window, but it does not lock the cursor.
```c
void SituationHideCursor(void);
```
**Usage Example:**
```c
// In a media player application, hide the cursor when it's over the video playback area.
SituationHideCursor();
```

---
#### `SituationDisableCursor`
Hides the mouse cursor and locks it to the window, providing unbounded movement. This is ideal for 3D camera controls.
```c
void SituationDisableCursor(void);
```
**Usage Example:**
```c
// When entering a first-person 3D scene, disable the cursor for camera look.
SituationDisableCursor();
// Mouse delta can now be used to rotate the camera without the cursor leaving the window.
```

---
#### `SituationGetClipboardText`
Gets UTF-8 encoded text from the system clipboard. The returned string is heap-allocated and must be freed by the caller using `SituationFreeString`.
```c
SituationError SituationGetClipboardText(const char** out_text);
```
**Usage Example:**
```c
// In an input handler for Ctrl+V
if (SituationIsKeyDown(SIT_KEY_LEFT_CONTROL) && SituationIsKeyPressed(SIT_KEY_V)) {
    const char* clipboard_text = NULL;
    if (SituationGetClipboardText(&clipboard_text) == SITUATION_SUCCESS) {
        // Paste text into an input field.
        SituationFreeString((char*)clipboard_text);
    }
}
```

---
#### `SituationSetClipboardText`
Sets the system clipboard to the provided UTF-8 encoded text.
```c
SituationError SituationSetClipboardText(const char* text);
```
**Usage Example:**
```c
// In an input handler for Ctrl+C
if (SituationIsKeyDown(SIT_KEY_LEFT_CONTROL) && SituationIsKeyPressed(SIT_KEY_C)) {
    // Copy selected text to the clipboard.
    SituationSetClipboardText(selected_text);
}
```

---
#### `SituationIsFileDropped`
Checks if files were dropped onto the window this frame. This is a single-frame event.

```c
bool SituationIsFileDropped(void);
```

**Returns:** `true` if files were dropped this frame, `false` otherwise

**Usage Example:**
```c
// Check for dropped files
if (SituationIsFileDropped()) {
    int count;
    char** files = SituationLoadDroppedFiles(&count);
    
    printf("Dropped %d file(s):\n", count);
    for (int i = 0; i < count; i++) {
        printf("  - %s\n", files[i]);
        
        // Load based on extension
        if (strstr(files[i], ".png") || strstr(files[i], ".jpg")) {
            LoadTexture(files[i]);
        } else if (strstr(files[i], ".wav")) {
            LoadSound(files[i]);
        } else if (strstr(files[i], ".gltf")) {
            LoadModel(files[i]);
        }
    }
    
    SituationUnloadDroppedFiles(files, count);
}

// Show drop zone UI
if (SituationIsFileDropped()) {
    ShowDropSuccessAnimation();
} else if (IsDraggingOver()) {
    ShowDropZoneHighlight();
}

// Alternative: Use callback instead
void OnFileDrop(int count, const char** paths, void* user_data) {
    // Handle dropped files
}
SituationSetDropCallback(OnFileDrop, NULL);
```

**Notes:**
- Single-frame event, not a continuous state
- Use `SituationLoadDroppedFiles()` to get file paths
- Alternative: use `SituationSetDropCallback()` for callback-based handling
- Files must be freed with `SituationUnloadDroppedFiles()`

---
#### `SituationLoadDroppedFiles`
Get the paths of files dragged and dropped onto the window. The caller must free the returned list with `SituationUnloadDroppedFiles`.
```c
char** SituationLoadDroppedFiles(int* count);
```
**Usage Example:**
```c
// In the update loop
if (SituationIsFileDropped()) {
    int file_count;
    char** files = SituationLoadDroppedFiles(&file_count);
    for (int i = 0; i < file_count; i++) {
        printf("Dropped file: %s\n", files[i]);
    }
    SituationUnloadDroppedFiles(files, file_count); // Free the list
}
```

---
#### `SituationUnloadDroppedFiles`
Frees the memory for file paths returned by `SituationLoadDroppedFiles`.
```c
void SituationUnloadDroppedFiles(char** paths, int count);
```
**Usage Example:**
```c
int count;
char** files = SituationLoadDroppedFiles(&count);
// ... process files ...
SituationUnloadDroppedFiles(files, count); // Always free!
```

---
#### `SituationToggleBorderlessWindowed`
Toggles the window between borderless mode (no title bar, no borders) and decorated mode (with title bar and borders). Borderless mode is useful for custom title bars, immersive experiences, or "fake fullscreen" that allows easy alt-tabbing.
```c
void SituationToggleBorderlessWindowed(void);
```
**Usage Example:**
```c
// Toggle borderless on F11 key
if (SituationIsKeyPressed(SIT_KEY_F11)) {
    SituationToggleBorderlessWindowed();
}

// Start in borderless mode for custom UI
SituationInitInfo init = {
    .window_width = 1920,
    .window_height = 1080,
    .window_title = "My App",
    .initial_active_window_flags = SITUATION_FLAG_WINDOW_BORDERLESS
};
SituationInit(argc, argv, &init);

// Toggle for "fake fullscreen" (borderless maximized)
void enter_fake_fullscreen(void) {
    SituationToggleBorderlessWindowed();  // Remove borders
    SituationMaximizeWindow();            // Fill screen
}

// Custom title bar implementation
if (is_borderless && mouse_in_custom_titlebar) {
    if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) {
        // Handle custom title bar drag
        start_window_drag();
    }
}
```
**Notes:**
- Borderless mode removes title bar and window borders
- Useful for custom UI or "fake fullscreen" experiences
- Allows faster alt-tab than true fullscreen
- You'll need to implement custom window controls (close, minimize, etc.)
- Combine with `SituationMaximizeWindow()` for borderless fullscreen

---
#### `SituationMaximizeWindow`
Maximizes the window to fill the screen while maintaining window decorations (title bar, borders). Unlike fullscreen mode, the taskbar and window frame remain visible. The window must have the resizable flag set for this to work.
```c
void SituationMaximizeWindow(void);
```
**Usage Example:**
```c
// Maximize window on startup based on user preference
if (user_config.start_maximized) {
    SituationMaximizeWindow();
}

// Toggle maximize/restore on double-click
static double last_click_time = 0;
if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) {
    double current_time = SituationGetTime();
    if (current_time - last_click_time < 0.3) {  // Double-click
        if (SituationIsWindowMaximized()) {
            SituationRestoreWindow();
        } else {
            SituationMaximizeWindow();
        }
    }
    last_click_time = current_time;
}
```
**Notes:**
- Window must be resizable (set `SITUATION_FLAG_WINDOW_RESIZABLE` in init)
- Use `SituationIsWindowMaximized()` to check current state
- Use `SituationRestoreWindow()` to return to previous size
- Does not hide taskbar (use fullscreen for that)

---
#### `SituationMinimizeWindow`
Minimizes (iconifies) the window to the taskbar. The window remains running but is hidden from view. This is useful for background applications or when the user wants to temporarily hide the window.
```c
void SituationMinimizeWindow(void);
```
**Usage Example:**
```c
// Minimize on Escape key (for background apps)
if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
    SituationMinimizeWindow();
}

// Minimize when losing focus (for utility apps)
void on_focus_lost(int focused, void* user_data) {
    if (!focused && app_config.minimize_on_focus_loss) {
        SituationMinimizeWindow();
    }
}
SituationSetFocusCallback(on_focus_lost, NULL);

// Check if minimized before rendering (optimization)
if (SituationIsWindowMinimized()) {
    // Skip expensive rendering
    SituationPollInputEvents();
    return;
}
```
**Notes:**
- Window continues running in background
- Use `SituationIsWindowMinimized()` to check state
- Consider pausing rendering when minimized to save resources
- Use `SituationRestoreWindow()` to bring window back

---
#### `SituationRestoreWindow`
Restores a minimized or maximized window to its previous normal size and position. This returns the window to the state it was in before being minimized or maximized.
```c
void SituationRestoreWindow(void);
```
**Usage Example:**
```c
// Restore window when user clicks on taskbar icon
// (handled automatically by OS, but you can trigger programmatically)

// Restore from maximized state
if (SituationIsKeyPressed(SIT_KEY_F11)) {
    if (SituationIsWindowMaximized()) {
        SituationRestoreWindow();
    } else {
        SituationMaximizeWindow();
    }
}

// Restore from minimized state on notification
void on_notification_received(void) {
    if (SituationIsWindowMinimized()) {
        SituationRestoreWindow();
        SituationSetWindowFocused();  // Also bring to front
    }
}
```
**Notes:**
- Works for both minimized and maximized states
- Restores to the size/position before minimize/maximize
- Does not affect fullscreen state (use `SituationToggleFullscreen()`)
- Window will not be focused automatically (use `SituationSetWindowFocused()`)

---
#### `SituationSetWindowFocused`
Brings the window to the front and gives it input focus. This makes the window the active window that receives keyboard and mouse input. Useful for notification systems or when you need to grab the user's attention.
```c
void SituationSetWindowFocused(void);
```
**Usage Example:**
```c
// Focus window when important event occurs
void on_critical_error(const char* message) {
    if (SituationIsWindowMinimized()) {
        SituationRestoreWindow();
    }
    SituationSetWindowFocused();  // Bring to front
    ShowErrorDialog(message);
}

// Focus on notification click
void on_notification_clicked(void) {
    SituationSetWindowFocused();
    NavigateToNotificationSource();
}

// Auto-focus when receiving network message
if (received_message && !SituationHasWindowFocus()) {
    if (app_config.focus_on_message) {
        SituationSetWindowFocused();
    }
}
```
**Notes:**
- May not work on all platforms due to OS restrictions
- Some OSes prevent apps from stealing focus
- Window will flash in taskbar if focus steal is prevented
- Use `SituationHasWindowFocus()` to check if successful

---
#### `SituationSetWindowIcons`
Sets multiple window icons at different sizes for optimal display across various contexts (taskbar, alt-tab, title bar). The OS will automatically select the most appropriate size for each context.
```c
void SituationSetWindowIcons(SituationImage *images, int count);
```
**Parameters:**
- `images`: Array of `SituationImage` structures at different resolutions
- `count`: Number of images in the array

**Usage Example:**
```c
// Load multiple icon sizes for best quality
SituationImage icons[3];
SituationLoadImage("assets/icon_16x16.png", &icons[0]);   // Taskbar
SituationLoadImage("assets/icon_32x32.png", &icons[1]);   // Title bar
SituationLoadImage("assets/icon_256x256.png", &icons[2]); // Alt-Tab

SituationSetWindowIcons(icons, 3);

// Icons are copied by the OS, safe to unload
for (int i = 0; i < 3; i++) {
    SituationUnloadImage(icons[i]);
}

// Single icon (will be scaled as needed)
SituationImage icon;
if (SituationLoadImage("assets/icon.png", &icon) == SITUATION_SUCCESS) {
    SituationSetWindowIcons(&icon, 1);
    SituationUnloadImage(icon);
}
```
**Notes:**
- Recommended sizes: 16x16, 32x32, 48x48, 256x256
- Images are copied internally, safe to unload after call
- Use square images for best results
- PNG format recommended for transparency
- Call during initialization or when changing themes

---
#### `SituationSetWindowMinSize`
Sets the minimum dimensions the window can be resized to. This prevents users from making the window too small to be usable. Useful for ensuring UI elements remain visible and functional.
```c
void SituationSetWindowMinSize(int width, int height);
```
**Parameters:**
- `width`: Minimum window width in screen coordinates
- `height`: Minimum window height in screen coordinates

**Usage Example:**
```c
// Set minimum size to ensure UI is usable
SituationSetWindowMinSize(800, 600);

// Calculate minimum size based on UI layout
int min_width = sidebar_width + content_min_width + padding;
int min_height = toolbar_height + content_min_height + statusbar_height;
SituationSetWindowMinSize(min_width, min_height);

// Prevent window from being too small for text
int text_width = SituationMeasureText(font, "Minimum Text", 12).width;
SituationSetWindowMinSize(text_width + 40, 200);

// Update minimum size when UI changes
void on_sidebar_toggle(bool visible) {
    int min_width = visible ? 1000 : 600;
    SituationSetWindowMinSize(min_width, 400);
}
```
**Notes:**
- Window must be resizable for this to take effect
- Values are in logical screen coordinates, not pixels
- Setting 0 removes the minimum size constraint
- Does not resize the window if currently smaller
- Useful for responsive UI design

---
#### `SituationSetWindowMaxSize`
Sets the maximum dimensions the window can be resized to. This prevents users from making the window larger than necessary or larger than your content can handle. Useful for fixed-layout applications.
```c
void SituationSetWindowMaxSize(int width, int height);
```
**Parameters:**
- `width`: Maximum window width in screen coordinates
- `height`: Maximum window height in screen coordinates

**Usage Example:**
```c
// Limit window size for fixed-layout game
SituationSetWindowMaxSize(1920, 1080);

// Prevent window from being larger than content
int content_width = background_image.width;
int content_height = background_image.height;
SituationSetWindowMaxSize(content_width, content_height);

// Set max size based on monitor resolution
int monitor_width = SituationGetMonitorWidth(0);
int monitor_height = SituationGetMonitorHeight(0);
SituationSetWindowMaxSize(monitor_width - 100, monitor_height - 100);

// Remove max size constraint
SituationSetWindowMaxSize(0, 0);  // No limit
```
**Notes:**
- Window must be resizable for this to take effect
- Values are in logical screen coordinates, not pixels
- Setting 0 removes the maximum size constraint
- Does not resize the window if currently larger
- Combine with min size for fixed aspect ratio windows

---
#### `SituationIsWindowFullscreen`
Checks if the window is currently in fullscreen mode. Returns true for both exclusive fullscreen and borderless windowed modes.

```c
bool SituationIsWindowFullscreen(void);
```

**Returns:** `true` if window is fullscreen, `false` otherwise

**Usage Example:**
```c
// Toggle fullscreen on F11
if (SituationIsKeyPressed(SIT_KEY_F11)) {
    if (SituationIsWindowFullscreen()) {
        SituationClearWindowState(SITUATION_FLAG_FULLSCREEN_MODE);
    } else {
        SituationSetWindowState(SITUATION_FLAG_FULLSCREEN_MODE);
    }
}

// Show different UI in fullscreen
if (SituationIsWindowFullscreen()) {
    DrawFullscreenUI();
} else {
    DrawWindowedUI();
}
```

**Notes:**
- Returns true for both exclusive and borderless fullscreen
- Use with `SituationSetWindowState()` to toggle fullscreen
- Check before applying window decorations

---
#### `SituationIsWindowHidden`
Checks if the window is currently hidden from view. Hidden windows don't render but continue processing events.

```c
bool SituationIsWindowHidden(void);
```

**Returns:** `true` if window is hidden, `false` if visible

**Usage Example:**
```c
// Skip rendering if window is hidden
if (SituationIsWindowHidden()) {
    // Don't waste GPU cycles rendering
    return;
}

// Show/hide window with hotkey
if (SituationIsKeyPressed(SIT_KEY_H)) {
    if (SituationIsWindowHidden()) {
        SituationClearWindowState(SITUATION_FLAG_WINDOW_HIDDEN);
    } else {
        SituationSetWindowState(SITUATION_FLAG_WINDOW_HIDDEN);
    }
}
```

**Notes:**
- Hidden windows still process input and update logic
- Use for background applications or system tray apps
- Different from minimized state

---
#### `SituationIsWindowMinimized`
Checks if the window is currently minimized to the taskbar. Minimized windows typically don't render.

```c
bool SituationIsWindowMinimized(void);
```

**Returns:** `true` if window is minimized, `false` otherwise

**Usage Example:**
```c
// Pause game when minimized
if (SituationIsWindowMinimized()) {
    PauseGame();
    // Reduce CPU usage
    SituationSetTargetFPS(10);
} else {
    ResumeGame();
    SituationSetTargetFPS(60);
}

// Skip expensive updates when minimized
if (!SituationIsWindowMinimized()) {
    UpdatePhysics();
    RenderScene();
}
```

**Notes:**
- Minimized windows should reduce CPU/GPU usage
- Use `SITUATION_FLAG_WINDOW_ALWAYS_RUN` to keep running when minimized
- Check before rendering to save resources

---
#### `SituationIsWindowMaximized`
Checks if the window is currently maximized to fill the work area (screen minus taskbar).

```c
bool SituationIsWindowMaximized(void);
```

**Returns:** `true` if window is maximized, `false` otherwise

**Usage Example:**
```c
// Show maximize/restore button
if (SituationIsWindowMaximized()) {
    DrawRestoreButton();
} else {
    DrawMaximizeButton();
}

// Toggle maximize on double-click title bar
if (TitleBarDoubleClicked()) {
    if (SituationIsWindowMaximized()) {
        SituationRestoreWindow();
    } else {
        SituationMaximizeWindow();
    }
}
```

**Notes:**
- Maximized windows fill the work area, not the entire screen
- Different from fullscreen mode
- Window must be resizable to maximize

---
#### `SituationHasWindowFocus`
Checks if the window is currently focused and receiving input. Unfocused windows don't receive keyboard input.

```c
bool SituationHasWindowFocus(void);
```

**Returns:** `true` if window has focus, `false` otherwise

**Usage Example:**
```c
// Pause game when focus is lost
if (!SituationHasWindowFocus()) {
    PauseGame();
    ShowPauseMenu();
}

// Mute audio when unfocused
if (SituationHasWindowFocus()) {
    SituationSetAudioMasterVolume(1.0f);
} else {
    SituationSetAudioMasterVolume(0.0f);
}

// Show "Click to focus" overlay
if (!SituationHasWindowFocus()) {
    DrawOverlay("Click window to resume");
}
```

**Notes:**
- Unfocused windows don't receive keyboard input
- Use for auto-pause functionality
- Consider muting audio when unfocused

---
#### `SituationIsWindowResized`
Checks if the window was resized in the last frame. This is a single-frame event that triggers once per resize.

```c
bool SituationIsWindowResized(void);
```

**Returns:** `true` if window was resized this frame, `false` otherwise

**Usage Example:**
```c
// Recreate framebuffers on resize
if (SituationIsWindowResized()) {
    int width = SituationGetRenderWidth();
    int height = SituationGetRenderHeight();
    
    // Recreate render targets
    RecreateFramebuffers(width, height);
    
    // Update camera aspect ratio
    camera.aspect = (float)width / (float)height;
    UpdateProjectionMatrix(&camera);
}

// Log resize events
if (SituationIsWindowResized()) {
    printf("Window resized to %dx%d\n",
        SituationGetScreenWidth(),
        SituationGetScreenHeight());
}
```

**Notes:**
- Single-frame event, not a continuous state
- Use for recreating resolution-dependent resources
- Check before updating aspect ratios or viewports

---
#### `SituationGetWindowSize`
Gets the current logical window size in screen coordinates. This may differ from render size on high-DPI displays.

```c
void SituationGetWindowSize(int* width, int* height);
```

**Parameters:**
- `width` - Pointer to store window width
- `height` - Pointer to store window height

**Usage Example:**
```c
// Get window dimensions
int width, height;
SituationGetWindowSize(&width, &height);
printf("Window size: %dx%d\n", width, height);

// Center a dialog box
int dialog_width = 400;
int dialog_height = 300;
int window_width, window_height;
SituationGetWindowSize(&window_width, &window_height);

int dialog_x = (window_width - dialog_width) / 2;
int dialog_y = (window_height - dialog_height) / 2;
DrawDialog(dialog_x, dialog_y, dialog_width, dialog_height);

// Calculate aspect ratio
int w, h;
SituationGetWindowSize(&w, &h);
float aspect = (float)w / (float)h;
```

**Notes:**
- Returns logical size, not physical pixels
- Use `SituationGetRenderWidth/Height()` for framebuffer size
- Useful for UI layout and mouse coordinate calculations

---
#### `SituationGetMonitorCount`
Gets the number of connected monitors/displays on the system. Useful for multi-monitor setups and display selection.

```c
int SituationGetMonitorCount(void);
```

**Returns:** Number of connected monitors (minimum 1)

**Usage Example:**
```c
// Enumerate all connected monitors
int monitor_count = SituationGetMonitorCount();
printf("Detected %d monitor(s)\n", monitor_count);

for (int i = 0; i < monitor_count; i++) {
    vec2 pos = SituationGetMonitorPosition(i);
    int width = SituationGetMonitorWidth(i);
    int height = SituationGetMonitorHeight(i);
    int refresh = SituationGetMonitorRefreshRate(i);
    
    printf("Monitor %d: %dx%d @ %.0f,%.0f (%dHz)\n",
        i, width, height, pos[0], pos[1], refresh);
}

// Let user choose monitor for fullscreen
if (monitor_count > 1) {
    printf("Select monitor for fullscreen (0-%d): ", monitor_count - 1);
    int choice = GetUserInput();
    SituationSetWindowMonitor(choice);
}
```

**Notes:**
- Monitor IDs range from 0 to (count - 1)
- Primary monitor is typically ID 0
- Call `SituationRefreshDisplays()` if monitors are hot-plugged

---
#### `SituationRefreshDisplays`
Forces a refresh of the cached display information. Call this after monitors are connected/disconnected to update the monitor list.

```c
void SituationRefreshDisplays(void);
```

**Usage Example:**
```c
// Initial monitor count
int initial_count = SituationGetMonitorCount();
printf("Initial monitors: %d\n", initial_count);

// User connects/disconnects a monitor
printf("Connect or disconnect a monitor, then press Enter...\n");
getchar();

// Refresh display cache
SituationRefreshDisplays();

// Check updated count
int new_count = SituationGetMonitorCount();
printf("Updated monitors: %d\n", new_count);

if (new_count != initial_count) {
    printf("Monitor configuration changed!\n");
    // Update UI or window placement
}
```

**Notes:**
- Automatically called during `SituationInit()`
- Call manually when handling monitor hot-plug events
- Refreshes monitor positions, resolutions, and refresh rates

---
#### `SituationSetDisplayMode`
Sets the display mode (resolution and refresh rate) for a specific monitor. Can also toggle fullscreen mode.

```c
SituationError SituationSetDisplayMode(int monitor_id, const SituationDisplayMode* mode, bool fullscreen);
```

**Parameters:**
- `monitor_id` - The monitor to configure (0 = primary)
- `mode` - Display mode with width, height, and refresh rate
- `fullscreen` - Whether to enter fullscreen mode

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Set monitor to 1080p @ 144Hz fullscreen
SituationDisplayMode mode = {
    .width = 1920,
    .height = 1080,
    .refresh_rate = 144
};

if (SituationSetDisplayMode(0, &mode, true) == SITUATION_SUCCESS) {
    printf("Switched to 1080p @ 144Hz fullscreen\n");
} else {
    printf("Failed to set display mode\n");
}

// Return to windowed mode with desktop resolution
SituationDisplayMode desktop_mode = {
    .width = 1920,
    .height = 1080,
    .refresh_rate = 60
};
SituationSetDisplayMode(0, &desktop_mode, false);
```

**Notes:**
- Not all mode combinations are supported by all monitors
- Check available modes before setting
- Fullscreen mode may improve performance by reducing compositor overhead

---
#### `SituationGetMonitorPosition`
Gets the top-left position of a monitor in virtual desktop coordinates. Useful for multi-monitor window placement.

```c
vec2 SituationGetMonitorPosition(int monitor_id);
```

**Parameters:**
- `monitor_id` - The monitor to query (0 = primary)

**Returns:** 2D vector with X and Y position in pixels

**Usage Example:**
```c
// Position window on secondary monitor
int monitor_count = SituationGetMonitorCount();
if (monitor_count > 1) {
    vec2 secondary_pos = SituationGetMonitorPosition(1);
    int secondary_width = SituationGetMonitorWidth(1);
    int secondary_height = SituationGetMonitorHeight(1);
    
    // Center window on secondary monitor
    int window_width = 1280;
    int window_height = 720;
    int x = (int)secondary_pos[0] + (secondary_width - window_width) / 2;
    int y = (int)secondary_pos[1] + (secondary_height - window_height) / 2;
    
    SituationSetWindowPosition(x, y);
}
```

**Notes:**
- Primary monitor is typically at (0, 0)
- Other monitors have positions relative to primary
- Positions can be negative if monitors are arranged left/above primary

---
#### `SituationGetMonitorPhysicalWidth`
Gets the physical width of a monitor in millimeters. Useful for calculating DPI and physical size.

```c
int SituationGetMonitorPhysicalWidth(int monitor_id);
```

**Parameters:**
- `monitor_id` - The monitor to query

**Returns:** Physical width in millimeters

**Usage Example:**
```c
// Calculate monitor DPI
int physical_width_mm = SituationGetMonitorPhysicalWidth(0);
int physical_height_mm = SituationGetMonitorPhysicalHeight(0);
int pixel_width = SituationGetMonitorWidth(0);
int pixel_height = SituationGetMonitorHeight(0);

// Convert mm to inches (1 inch = 25.4mm)
float width_inches = physical_width_mm / 25.4f;
float height_inches = physical_height_mm / 25.4f;

// Calculate DPI
float dpi_x = pixel_width / width_inches;
float dpi_y = pixel_height / height_inches;

printf("Monitor: %.1f\" diagonal\n", 
    sqrtf(width_inches * width_inches + height_inches * height_inches));
printf("DPI: %.0f x %.0f\n", dpi_x, dpi_y);

// Adjust UI scaling based on DPI
if (dpi_x > 150.0f) {
    SetUIScale(2.0f);  // High DPI display
} else {
    SetUIScale(1.0f);  // Standard DPI
}
```

**Notes:**
- Returns 0 if physical size is unknown
- Some monitors report incorrect physical dimensions
- Use for DPI calculations and UI scaling

---
#### `SituationGetMonitorPhysicalHeight`
Gets the physical height of a monitor in millimeters. Useful for calculating DPI and physical size.

```c
int SituationGetMonitorPhysicalHeight(int monitor_id);
```

**Parameters:**
- `monitor_id` - The monitor to query

**Returns:** Physical height in millimeters

**Usage Example:**
```c
// Calculate monitor aspect ratio
int physical_width = SituationGetMonitorPhysicalWidth(0);
int physical_height = SituationGetMonitorPhysicalHeight(0);

float aspect_ratio = (float)physical_width / (float)physical_height;
printf("Physical aspect ratio: %.2f:1\n", aspect_ratio);

// Common aspect ratios
if (fabs(aspect_ratio - 16.0f/9.0f) < 0.1f) {
    printf("16:9 monitor (widescreen)\n");
} else if (fabs(aspect_ratio - 21.0f/9.0f) < 0.1f) {
    printf("21:9 monitor (ultrawide)\n");
} else if (fabs(aspect_ratio - 16.0f/10.0f) < 0.1f) {
    printf("16:10 monitor\n");
}
```

**Notes:**
- Returns 0 if physical size is unknown
- Use with `SituationGetMonitorPhysicalWidth()` for complete dimensions

---
#### `SituationGetMonitorRefreshRate`
Gets the refresh rate of a monitor in Hz. Useful for adaptive vsync and frame pacing.

```c
int SituationGetMonitorRefreshRate(int monitor_id);
```

**Parameters:**
- `monitor_id` - The monitor to query

**Returns:** Refresh rate in Hz (e.g., 60, 144, 240)

**Usage Example:**
```c
// Adapt frame rate to monitor refresh rate
int refresh_rate = SituationGetMonitorRefreshRate(0);
printf("Monitor refresh rate: %d Hz\n", refresh_rate);

// Set target frame rate
if (refresh_rate >= 144) {
    SetTargetFPS(144);
    printf("High refresh rate detected - targeting 144 FPS\n");
} else if (refresh_rate >= 120) {
    SetTargetFPS(120);
} else {
    SetTargetFPS(60);
}

// Calculate frame time
float target_frame_time = 1.0f / refresh_rate;
printf("Target frame time: %.2f ms\n", target_frame_time * 1000.0f);

// Enable adaptive vsync for high refresh displays
if (refresh_rate > 60) {
    SituationSetVSync(true);
}
```

**Notes:**
- Typical values: 60Hz (standard), 75Hz, 120Hz, 144Hz, 240Hz, 360Hz
- Variable refresh rate displays (G-Sync/FreeSync) report maximum rate
- Use for frame pacing and vsync decisions

---
#### `SituationSetCursor`
Sets the mouse cursor to a standard shape.
```c
void SituationSetCursor(SituationCursor cursor);
```

---
#### `SituationSetWindowStateProfiles`
Sets the flag profiles for when the window is focused vs. unfocused.
```c
SituationError SituationSetWindowStateProfiles(uint32_t active_flags, uint32_t inactive_flags);
```

---
#### `SituationApplyCurrentProfileWindowState`
Manually applies the appropriate window state profile based on current focus. Useful when you've modified profiles and want to apply changes immediately.

```c
SituationError SituationApplyCurrentProfileWindowState(void);
```

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Set up different profiles for focused/unfocused states
SituationWindowStateFlags focused_flags = 
    SITUATION_FLAG_VSYNC_HINT | 
    SITUATION_FLAG_WINDOW_TOPMOST;

SituationWindowStateFlags unfocused_flags = 
    SITUATION_FLAG_VSYNC_HINT;  // Remove topmost when unfocused

SituationSetWindowStateProfiles(focused_flags, unfocused_flags);

// Manually apply the current profile
SituationApplyCurrentProfileWindowState();

// Useful after runtime profile changes
if (settings.always_on_top_changed) {
    UpdateWindowProfiles();
    SituationApplyCurrentProfileWindowState();
}
```

**Notes:**
- Automatically called when window focus changes
- Use when you modify profiles at runtime
- Applies focused profile if window has focus, unfocused otherwise

---
#### `SituationToggleWindowStateFlags`
Toggles specific flags in the current profile and applies the result immediately. Useful for hotkey-based window state changes.

```c
SituationError SituationToggleWindowStateFlags(SituationWindowStateFlags flags_to_toggle);
```

**Parameters:**
- `flags_to_toggle` - Flags to toggle (XOR operation)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Toggle VSync with F1
if (SituationIsKeyPressed(SIT_KEY_F1)) {
    SituationToggleWindowStateFlags(SITUATION_FLAG_VSYNC_HINT);
}

// Toggle always-on-top with F2
if (SituationIsKeyPressed(SIT_KEY_F2)) {
    SituationToggleWindowStateFlags(SITUATION_FLAG_WINDOW_TOPMOST);
}

// Toggle multiple flags at once
if (SituationIsKeyPressed(SIT_KEY_F3)) {
    SituationToggleWindowStateFlags(
        SITUATION_FLAG_WINDOW_RESIZABLE | 
        SITUATION_FLAG_WINDOW_UNDECORATED
    );
}
```

**Notes:**
- Toggles flags in the current active profile
- Changes persist until profile is switched
- Use for hotkey-based window controls

---
#### `SituationGetCurrentActualWindowStateFlags`
Gets the actual current window state flags by querying the underlying window system (GLFW). This reflects the real window state, not just the requested flags.

```c
uint32_t SituationGetCurrentActualWindowStateFlags(void);
```

**Returns:** Bitmask of current window state flags

**Usage Example:**
```c
// Check actual window state
uint32_t flags = SituationGetCurrentActualWindowStateFlags();

if (flags & SITUATION_FLAG_FULLSCREEN_MODE) {
    printf("Window is fullscreen\n");
}

if (flags & SITUATION_FLAG_WINDOW_MAXIMIZED) {
    printf("Window is maximized\n");
}

// Verify VSync is actually enabled
if (flags & SITUATION_FLAG_VSYNC_HINT) {
    printf("VSync is active\n");
} else {
    printf("VSync is disabled\n");
}

// Debug window state
printf("Window flags: 0x%08X\n", flags);
if (flags & SITUATION_FLAG_WINDOW_RESIZABLE) printf("  - Resizable\n");
if (flags & SITUATION_FLAG_WINDOW_TOPMOST) printf("  - Always on top\n");
if (flags & SITUATION_FLAG_WINDOW_UNDECORATED) printf("  - Borderless\n");
```

**Notes:**
- Queries actual window state from GLFW
- May differ from requested flags if OS overrides them
- Useful for debugging window state issues

---
#### `SituationSetWindowStateProfiles`
Sets different flag profiles for when the window is focused versus unfocused. Allows automatic behavior changes based on focus state.

```c
SituationError SituationSetWindowStateProfiles(
    SituationWindowStateFlags focused_flags,
    SituationWindowStateFlags unfocused_flags
);
```

**Parameters:**
- `focused_flags` - Flags to apply when window has focus
- `unfocused_flags` - Flags to apply when window loses focus

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Keep window on top only when focused
SituationWindowStateFlags focused = 
    SITUATION_FLAG_VSYNC_HINT | 
    SITUATION_FLAG_WINDOW_TOPMOST |
    SITUATION_FLAG_WINDOW_RESIZABLE;

SituationWindowStateFlags unfocused = 
    SITUATION_FLAG_VSYNC_HINT |
    SITUATION_FLAG_WINDOW_RESIZABLE;  // Remove topmost

SituationSetWindowStateProfiles(focused, unfocused);

// Disable VSync when unfocused to save power
SituationWindowStateFlags focused_vsync = 
    SITUATION_FLAG_VSYNC_HINT;

SituationWindowStateFlags unfocused_no_vsync = 0;

SituationSetWindowStateProfiles(focused_vsync, unfocused_no_vsync);

// Borderless fullscreen when focused, windowed when unfocused
SituationWindowStateFlags focused_borderless = 
    SITUATION_FLAG_FULLSCREEN_MODE |
    SITUATION_FLAG_WINDOW_UNDECORATED;

SituationWindowStateFlags unfocused_windowed = 
    SITUATION_FLAG_WINDOW_RESIZABLE;

SituationSetWindowStateProfiles(focused_borderless, unfocused_windowed);
```

**Notes:**
- Profiles switch automatically on focus change
- Useful for power saving when unfocused
- Common pattern: disable VSync/topmost when unfocused

---

#### `SituationGetGLFWwindow`

Gets the underlying GLFW window handle.

```c
SITAPI GLFWwindow* SituationGetGLFWwindow(void);
```

**Usage Example:**
```c
GLFWwindow* glfw_window = SituationGetGLFWwindow();
// Use with GLFW functions
```

---
#### `SituationSetWindowMonitor`
Moves the window to a specific monitor. Useful for multi-monitor setups.
```c
void SituationSetWindowMonitor(int monitor_index);
```
**Usage Example:**
```c
// Move window to second monitor
if (SituationGetMonitorCount() > 1) {
    SituationSetWindowMonitor(1);
}
```

---
#### `SituationSetCursorPosCallback`
Sets a callback for mouse cursor position changes.
```c
void SituationSetCursorPosCallback(SituationCursorPosCallback callback, void* user_data);
```
**Usage Example:**
```c
void on_mouse_move(double x, double y, void* user_data) {
    printf("Mouse moved to: %.0f, %.0f\n", x, y);
}

SituationSetCursorPosCallback(on_mouse_move, NULL);
```

---
#### `SituationSetScrollCallback`
Sets a callback for mouse scroll wheel events.
```c
void SituationSetScrollCallback(SituationScrollCallback callback, void* user_data);
```
**Usage Example:**
```c
void on_scroll(double xoffset, double yoffset, void* user_data) {
    camera_zoom += yoffset * 0.1f;
}

SituationSetScrollCallback(on_scroll, NULL);
```

---
#### `SituationCreateRenderList`
Creates a render list for recording and replaying rendering commands. Render lists cache static geometry for efficient reuse without re-recording commands every frame.

```c
SituationRenderList* SituationCreateRenderList(size_t initial_capacity);
```

**Parameters:**
- `initial_capacity` - Initial number of commands to allocate space for

**Returns:** Pointer to new render list, or NULL on failure

**Usage Example:**
```c
// Create render list for static UI
SituationRenderList* ui_list = SituationCreateRenderList(100);

// Record UI commands once
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
// ... record drawing commands ...

// Replay every frame without re-recording
while (!SituationShouldClose()) {
    SituationBeginFrame();
    
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SituationReplayRenderList(cmd, ui_list);
    
    SituationEndFrame();
}

// Cleanup
SituationDestroyRenderList(ui_list);
```

**Notes:**
- Useful for caching static geometry
- Reduces CPU overhead for repeated draw calls
- Grows automatically if capacity is exceeded
- Must be destroyed with `SituationDestroyRenderList()`

---
#### `SituationDestroyRenderList`
Destroys a render list and frees all associated memory. Always call this when done with a render list.

```c
void SituationDestroyRenderList(SituationRenderList* list);
```

**Parameters:**
- `list` - Render list to destroy

**Usage Example:**
```c
// Create and use render list
SituationRenderList* list = SituationCreateRenderList(50);

// ... use the list ...

// Always cleanup
SituationDestroyRenderList(list);
list = NULL;  // Good practice

// Safe to call with NULL
SituationDestroyRenderList(NULL);  // No-op
```

**Notes:**
- Safe to call with NULL pointer
- Frees all recorded commands
- List cannot be used after destruction

---
#### `SituationResetRenderList`
Clears all commands from a render list, allowing it to be reused for new commands. The list's capacity is preserved.

```c
void SituationResetRenderList(SituationRenderList* list);
```

**Parameters:**
- `list` - Render list to reset

**Usage Example:**
```c
SituationRenderList* list = SituationCreateRenderList(100);

// Record commands
// ... record drawing ...

// Replay
SituationReplayRenderList(cmd, list);

// Clear and record new commands
SituationResetRenderList(list);
// ... record different drawing ...

// Replay new commands
SituationReplayRenderList(cmd, list);

// Cleanup
SituationDestroyRenderList(list);
```

**Notes:**
- Preserves allocated capacity
- Faster than destroying and recreating
- Use when render list content changes

---
#### `SituationReplayRenderList`
Replays all commands stored in a render list. This executes the cached commands without re-recording them.

```c
void SituationReplayRenderList(SituationCommandBuffer cmd, SituationRenderList* list);
```

**Parameters:**
- `cmd` - Command buffer to replay commands into
- `list` - Render list containing cached commands

**Usage Example:**
```c
// Create and record static background
SituationRenderList* background = SituationCreateRenderList(200);
// ... record background drawing commands ...

// Main loop
while (!SituationShouldClose()) {
    SituationBeginFrame();
    
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    
    // Replay cached background (no CPU overhead)
    SituationReplayRenderList(cmd, background);
    
    // Draw dynamic content normally
    DrawPlayer();
    DrawEnemies();
    
    SituationEndFrame();
}

SituationDestroyRenderList(background);
```

**Notes:**
- Efficient for static geometry
- Commands execute in recorded order
- Can replay same list multiple times per frame

---
#### `SituationGetRenderQueueDepth`
Gets the current number of pending render commands in the queue. Useful for performance monitoring and debugging.

```c
size_t SituationGetRenderQueueDepth(void);
```

**Returns:** Number of pending commands in the render queue

**Usage Example:**
```c
// Monitor render queue depth
size_t depth = SituationGetRenderQueueDepth();
printf("Render queue depth: %zu commands\n", depth);

// Warn if queue is getting too large
if (depth > 10000) {
    printf("Warning: High render queue depth! Consider batching.\n");
}

// Performance profiling
size_t before = SituationGetRenderQueueDepth();
DrawComplexScene();
size_t after = SituationGetRenderQueueDepth();
printf("Scene added %zu commands\n", after - before);
```

**Notes:**
- Useful for performance profiling
- High depth may indicate batching opportunities
- Resets after frame submission

---
#### `SituationExportRenderHistogram`
Exports rendering performance histogram data to a file for profiling and analysis. Captures timing data for different rendering stages.

```c
void SituationExportRenderHistogram(const char* filename);
```

**Parameters:**
- `filename` - Path to output file (CSV or JSON format)

**Usage Example:**
```c
// Enable profiling during development
#ifdef DEBUG
    // Run for a few frames
    for (int i = 0; i < 300; i++) {
        SituationBeginFrame();
        RenderScene();
        SituationEndFrame();
    }
    
    // Export profiling data
    SituationExportRenderHistogram("render_profile.csv");
    printf("Profiling data exported\n");
#endif

// Export on user request
if (SituationIsKeyPressed(SIT_KEY_F12)) {
    SituationExportRenderHistogram("render_histogram.csv");
    printf("Histogram exported\n");
}
```

**Notes:**
- Captures frame timing, draw call counts, etc.
- File format depends on extension (.csv or .json)
- Use for performance optimization
- Data accumulates until export

</details>
<details>
<summary><h3>Image Module</h3></summary>

**Overview:** The Image module is a comprehensive, CPU-side toolkit for all forms of image and font manipulation. It allows you to load images, generate new images programmatically, perform transformations, and render text. The `SituationImage` objects produced by this module are the primary source for creating GPU-side `SituationTexture`s.

### Structs and Enums

#### `Vector2`
A simple 2D vector, used for positions, sizes, and other 2D coordinates.
```c
typedef struct Vector2 {
    float x;
    float y;
} Vector2;
```
-   `x`: The x-component of the vector.
-   `y`: The y-component of the vector.

---
#### `Rectangle`
Represents a rectangle with a position (x, y) and dimensions (width, height).
```c
typedef struct Rectangle {
    float x;
    float y;
    float width;
    float height;
} Rectangle;
```
-   `x`, `y`: The screen coordinates of the top-left corner.
-   `width`, `height`: The dimensions of the rectangle.

---
#### `SituationImage`
A handle representing a CPU-side image. It contains the raw pixel data and metadata. All pixel data is stored in uncompressed 32-bit RGBA format unless otherwise specified. This struct is the primary source for creating GPU-side `SituationTexture`s.
```c
typedef struct SituationImage {
    void *data;
    int width;
    int height;
    int mipmaps;
    int format;
} SituationImage;
```
-   `data`: A pointer to the raw pixel data in system memory (RAM).
-   `width`: The width of the image in pixels.
-   `height`: The height of the image in pixels.
-   `mipmaps`: The number of mipmap levels generated for the image. `1` means no mipmaps.
-   `format`: The pixel format of the data (e.g., `SIT_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8`).

---
#### `SituationFont`
A handle representing a font, which includes a texture atlas of its characters (glyphs) and the data needed to render them. Fonts are typically loaded from TTF/OTF files and are used for both CPU-side (`SituationImageDrawText`) and GPU-side rendering.
```c
typedef struct SituationFont {
    int baseSize;
    int glyphCount;
    int glyphPadding;
    SituationTexture texture;
    Rectangle *recs;
    GlyphInfo *glyphs;
} SituationFont;
```
-   `baseSize`: The base font size (in pixels) that the font atlas was generated with.
-   `glyphCount`: The number of character glyphs packed into the texture atlas.
-   `glyphPadding`: The padding (in pixels) around each glyph in the atlas to prevent texture bleeding.
-   `texture`: The GPU `SituationTexture` handle for the font atlas image.
-   `recs`: A pointer to an array of `Rectangle` structs, defining the source rectangle for each glyph within the texture atlas.
-   `glyphs`: A pointer to an array of internal `GlyphInfo` structs, containing the character code, advance width, and offset for each glyph. This data is used for positioning characters correctly when rendering text.

---
#### `GlyphInfo`
Contains the rendering metrics for a single character glyph within a `SituationFont`. This struct is managed by the library and is used internally to calculate the correct position and spacing for each character when drawing text.
```c
typedef struct GlyphInfo {
    int value;
    int offsetX;
    int offsetY;
    int advanceX;
    SituationImage image;
} GlyphInfo;
```
-   `value`: The Unicode codepoint for the character (e.g., `65` for 'A').
-   `offsetX`, `offsetY`: The offset from the cursor position to the top-left corner of the glyph image when rendering.
-   `advanceX`: The horizontal distance to advance the cursor after rendering this glyph.
-   `image`: A CPU-side `SituationImage` containing the pixel data for the glyph. This is primarily used during the font atlas generation process.

### Functions

#### Image Loading and Unloading
---
#### `SituationLoadImage`
Loads an image from a file into CPU memory (RAM). Supported formats include PNG, BMP, TGA, and JPEG. All loaded images are converted to a 32-bit RGBA format.
```c
SituationError SituationLoadImage(const char *fileName, SituationImage* out_image);
```
**Usage Example:**
```c
// Load an image to be used for a player sprite.
SituationImage player_avatar;
if (SituationLoadImage("assets/sprites/player.png", &player_avatar) == SITUATION_SUCCESS) {
    // The image is now in CPU memory, ready to be manipulated or uploaded to the GPU.
    SituationTexture player_texture;
    SituationCreateTexture(player_avatar, true, &player_texture);
    // Once uploaded to a texture, the CPU-side copy can often be safely unloaded.
    SituationUnloadImage(player_avatar);
}
```

---
#### `SituationUnloadImage`
Unloads a CPU-side image and frees its associated memory.
```c
void SituationUnloadImage(SituationImage image);
```
**Usage Example:**
```c
SituationImage temp_image = SituationGenImageColor(128, 128, (ColorRGBA){255, 0, 255, 255});
// ... perform some operations on the image ...
SituationUnloadImage(temp_image); // Free the memory when done.
```
---
#### `SituationLoadImageFromMemory`
Loads an image from a data buffer in memory. The `fileType` parameter must include the leading dot (e.g., `.png`).
```c
SituationError SituationLoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, SituationImage* out_image);
```
**Usage Example:**
```c
// Assume 'g_embedded_player_png' is a byte array with an embedded PNG file,
// and 'g_embedded_player_png_len' is its size.
SituationImage player_img;
SituationLoadImageFromMemory(".png", g_embedded_player_png, g_embedded_player_png_len, &player_img);
// ... use player_img ...
SituationUnloadImage(player_img);
```

---
#### `SituationLoadImageFromTexture`
Creates a CPU-side `SituationImage` by reading back pixel data from a GPU `SituationTexture`. This is a slow operation (GPU-to-CPU transfer) and should be used sparingly.

```c
SituationImage SituationLoadImageFromTexture(SituationTexture texture);
```

**Parameters:**
- `texture` - GPU texture to read from

**Returns:** CPU-side image with pixel data, or invalid image on failure

**Usage Example:**
```c
// Read back render target for processing
SituationTexture render_target = GetRenderTargetTexture();
SituationImage cpu_image = SituationLoadImageFromTexture(render_target);

if (SituationIsImageValid(&cpu_image)) {
    // Process on CPU
    ApplyCustomFilter(&cpu_image);
    
    // Save to disk
    SituationExportImage(cpu_image, "processed.png");
    
    // Upload back to GPU
    SituationTexture new_texture;
    SituationCreateTexture(cpu_image, false, &new_texture);
    
    SituationUnloadImage(cpu_image);
}

// Screenshot functionality
SituationTexture screen_texture = GetScreenTexture();
SituationImage screenshot = SituationLoadImageFromTexture(screen_texture);
SituationExportImage(screenshot, "screenshot.png");
SituationUnloadImage(screenshot);

// Debug texture contents
SituationImage debug_img = SituationLoadImageFromTexture(texture);
printf("Texture size: %dx%d, channels: %d\n",
    debug_img.width, debug_img.height, debug_img.channels);
// Inspect pixel data...
SituationUnloadImage(debug_img);
```

**Notes:**
- **SLOW** - causes GPU-to-CPU transfer and pipeline stall
- Use only when necessary (screenshots, debugging)
- Avoid in performance-critical code
- Returned image must be freed with `SituationUnloadImage()`
- Check validity with `SituationIsImageValid()`

---
#### `SituationExportImage`
Exports the pixel data of a `SituationImage` to a file. The file format is determined by the extension. Currently, `.png` and `.bmp` are supported.
```c
SituationError SituationExportImage(SituationImage image, const char *fileName);
```
**Usage Example:**
```c
// Take a screenshot and save it as a PNG.
SituationImage screenshot = {0};
if (SituationLoadImageFromScreen(&screenshot) == SITUATION_SUCCESS) {
    SituationExportImage(screenshot, "screenshots/capture.png");
    SituationUnloadImage(screenshot);
}
```

---
#### Image Generation and Manipulation

---
#### Image Generation and Manipulation
---
#### `SituationImageFromImage`
Creates a new `SituationImage` by copying a sub-rectangle from a source image. This is useful for extracting sprites from a spritesheet.
```c
SituationImage SituationImageFromImage(SituationImage image, Rectangle rect);
```
**Usage Example:**
```c
// Extract a 16x16 sprite from a larger spritesheet.
SituationImage spritesheet;
if (SituationLoadImage("assets/sprites.png", &spritesheet) == SITUATION_SUCCESS) {
    Rectangle sprite_rect = { .x = 32, .y = 16, .width = 16, .height = 16 };
    SituationImage single_sprite = SituationImageFromImage(spritesheet, sprite_rect);
    // 'single_sprite' is now a new 16x16 image that can be used independently.
    // ... use single_sprite ...
    SituationUnloadImage(single_sprite);
    SituationUnloadImage(spritesheet);
}
```

---
#### `SituationImageCopy`
Creates a new image by making a deep copy of another image. This allocates new memory and copies all pixel data, allowing you to modify the copy without affecting the original.

```c
SituationError SituationImageCopy(SituationImage image, SituationImage* out_image);
```

**Parameters:**
- `image` - The source image to copy from
- `out_image` - Pointer to receive the newly created image copy

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Load an image and create a backup copy
SituationImage original = SituationLoadImage("photo.png");
SituationImage backup;

if (SituationImageCopy(original, &backup) == SITUATION_SUCCESS) {
    // Modify the original without affecting the backup
    SituationImageFlipVertical(&original);
    SituationImageAdjustHSV(&original, 0.1f, 1.2f, 1.0f);
    
    // Save both versions
    SituationSaveImage(original, "photo_modified.png");
    SituationSaveImage(backup, "photo_original.png");
    
    SituationUnloadImage(backup);
}
SituationUnloadImage(original);
```

**Notes:**
- Allocates new memory for the copy - remember to call `SituationUnloadImage()` on both images
- Copies all pixel data, dimensions, and channel information
- Useful for creating variations of an image or preserving original state

---
#### `SituationCreateImage`
Allocates a new SituationImage container with UNINITIALIZED data. You must fill the image data manually after creation.

```c
SituationError SituationCreateImage(int width, int height, int channels, SituationImage* out_image);
```

**Parameters:**
- `width` - Image width in pixels
- `height` - Image height in pixels
- `channels` - Number of color channels (1=grayscale, 3=RGB, 4=RGBA)
- `out_image` - Pointer to receive the created image

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create empty image and fill manually
SituationImage image;
if (SituationCreateImage(256, 256, 4, &image) == SITUATION_SUCCESS) {
    // Fill with procedural pattern
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            int index = (y * 256 + x) * 4;
            image.data[index + 0] = x;        // R
            image.data[index + 1] = y;        // G
            image.data[index + 2] = 128;      // B
            image.data[index + 3] = 255;      // A
        }
    }
    
    // Upload to GPU
    SituationTexture texture;
    SituationCreateTexture(image, false, &texture);
    
    SituationUnloadImage(image);
}

// Create image for custom rendering
SituationImage canvas;
SituationCreateImage(512, 512, 4, &canvas);

// Draw to it
SituationImageDrawRectangle(&canvas, 10, 10, 100, 100, RED);
SituationImageDrawText(&canvas, font, "Hello", 20, 20, WHITE);

// Use it
SituationTexture tex;
SituationCreateTexture(canvas, false, &tex);
SituationUnloadImage(canvas);
```

**Notes:**
- Data is UNINITIALIZED - contains garbage
- Use `SituationGenImageColor()` for solid color images
- Must call `SituationUnloadImage()` when done
- Data layout: row-major, channels interleaved

---
#### `SituationGenImageColor`
Generates a new image filled with a single, solid color.
```c
SituationError SituationGenImageColor(int width, int height, ColorRGBA color, SituationImage* out_image);
```
**Usage Example:**
```c
// Create a solid red 1x1 pixel image to use as a default texture.
SituationImage red_pixel;
if (SituationGenImageColor(1, 1, (ColorRGBA){255, 0, 0, 255}, &red_pixel) == SITUATION_SUCCESS) {
    SituationTexture default_texture;
    SituationCreateTexture(red_pixel, false, &default_texture);
    SituationUnloadImage(red_pixel);
}
```

---
#### `SituationGenImageGradient`
Generates an image with a linear, radial, or square gradient.
```c
SituationError SituationGenImageGradient(int width, int height, ColorRGBA tl, ColorRGBA tr, ColorRGBA bl, ColorRGBA br, SituationImage* out_image);
```
**Usage Example:**
```c
// Create a vertical gradient from red to black
SituationImage background;
if (SituationGenImageGradient(1280, 720, (ColorRGBA){255,0,0,255}, (ColorRGBA){255,0,0,255}, (ColorRGBA){0,0,0,255}, (ColorRGBA){0,0,0,255}, &background) == SITUATION_SUCCESS) {
    // ... use background ...
    SituationUnloadImage(background);
}
```

---
#### `SituationImageClearBackground`
Fills the entire destination image with a specified solid color, replacing all existing pixel data.

```c
void SituationImageClearBackground(SituationImage *dst, ColorRGBA color);
```

**Parameters:**
- `dst` - Pointer to the image to fill
- `color` - The color to fill with (RGBA format)

**Usage Example:**
```c
// Create an image and fill it with a solid blue background
SituationImage canvas;
SituationCreateImage(800, 600, 4, &canvas);
SituationImageClearBackground(&canvas, (ColorRGBA){30, 60, 120, 255});

// Now draw on top of the blue background
SituationImage logo = SituationLoadImage("logo.png");
SituationImageDraw(&canvas, logo, 
    (Rectangle){0, 0, logo.width, logo.height},
    (Rectangle){100, 100, 200, 200},
    (ColorRGBA){255, 255, 255, 255});

SituationUnloadImage(logo);
SituationUnloadImage(canvas);
```

**Notes:**
- Modifies the image in-place
- Fills all pixels regardless of the image's channel count
- Useful for initializing canvases before drawing operations

---
#### `SituationImageDraw`
Draws a source image (or a sub-rectangle of it) onto a destination image, with scaling and tinting.
```c
void SituationImageDraw(SituationImage *dst, SituationImage src, Rectangle srcRect, Rectangle dstRect, ColorRGBA tint);
```

---
#### `SituationImageDrawRectangle` / `SituationImageDrawLine`
Draws a colored rectangle or line directly onto an image's pixel data.
```c
void SituationImageDrawRectangle(SituationImage *dst, Rectangle rect, ColorRGBA color);
void SituationImageDrawLine(SituationImage *dst, Vector2 start, Vector2 end, ColorRGBA color);
```
**Usage Example:**
```c
// Create a canvas and draw a red border around it.
SituationImage canvas = SituationGenImageColor(256, 256, (ColorRGBA){255,255,255,255});
Rectangle border = { .x = 0, .y = 0, .width = 256, .height = 256 };
SituationImageDrawRectangleLines(&canvas, border, 4, (ColorRGBA){255,0,0,255});
```

---
#### `SituationImageCrop` / `SituationImageResize`
Crops or resizes an image in-place.
```c
void SituationImageCrop(SituationImage *image, Rectangle crop);
void SituationImageResize(SituationImage *image, int newWidth, int newHeight);
```
**Usage Example:**
```c
SituationImage atlas;
if (SituationLoadImage("sprite_atlas.png", &atlas) == SITUATION_SUCCESS) {
    // Crop the atlas to get the first sprite (e.g., a 32x32 sprite at top-left)
    SituationImageCrop(&atlas, (Rectangle){0, 0, 32, 32});
    // Now 'atlas' contains only the cropped sprite data.
    SituationUnloadImage(atlas);
}
```

---
#### `SituationImageFlipVertical` / `SituationImageFlipHorizontal`
Flips an image vertically or horizontally in-place.
```c
void SituationImageFlipVertical(SituationImage *image);
void SituationImageFlipHorizontal(SituationImage *image);
```

---
#### `SituationImageRotate`
Rotates an image by multiples of 90 degrees clockwise. The image is modified in-place, with dimensions being swapped for 90° and 270° rotations.

```c
void SituationImageRotate(SituationImage *image, int rotations);
```

**Parameters:**
- `image` - Pointer to the image to rotate (modified in-place)
- `rotations` - Number of 90-degree clockwise rotations (0-3, values wrap around)

**Usage Example:**
```c
// Load an image and create rotated versions
SituationImage original = SituationLoadImage("photo.png");

// Rotate 90 degrees clockwise
SituationImage rot90;
SituationImageCopy(original, &rot90);
SituationImageRotate(&rot90, 1);
SituationSaveImage(rot90, "photo_90.png");

// Rotate 180 degrees
SituationImage rot180;
SituationImageCopy(original, &rot180);
SituationImageRotate(&rot180, 2);
SituationSaveImage(rot180, "photo_180.png");

// Rotate 270 degrees (or 90 counter-clockwise)
SituationImage rot270;
SituationImageCopy(original, &rot270);
SituationImageRotate(&rot270, 3);
SituationSaveImage(rot270, "photo_270.png");

SituationUnloadImage(original);
SituationUnloadImage(rot90);
SituationUnloadImage(rot180);
SituationUnloadImage(rot270);
```

**Notes:**
- Only supports 90-degree increments (no arbitrary angles)
- Modifies the image in-place
- For 90° and 270° rotations, width and height are swapped
- Very fast operation (no interpolation needed)
- Useful for correcting image orientation or creating sprite variations

---
#### `SituationImageColorTint` / `SituationImageColorInvert`
Applies a color tint or inverts the colors of an image in-place.
```c
void SituationImageColorTint(SituationImage *image, ColorRGBA color);
void SituationImageColorInvert(SituationImage *image);
```

---
#### `SituationImageColorGrayscale` / `SituationImageColorContrast` / `SituationImageColorBrightness`
Adjusts the grayscale, contrast, or brightness of an image in-place.
```c
void SituationImageColorGrayscale(SituationImage *image);
void SituationImageColorContrast(SituationImage *image, float contrast);
void SituationImageColorBrightness(SituationImage *image, int brightness);
```

---
#### `SituationImageAlphaMask` / `SituationImagePremultiplyAlpha`
Applies an alpha mask to an image or premultiplies the color channels by the alpha channel.
```c
void SituationImageAlphaMask(SituationImage *image, SituationImage alphaMask);
void SituationImagePremultiplyAlpha(SituationImage *image);
```

---
#### Font and Text Rendering
---
#### `SituationLoadFont` / `SituationUnloadFont`
Loads a font from a TTF/OTF file for CPU-side rendering, and later unloads it.
```c
SituationError SituationLoadFont(const char *fileName, SituationFont* out_font);
void SituationUnloadFont(SituationFont font);
```

---
#### `SituationLoadFontFromMemory`

Loads a TrueType/OpenType font from a data buffer in memory.

```c
SituationError SituationLoadFontFromMemory(const void *fileData, int dataSize, SituationFont* out_font);
```

**Parameters:**
- `fileData`: Pointer to the font file data in memory
- `dataSize`: Size of the font data in bytes
- `out_font`: Pointer to receive the loaded font handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

---
#### `SituationLoadBitmapFontFromMemory`

Loads a raw bitmap font from memory for pixel-perfect retro aesthetics. Unlike TrueType fonts, bitmap fonts are pre-rendered at a fixed size and stored as a grid of character images.

```c
SituationError SituationLoadBitmapFontFromMemory(
    const unsigned char* data, 
    int char_width, 
    int char_height, 
    int num_chars, 
    SituationFont* out_font
);
```

**Parameters:**
- `data`: Pointer to raw bitmap data (1 byte per pixel, row-major order)
- `char_width`: Width of each character in pixels (e.g., 8 for an 8x8 font)
- `char_height`: Height of each character in pixels
- `num_chars`: Number of characters in the font (typically 256 for full ASCII/CP437)
- `out_font`: Pointer to receive the loaded font handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**New in v2.3.38:** Added for native bitmap font support, enabling retro-style pixel-perfect text rendering.

**Usage Example:**
```c
// Load the built-in 8x8 CP437 font
extern const unsigned char sit_default_8x8_font[256 * 8];
SituationFont retro_font;
if (SituationLoadBitmapFontFromMemory(sit_default_8x8_font, 8, 8, 256, &retro_font) == SITUATION_SUCCESS) {
    // Use the font for retro-style text rendering
    SituationImageDrawText(&canvas, retro_font, "RETRO TEXT", pos, 8, 1, WHITE);
}
```

**Note:** Bitmap fonts are ideal for:
- Retro/pixel art games
- Terminal emulators
- Fixed-width console output
- Situations where TrueType rendering overhead is undesirable

---
#### `SituationBakeFontAtlas`
Generates a texture atlas (a single image containing all characters) from a font. This pre-renders all glyphs to a texture for efficient rendering. Usually called automatically, but can be used for custom font sizes.

```c
SituationError SituationBakeFontAtlas(SituationFont* font, float fontSizePixels);
```

**Parameters:**
- `font` - Font to bake atlas for
- `fontSizePixels` - Size to render glyphs at (e.g., 16, 24, 32)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Load font and bake custom size
SituationFont font;
if (SituationLoadFont("fonts/arial.ttf", &font) == SITUATION_SUCCESS) {
    // Bake atlas at 24px
    SituationBakeFontAtlas(&font, 24.0f);
    
    // Now use the font
    SituationCmdDrawText(cmd, font, "Hello", 100, 100, 24.0f, WHITE);
}

// Pre-bake multiple sizes for different UI elements
SituationFont ui_font;
SituationLoadFont("fonts/roboto.ttf", &ui_font);

// Bake for title text (32px)
SituationBakeFontAtlas(&ui_font, 32.0f);

// Bake for body text (16px)
SituationBakeFontAtlas(&ui_font, 16.0f);

// Bake for small text (12px)
SituationBakeFontAtlas(&ui_font, 12.0f);

// Custom atlas for high-DPI displays
SituationFont hd_font;
SituationLoadFont("fonts/font.ttf", &hd_font);
SituationBakeFontAtlas(&hd_font, 48.0f);  // 2x size for retina
```

**Notes:**
- Usually called automatically on first use
- Call manually to pre-bake specific sizes
- Larger sizes use more texture memory
- Atlas is cached per font size
- Useful for pre-loading fonts at startup

---
#### `SituationMeasureText`
Measures the dimensions of a string of text if it were to be rendered with a specific font, size, and spacing.
```c
Rectangle SituationMeasureText(SituationFont font, const char *text, float fontSize);
```
**Usage Example:**
```c
const char* button_text = "Click Me!";
Rectangle text_size = SituationMeasureText(my_font, button_text, 20);
// Now you can create a button rectangle that perfectly fits the text.
Rectangle button_rect = { .x = 100, .y = 100, .width = text_size.width + 20, .height = text_size.height + 10 };
```

---
#### `SituationImageDrawText`
Draws a simple, tinted text string onto an image.
```c
void SituationImageDrawText(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, ColorRGBA tint);
```
**Usage Example:**
```c
SituationImage canvas;
SituationGenImageColor(800, 600, (ColorRGBA){20, 20, 20, 255}, &canvas);
SituationFont my_font;
if (SituationLoadFont("fonts/my_font.ttf", &my_font) == SITUATION_SUCCESS) {
    SituationImageDrawText(&canvas, my_font, "Hello, World!", (Vector2){50, 50}, 40, 1, (ColorRGBA){255, 255, 255, 255});

    // ... you can now upload 'canvas' to a GPU texture ...

    SituationUnloadFont(my_font);
}
SituationUnloadImage(canvas);
```

---
#### Pixel-Level Access
---
#### `SituationGetPixelColor`
Gets the color of a single pixel from an image.
```c
ColorRGBA SituationGetPixelColor(SituationImage image, int x, int y);
```
**Usage Example:**
```c
SituationImage my_image;
if (SituationLoadImage("assets/my_image.png", &my_image) == SITUATION_SUCCESS) {
    ColorRGBA top_left_pixel = SituationGetPixelColor(my_image, 0, 0);
    printf("Top-left pixel color: R%d G%d B%d A%d\n",
           top_left_pixel.r, top_left_pixel.g, top_left_pixel.b, top_left_pixel.a);
    SituationUnloadImage(my_image);
}
```

---
#### `SituationSetPixelColor`
Sets the color of a single pixel in an image.
```c
void SituationSetPixelColor(SituationImage* image, int x, int y, ColorRGBA color);
```
**Usage Example:**
```c
SituationImage canvas = SituationGenImageColor(10, 10, (ColorRGBA){0, 0, 0, 255});
// Draw a red pixel in the center
SituationSetPixelColor(&canvas, 5, 5, (ColorRGBA){255, 0, 0, 255});
// ... use the canvas ...
SituationUnloadImage(canvas);
```

---
#### `SituationIsImageValid`
Checks if an image has been loaded successfully and contains valid data. This is useful for error checking after loading operations.

```c
bool SituationIsImageValid(SituationImage image);
```

**Parameters:**
- `image` - The image to validate

**Returns:** `true` if the image is valid and ready to use, `false` otherwise

**Usage Example:**
```c
// Safely load an image with error checking
SituationImage texture_image = SituationLoadImage("textures/wall.png");

if (SituationIsImageValid(texture_image)) {
    // Image loaded successfully, create GPU texture
    SituationTexture wall_texture = SituationCreateTextureFromImage(texture_image);
    SituationUnloadImage(texture_image);
    
    // Use the texture...
} else {
    // Image failed to load, use fallback
    printf("Failed to load wall.png, using default texture\n");
    SituationImage fallback;
    SituationGenImageColor(64, 64, (ColorRGBA){128, 128, 128, 255}, &fallback);
    SituationTexture wall_texture = SituationCreateTextureFromImage(fallback);
    SituationUnloadImage(fallback);
}
```

**Notes:**
- Returns `false` if the image data pointer is NULL or dimensions are invalid
- Always check validity after loading from files to handle missing or corrupted files
- Useful for implementing fallback textures in asset loading systems

---
#### `SituationImageDrawAlpha`
Draws a portion of a source image onto a destination image with alpha blending and optional color tinting. This respects the alpha channel of both images for proper transparency compositing.

```c
void SituationImageDrawAlpha(SituationImage *dst, SituationImage src, Rectangle srcRect, Vector2 dstPos, ColorRGBA tint);
```

**Parameters:**
- `dst` - Pointer to the destination image
- `src` - The source image to draw from
- `srcRect` - Rectangle defining the portion of the source image to draw
- `dstPos` - Position in the destination image to draw to
- `tint` - Color tint to apply (use white {255,255,255,255} for no tint)

**Usage Example:**
```c
// Create a composite image with multiple layers
SituationImage canvas;
SituationCreateImage(800, 600, 4, &canvas);
SituationImageClearBackground(&canvas, (ColorRGBA){50, 50, 50, 255});

// Draw background layer
SituationImage background = SituationLoadImage("background.png");
SituationImageDrawAlpha(&canvas, background,
    (Rectangle){0, 0, background.width, background.height},
    (Vector2){0, 0},
    (ColorRGBA){255, 255, 255, 255});

// Draw character sprite with transparency
SituationImage character = SituationLoadImage("character.png");
SituationImageDrawAlpha(&canvas, character,
    (Rectangle){0, 0, 64, 64},
    (Vector2){300, 400},
    (ColorRGBA){255, 255, 255, 255});

// Draw UI element with red tint and 50% opacity
SituationImage icon = SituationLoadImage("icon.png");
SituationImageDrawAlpha(&canvas, icon,
    (Rectangle){0, 0, 32, 32},
    (Vector2){750, 20},
    (ColorRGBA){255, 100, 100, 128});

SituationSaveImage(canvas, "composite.png");
SituationUnloadImage(background);
SituationUnloadImage(character);
SituationUnloadImage(icon);
SituationUnloadImage(canvas);
```

**Notes:**
- Properly handles alpha blending using the alpha channels of both images
- Tint color's alpha channel controls overall opacity
- Source rectangle can be used to draw sprite atlas regions
- Useful for compositing layers, sprite rendering, and UI overlays

---
#### `SituationImageResize`
Resizes an image to new dimensions using high-quality bicubic interpolation. The image is modified in-place, with memory being reallocated as needed.

```c
void SituationImageResize(SituationImage *image, int newWidth, int newHeight);
```

**Parameters:**
- `image` - Pointer to the image to resize (modified in-place)
- `newWidth` - The new width in pixels
- `newHeight` - The new height in pixels

**Usage Example:**
```c
// Load a high-resolution image and create a thumbnail
SituationImage photo = SituationLoadImage("photo_4k.png");
printf("Original size: %dx%d\n", photo.width, photo.height);

// Resize to thumbnail dimensions
SituationImageResize(&photo, 256, 256);
printf("Thumbnail size: %dx%d\n", photo.width, photo.height);

// Save the thumbnail
SituationSaveImage(photo, "photo_thumb.png");
SituationUnloadImage(photo);
```

**Notes:**
- Uses bicubic interpolation for high-quality scaling
- Modifies the image in-place - original data is lost
- Memory is automatically reallocated to fit the new dimensions
- For creating multiple sizes, use `SituationImageCopy()` first to preserve the original

---
#### `SituationImageFlip`
Flips an image horizontally, vertically, or both. The image is modified in-place.

```c
void SituationImageFlip(SituationImage *image, SituationImageFlipMode mode);
```

**Parameters:**
- `image` - Pointer to the image to flip (modified in-place)
- `mode` - The flip mode: `SITUATION_FLIP_HORIZONTAL`, `SITUATION_FLIP_VERTICAL`, or `SITUATION_FLIP_BOTH`

**Usage Example:**
```c
// Load an image and create mirrored versions
SituationImage original = SituationLoadImage("arrow.png");

// Create horizontal mirror
SituationImage h_mirror;
SituationImageCopy(original, &h_mirror);
SituationImageFlip(&h_mirror, SITUATION_FLIP_HORIZONTAL);
SituationSaveImage(h_mirror, "arrow_h_flip.png");

// Create vertical mirror
SituationImage v_mirror;
SituationImageCopy(original, &v_mirror);
SituationImageFlip(&v_mirror, SITUATION_FLIP_VERTICAL);
SituationSaveImage(v_mirror, "arrow_v_flip.png");

// Create 180-degree rotation (both flips)
SituationImage rotated;
SituationImageCopy(original, &rotated);
SituationImageFlip(&rotated, SITUATION_FLIP_BOTH);
SituationSaveImage(rotated, "arrow_180.png");

SituationUnloadImage(original);
SituationUnloadImage(h_mirror);
SituationUnloadImage(v_mirror);
SituationUnloadImage(rotated);
```

**Notes:**
- Modifies the image in-place
- Very fast operation (no interpolation needed)
- Useful for creating sprite variations or correcting image orientation

---
#### `SituationImageAdjustHSV`
Adjusts the Hue, Saturation, and Value (brightness) of an image with optional mixing. This allows for color correction, stylization, and visual effects.

```c
void SituationImageAdjustHSV(SituationImage *image, float hue_shift, float sat_factor, float val_factor, float mix);
```

**Parameters:**
- `image` - Pointer to the image to adjust (modified in-place)
- `hue_shift` - Hue rotation in normalized range (-1.0 to 1.0, where 1.0 = 360°)
- `sat_factor` - Saturation multiplier (0.0 = grayscale, 1.0 = original, >1.0 = more saturated)
- `val_factor` - Value/brightness multiplier (0.0 = black, 1.0 = original, >1.0 = brighter)
- `mix` - Blend factor between original and adjusted (0.0 = original, 1.0 = fully adjusted)

**Usage Example:**
```c
// Load an image and create color variations
SituationImage photo = SituationLoadImage("landscape.png");

// Create a warmer version (shift hue toward red/orange)
SituationImage warm;
SituationImageCopy(photo, &warm);
SituationImageAdjustHSV(&warm, 0.05f, 1.1f, 1.05f, 1.0f);
SituationSaveImage(warm, "landscape_warm.png");

// Create a cooler version (shift hue toward blue)
SituationImage cool;
SituationImageCopy(photo, &cool);
SituationImageAdjustHSV(&cool, -0.1f, 1.2f, 0.95f, 1.0f);
SituationSaveImage(cool, "landscape_cool.png");

// Create a desaturated version (50% grayscale)
SituationImage desaturated;
SituationImageCopy(photo, &desaturated);
SituationImageAdjustHSV(&desaturated, 0.0f, 0.0f, 1.0f, 0.5f);
SituationSaveImage(desaturated, "landscape_desat.png");

SituationUnloadImage(photo);
SituationUnloadImage(warm);
SituationUnloadImage(cool);
SituationUnloadImage(desaturated);
```

**Notes:**
- Modifies the image in-place
- Hue shift wraps around (e.g., red → orange → yellow → green → cyan → blue → magenta → red)
- Saturation factor of 0.0 produces grayscale
- Value factor affects overall brightness
- Mix parameter allows subtle adjustments by blending with original

---
#### `SituationUnloadFont`
Unloads a CPU-side font and frees its memory. Always call this when done with a font to prevent memory leaks.

```c
void SituationUnloadFont(SituationFont font);
```

**Parameters:**
- `font` - Font to unload

**Usage Example:**
```c
// Load and use font
SituationFont font;
if (SituationLoadFont("fonts/arial.ttf", &font) == SITUATION_SUCCESS) {
    // Use the font
    SituationCmdDrawText(cmd, font, "Hello", 100, 100, 24.0f, WHITE);
    
    // Cleanup when done
    SituationUnloadFont(font);
}

// Proper resource management
SituationFont ui_font;
SituationFont title_font;

void LoadFonts() {
    SituationLoadFont("fonts/roboto.ttf", &ui_font);
    SituationLoadFont("fonts/title.ttf", &title_font);
}

void UnloadFonts() {
    SituationUnloadFont(ui_font);
    SituationUnloadFont(title_font);
}

// Font switching
SituationFont current_font;
SituationLoadFont("fonts/default.ttf", &current_font);

// Switch to different font
SituationUnloadFont(current_font);
SituationLoadFont("fonts/fancy.ttf", &current_font);

// Cleanup at exit
SituationUnloadFont(current_font);

// Multiple fonts
#define MAX_FONTS 5
SituationFont fonts[MAX_FONTS];
const char* font_paths[] = {
    "fonts/regular.ttf",
    "fonts/bold.ttf",
    "fonts/italic.ttf",
    "fonts/mono.ttf",
    "fonts/title.ttf"
};

// Load all
for (int i = 0; i < MAX_FONTS; i++) {
    SituationLoadFont(font_paths[i], &fonts[i]);
}

// Cleanup all
for (int i = 0; i < MAX_FONTS; i++) {
    SituationUnloadFont(fonts[i]);
}
```

**Notes:**
- Frees font atlas texture and glyph data
- Safe to call multiple times with same font
- Always call before application exit
- Don't use font after unloading
- Unloading doesn't affect already-rendered text

---
#### `SituationImageDrawCodepoint`
Draws a single Unicode character onto an image with advanced styling options including rotation, skew, fill color, and outline.

```c
void SituationImageDrawCodepoint(SituationImage *dst, SituationFont font, int codepoint, Vector2 position, float fontSize, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness);
```

**Parameters:**
- `dst` - Pointer to the destination image
- `font` - The font to use for rendering
- `codepoint` - Unicode codepoint of the character to draw (e.g., 'A' = 65, '★' = 9733)
- `position` - Position to draw the character (top-left corner)
- `fontSize` - Font size in pixels
- `rotationDegrees` - Rotation angle in degrees (clockwise)
- `skewFactor` - Horizontal skew factor (0.0 = no skew, positive = italic-like effect)
- `fillColor` - Color for the character fill
- `outlineColor` - Color for the character outline
- `outlineThickness` - Thickness of the outline in pixels (0.0 = no outline)

**Usage Example:**
```c
// Create a canvas and draw styled characters
SituationImage canvas;
SituationCreateImage(512, 512, 4, &canvas);
SituationImageClearBackground(&canvas, (ColorRGBA){255, 255, 255, 255});

SituationFont font = SituationLoadFont("fonts/arial.ttf", 64);

// Draw a red 'A' with black outline
SituationImageDrawCodepoint(&canvas, font, 'A', 
    (Vector2){50, 50}, 64.0f, 0.0f, 0.0f,
    (ColorRGBA){255, 0, 0, 255},
    (ColorRGBA){0, 0, 0, 255}, 2.0f);

// Draw a rotated blue star with yellow outline
SituationImageDrawCodepoint(&canvas, font, 9733, // Unicode star ★
    (Vector2){200, 200}, 72.0f, 45.0f, 0.0f,
    (ColorRGBA){0, 100, 255, 255},
    (ColorRGBA){255, 255, 0, 255}, 3.0f);

// Draw an italicized green '@' symbol
SituationImageDrawCodepoint(&canvas, font, '@',
    (Vector2){350, 100}, 48.0f, 0.0f, 0.3f,
    (ColorRGBA){0, 200, 0, 255},
    (ColorRGBA){0, 0, 0, 255}, 1.5f);

SituationSaveImage(canvas, "styled_characters.png");
SituationUnloadFont(font);
SituationUnloadImage(canvas);
```

**Notes:**
- Useful for creating logos, icons, or decorative text elements
- Supports full Unicode range (emoji, symbols, international characters)
- Rotation is applied around the character's origin point
- Skew factor creates italic-like effects
- Outline is drawn outside the character fill

---
#### `SituationImageDrawTextEx`
Draws a text string onto an image with advanced styling options including rotation, skew, spacing, fill color, and outline.

```c
void SituationImageDrawTextEx(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness);
```

**Parameters:**
- `dst` - Pointer to the destination image
- `font` - The font to use for rendering
- `text` - UTF-8 encoded text string to draw
- `position` - Position to draw the text (top-left corner)
- `fontSize` - Font size in pixels
- `spacing` - Additional spacing between characters in pixels
- `rotationDegrees` - Rotation angle in degrees (clockwise)
- `skewFactor` - Horizontal skew factor (0.0 = no skew, positive = italic-like effect)
- `fillColor` - Color for the text fill
- `outlineColor` - Color for the text outline
- `outlineThickness` - Thickness of the outline in pixels (0.0 = no outline)

**Usage Example:**
```c
// Create a thumbnail with styled text overlay
SituationImage thumbnail = SituationLoadImage("video_frame.png");
SituationFont bold_font = SituationLoadFont("fonts/bold.ttf", 48);

// Draw title with white text and black outline
SituationImageDrawTextEx(&thumbnail, bold_font, "EPISODE 1",
    (Vector2){50, 50}, 48.0f, 2.0f, 0.0f, 0.0f,
    (ColorRGBA){255, 255, 255, 255},
    (ColorRGBA){0, 0, 0, 255}, 3.0f);

// Draw subtitle with yellow italic text
SituationImageDrawTextEx(&thumbnail, bold_font, "The Beginning",
    (Vector2){50, 110}, 32.0f, 1.0f, -5.0f, 0.2f,
    (ColorRGBA){255, 255, 0, 255},
    (ColorRGBA){0, 0, 0, 255}, 2.0f);

// Draw watermark with semi-transparent text
SituationImageDrawTextEx(&thumbnail, bold_font, "© 2026 Studio",
    (Vector2){thumbnail.width - 200, thumbnail.height - 40}, 
    20.0f, 0.5f, 0.0f, 0.0f,
    (ColorRGBA){255, 255, 255, 128},
    (ColorRGBA){0, 0, 0, 0}, 0.0f);

SituationSaveImage(thumbnail, "thumbnail.png");
SituationUnloadFont(bold_font);
SituationUnloadImage(thumbnail);
```

**Notes:**
- Supports full UTF-8 text including emoji and international characters
- Spacing parameter adds extra pixels between each character
- Rotation is applied to the entire text string
- Skew factor creates italic-like effects
- Outline is drawn outside the text fill
- Useful for creating thumbnails, watermarks, memes, and image overlays

---

#### `SituationImageDraw`
Draws a source image onto a destination image.
```c
SITAPI void SituationImageDraw(SituationImage *dst, SituationImage src, Rectangle srcRect, Vector2 dstPos);
```
**Usage Example:**
```c
SituationImage canvas;
if (SituationGenImageColor(256, 26, (ColorRGBA){255, 255, 255, 255}, &canvas) == SITUATION_SUCCESS) {
    SituationImage sprite;
    if (SituationLoadImage("assets/sprite.png", &sprite) == SITUATION_SUCCESS) {
        Rectangle sprite_rect = { .x = 0, .y = 0, .width = 16, .height = 16 };
        Vector2 position = { .x = 120, .y = 120 };
        SituationImageDraw(&canvas, sprite, sprite_rect, position);
        SituationUnloadImage(sprite);
    }
    // ... use canvas ...
    SituationUnloadImage(canvas);
}
```

---

#### `SituationGenImageGradient`
Generates an image with a linear gradient.
```c
SITAPI SituationError SituationGenImageGradient(int width, int height, ColorRGBA tl, ColorRGBA tr, ColorRGBA bl, ColorRGBA br, SituationImage* out_image);
```
**Usage Example:**
```c
// Create a vertical gradient from red to black
SituationImage background;
if (SituationGenImageGradient(1280, 720, (ColorRGBA){255,0,0,255}, (ColorRGBA){255,0,0,255}, (ColorRGBA){0,0,0,255}, (ColorRGBA){0,0,0,255}, &background) == SITUATION_SUCCESS) {
    // ... use background ...
    SituationUnloadImage(background);
}
```

---

#### `SituationMeasureText`
Measures the dimensions of a string of text if it were to be rendered with a specific font and size.
```c
SITAPI Rectangle SituationMeasureText(SituationFont font, const char *text, float fontSize);
```
**Usage Example:**
```c
const char* button_text = "Click Me!";
SituationFont my_font;
if (SituationLoadFont("fonts/my_font.ttf", &my_font) == SITUATION_SUCCESS) {
    Rectangle text_bounds = SituationMeasureText(my_font, button_text, 20);
    // Now you can create a button rectangle that perfectly fits the text.
    Rectangle button_rect = { .x = 100, .y = 100, .width = text_bounds.width + 20, .height = text_bounds.height + 10 };
    SituationUnloadFont(my_font);
}
```

---
#### `SituationBlitRawDataToImage`
Copies raw pixel data directly into an image at a specified position.
```c
void SituationBlitRawDataToImage(
    SituationImage* dst,
    const unsigned char* src_data,
    int src_width,
    int src_height,
    int dst_x,
    int dst_y
);
```
**Usage Example:**
```c
// Copy a 32x32 sprite into a larger texture atlas
unsigned char sprite_data[32 * 32 * 4]; // RGBA data
SituationBlitRawDataToImage(&atlas, sprite_data, 32, 32, 0, 0);
```

---
#### `SituationImageDrawTextFormatted`
Draws formatted text (printf-style) onto an image.
```c
void SituationImageDrawTextFormatted(
    SituationImage* dst,
    SituationFont font,
    Vector2 pos,
    ColorRGBA color,
    const char* format,
    ...
);
```
**Usage Example:**
```c
// Draw dynamic text with variables
int score = 1000;
SituationImageDrawTextFormatted(&image, font, (Vector2){10, 10}, WHITE, "Score: %d", score);
```

</details>
<details>
<summary><h3>Graphics Module</h3></summary>

**Overview:** The Graphics module forms the core of the rendering pipeline, offering a powerful, backend-agnostic API for interacting with the GPU. It is responsible for all GPU resource management (meshes, shaders, textures) and its command-buffer-centric design (`SituationCmd...`) allows you to precisely sequence rendering operations.

### Structs, Enums, and Handles

#### `SituationCommandBuffer`
An opaque handle representing a command buffer, which is a list of rendering commands to be executed by the GPU. Command buffers record all rendering operations and are submitted to the GPU for execution.

```c
typedef struct SituationCommandBuffer_t* SituationCommandBuffer;
```

**Description:**
Command buffers are the primary way to record rendering work in Situation. All drawing commands (`SituationCmdDraw...`), state changes (`SituationCmdBind...`), and render passes are recorded into a command buffer before being submitted to the GPU.

**Usage Pattern:**
```c
// Get the main command buffer
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

// Begin a render pass
SituationRenderPassInfo pass_info = {
    .target_display_id = -1,  // Main window
    .clear_color = {0.1f, 0.1f, 0.1f, 1.0f}
};
SituationCmdBeginRenderPass(cmd, &pass_info);

// Record rendering commands
SituationCmdBindPipeline(cmd, shader);
SituationCmdBindDescriptorSet(cmd, 0, descriptor_set);
SituationCmdDrawMesh(cmd, mesh);

// End the render pass
SituationCmdEndRenderPass(cmd);

// Command buffer is automatically submitted at end of frame
```

**Key Concepts:**
- **Recording:** Commands are recorded into the buffer but not executed immediately
- **Submission:** The buffer is submitted to the GPU at the end of the frame
- **Reuse:** The main command buffer is reused each frame
- **Thread Safety:** Command buffers are not thread-safe; use one per thread

**Common Operations:**
```c
// Render pass management
SituationCmdBeginRenderPass(cmd, &pass_info);
SituationCmdEndRenderPass(cmd);

// Pipeline binding
SituationCmdBindPipeline(cmd, shader);
SituationCmdBindComputePipeline(cmd, compute_shader);

// Resource binding
SituationCmdBindDescriptorSet(cmd, 0, descriptor_set);
SituationCmdBindTextureSet(cmd, 0, texture_set);

// Drawing
SituationCmdDrawMesh(cmd, mesh);
SituationCmdDrawIndexed(cmd, index_count, 1, 0, 0, 0);
SituationCmdDispatch(cmd, work_groups_x, work_groups_y, work_groups_z);

// State changes
SituationCmdSetPushConstant(cmd, 0, &data, sizeof(data));
SituationCmdSetScissor(cmd, x, y, width, height);
SituationCmdPipelineBarrier(cmd, &barrier);
```

**Notes:**
- Opaque handle - internal structure is hidden
- Main command buffer obtained via `SituationGetMainCommandBuffer()`
- Commands execute in the order they're recorded
- Automatically submitted at `SituationEndFrame()`
- Reset automatically each frame

---
#### `SituationAttachmentInfo`
Describes a single attachment (like a color or depth buffer) for a render pass. It specifies how the attachment's contents should be handled at the beginning and end of the pass.
```c
typedef struct SituationAttachmentInfo {
    SituationAttachmentLoadOp  loadOp;
    SituationAttachmentStoreOp storeOp;
    SituationClearValue        clear;
} SituationAttachmentInfo;
```
-   `loadOp`: The action to take at the beginning of the render pass.
    -   `SIT_LOAD_OP_LOAD`: Preserve the existing contents of the attachment.
    -   `SIT_LOAD_OP_CLEAR`: Clear the attachment to the value specified in `clear`.
    -   `SIT_LOAD_OP_DONT_CARE`: The existing contents are undefined and can be discarded.
-   `storeOp`: The action to take at the end of the render pass.
    -   `SIT_STORE_OP_STORE`: Store the rendered contents to memory.
    -   `SIT_STORE_OP_DONT_CARE`: The rendered contents may be discarded.
-   `clear`: A struct containing the color or depth/stencil values to use if `loadOp` is `SIT_LOAD_OP_CLEAR`.

---
#### `SituationClearValue`
A union that specifies the clear values for color and depth/stencil attachments. It is used within the `SituationAttachmentInfo` struct to define what value an attachment should be cleared to at the start of a render pass.
```c
typedef union SituationClearValue {
    ColorRGBA color;
    struct {
        double depth;
        int32_t stencil;
    } depth_stencil;
} SituationClearValue;
```
-   `color`: The RGBA color value to clear a color attachment to.
-   `depth_stencil`: A struct containing the depth and stencil values to clear a depth/stencil attachment to.
    -   `depth`: The depth value, typically `1.0` for clearing.
    -   `stencil`: The stencil value, typically `0`.

---
#### `SituationRenderPassInfo`
Configures a rendering pass. This is a crucial struct used with `SituationCmdBeginRenderPass()` to define the render target and its initial state.
```c
typedef struct SituationRenderPassInfo {
    int                     display_id;
    SituationAttachmentInfo color_attachment;
    SituationAttachmentInfo depth_attachment;
} SituationRenderPassInfo;
```
-   `display_id`: The ID of a `SituationVirtualDisplay` to render to. Use `-1` to target the main window's backbuffer.
-   `color_attachment`: Configuration for the color buffer, including load/store operations and clear color.
-   `depth_attachment`: Configuration for the depth buffer, including load/store operations and clear value.

---
#### `ViewDataUBO`
Defines the standard memory layout for a Uniform Buffer Object (UBO) containing camera projection and view matrices. You don't typically create this struct directly; rather, you should structure your GLSL uniform blocks to match this layout to be compatible with the library's default scene data.
```c
typedef struct ViewDataUBO {
    mat4 view;
    mat4 projection;
} ViewDataUBO;
```
-   `view`: The view matrix, which transforms world-space coordinates to view-space (camera) coordinates.
-   `projection`: The projection matrix, which transforms view-space coordinates to clip-space coordinates.

---
#### Resource Handles
The following are opaque handles to GPU resources. Their internal structure is not exposed to the user. You create them with `SituationCreate...` or `SituationLoad...` functions and free them with their corresponding `SituationDestroy...` or `SituationUnload...` functions.

#### `SituationMesh`
An opaque handle to a self-contained GPU resource representing a drawable mesh. A mesh bundles a vertex buffer and an optional index buffer, representing a complete piece of geometry that can be rendered with a single command.
```c
typedef struct SituationMesh {
    uint64_t id;
    int index_count;
} SituationMesh;
```
- **Creation:** `SituationCreateMesh()`
- **Usage:** `SituationCmdDrawMesh()`
- **Destruction:** `SituationDestroyMesh()`

---
#### `SituationBuffer`
An opaque handle to a generic region of GPU memory. Buffers are highly versatile and can be used to store vertex data, index data, uniform data for shaders (UBOs), or general-purpose storage data (SSBOs). The intended usage is specified on creation using `SituationBufferUsageFlags`.
```c
typedef struct SituationBuffer {
    uint64_t id;
    size_t size_in_bytes;
} SituationBuffer;
```
- **Creation:** `SituationCreateBuffer()`
- **Usage:** `SituationUpdateBuffer()`, `SituationCmdBindVertexBuffer()`, `SituationCmdBindIndexBuffer()`, `SituationCmdBindDescriptorSet()`
- **Destruction:** `SituationDestroyBuffer()`

---
#### `SituationComputePipeline`
An opaque handle representing a compiled compute shader program. It encapsulates a single compute shader stage and its resource layout, ready to be dispatched for general-purpose GPU computation.
```c
typedef struct SituationComputePipeline {
    uint64_t id;
} SituationComputePipeline;
```
- **Creation:** `SituationCreateComputePipeline()`
- **Usage:** `SituationCmdBindComputePipeline()`, `SituationCmdDispatch()`
- **Destruction:** `SituationDestroyComputePipeline()`

---
#### `SituationShader`
An opaque handle representing a compiled graphics shader pipeline. It encapsulates a vertex shader, a fragment shader, and the state required to use them for rendering (like vertex input layout and descriptor set layouts).
```c
typedef struct SituationShader {
    uint64_t id;
} SituationShader;
```
- **Creation:** `SituationLoadShader()`, `SituationLoadShaderFromMemory()`
- **Usage:** `SituationCmdBindPipeline()`
- **Destruction:** `SituationUnloadShader()`

---
#### `SituationTexture`
An opaque handle to a GPU texture resource. Textures are created by uploading `SituationImage` data from the CPU. They are used by shaders for sampling colors (e.g., albedo maps) or as storage images for compute operations.
```c
typedef struct SituationTexture {
    uint64_t id;
    int width;
    int height;
    int mipmaps;
} SituationTexture;
```
-   `width`, `height`: The dimensions of the texture in pixels.
-   `mipmaps`: The number of mipmap levels in the texture.
- **Creation:** `SituationCreateTexture()`
- **Usage:** `SituationCmdBindShaderTexture()`, `SituationCmdBindComputeTexture()`
- **Destruction:** `SituationDestroyTexture()`

---
#### `SituationModelMesh`
Represents a single drawable sub-mesh within a larger `SituationModel`. It combines the raw geometry (`SituationMesh`) with a full PBR (Physically-Based Rendering) material definition, including color factors and texture maps.
```c
typedef struct SituationModelMesh {
    SituationMesh mesh;
    // Material Data
    int material_id;
    char material_name[SITUATION_MAX_NAME_LEN];
    ColorRGBA base_color_factor;
    float metallic_factor;
    float roughness_factor;
    vec3 emissive_factor;
    float alpha_cutoff;
    bool double_sided;
    // Texture Maps (if available)
    SituationTexture base_color_texture;
    SituationTexture metallic_roughness_texture;
    SituationTexture normal_texture;
    SituationTexture occlusion_texture;
    SituationTexture emissive_texture;
} SituationModelMesh;
```
-   `mesh`: The `SituationMesh` handle containing the vertex and index buffers for this part of the model.
-   `material_name`: The name of the material.
-   `base_color_factor`, `metallic_factor`, `roughness_factor`: PBR material parameters.
-   `base_color_texture`, `metallic_roughness_texture`, etc.: Handles to the GPU textures used by this material.

---
#### `SituationModel`
A handle representing a complete 3D model, loaded from a file (e.g., GLTF). It acts as a container for all the `SituationModelMesh` and `SituationTexture` resources that make up the model.
```c
typedef struct SituationModel {
    SituationModelMesh* meshes;
    SituationTexture* all_model_textures;
    int mesh_count;
    int texture_count;
} SituationModel;
```
-   `meshes`: A pointer to an array of the model's sub-meshes.
-   `all_model_textures`: A pointer to an array of all unique textures used by the model.
-   `mesh_count`, `texture_count`: The number of meshes and textures in their respective arrays.
- **Creation:** `SituationLoadModel()`
- **Usage:** `SituationDrawModel()`
- **Destruction:** `SituationUnloadModel()`

---
#### `SituationBufferUsageFlags`
Specifies how a `SituationBuffer` will be used. This helps the driver place the buffer in the most optimal memory. Combine flags using the bitwise `|` operator.
| Flag | Description |
|---|---|
| `SITUATION_BUFFER_USAGE_VERTEX_BUFFER` | The buffer will be used as a source of vertex data. |
| `SITUATION_BUFFER_USAGE_INDEX_BUFFER` | The buffer will be used as a source of index data. |
| `SITUATION_BUFFER_USAGE_UNIFORM_BUFFER` | The buffer will be used as a Uniform Buffer Object (UBO). |
| `SITUATION_BUFFER_USAGE_STORAGE_BUFFER` | The buffer will be used as a Shader Storage Buffer Object (SSBO). |
| `SITUATION_BUFFER_USAGE_INDIRECT_BUFFER`| The buffer will be used for indirect drawing commands. |
| `SITUATION_BUFFER_USAGE_TRANSFER_SRC`| The buffer can be used as a source for a copy operation. |
| `SITUATION_BUFFER_USAGE_TRANSFER_DST`| The buffer can be used as a destination for a copy operation. |

---
#### `SituationComputeLayoutType`
Defines a set of common, pre-configured layouts for compute pipelines, telling the GPU what kind of resources the shader expects.

| Type | Description |
|---|---|
| `SIT_COMPUTE_LAYOUT_ONE_SSBO` | One SSBO at binding 0 (Set 0). |
| `SIT_COMPUTE_LAYOUT_TWO_SSBOS` | Two SSBOs at bindings 0 and 1 (Set 0). |
| `SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO` | One Storage Image at binding 0, one SSBO at binding 1 (Set 0). |
| `SIT_COMPUTE_LAYOUT_PUSH_CONSTANT` | 64-byte push constant range (no descriptor sets). |
| `SIT_COMPUTE_LAYOUT_EMPTY` | No external resources. |
| `SIT_COMPUTE_LAYOUT_BUFFER_IMAGE` | One SSBO at binding 0, one Storage Image at binding 1 (Set 0). |
| `SIT_COMPUTE_LAYOUT_TERMINAL` | Specialized layout: SSBO (Set 0), Storage Image (Set 1), Font Sampler (Set 2). |

---
#### Resource Handles
`SituationMesh`, `SituationShader`, `SituationTexture`, `SituationBuffer`, `SituationModel`, `SituationComputePipeline`: These are opaque handles to GPU resources. Their internal structure is not exposed to the user. You create them with `SituationCreate...` or `SituationLoad...` functions and free them with their corresponding `SituationDestroy...` or `SituationUnload...` functions.

---
#### `SituationBufferUsageFlags`
Specifies how a `SituationBuffer` will be used. This helps the driver place the buffer in the most optimal memory.
| Flag | Description |
|---|---|
| `SIT_BUFFER_USAGE_VERTEX` | The buffer will be used as a vertex buffer. |
| `SIT_BUFFER_USAGE_INDEX` | The buffer will be used as an index buffer. |
| `SIT_BUFFER_USAGE_UNIFORM` | The buffer will be used as a Uniform Buffer Object (UBO). |
| `SIT_BUFFER_USAGE_STORAGE` | The buffer will be used as a Shader Storage Buffer Object (SSBO). |
| `SIT_BUFFER_USAGE_TRANSFER_SRC`| The buffer can be used as a source for a copy operation. |
| `SIT_BUFFER_USAGE_TRANSFER_DST`| The buffer can be used as a destination for a copy operation. |

---
#### `SituationComputeLayoutType`
Defines the descriptor set layout for a compute pipeline, telling the GPU what kind of resources the shader expects.
| Type | Description |
|---|---|
| `SIT_COMPUTE_LAYOUT_EMPTY`| The compute shader does not use any resources. |
| `SIT_COMPUTE_LAYOUT_IMAGE`| The pipeline expects a single storage image to be bound at binding 0. |
| `SIT_COMPUTE_LAYOUT_BUFFER`| The pipeline expects a single storage buffer to be bound at binding 0. |
| `SIT_COMPUTE_LAYOUT_BUFFER_X2`| The pipeline expects two storage buffers to be bound at bindings 0 and 1. |


#### Functions
### Functions

#### Frame Lifecycle & Command Buffer
These functions control the overall rendering loop.

---
---
#### `SituationAcquireFrameCommandBuffer`
Prepares the backend for a new frame of rendering, acquiring the next available render target from the swap chain. This is the first function to call in the render phase and it must be guarded by a conditional check. It returns `false` if the frame cannot be acquired (e.g., because the window is minimized), in which case you should skip all rendering for that frame.
```c
bool SituationAcquireFrameCommandBuffer(void);
```
**Usage Example:**
```c
// At the start of the rendering phase
if (SituationAcquireFrameCommandBuffer()) {
    // It's safe to record rendering commands now.
    // ...
    SituationEndFrame();
} else {
    // Skip rendering for this frame.
}
```

---
#### `SituationEndFrame`
Submits all recorded commands for the frame to the GPU for execution and presents the final rendered image to the screen. This is the last function to call in the render phase.
```c
SituationError SituationEndFrame(void);
```
**Usage Example:**
```c
// At the very end of the rendering phase
if (SituationAcquireFrameCommandBuffer()) {
    // ... record all rendering commands ...

    // Finally, submit and present the frame.
    SituationEndFrame();
}
```

---
#### `SituationGetMainCommandBuffer`
Gets a handle to the main command buffer. This command buffer is used for all rendering that targets the main window or virtual displays.

```c
SituationCommandBuffer SituationGetMainCommandBuffer(void);
```

**Returns:** Handle to the main command buffer

**Usage Example:**
```c
// Basic rendering
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

SituationRenderPassInfo pass_info = {
    .target_display_id = -1,  // Main window
    .clear_color = {0.1f, 0.1f, 0.1f, 1.0f}
};

SituationCmdBeginRenderPass(cmd, &pass_info);
SituationCmdBindPipeline(cmd, shader);
SituationCmdDrawMesh(cmd, mesh);
SituationCmdEndRenderPass(cmd);

// Multi-pass rendering
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

// Pass 1: Render scene to virtual display
SituationRenderPassInfo vd_pass = {
    .target_display_id = 0,
    .clear_color = {0.0f, 0.0f, 0.0f, 1.0f}
};
SituationCmdBeginRenderPass(cmd, &vd_pass);
RenderScene();
SituationCmdEndRenderPass(cmd);

// Pass 2: Post-process to main window
SituationRenderPassInfo main_pass = {
    .target_display_id = -1,
    .clear_color = {0.0f, 0.0f, 0.0f, 1.0f}
};
SituationCmdBeginRenderPass(cmd, &main_pass);
ApplyBloom();
SituationCmdEndRenderPass(cmd);
```

**Notes:**
- Returns the same buffer each frame
- All rendering commands go through this buffer
- Automatically submitted at end of frame
- Thread-safe for single-threaded rendering

---
#### `SituationCmdBeginRenderPass` / `SituationCmdEndRenderPass`
Begins and ends a render pass. A render pass defines the render target (e.g., the main window or a virtual display) and how its attachments (color, depth) should be handled. All drawing commands must be recorded between these two calls.
```c
SituationError SituationCmdBeginRenderPass(SituationCommandBuffer cmd, const SituationRenderPassInfo* info);
SituationError SituationCmdEndRenderPass(SituationCommandBuffer cmd);
```
**Usage Example:**
```c
// Define a render pass that clears the screen to a dark blue color.
SituationRenderPassInfo pass_info = {
    .color_load_action = SIT_LOAD_ACTION_CLEAR,
    .clear_color = {20, 30, 40, 255},
    .color_store_action = SIT_STORE_ACTION_STORE,
    .virtual_display_id = -1 // Target the main window
};
SituationCmdBeginRenderPass(SituationGetMainCommandBuffer(), &pass_info);

// ... All your drawing commands for this pass go here ...

SituationCmdEndRenderPass(SituationGetMainCommandBuffer());
```

---
#### Rendering Commands
These functions record drawing and state-setting operations into the command buffer.

---

#### `SituationPauseApp`

Pauses the application.

```c
SITAPI void SituationPauseApp(void);
```

**Usage Example:**
```c
SituationPauseApp();
```

---

#### `SituationGetMonitorHeight`

Gets the current height of a monitor in screen coordinates.

```c
SITAPI int SituationGetMonitorHeight(int monitor_id);
```

**Usage Example:**
```c
int primary_monitor_height = SituationGetMonitorHeight(0);
printf("Primary monitor height: %d\n", primary_monitor_height);
```

---
#### `SituationCmdSetViewport` / `SituationCmdSetScissor`
Sets the dynamic viewport or scissor rectangle for the current render pass. The viewport transforms the normalized device coordinates to window coordinates, while the scissor rectangle discards fragments outside its bounds.
```c
SituationError SituationCmdSetViewport(SituationCommandBuffer cmd, float x, float y, float width, float height);
SituationError SituationCmdSetScissor(SituationCommandBuffer cmd, int x, int y, int width, int height);
```
**Usage Example:**
```c
// Render to the left half of the screen for a split-screen effect.
int w = SituationGetRenderWidth();
int h = SituationGetRenderHeight();
SituationCmdSetViewport(SituationGetMainCommandBuffer(), 0, 0, w / 2.0f, h);
SituationCmdSetScissor(SituationGetMainCommandBuffer(), 0, 0, w / 2, h);
```

---
#### `SituationCmdBindPipeline`
Binds a graphics pipeline (which includes the shader program and its state) for subsequent drawing commands. All draws following this call will use this pipeline until a new one is bound.
```c
SituationError SituationCmdBindPipeline(SituationCommandBuffer cmd, SituationShader shader);
```
**Usage Example:**
```c
// Bind the main shader for drawing the 3D scene.
SituationCmdBindPipeline(SituationGetMainCommandBuffer(), my_3d_shader);
SituationCmdDrawMesh(SituationGetMainCommandBuffer(), my_scene_mesh);

// Bind a different shader for drawing the UI.
SituationCmdBindPipeline(SituationGetMainCommandBuffer(), my_ui_shader);
SituationCmdDrawMesh(SituationGetMainCommandBuffer(), my_ui_mesh);
```

---
#### `SituationCmdBindVertexBuffer`
Binds a vertex buffer for subsequent drawing commands. The bound vertex buffer provides the vertex data (position, color, normals, etc.) for the draws that follow.
```c
void SituationCmdBindVertexBuffer(SituationCommandBuffer cmd, SituationBuffer buffer);
```
**Usage Example:**
```c
// Before drawing, bind the vertex buffer containing your model's vertices.
SituationCmdBindVertexBuffer(cmd, my_model_vertex_buffer);
SituationCmdDraw(cmd, 0, 36); // Draw 36 vertices from the bound buffer.
```

---
#### `SituationCmdBindIndexBuffer`
Binds an index buffer for subsequent indexed drawing commands (`SituationCmdDrawIndexed`). An index buffer tells the GPU the order in which to draw vertices from the vertex buffer, allowing for the reuse of vertices and more efficient rendering of complex meshes.
```c
void SituationCmdBindIndexBuffer(SituationCommandBuffer cmd, SituationBuffer buffer);
```
**Usage Example:**
```c
// Before an indexed draw, bind both the vertex and index buffers.
SituationCmdBindVertexBuffer(cmd, my_mesh_vbo);
SituationCmdBindIndexBuffer(cmd, my_mesh_ibo);
// Draw using the index buffer. This will draw 12 triangles (36 indices).
SituationCmdDrawIndexed(cmd, 0, 36, 0);
```

---
#### `SituationCmdBindShaderBuffer` / `SituationCmdBindShaderTexture`
Binds a uniform/storage buffer or a texture to a specific binding point, making it accessible to the currently bound shader. The `binding` index corresponds to the `binding = N` layout qualifier in the GLSL shader code.
```c
void SituationCmdBindShaderBuffer(SituationCommandBuffer cmd, int binding, SituationBuffer buffer);
void SituationCmdBindShaderTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture);
```
**Usage Example:**
```c
/* GLSL Shader Code:
layout(binding = 0) uniform sampler2D u_albedo;
layout(binding = 1) uniform SceneData {
    mat4 view;
    mat4 proj;
} u_scene;
*/

// In C, bind the corresponding resources to the correct binding points:
SituationCmdBindShaderTexture(cmd, 0, my_albedo_texture);
SituationCmdBindShaderBuffer(cmd, 1, my_scene_ubo);
```

---
#### `SituationCmdDraw` / `SituationCmdDrawIndexed`
Records a non-indexed or indexed drawing command into the command buffer. `SituationCmdDraw` draws vertices sequentially from the bound vertex buffer, while `SituationCmdDrawIndexed` uses the bound index buffer to determine the order of vertices.
```c
SituationError SituationCmdDraw(SituationCommandBuffer cmd, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
SituationError SituationCmdDrawIndexed(SituationCommandBuffer cmd, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance);
```
**Usage Example:**
```c
// Draw a mesh using previously bound vertex and index buffers.
SituationCmdBindVertexBuffer(cmd, my_vbo);
SituationCmdBindIndexBuffer(cmd, my_ibo);
// Draw 36 indices, starting from the beginning of the index buffer.
SituationCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
```

---
#### `SituationCmdDrawMesh`
A high-level drawing command that records a command to draw a complete, pre-configured `SituationMesh` object. This is often more convenient than binding vertex and index buffers manually.
```c
SituationError SituationCmdDrawMesh(SituationCommandBuffer cmd, SituationMesh mesh);
```
**Usage Example:**
```c
// Bind the shader you want to use for this mesh.
SituationCmdBindPipeline(SituationGetMainCommandBuffer(), my_shader);
// The mesh object contains its own vertex and index buffers, which are automatically used.
SituationCmdDrawMesh(SituationGetMainCommandBuffer(), my_complex_model_mesh);
```

---
#### `SituationCmdDrawQuad`
Records a command to draw a simple, colored, and transformed 2D quad. This uses an internally managed quad mesh, so you don't need to create your own. It's useful for debug rendering, particles, or simple UI elements.
```c
SituationError SituationCmdDrawQuad(SituationCommandBuffer cmd, mat4 model, Vector4 color);
```
**Usage Example:**
```c
// Create a transformation matrix to position and scale the quad.
mat4 transform;
glm_translate_make(transform, (vec3){100.0f, 200.0f, 0.0f});
glm_scale_uni(transform, 50.0f); // Make it 50x50 pixels

// Define a color (in this case, magenta).
Vector4 quad_color = {{1.0f, 0.0f, 1.0f, 1.0f}};

// Record the draw command.
SituationCmdDrawQuad(SituationGetMainCommandBuffer(), transform, quad_color);
```

---
#### `SituationCmdDrawTexture`
Draws a portion of a texture with transformation, rotation, and tint. This is a high-level convenience function for sprite rendering.
```c
SituationError SituationCmdDrawTexture(
    SituationCommandBuffer cmd,
    SituationTexture texture,
    Rectangle source,
    Rectangle dest,
    Vector2 origin,
    float rotation,
    ColorRGBA tint
);
```
**Parameters:**
- `source`: Source rectangle in the texture (in pixels)
- `dest`: Destination rectangle on screen (in pixels)
- `origin`: Rotation origin point (relative to dest)
- `rotation`: Rotation angle in radians
- `tint`: Color tint to apply (use WHITE for no tint)

**Usage Example:**
```c
// Draw a sprite from a texture atlas
Rectangle source = {0, 0, 32, 32};  // 32x32 sprite in atlas
Rectangle dest = {100, 100, 64, 64}; // Draw at 2x scale
Vector2 origin = {32, 32};           // Rotate around center
float rotation = glm_rad(45.0f);     // 45 degree rotation
ColorRGBA tint = {255, 255, 255, 255}; // No tint

SituationCmdDrawTexture(cmd, sprite_atlas, source, dest, origin, rotation, tint);
```

---
#### `SituationCmdDrawText`
Draws a text string using GPU-accelerated textured quads. Uses the default font size.
```c
SituationError SituationCmdDrawText(
    SituationCommandBuffer cmd,
    SituationFont font,
    const char* text,
    Vector2 pos,
    ColorRGBA color
);
```
**Usage Example:**
```c
// Draw simple text
Vector2 pos = {10, 10};
ColorRGBA white = {255, 255, 255, 255};
SituationCmdDrawText(cmd, my_font, "Hello, World!", pos, white);
```

---
#### `SituationCmdDrawTextEx`
Advanced text drawing with custom font size and character spacing.
```c
SituationError SituationCmdDrawTextEx(
    SituationCommandBuffer cmd,
    SituationFont font,
    const char* text,
    Vector2 pos,
    float fontSize,
    float spacing,
    ColorRGBA color
);
```
**Usage Example:**
```c
// Draw large, spaced-out text
Vector2 pos = {100, 100};
ColorRGBA yellow = {255, 255, 0, 255};
SituationCmdDrawTextEx(cmd, my_font, "GAME OVER", pos, 48.0f, 2.0f, yellow);
```

---
#### `SituationCmdPresent`
Submits a command to copy a texture to the main window's swapchain. This is primarily used in compute-only rendering pipelines where you render to a texture and then present it.
```c
SituationError SituationCmdPresent(SituationCommandBuffer cmd, SituationTexture texture);
```
**Usage Example:**
```c
// Render to texture, then present
SituationCmdBeginRenderToDisplay(cmd, offscreen_texture);
// ... render scene ...
SituationCmdEndRender(cmd);

// Present the offscreen texture to the window
SituationCmdPresent(cmd, offscreen_texture);
```

---
#### `SituationCmdBindSampledTexture`
Binds a texture as a sampled image (sampler2D) to a specific binding point in shaders.
```c
SituationError SituationCmdBindSampledTexture(
    SituationCommandBuffer cmd,
    int binding,
    SituationTexture texture
);
```
**Usage Example:**
```c
// Bind texture to binding point 0 in fragment shader
SituationCmdBindSampledTexture(cmd, 0, diffuse_texture);
```

---
#### `SituationCmdBindDescriptorSetDynamic`
Binds a dynamic buffer descriptor set with an offset. This allows you to use a single large buffer for multiple objects by offsetting into it.
```c
SituationError SituationCmdBindDescriptorSetDynamic(
    SituationCommandBuffer cmd,
    uint32_t set_index,
    SituationBuffer buffer,
    uint32_t dynamic_offset
);
```
**Usage Example:**
```c
// Bind different parts of a large uniform buffer for each object
for (int i = 0; i < object_count; i++) {
    uint32_t offset = i * sizeof(ObjectUniforms);
    SituationCmdBindDescriptorSetDynamic(cmd, 0, uniform_buffer, offset);
    SituationCmdDrawMesh(cmd, objects[i].mesh);
}
```

---
#### Resource Management
These functions create and destroy GPU resources.

---

#### `SituationShowCursor`

Shows the cursor.

```c
SITAPI void SituationShowCursor(void);
```

**Usage Example:**
```c
SituationShowCursor();
```

---
#### `SituationCreateMesh`
Creates a self-contained GPU mesh from vertex and index data. This operation uploads the provided data to video memory.
```c
SituationError SituationCreateMesh(const void* vertex_data, int vertex_count, size_t vertex_stride, const uint32_t* index_data, int index_count, SituationMesh* out_mesh);
```
**Usage Example:**
```c
// Define vertex and index data for a quad.
MyVertex vertices[] = { ... };
uint32_t indices[] = { ... };

// Create the mesh resource.
SituationMesh quad_mesh;
if (SituationCreateMesh(vertices, 4, sizeof(MyVertex), indices, 6, &quad_mesh) == SITUATION_SUCCESS) {
    // ...
}
```

---
#### `SituationDestroyMesh`
Destroys a mesh and frees its associated GPU memory. The handle is invalidated after this call.
```c
void SituationDestroyMesh(SituationMesh* mesh);
```
**Usage Example:**
```c
// Assume quad_mesh is a valid handle created with SituationCreateMesh.
// At application shutdown or when the mesh is no longer needed:
SituationDestroyMesh(&quad_mesh);
// The quad_mesh handle is now invalid.
```

---
#### `SituationGetMeshData`
Retrieves the vertex and index data from a mesh for CPU-side processing.
```c
void SituationGetMeshData(
    SituationMesh mesh,
    float** out_vertices,
    uint32_t** out_indices,
    uint32_t* out_vertex_count,
    uint32_t* out_index_count
);
```
**Usage Example:**
```c
// Extract mesh data for physics collision
float* vertices;
uint32_t* indices;
uint32_t vertex_count, index_count;

SituationGetMeshData(mesh, &vertices, &indices, &vertex_count, &index_count);

// Use data for collision detection
BuildCollisionMesh(vertices, vertex_count, indices, index_count);
```

---
#### `SituationLoadShader`
Loads, compiles, and links a graphics shader pipeline from GLSL vertex and fragment shader files.
```c
SituationError SituationLoadShader(const char* vs_path, const char* fs_path, SituationShader* out_shader);
```
**Usage Example:**
```c
// At application startup, load the main shader.
SituationShader main_shader;
SituationLoadShader("shaders/main.vert", "shaders/main.frag", &main_shader);
```

---
#### `SituationUnloadShader`
Unloads a shader pipeline and frees its associated GPU resources.
```c
void SituationUnloadShader(SituationShader* shader);
```
**Usage Example:**
```c
// At application shutdown, unload the main shader.
SituationUnloadShader(&main_shader);
```

---
#### `SituationCreateTexture`
Creates a GPU texture from a CPU-side `SituationImage`. This involves uploading the pixel data from RAM to VRAM.
```c
SituationError SituationCreateTexture(SituationImage image, bool generate_mipmaps, SituationTexture* out_texture);
```
**Usage Example:**
```c
// Load a CPU image from a file.
SituationImage cpu_image;
if (SituationLoadImage("textures/player_character.png", &cpu_image) == SITUATION_SUCCESS) {
    // Create a GPU texture from the image, generating mipmaps for better quality.
    SituationTexture player_texture;
    if (SituationCreateTexture(cpu_image, true, &player_texture) == SITUATION_SUCCESS) {
        // The CPU-side image can now be unloaded as the data is on the GPU.
        SituationUnloadImage(cpu_image);
    }
}
```

---
#### `SituationLoadTexture`
Loads a texture from an image file.
```c
SituationTexture SituationLoadTexture(const char* filename);
```
**Usage Example:**
```c
SituationTexture diffuse = SituationLoadTexture("textures/diffuse.png");
```

---
#### `SituationCreateTextureEx`
Creates a texture with advanced options (mipmaps, filtering, wrapping).
```c
SituationTexture SituationCreateTextureEx(
    SituationImage* image,
    SituationTextureFilter filter,
    SituationTextureWrap wrap,
    bool generate_mipmaps
);
```
**Usage Example:**
```c
// Create texture with trilinear filtering and mipmaps
SituationTexture tex = SituationCreateTextureEx(
    &image,
    SIT_FILTER_TRILINEAR,
    SIT_WRAP_REPEAT,
    true  // Generate mipmaps
);
```

---
#### `SituationDestroyTexture`
Destroys a texture and frees its associated GPU memory. The handle is invalidated after this call.
```c
void SituationDestroyTexture(SituationTexture* texture);
```
**Usage Example:**
```c
// Assume player_texture is a valid handle.
// At application shutdown or when the texture is no longer needed:
SituationDestroyTexture(&player_texture);
// The player_texture handle is now invalid.
```

---
#### `SituationUpdateTexture`
Updates a texture with new pixel data from a `SituationImage`.
```c
void SituationUpdateTexture(SituationTexture texture, SituationImage image);
```
**Usage Example:**
```c
// Create a blank texture
SituationImage blank = SituationGenImageColor(256, 256, (ColorRGBA){0,0,0,255});
SituationTexture dynamic_texture;
SituationCreateTexture(blank, false, &dynamic_texture);
SituationUnloadImage(blank);

// Later, in the update loop, generate new image data
SituationImage new_data = generate_procedural_image();
SituationUpdateTexture(dynamic_texture, new_data);
SituationUnloadImage(new_data);
```

**Pro Tip (Zero-Copy Update):**
If you already have a raw data buffer (e.g., from a procedural generation function) and want to avoid allocating a new `SituationImage` on the heap, you can wrap your raw pointer in a stack-allocated `SituationImage`.
```c
// 'my_raw_pixels' is a pointer to your RGBA data.
SituationImage wrapper = {
    .width = 256,
    .height = 256,
    .data = my_raw_pixels,
    // .format defaults to 0 (RGBA), .mipmaps to 0/1
};
SituationUpdateTexture(dynamic_texture, wrapper);
// No need to call SituationUnloadImage(wrapper) since it owns nothing.
```

---
#### `SituationGetTextureHandle`
Retrieves the bindless texture handle for a texture. This 64-bit handle can be passed to shaders to enable bindless texture access, allowing thousands of textures to be used without explicit binding.

```c
uint64_t SituationGetTextureHandle(SituationTexture texture);
```

**Parameters:**
- `texture` - The texture to get the handle for

**Returns:** 64-bit bindless texture handle, or 0 if not supported

**Usage Example:**
```c
/* GLSL Shader with bindless textures:
#version 450
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require

layout(push_constant) uniform PushConstants {
    uint64_t texture_handle;
    mat4 mvp;
};

void main() {
    sampler2D tex = sampler2D(texture_handle);
    vec4 color = texture(tex, v_texcoord);
    frag_color = color;
}
*/

// Get bindless handles for multiple textures
uint64_t texture_handles[MAX_MATERIALS];
for (int i = 0; i < material_count; i++) {
    texture_handles[i] = SituationGetTextureHandle(materials[i].albedo_texture);
}

// Pass handle to shader via push constant
struct PushConstants {
    uint64_t texture_handle;
    mat4 mvp;
} pc = {
    .texture_handle = texture_handles[material_id],
    .mvp = mvp_matrix
};

SituationCmdSetPushConstant(cmd, 0, &pc, sizeof(pc));
SituationCmdDrawMesh(cmd, mesh);
```

**Notes:**
- **OpenGL:** Requires `GL_ARB_bindless_texture` extension
- **Vulkan:** Not yet implemented (returns 0)
- Enables rendering with thousands of unique textures without binding overhead
- Texture must remain valid while handle is in use
- Handles are resident (always accessible) once obtained
- Part of modern GPU-driven rendering techniques

---
#### `SituationCmdCopyTexture`
Records a command to copy pixel data from a source texture to a destination texture. This is a GPU-side operation that efficiently transfers texture data without CPU involvement.

```c
SituationError SituationCmdCopyTexture(SituationCommandBuffer cmd, SituationTexture src, SituationTexture dst);
```

**Parameters:**
- `cmd` - The command buffer to record into
- `src` - The source texture to copy from
- `dst` - The destination texture to copy to

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create a render target and a backup texture
SituationTexture render_target = SituationCreateRenderTexture(1920, 1080);
SituationTexture backup = SituationCreateTexture(1920, 1080, SITUATION_PIXEL_FORMAT_RGBA8);

// Render to the target
SituationRenderPassInfo pass = {
    .virtual_display_id = -1,
    .color_attachments[0] = render_target,
    .color_load_action = SIT_LOAD_ACTION_CLEAR
};
SituationCmdBeginRenderPass(cmd, &pass);
// ... rendering commands ...
SituationCmdEndRenderPass(cmd);

// Copy the render result to backup for post-processing
SituationCmdCopyTexture(cmd, render_target, backup);

// Apply post-processing effects to the backup
SituationCmdBindPipeline(cmd, blur_shader);
SituationCmdBindShaderTexture(cmd, 0, backup);
SituationCmdDrawQuad(cmd);
```

**Notes:**
- Source and destination textures must have compatible formats and dimensions
- Useful for feedback loops, copying render targets for post-processing, or backing up texture state
- This is a GPU-side operation - much faster than reading to CPU and uploading
- May require a pipeline barrier if the source texture was just written to
- On Vulkan, maps to `vkCmdCopyImage`; on OpenGL, uses `glCopyImageSubData`

---
#### `SituationGetTextureFormat`
Gets the internal GPU format of a texture.
```c
int SituationGetTextureFormat(SituationTexture texture);
```
**Usage Example:**
```c
int format = SituationGetTextureFormat(my_texture);
// The format will be one of the backend-specific pixel format enums (e.g., GL_RGBA8)
printf("Texture format ID: %d\n", format);
```

---
#### `SituationLoadModel`
Loads a 3D model from a file (GLTF, OBJ). This function parses the model file and uploads all associated meshes and materials to the GPU.
```c
SituationError SituationLoadModel(const char* file_path, SituationModel* out_model);
```
**Usage Example:**
```c
// At application startup, load the player model.
SituationModel player_model;
SituationLoadModel("models/player.gltf", &player_model);
```

---
#### `SituationUnloadModel`
Unloads a model and all of its associated resources (meshes, materials) from GPU memory.
```c
void SituationUnloadModel(SituationModel* model);
```
**Usage Example:**
```c
// At application shutdown, unload the player model.
SituationUnloadModel(&player_model);
```

---
#### `SituationCreateBuffer`
Creates a generic GPU buffer and optionally initializes it with data. Buffers can be used for vertices, indices, uniforms (UBOs), or storage (SSBOs).
```c
SituationError SituationCreateBuffer(size_t size, const void* initial_data, SituationBufferUsageFlags usage_flags, SituationBuffer* out_buffer);
```
**Usage Example:**
```c
// Create a uniform buffer for camera matrices
mat4 proj, view;
// ... calculate projection and view matrices ...
CameraMatrices ubo_data = { .projection = proj, .view = view };
SituationBuffer camera_ubo;
if (SituationCreateBuffer(sizeof(ubo_data), &ubo_data, SITUATION_BUFFER_USAGE_UNIFORM_BUFFER, &camera_ubo) == SITUATION_SUCCESS) {
    // ... use the buffer ...
}
```

---
#### `SituationDestroyBuffer`
Destroys a GPU buffer and frees its associated video memory. The handle is invalidated after this call.
```c
void SituationDestroyBuffer(SituationBuffer* buffer);
```
**Usage Example:**
```c
// Assume camera_ubo is a valid SituationBuffer handle created earlier.
// At application shutdown or when the buffer is no longer needed:
SituationDestroyBuffer(&camera_ubo);
// The camera_ubo handle is now invalid and should not be used.
```

---
#### `SituationGetBufferDeviceAddress`
Retrieves the GPU device address of a buffer as a 64-bit pointer. This address can be passed to shaders via push constants to enable bindless buffer access using the `buffer_reference` extension.

```c
uint64_t SituationGetBufferDeviceAddress(SituationBuffer buffer);
```

**Parameters:**
- `buffer` - The buffer to get the address of

**Returns:** 64-bit GPU device address, or 0 if not supported

**Usage Example:**
```c
/* GLSL Shader with buffer_reference:
#version 450
#extension GL_EXT_buffer_reference : require

layout(buffer_reference, std430) buffer VertexData {
    vec3 position;
    vec2 texcoord;
};

layout(push_constant) uniform PushConstants {
    uint64_t vertex_buffer_address;
    mat4 mvp;
};

void main() {
    VertexData vertex = VertexData(vertex_buffer_address);
    gl_Position = mvp * vec4(vertex.position, 1.0);
}
*/

// Create buffer and get its GPU address
SituationBuffer vertex_buffer = SituationCreateBuffer(
    vertex_count * sizeof(Vertex),
    SITUATION_BUFFER_USAGE_STORAGE);

uint64_t buffer_address = SituationGetBufferDeviceAddress(vertex_buffer);

// Pass address to shader via push constant
struct PushConstants {
    uint64_t vertex_buffer_address;
    mat4 mvp;
} pc = {
    .vertex_buffer_address = buffer_address,
    .mvp = mvp_matrix
};

SituationCmdSetPushConstant(cmd, 0, &pc, sizeof(pc));
SituationCmdDraw(cmd, vertex_count, 1, 0, 0);
```

**Notes:**
- Requires Vulkan backend with `VK_KHR_buffer_device_address` extension
- Returns 0 on OpenGL or if extension is not available
- Enables bindless rendering patterns for high-performance applications
- Buffer must remain valid while the address is in use
- Part of modern GPU-driven rendering techniques

---
#### `SituationUpdateBuffer`
Updates the contents of a GPU buffer with new data from the CPU. This is the primary way to send dynamic data (uniforms, vertex data, etc.) to the GPU each frame.

```c
SituationError SituationUpdateBuffer(SituationBuffer buffer, const void* data, size_t size);
```

**Parameters:**
- `buffer` - The buffer to update
- `data` - Pointer to the source data on CPU
- `size` - Number of bytes to copy

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Update camera uniform buffer each frame
typedef struct {
    mat4 view;
    mat4 projection;
    vec3 camera_pos;
} CameraUniforms;

CameraUniforms camera_data = {
    .view = view_matrix,
    .projection = proj_matrix,
    .camera_pos = camera_position
};

SituationUpdateBuffer(camera_ubo, &camera_data, sizeof(CameraUniforms));

// Update dynamic vertex buffer
Vertex vertices[MAX_PARTICLES];
GenerateParticleVertices(vertices, particle_count);
SituationUpdateBuffer(particle_vbo, vertices, particle_count * sizeof(Vertex));
```

**Important Rules:**
```c
// ✅ CORRECT: Update BEFORE recording draw commands
SituationUpdateBuffer(uniform_buffer, &data, sizeof(data));
SituationCmdBindDescriptorSet(cmd, 0, uniform_buffer);
SituationCmdDrawMesh(cmd, mesh);

// ❌ WRONG: Update AFTER recording draw (causes race condition)
SituationCmdDrawMesh(cmd, mesh);
SituationUpdateBuffer(uniform_buffer, &data, sizeof(data));  // Too late!
```

**Notes:**
- Must be called BEFORE recording draw commands that use the buffer
- On Vulkan, uses a ring buffer for zero-copy updates
- On OpenGL, uses persistent mapped buffers
- Size must not exceed the buffer's original size
- For dynamic buffers, use `SITUATION_BUFFER_USAGE_DYNAMIC_UNIFORM` flag

---
#### Compute Shaders

---
#### `SituationCreateComputePipeline`
Creates a compute pipeline from a GLSL compute shader file. Compute pipelines are used for GPU-accelerated parallel processing tasks like particle systems, physics simulations, and image processing.

```c
SituationError SituationCreateComputePipeline(const char* compute_shader_path, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline);
```

**Parameters:**
- `compute_shader_path` - Path to the GLSL compute shader file (.comp or .glsl)
- `layout_type` - Descriptor set layout type (determines binding model)
- `out_pipeline` - Pointer to receive the created pipeline handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create a compute pipeline for particle simulation
SituationComputePipeline particle_pipeline;
if (SituationCreateComputePipeline(
    "shaders/particles.comp",
    SITUATION_COMPUTE_LAYOUT_STANDARD,
    &particle_pipeline) != SITUATION_SUCCESS) {
    printf("Failed to create compute pipeline\n");
    return;
}

// Use the pipeline
SituationCmdBindComputePipeline(cmd, particle_pipeline);
SituationCmdBindComputeBuffer(cmd, 0, particle_buffer);
SituationCmdDispatch(cmd, particle_count / 256, 1, 1);

// Clean up when done
SituationDestroyComputePipeline(&particle_pipeline);
```

**Notes:**
- Shader must have a `#version 450` directive and `layout(local_size_x = N) in;`
- Layout type determines how resources are bound (standard vs bindless)
- Pipeline compilation happens at creation time
- Check for errors - shader compilation can fail

---
#### `SituationDestroyComputePipeline`
Destroys a compute pipeline and frees all associated GPU resources. The pipeline handle's ID is set to 0 after destruction.

```c
void SituationDestroyComputePipeline(SituationComputePipeline* pipeline);
```

**Parameters:**
- `pipeline` - Pointer to the compute pipeline to destroy (ID will be set to 0)

**Usage Example:**
```c
// Create pipeline
SituationComputePipeline blur_pipeline;
SituationCreateComputePipeline("shaders/blur.comp", 
    SITUATION_COMPUTE_LAYOUT_STANDARD, &blur_pipeline);

// Use pipeline...

// Clean up
SituationDestroyComputePipeline(&blur_pipeline);
// blur_pipeline.id is now 0
```

**Notes:**
- Safe to call multiple times (checks for valid ID)
- Waits for GPU to finish using the pipeline before destroying
- Always destroy pipelines before shutting down
- The pipeline pointer's ID field is set to 0 to prevent use-after-free

---
#### `SituationCmdBindComputePipeline`
Binds a compute pipeline to the command buffer for subsequent dispatch commands. All compute dispatches following this call will use this pipeline until a new one is bound.

```c
void SituationCmdBindComputePipeline(SituationCommandBuffer cmd, SituationComputePipeline pipeline);
```

**Parameters:**
- `cmd` - The command buffer to record into
- `pipeline` - The compute pipeline to bind

**Usage Example:**
```c
// Particle simulation with multiple compute passes
SituationComputePipeline update_positions, update_velocities, apply_forces;
// ... create pipelines ...

// Pass 1: Apply forces
SituationCmdBindComputePipeline(cmd, apply_forces);
SituationCmdBindComputeBuffer(cmd, 0, particle_buffer);
SituationCmdDispatch(cmd, particle_groups, 1, 1);

// Barrier: Ensure forces are applied before updating velocities
SituationCmdPipelineBarrier(cmd,
    SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
    SITUATION_BARRIER_COMPUTE_SHADER_READ);

// Pass 2: Update velocities
SituationCmdBindComputePipeline(cmd, update_velocities);
SituationCmdDispatch(cmd, particle_groups, 1, 1);

// Barrier again
SituationCmdPipelineBarrier(cmd,
    SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
    SITUATION_BARRIER_COMPUTE_SHADER_READ);

// Pass 3: Update positions
SituationCmdBindComputePipeline(cmd, update_positions);
SituationCmdDispatch(cmd, particle_groups, 1, 1);
```

**Notes:**
- Must be called before `SituationCmdDispatch()`
- Binding a new pipeline is a lightweight operation
- Use barriers between dependent compute passes
- Pipeline remains bound until explicitly changed

---
#### `SituationCmdBindComputeBuffer`
Binds a storage buffer to a specific binding point for use in a compute shader. The buffer can be read from and written to by the shader.

```c
void SituationCmdBindComputeBuffer(SituationCommandBuffer cmd, int binding, SituationBuffer buffer);
```

**Parameters:**
- `cmd` - The command buffer to record into
- `binding` - The binding point index (corresponds to `binding = N` in GLSL)
- `buffer` - The buffer to bind (must be created with `SITUATION_BUFFER_USAGE_STORAGE`)

**Usage Example:**
```c
/* GLSL Compute Shader:
#version 450
layout(local_size_x = 256) in;

layout(binding = 0) buffer ParticleData {
    vec4 positions[];
};

layout(binding = 1) buffer VelocityData {
    vec4 velocities[];
};

void main() {
    uint id = gl_GlobalInvocationID.x;
    positions[id] += velocities[id] * dt;
}
*/

// Create storage buffers
SituationBuffer position_buffer = SituationCreateBuffer(
    particle_count * sizeof(vec4),
    SITUATION_BUFFER_USAGE_STORAGE);
    
SituationBuffer velocity_buffer = SituationCreateBuffer(
    particle_count * sizeof(vec4),
    SITUATION_BUFFER_USAGE_STORAGE);

// Bind buffers to compute shader
SituationCmdBindComputePipeline(cmd, particle_pipeline);
SituationCmdBindComputeBuffer(cmd, 0, position_buffer);
SituationCmdBindComputeBuffer(cmd, 1, velocity_buffer);
SituationCmdDispatch(cmd, particle_count / 256, 1, 1);
```

**Notes:**
- Buffer must be created with `SITUATION_BUFFER_USAGE_STORAGE` flag
- Binding index must match the `binding = N` in the shader
- Buffers remain bound until explicitly changed
- Use barriers if the buffer is read after being written

---
#### `SituationCmdBindComputeTexture`
Binds a texture as a storage image to a specific binding point for use in a compute shader. The texture can be read from and written to by the shader.

```c
void SituationCmdBindComputeTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture);
```

**Parameters:**
- `cmd` - The command buffer to record into
- `binding` - The binding point index (corresponds to `binding = N` in GLSL)
- `texture` - The texture to bind (must support storage image usage)

**Usage Example:**
```c
/* GLSL Compute Shader:
#version 450
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, rgba8) uniform image2D input_image;
layout(binding = 1, rgba8) uniform image2D output_image;

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    vec4 color = imageLoad(input_image, pixel);
    
    // Apply blur or other effect
    vec4 processed = ApplyEffect(color);
    
    imageStore(output_image, pixel, processed);
}
*/

// Create textures with storage usage
SituationTexture input_tex = SituationCreateTexture(1920, 1080, 
    SITUATION_PIXEL_FORMAT_RGBA8);
SituationTexture output_tex = SituationCreateTexture(1920, 1080,
    SITUATION_PIXEL_FORMAT_RGBA8);

// Bind textures to compute shader
SituationCmdBindComputePipeline(cmd, image_process_pipeline);
SituationCmdBindComputeTexture(cmd, 0, input_tex);
SituationCmdBindComputeTexture(cmd, 1, output_tex);

// Dispatch with work groups covering the image
SituationCmdDispatch(cmd, 1920 / 16, 1080 / 16, 1);
```

**Notes:**
- Texture must support storage image usage
- Binding index must match the `binding = N` in the shader
- Format in shader (`rgba8`) must match texture format
- Use barriers if texture is read after being written
- Useful for image processing, procedural generation, and post-processing

---
#### `SituationCmdDispatch`
Records a command to execute a compute shader.
```c
void SituationCmdDispatch(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);
```
**Usage Example:**
```c
// In render loop, before the main render pass
SituationCmdBindComputePipeline(SituationGetMainCommandBuffer(), my_compute_pipeline);
SituationCmdBindComputeTexture(SituationGetMainCommandBuffer(), 0, my_storage_image);
// Dispatch a 16x16 grid of thread groups.
SituationCmdDispatch(SituationGetMainCommandBuffer(), 16, 16, 1);
// A pipeline barrier is needed here to sync with the graphics pass
```

---
#### `SituationGetComputeCommandBuffer`
Gets a command buffer specifically for compute shader operations.
```c
SituationCommandBuffer SituationGetComputeCommandBuffer(void);
```
**Usage Example:**
```c
// Dispatch compute shader
SituationCommandBuffer compute_cmd = SituationGetComputeCommandBuffer();
SituationCmdBindComputePipeline(compute_cmd, compute_pipeline);
SituationCmdDispatch(compute_cmd, work_groups_x, work_groups_y, work_groups_z);
```

---
#### `SituationCmdPipelineBarrier`
Inserts a memory barrier into the command buffer to synchronize memory access between different pipeline stages. This ensures that writes from one stage (e.g., compute shader) are visible to reads in another stage (e.g., vertex shader), preventing race conditions and data hazards.

```c
void SituationCmdPipelineBarrier(SituationCommandBuffer cmd, uint32_t src_flags, uint32_t dst_flags);
```

**Parameters:**
- `cmd` - The command buffer to record into
- `src_flags` - Source stage flags (what wrote the data) - bitmask of `SituationBarrierSrcFlags`
- `dst_flags` - Destination stage flags (what will read the data) - bitmask of `SituationBarrierDstFlags`

**Source Flags:**
- `SITUATION_BARRIER_VERTEX_SHADER_WRITE` - Vertex shader wrote to SSBO/Image
- `SITUATION_BARRIER_FRAGMENT_SHADER_WRITE` - Fragment shader wrote to SSBO/Image/Attachment
- `SITUATION_BARRIER_COMPUTE_SHADER_WRITE` - Compute shader wrote to Storage Buffer/Image
- `SITUATION_BARRIER_TRANSFER_WRITE` - Transfer operation (Copy/Blit/Fill) wrote data

**Destination Flags:**
- `SITUATION_BARRIER_VERTEX_SHADER_READ` - Vertex shader will read data
- `SITUATION_BARRIER_FRAGMENT_SHADER_READ` - Fragment shader will read data
- `SITUATION_BARRIER_COMPUTE_SHADER_READ` - Compute shader will read data
- `SITUATION_BARRIER_TRANSFER_READ` - Transfer operation will read data
- `SITUATION_BARRIER_INDIRECT_COMMAND_READ` - Data will be read as indirect command parameters

**Usage Example:**
```c
// GPU particle simulation: compute updates positions, then render them
SituationCmdBindComputePipeline(cmd, particle_update_pipeline);
SituationCmdBindComputeBuffer(cmd, 0, particle_ssbo);
SituationCmdDispatch(cmd, PARTICLE_GROUPS, 1, 1);

// CRITICAL: Ensure compute writes finish before vertex shader reads
SituationCmdPipelineBarrier(cmd,
    SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
    SITUATION_BARRIER_VERTEX_SHADER_READ);

// Now safe to render using the updated particle data
SituationCmdBindPipeline(cmd, particle_render_pipeline);
SituationCmdBindVertexBuffer(cmd, particle_ssbo);
SituationCmdDraw(cmd, PARTICLE_COUNT, 1, 0, 0);
```

**Notes:**
- Essential for compute-to-graphics workflows (particle systems, procedural generation, etc.)
- On Vulkan, maps to precise `vkCmdPipelineBarrier` with optimal stage/access masks
- On OpenGL, emulated using `glMemoryBarrier` (has performance cost)
- Automatically handles queue family ownership transfers on multi-queue systems
- Replaces the deprecated `SituationMemoryBarrier()` function

---
#### Virtual Displays

---
#### `SituationVirtualDisplay`
Represents a complete off-screen rendering target, often called a framebuffer object (FBO). It encapsulates not only the GPU resources (like color and depth textures) but also the state required to manage and composite it, such as its resolution, visibility, and blend mode. This is the core struct for implementing post-processing effects, rendering UI at a fixed resolution, or caching complex scenes.
```c
typedef struct SituationVirtualDisplay {
    // Core Properties
    int id;
    bool visible;
    bool is_dirty;
    vec2 resolution;
    vec2 offset;
    float opacity;
    int z_order;

    // Behavior
    double frame_time_multiplier;
    SituationScalingMode scaling_mode;
    SituationBlendMode blend_mode;

    // Backend-Specific GPU Resources
    union {
        struct {
            // OpenGL-specific handles
            uint32_t fbo_id;
            uint32_t texture_id;
            uint32_t rbo_id;
        } gl;
        struct {
            // Vulkan-specific handles
            VkFramebuffer framebuffer;
            VkRenderPass render_pass;
            VkSampler sampler;
            SituationTexture texture; // The texture containing the rendered output
        } vk;
    };
} SituationVirtualDisplay;
```
-   `id`: The unique identifier for the virtual display, used to reference it in API calls.
-   `visible`: If `true`, the display will be automatically drawn to the main window during the compositing phase (`SituationRenderVirtualDisplays`).
-   `is_dirty`: A flag used with time-multiplied displays. If `true`, the display is re-rendered; if `false`, the previous frame's content is reused, saving performance.
-   `resolution`: The internal width and height of the display's render textures in pixels.
-   `offset`: The top-left position (in screen coordinates) where the display will be drawn during compositing.
-   `opacity`: The opacity (0.0 to 1.0) of the display when it is blended onto the target.
-   `z_order`: An integer used to sort visible displays before compositing. Lower numbers are drawn first (further back).
-   `frame_time_multiplier`: Controls the update rate. `1.0` updates every frame, `0.5` every other frame, `0.0` only when marked dirty.
-   `scaling_mode`: An enum (`SituationScalingMode`) that determines how the display's texture is scaled if its resolution differs from its target area (e.g., `SITUATION_SCALING_STRETCH`, `SITUATION_SCALING_LETTERBOX`).
-   `blend_mode`: An enum (`SituationBlendMode`) that defines how the display is blended during compositing (e.g., `SITUATION_BLEND_ALPHA`, `SITUATION_BLEND_ADDITIVE`).
-   `gl`, `vk`: A union containing backend-specific handles to the underlying GPU resources. These are managed internally by the library.
---

#### `SituationCreateVirtualDisplay`
Creates an off-screen render target (virtual display) that can be rendered to independently and then composited onto the main window or other displays. Virtual displays are essential for multi-pass rendering, post-processing effects, and UI layering.

```c
SituationError SituationCreateVirtualDisplay(Vector2 resolution, double frame_time_mult, int z_order, SituationScalingMode scaling_mode, SituationBlendMode blend_mode, int* out_id);
```

**Parameters:**
- `resolution` - Width and height of the virtual display in pixels
- `frame_time_mult` - Time multiplier for animations (1.0 = normal speed, 0.5 = half speed, 2.0 = double speed)
- `z_order` - Rendering order (lower values render first, higher values on top)
- `scaling_mode` - How to scale when compositing: `SITUATION_SCALING_FIT`, `SITUATION_SCALING_FILL`, `SITUATION_SCALING_STRETCH`
- `blend_mode` - Blending mode: `SITUATION_BLEND_ALPHA`, `SITUATION_BLEND_ADDITIVE`, `SITUATION_BLEND_MULTIPLY`
- `out_id` - Pointer to receive the virtual display ID

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create a low-res virtual display for 3D scene (performance optimization)
int scene_display;
SituationCreateVirtualDisplay(
    (Vector2){1280, 720},  // Half of 4K resolution
    1.0,                    // Normal time
    0,                      // Render first (background)
    SITUATION_SCALING_FIT,  // Maintain aspect ratio
    SITUATION_BLEND_ALPHA,  // Standard alpha blending
    &scene_display
);

// Create a full-res virtual display for UI (sharp text)
int ui_display;
SituationCreateVirtualDisplay(
    (Vector2){2560, 1440},  // Full 4K resolution
    1.0,
    10,                     // Render on top
    SITUATION_SCALING_STRETCH,
    SITUATION_BLEND_ALPHA,
    &ui_display
);

// Create a slow-motion effect display
int slowmo_display;
SituationCreateVirtualDisplay(
    (Vector2){1920, 1080},
    0.25,                   // 1/4 speed for slow-motion
    5,
    SITUATION_SCALING_FIT,
    SITUATION_BLEND_ALPHA,
    &slowmo_display
);
```

**Notes:**
- Virtual displays are automatically composited in z_order during `SituationRenderVirtualDisplays()`
- Each virtual display has its own color and depth buffers
- Use lower resolutions for performance-critical 3D scenes
- Frame time multiplier affects time-based animations within that display
- Virtual displays can be used as textures via `SituationGetVirtualDisplayTexture()`

---
#### `SituationDestroyVirtualDisplay`
Destroys a virtual display and frees all associated GPU resources (framebuffers, textures, depth buffers). The display ID is set to -1 after destruction.

```c
SituationError SituationDestroyVirtualDisplay(int* display_id);
```

**Parameters:**
- `display_id` - Pointer to the virtual display ID to destroy (will be set to -1)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create temporary virtual display for screenshot
int screenshot_display;
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 0, 
    SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &screenshot_display);

// Render scene to it
SituationRenderPassInfo pass = { .display_id = screenshot_display };
SituationCmdBeginRenderPass(cmd, &pass);
RenderScene();
SituationCmdEndRenderPass(cmd);

// Save screenshot
SituationTexture screenshot_tex = SituationGetVirtualDisplayTexture(screenshot_display);
SituationImage img = SituationGetImageFromTexture(screenshot_tex);
SituationSaveImage(img, "screenshot.png");
SituationUnloadImage(img);

// Clean up
SituationDestroyVirtualDisplay(&screenshot_display);
// screenshot_display is now -1
```

**Notes:**
- Always destroy virtual displays when no longer needed to free GPU memory
- The display ID pointer is set to -1 to prevent use-after-free
- Destroying a display invalidates any texture handles obtained from it
- Safe to call multiple times (checks for valid ID)

---
#### `SituationSetVirtualDisplayVisible`
Controls whether a virtual display should be composited onto the render target during `SituationRenderVirtualDisplays()`. Hidden displays are still rendered to but not displayed.

```c
void SituationSetVirtualDisplayVisible(int display_id, bool visible);
```

**Parameters:**
- `display_id` - The virtual display ID
- `visible` - `true` to show during compositing, `false` to hide

**Usage Example:**
```c
// Create displays for different game states
int gameplay_display, menu_display, pause_display;
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 0, 
    SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &gameplay_display);
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 5, 
    SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &menu_display);
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 10, 
    SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &pause_display);

// Game state management
switch (game_state) {
    case STATE_GAMEPLAY:
        SituationSetVirtualDisplayVisible(gameplay_display, true);
        SituationSetVirtualDisplayVisible(menu_display, false);
        SituationSetVirtualDisplayVisible(pause_display, false);
        break;
    case STATE_PAUSED:
        SituationSetVirtualDisplayVisible(gameplay_display, true);  // Show dimmed
        SituationSetVirtualDisplayVisible(menu_display, false);
        SituationSetVirtualDisplayVisible(pause_display, true);     // Show pause menu
        break;
    case STATE_MENU:
        SituationSetVirtualDisplayVisible(gameplay_display, false);
        SituationSetVirtualDisplayVisible(menu_display, true);
        SituationSetVirtualDisplayVisible(pause_display, false);
        break;
}
```

**Notes:**
- Hidden displays are still rendered to (useful for keeping state)
- Use this for UI layer management and game state transitions
- More efficient than destroying/recreating displays
- Visibility only affects compositing, not rendering to the display

---
#### `SituationGetVirtualDisplayTexture`
Gets a handle to the underlying color buffer texture of a virtual display. This allows you to use the rendered output as an input texture for post-processing, UI elements, or other rendering passes.

```c
SituationTexture SituationGetVirtualDisplayTexture(int display_id);
```

**Parameters:**
- `display_id` - The virtual display ID

**Returns:** Texture handle for the virtual display's color buffer

**Usage Example:**
```c
// Create a virtual display for 3D scene
int scene_display;
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 0,
    SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &scene_display);

// Render 3D scene to virtual display
SituationRenderPassInfo scene_pass = { .display_id = scene_display };
SituationCmdBeginRenderPass(cmd, &scene_pass);
Render3DScene();
SituationCmdEndRenderPass(cmd);

// Get the rendered scene as a texture
SituationTexture scene_texture = SituationGetVirtualDisplayTexture(scene_display);

// Apply post-processing effects
SituationRenderPassInfo post_pass = { .display_id = -1 };
SituationCmdBeginRenderPass(cmd, &post_pass);
SituationCmdBindPipeline(cmd, blur_shader);
SituationCmdBindShaderTexture(cmd, 0, scene_texture);
SituationCmdDrawQuad(cmd);
SituationCmdEndRenderPass(cmd);

// Or use as a texture on a 3D object (security camera feed, portal, etc.)
SituationCmdBindShaderTexture(cmd, 0, scene_texture);
SituationCmdDrawMesh(cmd, tv_screen_mesh);
```

**Notes:**
- The texture is valid as long as the virtual display exists
- Texture is automatically updated each frame when you render to the display
- Useful for post-processing chains, mirrors, portals, and security camera effects
- The texture handle becomes invalid after `SituationDestroyVirtualDisplay()`

---
#### `SituationRenderVirtualDisplays`
Composites all visible virtual displays onto the current render target.
```c
SituationError SituationRenderVirtualDisplays(SituationCommandBuffer cmd);
```
**Usage Example:**
```c
// At init: Create a display for the 3D scene
int scene_vd;
SituationCreateVirtualDisplay((Vector2){640, 360}, 1.0, 0, SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &scene_vd);

// In render loop:
// 1. Render scene to the virtual display
SituationRenderPassInfo scene_pass = { .display_id = scene_vd };
SituationCmdBeginRenderPass(cmd, &scene_pass);
// ... draw 3D models ...
SituationCmdEndRenderPass(cmd);

// 2. Render to the main window
SituationRenderPassInfo final_pass = { .display_id = -1 };
SituationCmdBeginRenderPass(cmd, &final_pass);
// This composites the 3D scene from its virtual display onto the main window
SituationRenderVirtualDisplays(cmd);
// ... draw UI on top ...
SituationCmdEndRenderPass(cmd);
```

---
#### Legacy Shader Uniforms
---
#### `SituationGetShaderLocation`
Gets the location of a uniform variable in a shader by name.
```c
int SituationGetShaderLocation(SituationShader shader, const char* uniformName);
```
**Usage Example:**
```c
// Get the location of the "u_time" uniform in the shader.
int time_uniform_loc = SituationGetShaderLocation(my_shader, "u_time");
// This location can then be used with SituationSetShaderValue.
```

---
#### `SituationGetShaderLocationAttrib`
Gets the location of a vertex attribute in a shader by name.
```c
int SituationGetShaderLocationAttrib(SituationShader shader, const char* attribName);
```
**Usage Example:**
```c
// Get the location of the "a_color" vertex attribute.
int color_attrib_loc = SituationGetShaderLocationAttrib(my_shader, "a_color");
// This is useful for advanced, custom vertex buffer layouts.
```

---
#### `SituationSetShaderValue`
Sets a uniform value in a shader.
```c
void SituationSetShaderValue(SituationShader shader, int locIndex, const void* value, int uniformType);
```
**Usage Example:**
```c
int time_loc = SituationGetShaderLocation(my_shader, "u_time");
float current_time = (float)SituationGetTime();
// Note: This is a legacy way to set uniforms. Using UBOs with SituationCmdBindShaderBuffer is preferred.
SituationSetShaderValue(my_shader, time_loc, &current_time, SIT_UNIFORM_FLOAT);
```

---
#### `SituationSetShaderValueMatrix`
Sets a matrix uniform value in a shader.
```c
void SituationSetShaderValueMatrix(SituationShader shader, int locIndex, mat4 mat);
```
**Usage Example:**
```c
int mvp_loc = SituationGetShaderLocation(my_shader, "u_mvp");
mat4 mvp_matrix = /* ... calculate matrix ... */;
SituationSetShaderValueMatrix(my_shader, mvp_loc, mvp_matrix);
```

---
#### `SituationSetShaderValueTexture`
Sets a texture uniform value in a shader.
```c
void SituationSetShaderValueTexture(SituationShader shader, int locIndex, SituationTexture texture);
```
**Usage Example:**
```c
int albedo_loc = SituationGetShaderLocation(my_shader, "u_albedo_texture");
// This tells the shader to use my_texture for the texture sampler at `albedo_loc`.
SituationSetShaderValueTexture(my_shader, albedo_loc, my_texture);
```

---
#### Miscellaneous

---
#### `SituationLoadImageFromScreen`
Captures the current contents of the main window's backbuffer into a CPU-side image.
```c
SituationError SituationLoadImageFromScreen(SituationImage* out_image);
```
**Usage Example:**
```c
if (SituationIsKeyPressed(SIT_KEY_F12)) {
    SituationImage screenshot = {0};
    if (SituationLoadImageFromScreen(&screenshot) == SITUATION_SUCCESS) {
        SituationExportImage(screenshot, "screenshot.png");
        SituationUnloadImage(screenshot);
    }
}
```

---
#### `SituationCmdBeginRenderToDisplay`
**[DEPRECATED]** Begins a render pass on a target (-1 for main window), clearing it. Use `SituationCmdBeginRenderPass()` instead for more control.

```c
SituationError SituationCmdBeginRenderToDisplay(SituationCommandBuffer cmd, int display_id, ColorRGBA clear_color);
```

**Migration Guide:**

**Old Code:**
```c
// DEPRECATED
ColorRGBA clear = {20, 30, 40, 255};
SituationCmdBeginRenderToDisplay(cmd, -1, clear);
// ... draw commands ...
SituationCmdEndRender(cmd);
```

**New Code:**
```c
// RECOMMENDED
SituationRenderPassInfo pass_info = {
    .target_display_id = -1,  // Main window
    .clear_color = {0.078f, 0.117f, 0.156f, 1.0f},  // Normalized
    .color_load_action = SIT_LOAD_ACTION_CLEAR,
    .color_store_action = SIT_STORE_ACTION_STORE
};
SituationCmdBeginRenderPass(cmd, &pass_info);
// ... draw commands ...
SituationCmdEndRenderPass(cmd);
```

**Why Deprecated:**
- `SituationCmdBeginRenderPass()` provides more control (load/store actions)
- Consistent with modern Vulkan terminology
- Supports depth/stencil attachments
- Better performance with explicit load/store actions

---
#### `SituationCmdEndRender`
**[DEPRECATED]** End the current render pass. Use `SituationCmdEndRenderPass()` instead.

```c
SituationError SituationCmdEndRender(SituationCommandBuffer cmd);
```

**Migration Guide:**

**Old Code:**
```c
// DEPRECATED
SituationCmdBeginRenderToDisplay(cmd, -1, clear_color);
// ... draw commands ...
SituationCmdEndRender(cmd);
```

**New Code:**
```c
// RECOMMENDED
SituationCmdBeginRenderPass(cmd, &pass_info);
// ... draw commands ...
SituationCmdEndRenderPass(cmd);
```

**Why Deprecated:**
- Matches `SituationCmdBeginRenderPass()` naming
- More explicit about what's being ended
- Consistent with Vulkan terminology

---
#### `SituationCmdSetScissor`
Sets the dynamic scissor rectangle to clip rendering. Only pixels inside the scissor rect will be drawn.

```c
void SituationCmdSetScissor(SituationCommandBuffer cmd, int x, int y, int width, int height);
```

**Parameters:**
- `cmd` - Command buffer to record into
- `x` - Left edge of scissor rectangle
- `y` - Top edge of scissor rectangle
- `width` - Width of scissor rectangle
- `height` - Height of scissor rectangle

**Usage Example:**
```c
// Clip rendering to a specific region
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

// Enable scissor test for UI panel
SituationCmdSetScissor(cmd, 100, 100, 400, 300);
DrawUIPanel();  // Only visible within scissor rect

// Reset to full screen
int screen_width = SituationGetRenderWidth();
int screen_height = SituationGetRenderHeight();
SituationCmdSetScissor(cmd, 0, 0, screen_width, screen_height);

// Split-screen rendering
// Left half
SituationCmdSetScissor(cmd, 0, 0, screen_width / 2, screen_height);
RenderPlayer1View();

// Right half
SituationCmdSetScissor(cmd, screen_width / 2, 0, screen_width / 2, screen_height);
RenderPlayer2View();

// Restore full screen
SituationCmdSetScissor(cmd, 0, 0, screen_width, screen_height);
```

**Notes:**
- Coordinates are in pixels, origin at top-left
- Scissor test must be enabled in pipeline state
- Useful for UI clipping and split-screen
- Reset to full screen when done

---
#### `SituationCmdSetPushConstant`
Sets a small block of per-draw uniform data (push constant) that is sent directly to the shader without requiring a buffer. Push constants are the fastest way to send small amounts of data (typically up to 128 bytes) that change frequently per draw call.

```c
SituationError SituationCmdSetPushConstant(SituationCommandBuffer cmd, uint32_t contract_id, const void* data, size_t size);
```

**Parameters:**
- `cmd` - The command buffer to record into
- `contract_id` - The push constant block index (corresponds to shader layout)
- `data` - Pointer to the data to copy
- `size` - Size of the data in bytes (typically ≤ 128 bytes)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
/* GLSL Shader:
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
} pc;
*/

typedef struct {
    mat4 model;
    vec4 color;
} PushConstants;

// Update push constants before each draw
for (int i = 0; i < object_count; i++) {
    PushConstants pc = {
        .model = objects[i].transform,
        .color = objects[i].color
    };
    
    SituationCmdSetPushConstant(cmd, 0, &pc, sizeof(PushConstants));
    SituationCmdDrawMesh(cmd, objects[i].mesh);
}
```

**Notes:**
- Push constants are ideal for per-draw data like model matrices, colors, or time values
- Much faster than updating uniform buffers for small, frequently-changing data
- Data is copied by value into the command buffer, so the source pointer doesn't need to remain valid
- Maximum size is typically 128 bytes (guaranteed by Vulkan spec)
- For larger data or data shared across multiple draws, use uniform buffers instead

---
#### `SituationCmdBindDescriptorSet`
Binds a buffer's descriptor set (uniform buffer or storage buffer) to a specific set index in the shader pipeline. This is the primary API for binding buffers to shaders, replacing older functions like `SituationCmdBindUniformBuffer`.

```c
SituationError SituationCmdBindDescriptorSet(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer);
```

**Parameters:**
- `cmd` - The command buffer to record into
- `set_index` - The descriptor set index (corresponds to `set = N` in GLSL)
- `buffer` - The buffer handle to bind (UBO or SSBO)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
/* GLSL Shader:
layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 projection;
} u_camera;
*/

// Create and update the camera uniform buffer
SituationBuffer camera_ubo = SituationCreateBuffer(sizeof(CameraData), 
    SITUATION_BUFFER_USAGE_UNIFORM);
SituationUpdateBuffer(camera_ubo, &camera_data, sizeof(CameraData));

// Bind to set 0 before drawing
SituationCmdBindDescriptorSet(cmd, 0, camera_ubo);
SituationCmdDrawMesh(cmd, scene_mesh);
```

**Notes:**
- Automatically handles dynamic buffers (created with `SITUATION_BUFFER_USAGE_DYNAMIC_UNIFORM`) by using offset 0
- For dynamic buffers with custom offsets, use `SituationCmdBindDescriptorSetDynamic()`
- This is a low-overhead operation on Vulkan thanks to persistent descriptor sets
- Set indices must match the `set = N` layout qualifier in your shader

---
#### `SituationCmdBindTextureSet`
Binds a texture's descriptor set to a specific set index in the shader pipeline. This is the primary API for binding textures to shaders, supporting both sampled textures and storage images.

```c
SituationError SituationCmdBindTextureSet(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture);
```

**Parameters:**
- `cmd` - The command buffer to record into
- `set_index` - The descriptor set index (corresponds to `set = N` in GLSL)
- `texture` - The texture handle to bind

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
/* GLSL Shader:
layout(set = 1, binding = 0) uniform sampler2D u_albedo;
layout(set = 2, binding = 0) uniform sampler2D u_normal;
*/

// Load textures
SituationTexture albedo = SituationLoadTexture("textures/albedo.png");
SituationTexture normal = SituationLoadTexture("textures/normal.png");

// Bind textures to their respective sets
SituationCmdBindTextureSet(cmd, 1, albedo);
SituationCmdBindTextureSet(cmd, 2, normal);
SituationCmdDrawMesh(cmd, model_mesh);
```

**Notes:**
- Works with both sampled textures and storage images
- Set indices must match the `set = N` layout qualifier in your shader
- Replaces older functions like `SituationCmdBindTexture()`
- Persistent descriptor sets make this a fast operation on Vulkan

---
#### `SituationCmdBindComputeTexture`
Binds a texture as a storage image for compute shaders. Storage images allow compute shaders to read and write to textures.

```c
SituationError SituationCmdBindComputeTexture(SituationCommandBuffer cmd, uint32_t binding, SituationTexture texture);
```

**Parameters:**
- `cmd` - Command buffer to record into
- `binding` - Binding point (matches shader layout binding)
- `texture` - Texture to bind as storage image

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create texture for compute shader output
SituationTexture output_texture;
SituationCreateTexture2D(512, 512, SITUATION_FORMAT_RGBA8, &output_texture);

// Bind to compute shader
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
SituationCmdBindComputePipeline(cmd, compute_pipeline);
SituationCmdBindComputeTexture(cmd, 0, output_texture);  // binding = 0

// Dispatch compute shader
SituationCmdDispatch(cmd, 512 / 16, 512 / 16, 1);  // 16x16 work groups

// Use barrier before reading
SituationPipelineBarrierInfo barrier = {
    .src_stage = SITUATION_PIPELINE_STAGE_COMPUTE_SHADER,
    .dst_stage = SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER,
    .src_access = SITUATION_ACCESS_SHADER_WRITE,
    .dst_access = SITUATION_ACCESS_SHADER_READ
};
SituationCmdPipelineBarrier(cmd, &barrier);

// Now safe to read in fragment shader
SituationCmdBindTextureSet(cmd, 0, output_texture);

// Shader example (GLSL)
// layout(binding = 0, rgba8) uniform image2D output_image;
// void main() {
//     ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
//     vec4 color = vec4(1.0, 0.0, 0.0, 1.0);
//     imageStore(output_image, pixel, color);
// }
```

**Notes:**
- Texture must be created with storage usage flag
- Binding matches shader layout binding
- Use barriers when transitioning between compute and graphics
- Storage images support read/write operations

---
#### `SituationCmdSetVertexAttribute`
**[OpenGL Only]** Defines the format and layout of a vertex attribute for the currently active Vertex Array Object (VAO). This function is used to describe how vertex data in a buffer should be interpreted by the shader.

```c
SituationError SituationCmdSetVertexAttribute(SituationCommandBuffer cmd, uint32_t location, int size, SituationDataType type, bool normalized, size_t offset);
```

**Parameters:**
- `cmd` - The command buffer to record into
- `location` - The attribute location (corresponds to `location = N` in GLSL)
- `size` - Number of components (1-4, e.g., 3 for vec3)
- `type` - Data type (e.g., `SITUATION_DATA_TYPE_FLOAT`)
- `normalized` - Whether integer types should be normalized to [0,1] or [-1,1]
- `offset` - Byte offset from the start of the vertex structure

**Returns:** `SITUATION_SUCCESS` on OpenGL, `SITUATION_ERROR_NOT_IMPLEMENTED` on Vulkan

**Usage Example:**
```c
/* GLSL Shader:
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texcoord;
layout(location = 2) in vec3 a_normal;
*/

typedef struct {
    vec3 position;  // offset 0
    vec2 texcoord;  // offset 12
    vec3 normal;    // offset 20
} Vertex;

// Define vertex format (OpenGL only)
SituationCmdSetVertexAttribute(cmd, 0, 3, SITUATION_DATA_TYPE_FLOAT, false, offsetof(Vertex, position));
SituationCmdSetVertexAttribute(cmd, 1, 2, SITUATION_DATA_TYPE_FLOAT, false, offsetof(Vertex, texcoord));
SituationCmdSetVertexAttribute(cmd, 2, 3, SITUATION_DATA_TYPE_FLOAT, false, offsetof(Vertex, normal));
```

**Notes:**
- **OpenGL Only:** This function only works on the OpenGL backend. Vulkan pipelines have immutable vertex formats defined at pipeline creation time
- For Vulkan, vertex formats are specified when creating the shader/pipeline
- This is considered a legacy API; modern code should use SSBO vertex pulling where possible
- The command is recorded into the soft command buffer and executed during frame submission

---
#### `SituationCmdDrawIndexed`
Records an indexed draw call to the command buffer. This is the core rendering command for drawing meshes with index buffers.

```c
SituationError SituationCmdDrawIndexed(
    SituationCommandBuffer cmd,
    uint32_t index_count,
    uint32_t instance_count,
    uint32_t first_index,
    int32_t vertex_offset,
    uint32_t first_instance
);
```

**Parameters:**
- `cmd` - Command buffer to record into
- `index_count` - Number of indices to draw
- `instance_count` - Number of instances to draw (1 for non-instanced)
- `first_index` - Offset into the index buffer
- `vertex_offset` - Value added to vertex index before indexing into vertex buffer
- `first_instance` - First instance ID (usually 0)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Basic indexed draw
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

// Set up render pass
SituationRenderPassInfo pass_info = {
    .target_display_id = -1,  // Main window
    .clear_color = {0.1f, 0.1f, 0.1f, 1.0f}
};
SituationCmdBeginRenderPass(cmd, &pass_info);

// Bind pipeline and resources
SituationCmdBindPipeline(cmd, shader);
SituationCmdBindDescriptorSet(cmd, 0, descriptor_set);

// Draw mesh (36 indices, 1 instance)
SituationCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

SituationCmdEndRenderPass(cmd);

// Instanced rendering
SituationCmdDrawIndexed(cmd, mesh.index_count, 100, 0, 0, 0);  // Draw 100 instances

// Draw subset of mesh
SituationCmdDrawIndexed(cmd, 12, 1, 24, 0, 0);  // Draw indices 24-35
```

**Notes:**
- Must be called inside a render pass
- Requires bound pipeline and vertex/index buffers
- Use instance_count > 1 for instanced rendering
- vertex_offset is added to each index value

---
#### `SituationCmdEndRenderPass`
Ends the current render pass. Must be called after `SituationCmdBeginRenderPass` and all drawing commands.

```c
SituationError SituationCmdEndRenderPass(SituationCommandBuffer cmd);
```

**Parameters:**
- `cmd` - Command buffer containing the render pass

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Basic render pass
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

SituationRenderPassInfo pass_info = {
    .target_display_id = -1,
    .clear_color = {0.0f, 0.0f, 0.0f, 1.0f}
};

SituationCmdBeginRenderPass(cmd, &pass_info);

// Draw commands here
SituationCmdBindPipeline(cmd, shader);
SituationCmdDrawMesh(cmd, mesh);

// Always end the render pass
SituationCmdEndRenderPass(cmd);

// Multi-pass rendering
// Pass 1: Render to virtual display
SituationRenderPassInfo vd_pass = {
    .target_display_id = 0,
    .clear_color = {0.0f, 0.0f, 0.0f, 1.0f}
};
SituationCmdBeginRenderPass(cmd, &vd_pass);
RenderScene();
SituationCmdEndRenderPass(cmd);

// Pass 2: Post-process to main window
SituationRenderPassInfo main_pass = {
    .target_display_id = -1,
    .clear_color = {0.0f, 0.0f, 0.0f, 1.0f}
};
SituationCmdBeginRenderPass(cmd, &main_pass);
ApplyPostProcessing();
SituationCmdEndRenderPass(cmd);
```

**Notes:**
- Must match every `SituationCmdBeginRenderPass`
- Finalizes rendering to the target
- Required before starting a new render pass
- Errors if no render pass is active

---
#### `SituationLoadShaderFromMemory`
Creates a graphics shader pipeline from in-memory GLSL source code. This is useful for procedurally generated shaders, embedded shaders, or runtime shader compilation.

```c
SituationError SituationLoadShaderFromMemory(const char* vs_code, const char* fs_code, SituationShader* out_shader);
```

**Parameters:**
- `vs_code` - Null-terminated string containing vertex shader GLSL source
- `fs_code` - Null-terminated string containing fragment shader GLSL source
- `out_shader` - Pointer to receive the created shader handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Embed shaders directly in code
const char* vertex_shader = 
    "#version 450\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec2 a_texcoord;\n"
    "layout(location = 0) out vec2 v_texcoord;\n"
    "layout(push_constant) uniform PC { mat4 mvp; };\n"
    "void main() {\n"
    "    gl_Position = mvp * vec4(a_position, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "}\n";

const char* fragment_shader =
    "#version 450\n"
    "layout(location = 0) in vec2 v_texcoord;\n"
    "layout(location = 0) out vec4 frag_color;\n"
    "layout(binding = 0) uniform sampler2D u_texture;\n"
    "void main() {\n"
    "    frag_color = texture(u_texture, v_texcoord);\n"
    "}\n";

SituationShader shader;
if (SituationLoadShaderFromMemory(vertex_shader, fragment_shader, &shader) != SITUATION_SUCCESS) {
    printf("Failed to compile shader\n");
    return;
}

// Use the shader
SituationCmdBindPipeline(cmd, shader);
SituationCmdDrawMesh(cmd, mesh);

// Clean up
SituationUnloadShader(shader);
```

**Advanced Example (Procedural Shader Generation):**
```c
// Generate shader with configurable parameters
char fs_code[4096];
snprintf(fs_code, sizeof(fs_code),
    "#version 450\n"
    "layout(location = 0) in vec2 v_texcoord;\n"
    "layout(location = 0) out vec4 frag_color;\n"
    "layout(binding = 0) uniform sampler2D u_texture;\n"
    "void main() {\n"
    "    vec4 color = texture(u_texture, v_texcoord);\n"
    "    color.rgb *= vec3(%.2f, %.2f, %.2f);\n"  // Tint color
    "    frag_color = color;\n"
    "}\n",
    tint_r, tint_g, tint_b);

SituationShader tinted_shader;
SituationLoadShaderFromMemory(standard_vs, fs_code, &tinted_shader);
```

**Notes:**
- Shader source must be complete, valid GLSL
- Compilation errors are logged to the console
- Useful for shader hot-reloading during development
- Can embed shaders to avoid external file dependencies
- Performance is identical to file-based shaders after compilation

---
#### `SituationSetShaderUniform`
**[OpenGL Only]** Sets a uniform variable value by name. This is a legacy API that uses a cache to avoid redundant `glGetUniformLocation` calls.

```c
SituationError SituationSetShaderUniform(SituationShader shader, const char* uniform_name, const void* data, SituationUniformType type);
```

**Parameters:**
- `shader` - The shader to set the uniform in
- `uniform_name` - Name of the uniform variable (e.g., "u_time")
- `data` - Pointer to the uniform data
- `type` - Type of the uniform (e.g., `SITUATION_UNIFORM_FLOAT`, `SITUATION_UNIFORM_VEC3`, `SITUATION_UNIFORM_MAT4`)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Set various uniform types
float time = SituationGetTime();
SituationSetShaderUniform(shader, "u_time", &time, SITUATION_UNIFORM_FLOAT);

vec3 light_pos = {10.0f, 20.0f, 5.0f};
SituationSetShaderUniform(shader, "u_light_position", &light_pos, SITUATION_UNIFORM_VEC3);

mat4 model_matrix;
glm_mat4_identity(model_matrix);
SituationSetShaderUniform(shader, "u_model", model_matrix, SITUATION_UNIFORM_MAT4);

vec4 tint_color = {1.0f, 0.5f, 0.5f, 1.0f};
SituationSetShaderUniform(shader, "u_tint", &tint_color, SITUATION_UNIFORM_VEC4);
```

**Notes:**
- **OpenGL Only** - Not available on Vulkan backend
- For Vulkan, use uniform buffers with `SituationUpdateBuffer()` instead
- Uses an internal cache to avoid repeated `glGetUniformLocation` calls
- Less efficient than uniform buffers for frequently-changing data
- Recommended to migrate to uniform buffers for cross-platform code

---
#### `SituationCreateComputePipeline`
Creates a compute pipeline from a GLSL compute shader file. Compute pipelines are used for GPU-accelerated parallel processing.

```c
SituationError SituationCreateComputePipeline(const char* compute_shader_path, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline);
```

**Parameters:**
- `compute_shader_path` - Path to GLSL compute shader file (.comp)
- `layout_type` - Descriptor set layout (e.g., `SITUATION_COMPUTE_LAYOUT_BUFFER_ONLY`)
- `out_pipeline` - Pointer to receive the created pipeline handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create compute pipeline for particle simulation
SituationComputePipeline particle_pipeline;
if (SituationCreateComputePipeline(
    "shaders/particles.comp",
    SITUATION_COMPUTE_LAYOUT_BUFFER_ONLY,
    &particle_pipeline) != SITUATION_SUCCESS) {
    printf("Failed to create compute pipeline\n");
    return;
}

// Use the pipeline
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
SituationCmdBindComputePipeline(cmd, particle_pipeline);
SituationCmdBindComputeBuffer(cmd, 0, particle_buffer);
SituationCmdDispatch(cmd, particle_count / 256, 1, 1);

// Cleanup
SituationDestroyComputePipeline(particle_pipeline);

// Image processing pipeline
SituationComputePipeline blur_pipeline;
SituationCreateComputePipeline(
    "shaders/blur.comp",
    SITUATION_COMPUTE_LAYOUT_TEXTURE_ONLY,
    &blur_pipeline);

// Bind and dispatch
SituationCmdBindComputePipeline(cmd, blur_pipeline);
SituationCmdBindComputeTexture(cmd, 0, input_texture);
SituationCmdBindComputeTexture(cmd, 1, output_texture);
SituationCmdDispatch(cmd, width / 16, height / 16, 1);
```

**Notes:**
- Shader must be GLSL compute shader (#version 450)
- Layout type must match shader's descriptor set layout
- Pipeline must be destroyed with `SituationDestroyComputePipeline()`
- Use `SituationCreateComputePipelineFromMemory()` for runtime generation

---
#### `SituationCreateComputePipelineFromMemory`
Creates a compute pipeline from in-memory GLSL source code. This is useful for procedurally generated shaders, embedded shaders, or runtime shader compilation.

```c
SituationError SituationCreateComputePipelineFromMemory(const char* compute_shader_source, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline);
```

**Parameters:**
- `compute_shader_source` - Null-terminated string containing GLSL compute shader source
- `layout_type` - Descriptor set layout type
- `out_pipeline` - Pointer to receive the created pipeline handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Procedurally generate a compute shader
const char* shader_source = 
    "#version 450\n"
    "layout(local_size_x = 256) in;\n"
    "layout(binding = 0) buffer Data { float values[]; };\n"
    "void main() {\n"
    "    uint id = gl_GlobalInvocationID.x;\n"
    "    values[id] *= 2.0;\n"
    "}\n";

SituationComputePipeline pipeline;
if (SituationCreateComputePipelineFromMemory(
    shader_source,
    SITUATION_COMPUTE_LAYOUT_STANDARD,
    &pipeline) != SITUATION_SUCCESS) {
    printf("Failed to compile compute shader\n");
    return;
}

// Use the pipeline
SituationCmdBindComputePipeline(cmd, pipeline);
SituationCmdBindComputeBuffer(cmd, 0, data_buffer);
SituationCmdDispatch(cmd, data_count / 256, 1, 1);

SituationDestroyComputePipeline(&pipeline);
```

**Advanced Example (Runtime Shader Generation):**
```c
// Generate shader with configurable work group size
char shader_code[2048];
snprintf(shader_code, sizeof(shader_code),
    "#version 450\n"
    "layout(local_size_x = %d) in;\n"
    "layout(binding = 0) buffer Particles { vec4 positions[]; };\n"
    "layout(push_constant) uniform Constants { float dt; };\n"
    "void main() {\n"
    "    uint id = gl_GlobalInvocationID.x;\n"
    "    positions[id].y -= 9.8 * dt;\n"  // Gravity
    "}\n",
    optimal_work_group_size);

SituationComputePipeline gravity_pipeline;
SituationCreateComputePipelineFromMemory(shader_code,
    SITUATION_COMPUTE_LAYOUT_STANDARD, &gravity_pipeline);
```

**Notes:**
- Shader source must be a complete, valid GLSL compute shader
- Compilation errors are logged to the console
- Useful for shader hot-reloading during development
- Can embed shaders directly in the executable
- Performance is identical to file-based pipelines after compilation

---
#### `SituationDestroyComputePipeline`
Destroys a compute pipeline and frees all associated GPU resources. Always call this when done with a compute pipeline.

```c
void SituationDestroyComputePipeline(SituationComputePipeline* pipeline);
```

**Parameters:**
- `pipeline` - Pointer to compute pipeline to destroy

**Usage Example:**
```c
// Create and use compute pipeline
SituationComputePipeline pipeline;
SituationCreateComputePipeline("shaders/compute.comp", SITUATION_COMPUTE_LAYOUT_BUFFER_ONLY, &pipeline);

// Use the pipeline
SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
SituationCmdBindComputePipeline(cmd, pipeline);
SituationCmdDispatch(cmd, 64, 1, 1);

// Cleanup when done
SituationDestroyComputePipeline(&pipeline);

// Proper resource management
SituationComputePipeline pipelines[3];

void LoadComputeShaders() {
    SituationCreateComputePipeline("shaders/particles.comp", SITUATION_COMPUTE_LAYOUT_BUFFER_ONLY, &pipelines[0]);
    SituationCreateComputePipeline("shaders/blur.comp", SITUATION_COMPUTE_LAYOUT_TEXTURE_ONLY, &pipelines[1]);
    SituationCreateComputePipeline("shaders/physics.comp", SITUATION_COMPUTE_LAYOUT_STANDARD, &pipelines[2]);
}

void UnloadComputeShaders() {
    for (int i = 0; i < 3; i++) {
        SituationDestroyComputePipeline(&pipelines[i]);
    }
}

// Safe to call multiple times
SituationComputePipeline pipeline;
SituationCreateComputePipeline("test.comp", SITUATION_COMPUTE_LAYOUT_BUFFER_ONLY, &pipeline);
SituationDestroyComputePipeline(&pipeline);
// pipeline.id is now 0
```

**Notes:**
- Frees shader modules and pipeline state
- Sets pipeline ID to 0 after destruction
- Safe to call multiple times (idempotent)
- Always call before application exit
- Don't use pipeline after destruction

---
#### `SituationGetBufferData`
Reads data back from a GPU buffer to CPU memory. This is useful for debugging, readback of compute shader results, or retrieving GPU-generated data.

```c
SituationError SituationGetBufferData(SituationBuffer buffer, size_t offset, size_t size, void* out_data);
```

**Parameters:**
- `buffer` - The buffer to read from
- `offset` - Byte offset into the buffer to start reading
- `size` - Number of bytes to read
- `out_data` - Pointer to CPU memory to receive the data

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Run compute shader to process data
SituationCmdBindComputePipeline(cmd, process_pipeline);
SituationCmdBindComputeBuffer(cmd, 0, result_buffer);
SituationCmdDispatch(cmd, work_groups, 1, 1);

// Wait for GPU to finish
SituationEndFrame();
SituationWaitForGPU();

// Read results back to CPU
float results[1024];
if (SituationGetBufferData(result_buffer, 0, sizeof(results), results) == SITUATION_SUCCESS) {
    // Process results on CPU
    for (int i = 0; i < 1024; i++) {
        printf("Result[%d] = %.2f\n", i, results[i]);
    }
}
```

**Debugging Example:**
```c
// Verify uniform buffer contents
CameraUniforms camera_data_readback;
SituationGetBufferData(camera_ubo, 0, sizeof(CameraUniforms), &camera_data_readback);

printf("Camera position: %.2f, %.2f, %.2f\n",
    camera_data_readback.camera_pos[0],
    camera_data_readback.camera_pos[1],
    camera_data_readback.camera_pos[2]);
```

**Notes:**
- **Performance Warning:** GPU-to-CPU readback is SLOW - avoid in performance-critical code
- Causes a GPU pipeline stall - use sparingly
- Primarily for debugging and one-time data retrieval
- For frequent readback, consider using staging buffers
- Ensure GPU has finished writing to the buffer before reading

---
#### `SituationConfigureVirtualDisplay`
Reconfigures an existing virtual display's properties without destroying and recreating it. This allows dynamic adjustment of display behavior at runtime.

```c
SituationError SituationConfigureVirtualDisplay(int display_id, vec2 offset, float opacity, int z_order, bool visible, double frame_time_mult, SituationBlendMode blend_mode);
```

**Parameters:**
- `display_id` - The virtual display ID to configure
- `offset` - Position offset when compositing (in pixels)
- `opacity` - Overall opacity (0.0 = fully transparent, 1.0 = fully opaque)
- `z_order` - Rendering order (lower values render first)
- `visible` - Whether to composite this display
- `frame_time_mult` - Time multiplier for animations
- `blend_mode` - Blending mode for compositing

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create a virtual display for damage flash effect
int damage_flash_display;
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 100,
    SITUATION_SCALING_STRETCH, SITUATION_BLEND_ADDITIVE, &damage_flash_display);

// Initially invisible
SituationConfigureVirtualDisplay(damage_flash_display, 
    (vec2){0, 0}, 0.0f, 100, false, 1.0, SITUATION_BLEND_ADDITIVE);

// When player takes damage, fade in red overlay
float damage_opacity = 0.8f;
SituationConfigureVirtualDisplay(damage_flash_display,
    (vec2){0, 0}, damage_opacity, 100, true, 1.0, SITUATION_BLEND_ADDITIVE);

// Fade out over time
for (float t = 0.8f; t > 0.0f; t -= delta_time * 2.0f) {
    SituationConfigureVirtualDisplay(damage_flash_display,
        (vec2){0, 0}, t, 100, true, 1.0, SITUATION_BLEND_ADDITIVE);
}

// Hide when done
SituationConfigureVirtualDisplay(damage_flash_display,
    (vec2){0, 0}, 0.0f, 100, false, 1.0, SITUATION_BLEND_ADDITIVE);
```

**Notes:**
- More efficient than destroying and recreating displays
- Useful for animated transitions, fades, and dynamic UI layers
- Opacity affects the entire display during compositing
- Offset allows for screen shake or parallax effects

---
#### `SituationGetVirtualDisplay`
Gets a pointer to the internal state structure of a virtual display. This provides direct access to display properties for advanced use cases.

```c
SituationVirtualDisplay* SituationGetVirtualDisplay(int display_id);
```

**Parameters:**
- `display_id` - The virtual display ID

**Returns:** Pointer to the virtual display structure, or NULL if invalid

**Usage Example:**
```c
// Query virtual display properties
SituationVirtualDisplay* vd = SituationGetVirtualDisplay(my_display_id);
if (vd != NULL) {
    printf("Display resolution: %dx%d\n", vd->width, vd->height);
    printf("Z-order: %d\n", vd->z_order);
    printf("Visible: %s\n", vd->visible ? "yes" : "no");
    printf("Frame time multiplier: %.2f\n", vd->frame_time_mult);
    
    // Access framebuffer handle for advanced operations
    printf("Framebuffer ID: %u\n", vd->framebuffer_id);
}
```

**Notes:**
- Returns NULL for invalid display IDs
- Provides read access to internal state
- Modifying the structure directly is not recommended - use configuration functions instead
- Useful for debugging and advanced rendering techniques

---
#### `SituationSetVirtualDisplayScalingMode`
Changes how a virtual display is scaled when composited onto the render target. This affects how the display adapts to different window sizes.

```c
SituationError SituationSetVirtualDisplayScalingMode(int display_id, SituationScalingMode scaling_mode);
```

**Parameters:**
- `display_id` - The virtual display ID
- `scaling_mode` - Scaling mode:
  - `SITUATION_SCALING_FIT` - Maintain aspect ratio, letterbox if needed
  - `SITUATION_SCALING_FILL` - Fill entire area, may crop edges
  - `SITUATION_SCALING_STRETCH` - Stretch to fill, may distort

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create displays with different scaling behaviors
int game_display, ui_display;
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 0,
    SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &game_display);
SituationCreateVirtualDisplay((Vector2){1920, 1080}, 1.0, 10,
    SITUATION_SCALING_STRETCH, SITUATION_BLEND_ALPHA, &ui_display);

// User changes window size - adjust scaling
if (window_aspect_ratio != target_aspect_ratio) {
    // Keep game at correct aspect ratio (letterbox if needed)
    SituationSetVirtualDisplayScalingMode(game_display, SITUATION_SCALING_FIT);
    
    // Stretch UI to fill entire window
    SituationSetVirtualDisplayScalingMode(ui_display, SITUATION_SCALING_STRETCH);
}

// For ultrawide monitors, fill the screen
if (is_ultrawide) {
    SituationSetVirtualDisplayScalingMode(game_display, SITUATION_SCALING_FILL);
}
```

**Notes:**
- `FIT` mode maintains aspect ratio with black bars if needed
- `FILL` mode may crop content but avoids black bars
- `STRETCH` mode distorts the image but uses full screen
- Can be changed at runtime to adapt to window resizing

---
#### `SituationGetVirtualDisplaySize`
Retrieves the internal resolution (width and height) of a virtual display's render target.

```c
void SituationGetVirtualDisplaySize(int display_id, int* width, int* height);
```

**Parameters:**
- `display_id` - The virtual display ID
- `width` - Pointer to receive the width in pixels
- `height` - Pointer to receive the height in pixels

**Usage Example:**
```c
// Query display size for dynamic rendering
int vd_width, vd_height;
SituationGetVirtualDisplaySize(my_display_id, &vd_width, &vd_height);

// Calculate aspect ratio
float aspect = (float)vd_width / (float)vd_height;
printf("Virtual display: %dx%d (aspect: %.2f)\n", vd_width, vd_height, aspect);

// Set viewport to match display size
SituationCmdSetViewport(cmd, 0, 0, vd_width, vd_height);

// Create projection matrix matching display resolution
mat4 projection;
glm_perspective(FOV, aspect, NEAR_PLANE, FAR_PLANE, projection);
```

**Notes:**
- Returns the internal render target size, not the composited size
- Useful for setting viewports and calculating aspect ratios
- The size is fixed at creation time (set in `SituationCreateVirtualDisplay()`)

---
#### `SituationDrawModel`
Draws all sub-meshes of a model with a single root transformation. This is a convenience function that draws all meshes in the model hierarchy.

```c
void SituationDrawModel(SituationCommandBuffer cmd, SituationModel model, mat4 transform);
```

**Parameters:**
- `cmd` - Command buffer to record into
- `model` - Model to draw (contains multiple meshes)
- `transform` - Root transformation matrix (model-to-world)

**Usage Example:**
```c
// Load and draw a model
SituationModel character;
SituationLoadModel("models/character.gltf", &character);

// Draw with transformation
mat4 transform;
glm_mat4_identity(transform);
glm_translate(transform, (vec3){0.0f, 0.0f, -5.0f});
glm_rotate(transform, glm_rad(45.0f), (vec3){0.0f, 1.0f, 0.0f});
glm_scale(transform, (vec3){2.0f, 2.0f, 2.0f});

SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
SituationCmdBindPipeline(cmd, shader);
SituationDrawModel(cmd, character, transform);

// Draw multiple instances
for (int i = 0; i < enemy_count; i++) {
    mat4 enemy_transform;
    glm_mat4_identity(enemy_transform);
    glm_translate(enemy_transform, enemies[i].position);
    glm_rotate(enemy_transform, enemies[i].rotation, (vec3){0.0f, 1.0f, 0.0f});
    
    SituationDrawModel(cmd, enemy_model, enemy_transform);
}

// Draw with camera
mat4 mvp;
glm_mat4_mul(camera.projection, camera.view, mvp);
glm_mat4_mul(mvp, model_transform, mvp);

// Set MVP as push constant
SituationCmdSetPushConstant(cmd, 0, &mvp, sizeof(mat4));
SituationDrawModel(cmd, model, model_transform);
```

**Notes:**
- Draws all sub-meshes in the model
- Each sub-mesh may have different materials
- Transform is applied to all sub-meshes
- Requires bound pipeline and descriptor sets
- Use `SituationCmdDrawMesh()` for individual meshes

---
#### `SituationSaveModelAsGltf`
Exports a model to a human-readable .gltf and a .bin file for debugging and inspection.

```c
bool SituationSaveModelAsGltf(SituationModel model, const char* file_path);
```

**Parameters:**
- `model` - Model to export
- `file_path` - Output path (without extension, .gltf and .bin will be added)

**Returns:** `true` on success, `false` on failure

**Usage Example:**
```c
// Export procedurally generated model
SituationModel terrain = GenerateTerrainModel();
if (SituationSaveModelAsGltf(terrain, "exports/terrain")) {
    printf("Model exported to exports/terrain.gltf and exports/terrain.bin\n");
}

// Debug model loading
SituationModel model;
if (SituationLoadModel("models/character.gltf", &model) == SITUATION_SUCCESS) {
    // Re-export to verify loading
    SituationSaveModelAsGltf(model, "debug/character_reexport");
}

// Export modified model
SituationModel original;
SituationLoadModel("models/original.gltf", &original);

// Modify model...
ModifyModelGeometry(&original);

// Save modified version
SituationSaveModelAsGltf(original, "models/modified");
```

**Notes:**
- Creates two files: .gltf (JSON) and .bin (binary data)
- GLTF format is human-readable for debugging
- Useful for exporting procedural geometry
- Can be re-imported into modeling tools
- Preserves mesh, material, and transform data

---
#### `SituationTakeScreenshot`
Takes a screenshot of the current frame and saves it to a file. Supports PNG and BMP formats based on file extension.

```c
SituationError SituationTakeScreenshot(const char *fileName);
```

**Parameters:**
- `fileName` - Output file path (extension determines format: .png or .bmp)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Take screenshot with F12
if (SituationIsKeyPressed(SIT_KEY_F12)) {
    // Generate timestamped filename
    time_t now = time(NULL);
    char filename[256];
    snprintf(filename, sizeof(filename), "screenshots/screenshot_%ld.png", now);
    
    if (SituationTakeScreenshot(filename) == SITUATION_SUCCESS) {
        printf("Screenshot saved: %s\n", filename);
    } else {
        printf("Failed to save screenshot\n");
    }
}

// Sequential screenshots
static int screenshot_count = 0;
if (SituationIsKeyPressed(SIT_KEY_F12)) {
    char filename[256];
    snprintf(filename, sizeof(filename), "screenshot_%04d.png", screenshot_count++);
    SituationTakeScreenshot(filename);
}

// BMP format
SituationTakeScreenshot("screenshot.bmp");

// PNG format (recommended)
SituationTakeScreenshot("screenshot.png");

// Create screenshots directory if needed
#ifdef _WIN32
    _mkdir("screenshots");
#else
    mkdir("screenshots", 0755);
#endif
SituationTakeScreenshot("screenshots/capture.png");
```

**Notes:**
- Captures the current frame buffer
- PNG format is recommended (smaller file size)
- BMP format is faster but larger
- Creates parent directories if they don't exist
- Overwrites existing files without warning

---
#### `SituationCmdBindUniformBuffer`
**[DEPRECATED]** Binds a Uniform Buffer Object (UBO) to a shader binding point. This function is deprecated in favor of the more explicit `SituationCmdBindDescriptorSet()`.

```c
SituationError SituationCmdBindUniformBuffer(SituationCommandBuffer cmd, uint32_t contract_id, SituationBuffer buffer);
```

**Migration Guide:**
```c
// OLD (Deprecated):
SituationCmdBindUniformBuffer(cmd, 0, camera_ubo);

// NEW (Recommended):
SituationCmdBindDescriptorSet(cmd, 0, camera_ubo);
```

**Why Deprecated:**
- `SituationCmdBindDescriptorSet()` provides a unified API for both UBOs and SSBOs
- More consistent with modern Vulkan terminology
- Better performance on Vulkan backend with persistent descriptor sets

**Notes:**
- Still functional but will be removed in a future version
- Update code to use `SituationCmdBindDescriptorSet()` for forward compatibility

---
#### `SituationCmdBindTexture`
**[DEPRECATED]** Binds a texture and sampler to a shader binding point. This function is deprecated in favor of `SituationCmdBindTextureSet()`.

```c
SituationError SituationCmdBindTexture(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture);
```

**Migration Guide:**
```c
// OLD (Deprecated):
SituationCmdBindTexture(cmd, 0, albedo_texture);

// NEW (Recommended):
SituationCmdBindTextureSet(cmd, 0, albedo_texture);
```

**Why Deprecated:**
- `SituationCmdBindTextureSet()` provides unified API for all texture types
- Consistent naming with descriptor set model
- Better performance with persistent descriptor sets on Vulkan

**Notes:**
- Still functional but will be removed in a future version
- Update code to use `SituationCmdBindTextureSet()` for forward compatibility

---
#### `SituationCmdBindComputeBuffer`
**[DEPRECATED]** Binds a buffer to a compute shader binding point. This function still works but the naming is being standardized.

```c
SituationError SituationCmdBindComputeBuffer(SituationCommandBuffer cmd, uint32_t binding, SituationBuffer buffer);
```

**Migration Guide:**
```c
// OLD (Deprecated):
SituationCmdBindComputeBuffer(cmd, 0, particle_buffer);

// NEW (Recommended):
SituationCmdBindComputeBuffer(cmd, 0, particle_buffer);  // Same function, just marked deprecated
```

**Why Deprecated:**
- Function is being consolidated into the main binding API
- May be merged with `SituationCmdBindDescriptorSet()` in future versions

**Notes:**
- Currently still the correct function to use for compute buffers
- Deprecation warning is for future API consolidation

---
#### `SituationLoadComputeShader`
**[DEPRECATED]** Loads a compute shader from a file. Use `SituationCreateComputePipeline()` instead for better API consistency.

```c
SituationError SituationLoadComputeShader(const char* cs_path, SituationShader* out_shader);
```

**Migration Guide:**
```c
// OLD (Deprecated):
SituationShader compute_shader;
SituationLoadComputeShader("shaders/particles.comp", &compute_shader);

// NEW (Recommended):
SituationComputePipeline compute_pipeline;
SituationCreateComputePipeline("shaders/particles.comp", 
    SITUATION_COMPUTE_LAYOUT_STANDARD, &compute_pipeline);
```

**Why Deprecated:**
- Compute shaders now use dedicated `SituationComputePipeline` type
- Clearer separation between graphics and compute pipelines
- Better type safety and API clarity

**Notes:**
- Returns `SituationShader` instead of `SituationComputePipeline`
- Update to `SituationCreateComputePipeline()` for new code

---
#### `SituationLoadComputeShaderFromMemory`
**[DEPRECATED]** Creates a compute shader from memory. Use `SituationCreateComputePipelineFromMemory()` instead.

```c
SituationError SituationLoadComputeShaderFromMemory(const char* cs_code, SituationShader* out_shader);
```

**Migration Guide:**
```c
// OLD (Deprecated):
const char* shader_code = "#version 450\n...";
SituationShader compute_shader;
SituationLoadComputeShaderFromMemory(shader_code, &compute_shader);

// NEW (Recommended):
const char* shader_code = "#version 450\n...";
SituationComputePipeline compute_pipeline;
SituationCreateComputePipelineFromMemory(shader_code,
    SITUATION_COMPUTE_LAYOUT_STANDARD, &compute_pipeline);
```

**Why Deprecated:**
- Compute shaders now use dedicated `SituationComputePipeline` type
- Consistent with file-based compute pipeline creation
- Better type safety

**Notes:**
- Returns `SituationShader` instead of `SituationComputePipeline`
- Update to `SituationCreateComputePipelineFromMemory()` for new code

---
#### `SituationMemoryBarrier`
**[DEPRECATED]** Inserts a coarse-grained memory barrier. Use `SituationCmdPipelineBarrier()` instead for fine-grained control.

```c
void SituationMemoryBarrier(SituationCommandBuffer cmd, uint32_t barrier_bits);
```

**Migration Guide:**

**Old Code:**
```c
// DEPRECATED - Coarse-grained barrier
SituationMemoryBarrier(cmd, SITUATION_BARRIER_COMPUTE_TO_GRAPHICS);
```

**New Code:**
```c
// RECOMMENDED - Fine-grained barrier
SituationPipelineBarrierInfo barrier = {
    .src_stage = SITUATION_PIPELINE_STAGE_COMPUTE_SHADER,
    .dst_stage = SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER,
    .src_access = SITUATION_ACCESS_SHADER_WRITE,
    .dst_access = SITUATION_ACCESS_SHADER_READ
};
SituationCmdPipelineBarrier(cmd, &barrier);
```

**Why Deprecated:**
- `SituationCmdPipelineBarrier()` provides fine-grained control over synchronization
- Better performance by specifying exact pipeline stages
- More explicit about memory access patterns
- Matches Vulkan's pipeline barrier model

**Common Migration Patterns:**
```c
// Compute-to-Graphics barrier
SituationPipelineBarrierInfo barrier = {
    .src_stage = SITUATION_PIPELINE_STAGE_COMPUTE_SHADER,
    .dst_stage = SITUATION_PIPELINE_STAGE_VERTEX_SHADER | SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER,
    .src_access = SITUATION_ACCESS_SHADER_WRITE,
    .dst_access = SITUATION_ACCESS_SHADER_READ
};
SituationCmdPipelineBarrier(cmd, &barrier);

// Graphics-to-Compute barrier
barrier.src_stage = SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER;
barrier.dst_stage = SITUATION_PIPELINE_STAGE_COMPUTE_SHADER;
barrier.src_access = SITUATION_ACCESS_COLOR_ATTACHMENT_WRITE;
barrier.dst_access = SITUATION_ACCESS_SHADER_READ;
SituationCmdPipelineBarrier(cmd, &barrier);
```

</details>
<details>
<summary><h3>Input Module</h3></summary>

**Overview:** The Input module provides a flexible interface for handling user input from keyboards, mice, and gamepads. It supports both state polling (e.g., `SituationIsKeyDown()`) for continuous actions and event-driven callbacks (e.g., `SituationSetKeyCallback()`) for discrete events.

### Key Codes
The library defines a comprehensive set of key codes for use with keyboard input functions.

| Key Code | Value | Description |
|---|---|---|
| `SIT_KEY_SPACE` | 32 | Spacebar |
| `SIT_KEY_APOSTROPHE` | 39 | ' |
| `SIT_KEY_COMMA` | 44 | , |
| `SIT_KEY_MINUS` | 45 | - |
| `SIT_KEY_PERIOD` | 46 | . |
| `SIT_KEY_SLASH` | 47 | / |
| `SIT_KEY_0` | 48 | 0 |
| `SIT_KEY_1` | 49 | 1 |
| `SIT_KEY_2` | 50 | 2 |
| `SIT_KEY_3` | 51 | 3 |
| `SIT_KEY_4` | 52 | 4 |
| `SIT_KEY_5` | 53 | 5 |
| `SIT_KEY_6` | 54 | 6 |
| `SIT_KEY_7` | 55 | 7 |
| `SIT_KEY_8` | 56 | 8 |
| `SIT_KEY_9` | 57 | 9 |
| `SIT_KEY_SEMICOLON` | 59 | ; |
| `SIT_KEY_EQUAL` | 61 | = |
| `SIT_KEY_A` | 65 | A |
| `SIT_KEY_B` | 66 | B |
| `SIT_KEY_C` | 67 | C |
| `SIT_KEY_D` | 68 | D |
| `SIT_KEY_E` | 69 | E |
| `SIT_KEY_F` | 70 | F |
| `SIT_KEY_G` | 71 | G |
| `SIT_KEY_H` | 72 | H |
| `SIT_KEY_I` | 73 | I |
| `SIT_KEY_J` | 74 | J |
| `SIT_KEY_K` | 75 | K |
| `SIT_KEY_L` | 76 | L |
| `SIT_KEY_M` | 77 | M |
| `SIT_KEY_N` | 78 | N |
| `SIT_KEY_O` | 79 | O |
| `SIT_KEY_P` | 80 | P |
| `SIT_KEY_Q` | 81 | Q |
| `SIT_KEY_R` | 82 | R |
| `SIT_KEY_S` | 83 | S |
| `SIT_KEY_T` | 84 | T |
| `SIT_KEY_U` | 85 | U |
| `SIT_KEY_V` | 86 | V |
| `SIT_KEY_W` | 87 | W |
| `SIT_KEY_X` | 88 | X |
| `SIT_KEY_Y` | 89 | Y |
| `SIT_KEY_Z` | 90 | Z |
| `SIT_KEY_LEFT_BRACKET` | 91 | [ |
| `SIT_KEY_BACKSLASH` | 92 | \ |
| `SIT_KEY_RIGHT_BRACKET` | 93 | ] |
| `SIT_KEY_GRAVE_ACCENT` | 96 | ` |
| `SIT_KEY_WORLD_1` | 161 | non-US #1 |
| `SIT_KEY_WORLD_2` | 162 | non-US #2 |
| `SIT_KEY_ESCAPE` | 256 | Escape |
| `SIT_KEY_ENTER` | 257 | Enter |
| `SIT_KEY_TAB` | 258 | Tab |
| `SIT_KEY_BACKSPACE` | 259 | Backspace |
| `SIT_KEY_INSERT` | 260 | Insert |
| `SIT_KEY_DELETE` | 261 | Delete |
| `SIT_KEY_RIGHT` | 262 | Right Arrow |
| `SIT_KEY_LEFT` | 263 | Left Arrow |
| `SIT_KEY_DOWN` | 264 | Down Arrow |
| `SIT_KEY_UP` | 265 | Up Arrow |
| `SIT_KEY_PAGE_UP` | 266 | Page Up |
| `SIT_KEY_PAGE_DOWN` | 267 | Page Down |
| `SIT_KEY_HOME` | 268 | Home |
| `SIT_KEY_END` | 269 | End |
| `SIT_KEY_CAPS_LOCK` | 280 | Caps Lock |
| `SIT_KEY_SCROLL_LOCK` | 281 | Scroll Lock |
| `SIT_KEY_NUM_LOCK` | 282 | Num Lock |
| `SIT_KEY_PRINT_SCREEN` | 283 | Print Screen |
| `SIT_KEY_PAUSE` | 284 | Pause |
| `SIT_KEY_F1` | 290 | F1 |
| `SIT_KEY_F2` | 291 | F2 |
| `SIT_KEY_F3` | 292 | F3 |
| `SIT_KEY_F4` | 293 | F4 |
| `SIT_KEY_F5` | 294 | F5 |
| `SIT_KEY_F6` | 295 | F6 |
| `SIT_KEY_F7` | 296 | F7 |
| `SIT_KEY_F8` | 297 | F8 |
| `SIT_KEY_F9` | 298 | F9 |
| `SIT_KEY_F10` | 299 | F10 |
| `SIT_KEY_F11` | 300 | F11 |
| `SIT_KEY_F12` | 301 | F12 |
| `SIT_KEY_F13` | 302 | F13 |
| `SIT_KEY_F14` | 303 | F14 |
| `SIT_KEY_F15` | 304 | F15 |
| `SIT_KEY_F16` | 305 | F16 |
| `SIT_KEY_F17` | 306 | F17 |
| `SIT_KEY_F18` | 307 | F18 |
| `SIT_KEY_F19` | 308 | F19 |
| `SIT_KEY_F20` | 309 | F20 |
| `SIT_KEY_F21` | 310 | F21 |
| `SIT_KEY_F22` | 311 | F22 |
| `SIT_KEY_F23` | 312 | F23 |
| `SIT_KEY_F24` | 313 | F24 |
| `SIT_KEY_F25` | 314 | F25 |
| `SIT_KEY_KP_0` | 320 | Keypad 0 |
| `SIT_KEY_KP_1` | 321 | Keypad 1 |
| `SIT_KEY_KP_2` | 322 | Keypad 2 |
| `SIT_KEY_KP_3` | 323 | Keypad 3 |
| `SIT_KEY_KP_4` | 324 | Keypad 4 |
| `SIT_KEY_KP_5` | 325 | Keypad 5 |
| `SIT_KEY_KP_6` | 326 | Keypad 6 |
| `SIT_KEY_KP_7` | 327 | Keypad 7 |
| `SIT_KEY_KP_8` | 328 | Keypad 8 |
| `SIT_KEY_KP_9` | 329 | Keypad 9 |
| `SIT_KEY_KP_DECIMAL` | 330 | Keypad . |
| `SIT_KEY_KP_DIVIDE` | 331 | Keypad / |
| `SIT_KEY_KP_MULTIPLY` | 332 | Keypad * |
| `SIT_KEY_KP_SUBTRACT` | 333 | Keypad - |
| `SIT_KEY_KP_ADD` | 334 | Keypad + |
| `SIT_KEY_KP_ENTER` | 335 | Keypad Enter |
| `SIT_KEY_KP_EQUAL` | 336 | Keypad = |
| `SIT_KEY_LEFT_SHIFT` | 340 | Left Shift |
| `SIT_KEY_LEFT_CONTROL` | 341 | Left Control |
| `SIT_KEY_LEFT_ALT` | 342 | Left Alt |
| `SIT_KEY_LEFT_SUPER` | 343 | Left Super/Windows/Command |
| `SIT_KEY_RIGHT_SHIFT` | 344 | Right Shift |
| `SIT_KEY_RIGHT_CONTROL` | 345 | Right Control |
| `SIT_KEY_RIGHT_ALT` | 346 | Right Alt |
| `SIT_KEY_RIGHT_SUPER` | 347 | Right Super/Windows/Command |
| `SIT_KEY_MENU` | 348 | Menu |

### Modifier Bitmasks
These bitmasks are used in callback functions to check for modifier key states.

| Bitmask | Value |
|---|---|
| `SIT_MOD_SHIFT` | 0x0001 |
| `SIT_MOD_CONTROL` | 0x0002 |
| `SIT_MOD_ALT` | 0x0004 |
| `SIT_MOD_SUPER` | 0x0008 |
| `SIT_MOD_CAPS_LOCK` | 0x0010 |
| `SIT_MOD_NUM_LOCK` | 0x0020 |

### Callbacks
The input module allows you to register callback functions to be notified of input events as they happen, as an alternative to polling for state each frame.

#### `SituationKeyCallback`
`typedef void (*SituationKeyCallback)(int key, int scancode, int action, int mods, void* user_data);`
-   `key`: The keyboard key that was pressed or released (e.g., `SIT_KEY_A`).
-   `scancode`: The system-specific scancode of the key.
-   `action`: The key action (`SIT_PRESS`, `SIT_RELEASE`, or `SIT_REPEAT`).
-   `mods`: A bitmask of modifier keys that were held down (`SIT_MOD_SHIFT`, `SIT_MOD_CONTROL`, etc.).
-   `user_data`: The custom user data pointer you provided when setting the callback.

---
#### `SituationMouseButtonCallback`
`typedef void (*SituationMouseButtonCallback)(int button, int action, int mods, void* user_data);`
-   `button`: The mouse button that was pressed or released (e.g., `SIT_MOUSE_BUTTON_LEFT`).
-   `action`: The button action (`SIT_PRESS` or `SIT_RELEASE`).
-   `mods`: A bitmask of modifier keys.
-   `user_data`: Custom user data.

---
#### `SituationCursorPosCallback`
`typedef void (*SituationCursorPosCallback)(double xpos, double ypos, void* user_data);`
-   `xpos`, `ypos`: The new cursor position in screen coordinates.
-   `user_data`: Custom user data.

---
#### `SituationScrollCallback`
`typedef void (*SituationScrollCallback)(double xoffset, double yoffset, void* user_data);`
-   `xoffset`, `yoffset`: The scroll offset.
-   `user_data`: Custom user data.

#### Functions
### Functions

#### Keyboard Input
---
#### `SituationIsKeyDown` / `SituationIsKeyUp`
Checks if a key is currently being held down or is up. This checks the *state* of the key and will return `true` for every frame the key is held. It is ideal for continuous actions like character movement.
```c
bool SituationIsKeyDown(int key);
bool SituationIsKeyUp(int key);
```
**Usage Example:**
```c
// For continuous movement, check the key state every frame.
if (SituationIsKeyDown(SIT_KEY_W)) {
    player.y -= PLAYER_SPEED * SituationGetFrameTime();
}
if (SituationIsKeyDown(SIT_KEY_S)) {
    player.y += PLAYER_SPEED * SituationGetFrameTime();
}
```
---
#### `SituationIsKeyPressed` / `SituationIsKeyReleased`
Checks if a key was just pressed down or released in the current frame. This is a single-trigger *event* and will only return `true` for the exact frame the action occurred. It is ideal for discrete actions like jumping or opening a menu.
```c
bool SituationIsKeyPressed(int key);
bool SituationIsKeyReleased(int key);
```
**Usage Example:**
```c
// For a discrete action like jumping, use the key pressed event.
if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
    player.velocity_y = JUMP_FORCE;
}

// Toggling a menu is another good use case for a single-trigger event.
if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
    g_is_menu_open = !g_is_menu_open;
}
```

---
#### `SituationGetKeyPressed`
Gets the last key pressed.
```c
int SituationGetKeyPressed(void);
```
**Usage Example:**
```c
int last_key = SituationGetKeyPressed();
if (last_key > 0) {
    // A key was pressed this frame, you can use it for text input or debug commands.
    printf("Key pressed: %c\n", (char)last_key);
}
```

---
#### `SituationGetKeyPressedEx`
Gets the next key from the press queue along with its platform-specific scancode. This is useful when you need both the logical key and the physical key location.
```c
int SituationGetKeyPressedEx(int* out_scancode);
```
**Usage Example:**
```c
int scancode = 0;
int key = SituationGetKeyPressedEx(&scancode);
if (key > 0) {
    printf("Key %d pressed (scancode: %d)\n", key, scancode);
}
```

---
#### `SituationPeekKeyPressed`
Peeks at the next key in the press queue without consuming it. This allows you to check what key is next without removing it from the queue.
```c
int SituationPeekKeyPressed(void);
```
**Usage Example:**
```c
int next_key = SituationPeekKeyPressed();
if (next_key == SIT_KEY_ENTER) {
    // Prepare to handle Enter key, but don't consume it yet
    printf("Enter key is next in queue\n");
}
```

---
#### `SituationPeekKeyPressedEx`
Peeks at the next key and its scancode without consuming them from the queue.
```c
int SituationPeekKeyPressedEx(int* out_scancode);
```
**Usage Example:**
```c
int scancode = 0;
int next_key = SituationPeekKeyPressedEx(&scancode);
if (next_key > 0) {
    printf("Next key: %d (scancode: %d)\n", next_key, scancode);
}
```

---
#### `SituationGetCharPressed`
Gets the next Unicode character from the text input queue. This is ideal for text entry fields as it handles character composition and international keyboards correctly.
```c
unsigned int SituationGetCharPressed(void);
```
**Usage Example:**
```c
// In a text input handler
unsigned int character = SituationGetCharPressed();
if (character > 0) {
    // Append character to text buffer (handle UTF-8 encoding as needed)
    AppendCharToTextBuffer(character);
}
```

---
#### `SituationIsLockKeyPressed`
Checks if a lock key (Caps Lock, Num Lock) is currently active/toggled on.
```c
bool SituationIsLockKeyPressed(int lock_key_mod);
```
**Usage Example:**
```c
// Check if Caps Lock is on
if (SituationIsLockKeyPressed(SIT_MOD_CAPS_LOCK)) {
    printf("Caps Lock is ON\n");
}

// Check if Num Lock is on
if (SituationIsLockKeyPressed(SIT_MOD_NUM_LOCK)) {
    printf("Num Lock is ON\n");
}
```

---
#### `SituationIsScrollLockOn`
Checks if Scroll Lock is currently toggled on.
```c
bool SituationIsScrollLockOn(void);
```
**Usage Example:**
```c
if (SituationIsScrollLockOn()) {
    printf("Scroll Lock is active\n");
}
```

---
#### `SituationIsModifierPressed`
Checks if a modifier key (Shift, Ctrl, Alt, Super) is currently pressed.
```c
bool SituationIsModifierPressed(int modifier);
```
**Usage Example:**
```c
// Check for Ctrl+S (Save)
if (SituationIsModifierPressed(SIT_MOD_CONTROL) && SituationIsKeyPressed(SIT_KEY_S)) {
    SaveFile();
}

// Check for Shift+Click
if (SituationIsModifierPressed(SIT_MOD_SHIFT) && SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) {
    SelectMultiple();
}
```

---
#### `SituationIsKeyUp`
Checks if a key is currently up (not pressed). This is the opposite of `SituationIsKeyDown`.
```c
bool SituationIsKeyUp(int key);
```
**Usage Example:**
```c
if (SituationIsKeyUp(SIT_KEY_SPACE)) {
    // Space bar is not being pressed
}
```

---
#### `SituationIsKeyReleased`
Checks if a key was just released this frame (single-frame event).
```c
bool SituationIsKeyReleased(int key);
```
**Usage Example:**
```c
if (SituationIsKeyReleased(SIT_KEY_SPACE)) {
    // Player just released the jump button
    EndCharging();
}
```

---
#### `SituationGetKeyScancode`
Gets the platform-specific scancode for a logical key code.
```c
int SituationGetKeyScancode(int key);
```
**Usage Example:**
```c
int scancode = SituationGetKeyScancode(SIT_KEY_W);
printf("W key scancode: %d\n", scancode);
```

---
#### `SituationGetCharFromScancode`
Converts a scancode to its character representation.
```c
int SituationGetCharFromScancode(int scancode);
```
**Usage Example:**
```c
int scancode = SituationGetKeyScancode(SIT_KEY_A);
int character = SituationGetCharFromScancode(scancode);
printf("Character: %c\n", character);
```

---
#### `SituationIsScancodeDown`
Checks if a key is pressed using its scancode instead of logical key code.
```c
bool SituationIsScancodeDown(int scancode);
```
**Usage Example:**
```c
// Useful for handling keyboard layouts that differ from QWERTY
int w_scancode = SituationGetKeyScancode(SIT_KEY_W);
if (SituationIsScancodeDown(w_scancode)) {
    MoveForward();
}
```

---
#### `SituationSetKeyCallback`
Sets a callback function for all keyboard key events.
```c
void SituationSetKeyCallback(SituationKeyCallback callback, void* user_data);
```
**Usage Example:**
```c
void my_key_logger(int key, int scancode, int action, int mods, void* user_data) {
    if (action == SIT_PRESS) {
        printf("Key pressed: %d\n", key);
    }
}
SituationSetKeyCallback(my_key_logger, NULL);
```

---
#### `SituationSetMouseButtonCallback` / `SituationSetCursorPosCallback` / `SituationSetScrollCallback`
Sets callback functions for mouse button, cursor movement, and scroll wheel events.
```c
void SituationSetMouseButtonCallback(SituationMouseButtonCallback callback, void* user_data);
void SituationSetCursorPosCallback(SituationCursorPosCallback callback, void* user_data);
void SituationSetScrollCallback(SituationScrollCallback callback, void* user_data);
```

---
#### `SituationSetCharCallback`
Sets a callback for Unicode character input, which is useful for text entry fields. The callback receives individual Unicode codepoints as they're typed.

```c
void SituationSetCharCallback(SituationCharCallback callback, void* user_data);
```

**Parameters:**
- `callback` - Function to call when character is typed (or NULL to remove)
- `user_data` - User data passed to callback

**Callback Signature:**
```c
typedef void (*SituationCharCallback)(unsigned int codepoint, void* user_data);
```

**Usage Example:**
```c
// Text input field
typedef struct {
    char buffer[256];
    int length;
} TextInput;

void OnCharInput(unsigned int codepoint, void* user_data) {
    TextInput* input = (TextInput*)user_data;
    
    // Convert codepoint to UTF-8 and append
    if (input->length < 255) {
        // Simple ASCII handling
        if (codepoint < 128) {
            input->buffer[input->length++] = (char)codepoint;
            input->buffer[input->length] = '\0';
        }
    }
}

// Set up text input
TextInput input = {0};
SituationSetCharCallback(OnCharInput, &input);

// In main loop
while (!SituationShouldClose()) {
    SituationPollInputEvents();  // Triggers callback
    
    // Handle backspace separately
    if (SituationIsKeyPressed(SIT_KEY_BACKSPACE) && input.length > 0) {
        input.buffer[--input.length] = '\0';
    }
    
    DrawTextInput(input.buffer);
}

// Cleanup
SituationSetCharCallback(NULL, NULL);
```

**Notes:**
- Receives Unicode codepoints, not raw key codes
- Handles keyboard layout automatically
- Use for text input, not game controls
- Call with NULL to remove callback

---
#### `SituationSetDropCallback`
Sets a callback that is fired when files are dragged and dropped onto the window. Useful for loading assets or opening files.

```c
void SituationSetDropCallback(SituationDropCallback callback, void* user_data);
```

**Parameters:**
- `callback` - Function to call when files are dropped (or NULL to remove)
- `user_data` - User data passed to callback

**Callback Signature:**
```c
typedef void (*SituationDropCallback)(int count, const char** paths, void* user_data);
```

**Usage Example:**
```c
// Handle dropped files
void OnFileDrop(int count, const char** paths, void* user_data) {
    printf("Dropped %d file(s):\n", count);
    
    for (int i = 0; i < count; i++) {
        printf("  - %s\n", paths[i]);
        
        // Load based on extension
        if (strstr(paths[i], ".png") || strstr(paths[i], ".jpg")) {
            LoadTexture(paths[i]);
        } else if (strstr(paths[i], ".wav") || strstr(paths[i], ".ogg")) {
            LoadSound(paths[i]);
        } else if (strstr(paths[i], ".gltf")) {
            LoadModel(paths[i]);
        }
    }
}

// Set up drop handler
SituationSetDropCallback(OnFileDrop, NULL);

// Alternative: Use polling instead of callback
if (SituationIsFileDropped()) {
    int count;
    char** paths = SituationLoadDroppedFiles(&count);
    // Process files...
    SituationUnloadDroppedFiles(paths, count);
}
```

**Notes:**
- Paths are absolute file paths
- Multiple files can be dropped at once
- Paths are valid only during callback
- Alternative: use `SituationIsFileDropped()` polling

---
#### `SituationSetFileDropCallback`
Sets a callback for file drop events. This is an alias for `SituationSetDropCallback` with the same functionality.

```c
void SituationSetFileDropCallback(SituationFileDropCallback callback, void* user_data);
```

**Parameters:**
- `callback` - Function to call when files are dropped
- `user_data` - User data passed to callback

**Usage Example:**
```c
// Same as SituationSetDropCallback
void OnFileDropped(int count, const char** paths, void* user_data) {
    for (int i = 0; i < count; i++) {
        printf("File dropped: %s\n", paths[i]);
    }
}

SituationSetFileDropCallback(OnFileDropped, NULL);
```

**Notes:**
- Identical to `SituationSetDropCallback`
- Use whichever name is clearer for your use case

---
#### `SituationSetFocusCallback`
Sets a callback for window focus events. Called when the window gains or loses focus.

```c
void SituationSetFocusCallback(SituationFocusCallback callback, void* user_data);
```

**Parameters:**
- `callback` - Function to call on focus change (or NULL to remove)
- `user_data` - User data passed to callback

**Callback Signature:**
```c
typedef void (*SituationFocusCallback)(bool focused, void* user_data);
```

**Usage Example:**
```c
// Handle focus changes
void OnFocusChanged(bool focused, void* user_data) {
    if (focused) {
        printf("Window gained focus\n");
        ResumeGame();
        SituationSetAudioMasterVolume(1.0f);
    } else {
        printf("Window lost focus\n");
        PauseGame();
        SituationSetAudioMasterVolume(0.0f);
    }
}

// Set up focus callback
SituationSetFocusCallback(OnFocusChanged, NULL);

// Alternative: Poll focus state
if (!SituationHasWindowFocus()) {
    PauseGame();
}
```

**Notes:**
- Called when window gains/loses focus
- Use for auto-pause functionality
- Alternative: poll with `SituationHasWindowFocus()`

---
#### `SituationSetResizeCallback`
Sets a callback function for window framebuffer resize events. Called when the window is resized.

```c
void SituationSetResizeCallback(SituationResizeCallback callback, void* user_data);
```

**Parameters:**
- `callback` - Function to call on resize (or NULL to remove)
- `user_data` - User data passed to callback

**Callback Signature:**
```c
typedef void (*SituationResizeCallback)(int width, int height, void* user_data);
```

**Usage Example:**
```c
// Handle window resize
void OnWindowResize(int width, int height, void* user_data) {
    printf("Window resized to %dx%d\n", width, height);
    
    // Update camera aspect ratio
    Camera* camera = (Camera*)user_data;
    camera->aspect = (float)width / (float)height;
    UpdateProjectionMatrix(camera);
    
    // Recreate framebuffers
    RecreateFramebuffers(width, height);
}

// Set up resize callback
Camera camera = {0};
SituationSetResizeCallback(OnWindowResize, &camera);

// Alternative: Poll resize event
if (SituationIsWindowResized()) {
    int width = SituationGetRenderWidth();
    int height = SituationGetRenderHeight();
    RecreateFramebuffers(width, height);
}
```

**Notes:**
- Called when framebuffer size changes
- Width/height are in pixels, not screen coordinates
- Use for recreating resolution-dependent resources
- Alternative: poll with `SituationIsWindowResized()`

---
#### `SituationSetCursor`
Sets the mouse cursor to a standard shape (arrow, hand, crosshair, etc.).

```c
void SituationSetCursor(SituationCursorShape shape);
```

**Parameters:**
- `shape` - Cursor shape to display

**Cursor Shapes:**
- `SITUATION_CURSOR_ARROW` - Standard arrow pointer
- `SITUATION_CURSOR_IBEAM` - Text input I-beam
- `SITUATION_CURSOR_CROSSHAIR` - Crosshair for aiming
- `SITUATION_CURSOR_HAND` - Pointing hand for links
- `SITUATION_CURSOR_HRESIZE` - Horizontal resize arrows
- `SITUATION_CURSOR_VRESIZE` - Vertical resize arrows

**Usage Example:**
```c
// Change cursor based on UI element
if (IsHoveringButton()) {
    SituationSetCursor(SITUATION_CURSOR_HAND);
} else if (IsHoveringTextInput()) {
    SituationSetCursor(SITUATION_CURSOR_IBEAM);
} else if (IsHoveringResizeHandle()) {
    SituationSetCursor(SITUATION_CURSOR_HRESIZE);
} else {
    SituationSetCursor(SITUATION_CURSOR_ARROW);
}

// Crosshair for aiming mode
if (IsAiming()) {
    SituationSetCursor(SITUATION_CURSOR_CROSSHAIR);
}
```

**Notes:**
- Changes cursor appearance immediately
- Use for UI feedback
- Cursor is hidden if disabled with `SituationDisableCursor()`

---
#### Clipboard
---
#### `SituationGetClipboardText`
Gets UTF-8 encoded text from the system clipboard. The returned string is heap-allocated and must be freed by the caller using `SituationFreeString`.
```c
SituationError SituationGetClipboardText(const char** out_text);
```
**Usage Example:**
```c
// In an input handler for Ctrl+V
if (SituationIsKeyDown(SIT_KEY_LEFT_CONTROL) && SituationIsKeyPressed(SIT_KEY_V)) {
    const char* clipboard_text = NULL;
    if (SituationGetClipboardText(&clipboard_text) == SITUATION_SUCCESS) {
        // Paste text into an input field.
        SituationFreeString((char*)clipboard_text);
    }
}
```
---
#### `SituationSetClipboardText`
Sets the system clipboard to the provided UTF-8 encoded text.
```c
SituationError SituationSetClipboardText(const char* text);
```
**Usage Example:**
```c
// In an input handler for Ctrl+C
if (SituationIsKeyDown(SIT_KEY_LEFT_CONTROL) && SituationIsKeyPressed(SIT_KEY_C)) {
    // Copy selected text to the clipboard.
    SituationSetClipboardText(selected_text);
}
```
---
#### Mouse Input
---
#### `SituationGetMousePosition` / `SituationGetMouseDelta`
Gets the mouse cursor position in screen coordinates, or the mouse movement since the last frame. `SituationGetMouseDelta` is particularly useful for implementing camera controls when the cursor is disabled.
```c
vec2 SituationGetMousePosition(void);
vec2 SituationGetMouseDelta(void);
```
**Usage Example:**
```c
// For a 3D camera, use the mouse delta to control pitch and yaw.
if (IsCursorDisabled()) { // Assuming you have a check for this state
    vec2 mouse_delta = SituationGetMouseDelta();
    camera.yaw   += mouse_delta[0] * MOUSE_SENSITIVITY;
    camera.pitch -= mouse_delta[1] * MOUSE_SENSITIVITY;
}
```
---
#### `SituationIsMouseButtonDown`
Checks if a mouse button is currently being held down. This is a *state* check and is suitable for continuous actions like dragging or aiming.
```c
bool SituationIsMouseButtonDown(int button);
```
**Usage Example:**
```c
// Useful for continuous actions like aiming down sights.
if (SituationIsMouseButtonDown(SIT_MOUSE_BUTTON_RIGHT)) {
    // Zoom in with weapon sights.
}
```

---
#### `SituationIsMouseButtonPressed`
Checks if a mouse button was just pressed down in the current frame. This is a single-trigger *event*, ideal for discrete actions like clicking a button or firing a weapon.
```c
bool SituationIsMouseButtonPressed(int button);
```
**Usage Example:**
```c
// Ideal for discrete actions like firing a weapon.
if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) {
    FireProjectile();
}
```

---
#### `SituationGetMouseButtonPressed`
Gets the mouse button that was pressed this frame.
```c
int SituationGetMouseButtonPressed(void);
```
**Usage Example:**
```c
// Useful for UI interactions where you need to know which button was clicked.
int clicked_button = SituationGetMouseButtonPressed();
if (clicked_button == SIT_MOUSE_BUTTON_LEFT) {
    // Handle left click on a UI element.
} else if (clicked_button == SIT_MOUSE_BUTTON_RIGHT) {
    // Open a context menu.
}
```

---
#### `SituationIsMouseButtonReleased`
Checks if a mouse button was released this frame. This is a single-frame event, ideal for detecting button releases.

```c
bool SituationIsMouseButtonReleased(int button);
```

**Parameters:**
- `button` - Mouse button to check (e.g., `SIT_MOUSE_BUTTON_LEFT`, `SIT_MOUSE_BUTTON_RIGHT`)

**Returns:** `true` if button was released this frame, `false` otherwise

**Usage Example:**
```c
// Detect click release
if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) {
    StartDragging();
}
if (SituationIsMouseButtonReleased(SIT_MOUSE_BUTTON_LEFT)) {
    StopDragging();
}

// Charge-up mechanic
static float charge_time = 0.0f;
if (SituationIsMouseButtonDown(SIT_MOUSE_BUTTON_LEFT)) {
    charge_time += SituationGetFrameTime();
    ShowChargeIndicator(charge_time);
}
if (SituationIsMouseButtonReleased(SIT_MOUSE_BUTTON_LEFT)) {
    FireWeapon(charge_time);
    charge_time = 0.0f;
}

// Context menu
if (SituationIsMouseButtonReleased(SIT_MOUSE_BUTTON_RIGHT)) {
    vec2 mouse_pos = SituationGetMousePosition();
    ShowContextMenu(mouse_pos[0], mouse_pos[1]);
}

// Drag and drop
static bool dragging = false;
static vec2 drag_start;

if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) {
    dragging = true;
    vec2 pos = SituationGetMousePosition();
    drag_start[0] = pos[0];
    drag_start[1] = pos[1];
}

if (SituationIsMouseButtonReleased(SIT_MOUSE_BUTTON_LEFT)) {
    if (dragging) {
        vec2 pos = SituationGetMousePosition();
        OnDragComplete(drag_start, pos);
        dragging = false;
    }
}
```

**Notes:**
- Single-frame event, not a continuous state
- Use for detecting button releases
- Pair with `SituationIsMouseButtonPressed()` for press/release logic
- Common buttons: `SIT_MOUSE_BUTTON_LEFT`, `SIT_MOUSE_BUTTON_RIGHT`, `SIT_MOUSE_BUTTON_MIDDLE`

---
#### `SituationSetMousePosition`
Sets the mouse cursor position within the window.
```c
void SituationSetMousePosition(Vector2 pos);
```
**Usage Example:**
```c
// Center the mouse cursor in the window
Vector2 center = {
    SituationGetScreenWidth() / 2.0f,
    SituationGetScreenHeight() / 2.0f
};
SituationSetMousePosition(center);
```

---
#### `SituationSetMouseOffset`
Sets a software offset for the mouse position. This is useful for implementing custom coordinate systems or UI scaling.
```c
void SituationSetMouseOffset(Vector2 offset);
```
**Usage Example:**
```c
// Offset mouse coordinates to account for a UI panel
Vector2 ui_offset = {200.0f, 50.0f};
SituationSetMouseOffset(ui_offset);
```

---
#### `SituationSetMouseScale`
Sets a software scale for the mouse position and delta. This is useful when rendering at a different resolution than the window size.
```c
void SituationSetMouseScale(Vector2 scale);
```
**Usage Example:**
```c
// Scale mouse coordinates when rendering at 1920x1080 in a 1280x720 window
Vector2 scale = {1920.0f / 1280.0f, 1080.0f / 720.0f};
SituationSetMouseScale(scale);
```

---
#### `SituationGetMouseWheelMove`
Gets the vertical mouse wheel movement for the current frame. Positive values indicate scrolling up/away from the user, negative values indicate scrolling down/towards the user.
```c
float SituationGetMouseWheelMove(void);
```
**Usage Example:**
```c
// Zoom camera based on mouse wheel
float wheel = SituationGetMouseWheelMove();
if (wheel != 0.0f) {
    camera_zoom += wheel * ZOOM_SPEED;
}
```

---
#### `SituationGetMouseWheelMoveV`
Gets both vertical and horizontal mouse wheel movement as a Vector2. The x component is horizontal scroll, y component is vertical scroll.
```c
Vector2 SituationGetMouseWheelMoveV(void);
```
**Usage Example:**
```c
// Handle both vertical and horizontal scrolling
Vector2 wheel = SituationGetMouseWheelMoveV();
scroll_offset_x += wheel.x * SCROLL_SPEED;
scroll_offset_y += wheel.y * SCROLL_SPEED;
```

---
#### `SituationGetMouseDelta`
Gets the mouse movement since the last frame. Useful for camera controls.
```c
Vector2 SituationGetMouseDelta(void);
```
**Usage Example:**
```c
// First-person camera control
Vector2 delta = SituationGetMouseDelta();
camera_yaw += delta.x * MOUSE_SENSITIVITY;
camera_pitch -= delta.y * MOUSE_SENSITIVITY;
```

---
#### Gamepad Input
---
#### `SituationIsJoystickPresent`
Checks if a joystick or gamepad is connected at the given joystick ID (0-15).
```c
bool SituationIsJoystickPresent(int jid);
```
**Usage Example:**
```c
// Check for a joystick at the first slot.
if (SituationIsJoystickPresent(0)) {
    printf("A joystick/gamepad is connected at JID 0.\n");
}
```

---
#### `SituationIsGamepad`
Checks if the joystick at the given ID has a standard gamepad mapping, making it compatible with the `SIT_GAMEPAD_*` enums.
```c
bool SituationIsGamepad(int jid);
```
**Usage Example:**
```c
// Before using gamepad-specific functions, check if the device has a standard mapping.
if (SituationIsJoystickPresent(0) && SituationIsGamepad(0)) {
    // Now it's safe to use functions like SituationIsGamepadButtonPressed.
}
```

---
#### `SituationGetJoystickName`
Gets the implementation-defined name of a joystick (e.g., "Xbox Controller").
```c
const char* SituationGetJoystickName(int jid);
```
**Usage Example:**
```c
#define GAMEPAD_ID 0
if (SituationIsJoystickPresent(GAMEPAD_ID) && SituationIsGamepad(GAMEPAD_ID)) {
    printf("Gamepad '%s' is ready.\n", SituationGetJoystickName(GAMEPAD_ID));
}
```
---
#### `SituationIsGamepadButtonDown` / `SituationIsGamepadButtonPressed`
Checks if a gamepad button is held down (state) or was just pressed (event).
```c
bool SituationIsGamepadButtonDown(int jid, int button);
bool SituationIsGamepadButtonPressed(int jid, int button);
```
**Usage Example:**
```c
if (SituationIsGamepadButtonPressed(GAMEPAD_ID, SIT_GAMEPAD_BUTTON_A)) {
    // Jump
}
```

---
#### `SituationGetGamepadButtonPressed`
Gets the next gamepad button from the press queue. This is useful for UI navigation where you don't care which specific gamepad was used.
```c
int SituationGetGamepadButtonPressed(void);
```
**Usage Example:**
```c
// In a menu system, accept input from any connected gamepad
int button = SituationGetGamepadButtonPressed();
if (button == SIT_GAMEPAD_BUTTON_A) {
    ConfirmSelection();
} else if (button == SIT_GAMEPAD_BUTTON_B) {
    CancelSelection();
}
```

---
#### `SituationIsGamepadButtonReleased`
Checks if a gamepad button was released this frame (a single-trigger event).
```c
bool SituationIsGamepadButtonReleased(int jid, int button);
```
**Usage Example:**
```c
// Detect when the player releases the trigger button
if (SituationIsGamepadButtonReleased(GAMEPAD_ID, SIT_GAMEPAD_BUTTON_RIGHT_TRIGGER)) {
    ReleaseCharge();
}
```

---
#### `SituationGetGamepadAxisValue`
Gets the value of a gamepad axis with deadzone applied. Returns a value between -1.0 and 1.0.
```c
float SituationGetGamepadAxisValue(int jid, int axis);
```
**Usage Example:**
```c
// Use left stick for character movement
float left_x = SituationGetGamepadAxisValue(GAMEPAD_ID, SIT_GAMEPAD_AXIS_LEFT_X);
float left_y = SituationGetGamepadAxisValue(GAMEPAD_ID, SIT_GAMEPAD_AXIS_LEFT_Y);

player_velocity.x = left_x * MOVE_SPEED;
player_velocity.y = left_y * MOVE_SPEED;

// Use right stick for camera control
float right_x = SituationGetGamepadAxisValue(GAMEPAD_ID, SIT_GAMEPAD_AXIS_RIGHT_X);
float right_y = SituationGetGamepadAxisValue(GAMEPAD_ID, SIT_GAMEPAD_AXIS_RIGHT_Y);

camera_yaw += right_x * CAMERA_SENSITIVITY * deltaTime;
camera_pitch += right_y * CAMERA_SENSITIVITY * deltaTime;
```

---
#### `SituationGetGamepadAxisCount`
Gets the number of axes available on a gamepad.
```c
int SituationGetGamepadAxisCount(int jid);
```
**Usage Example:**
```c
int axis_count = SituationGetGamepadAxisCount(GAMEPAD_ID);
printf("Gamepad has %d axes\n", axis_count);
```

---
#### `SituationSetGamepadMappings`
Loads a new set of gamepad mappings from a string. This allows you to add support for custom or non-standard controllers.
```c
int SituationSetGamepadMappings(const char* mappings);
```
**Usage Example:**
```c
// Load custom gamepad mappings from a file
const char* custom_mappings = LoadTextFile("gamecontrollerdb.txt");
if (SituationSetGamepadMappings(custom_mappings)) {
    printf("Custom gamepad mappings loaded successfully\n");
}
```

---
#### `SituationSetGamepadVibration`
Sets gamepad vibration/rumble intensity. Note: This is currently Windows-only.
```c
bool SituationSetGamepadVibration(int jid, float left_motor, float right_motor);
```
**Usage Example:**
```c
// Trigger a short rumble when the player takes damage
SituationSetGamepadVibration(GAMEPAD_ID, 0.8f, 0.5f);

// Stop vibration after a delay
// (In your update loop after the delay)
SituationSetGamepadVibration(GAMEPAD_ID, 0.0f, 0.0f);
```

---
#### `SituationSetJoystickCallback`
Sets a callback for joystick connection and disconnection events.
```c
void SituationSetJoystickCallback(SituationJoystickCallback callback, void* user_data);
```
**Usage Example:**
```c
void on_joystick_event(int jid, int event, void* user_data) {
    if (event == GLFW_CONNECTED) {
        printf("Joystick %d connected: %s\n", jid, SituationGetJoystickName(jid));
    } else if (event == GLFW_DISCONNECTED) {
        printf("Joystick %d disconnected\n", jid);
    }
}

SituationSetJoystickCallback(on_joystick_event, NULL);
```

---

#### `SituationSetMousePosition`
Sets the mouse cursor position within the window.
```c
SITAPI void SituationSetMousePosition(Vector2 pos);
```
**Usage Example:**
```c
// Center the mouse cursor in a 1280x720 window
Vector2 center = { .x = 1280 / 2.0f, .y = 720 / 2.0f };
SituationSetMousePosition(center);
```
</details>
<details>
<summary><h3>Audio Module</h3></summary>

**Overview:** The Audio module offers a full-featured audio engine capable of loading sounds (`SituationLoadSoundFromFile`) for low-latency playback and streaming longer tracks (`SituationLoadSoundFromStream`) to conserve memory. It supports device management, playback control (volume, pan, pitch), a built-in effects chain (filters, reverb), and custom real-time audio processors.

### Structs and Enums

#### `SituationAudioDeviceInfo`
Contains information about a single audio playback device available on the system.
```c
typedef struct SituationAudioDeviceInfo {
    int internal_id;
    char name[SITUATION_MAX_DEVICE_NAME_LEN];
    bool is_default;
    int min_channels, max_channels;
    int min_sample_rate, max_sample_rate;
} SituationAudioDeviceInfo;
```
-   `internal_id`: The ID used to select this device with `SituationSetAudioDevice()`.
-   `name`: The human-readable name of the device.
-   `is_default`: `true` if this is the operating system's default audio device.
-   `min_channels`, `max_channels`: The minimum and maximum number of channels supported by the device.
-   `min_sample_rate`, `max_sample_rate`: The minimum and maximum sample rates supported by the device.

---
#### `SituationAudioFormat`
Describes the format of audio data, used when initializing the audio device or loading sounds from custom streams.
```c
typedef struct SituationAudioFormat {
    int channels;
    int sample_rate;
    int bit_depth;
} SituationAudioFormat;
```
-   `channels`: Number of audio channels (e.g., 1 for mono, 2 for stereo).
-   `sample_rate`: Number of samples per second (e.g., 44100 Hz).
-   `bit_depth`: Number of bits per sample (e.g., 16-bit).

---
#### `SituationSound`
An opaque handle to a sound resource. This handle encapsulates all the necessary internal state for a sound, whether it's fully loaded into memory or streamed from a source. It is initialized by `SituationLoadSoundFromFile()` or `SituationLoadSoundFromStream()` and must be cleaned up with `SituationUnloadSound()`.
```c
typedef struct SituationSound {
    uint64_t id; // Internal unique ID
    // Internal data is not exposed to the user
} SituationSound;
```
- **Creation:** `SituationLoadSoundFromFile()`, `SituationLoadSoundFromStream()`
- **Usage:** `SituationPlayLoadedSound()`, `SituationSetSoundVolume()`
- **Destruction:** `SituationUnloadSound()`

---
#### `SituationFilterType`
Specifies the type of filter to apply to a sound.
| Type | Description |
|---|---|
| `SIT_FILTER_NONE` | No filter is applied. |
| `SIT_FILTER_LOW_PASS` | Allows low frequencies to pass through. |
| `SIT_FILTER_HIGH_PASS` | Allows high frequencies to pass through. |

#### Functions
### Functions

#### Audio Device Management
---
#### `SituationIsAudioDeviceReady`
Checks if the audio device has been successfully initialized via `SituationInit`. Always check this before attempting audio operations.

```c
bool SituationIsAudioDeviceReady(void);
```

**Returns:** `true` if audio device is initialized, `false` otherwise

**Usage Example:**
```c
// Check before playing audio
if (SituationIsAudioDeviceReady()) {
    SituationPlayLoadedSound(&sound);
} else {
    printf("Error: Audio device not initialized\n");
}

// Graceful degradation
if (!SituationIsAudioDeviceReady()) {
    printf("Running in silent mode (no audio device)\n");
    // Continue without audio
}

// Verify initialization
if (!SituationIsAudioDeviceReady()) {
    fprintf(stderr, "Failed to initialize audio device\n");
    // Check error message
    const char* error = SituationGetLastErrorMsg();
    fprintf(stderr, "Audio error: %s\n", error);
}
```

**Notes:**
- Returns false if audio init failed or was disabled
- Check before any audio operations
- Audio may fail on headless systems

---
#### `SituationIsAudioDevicePlaying`
Checks if the audio device is currently playing any sounds. Returns false if all sounds are stopped or paused.

```c
bool SituationIsAudioDevicePlaying(void);
```

**Returns:** `true` if any sound is playing, `false` if all stopped

**Usage Example:**
```c
// Wait for all sounds to finish
while (SituationIsAudioDevicePlaying()) {
    SituationPollInputEvents();
    SituationSleep(10);
}

// Pause game when no audio is playing
if (!SituationIsAudioDevicePlaying() && game_state == CUTSCENE) {
    // Cutscene audio finished
    EndCutscene();
}

// Show audio indicator
if (SituationIsAudioDevicePlaying()) {
    DrawAudioIcon();
}
```

**Notes:**
- Returns false if device is paused
- Checks all active sounds
- Useful for cutscene synchronization

---
#### `SituationGetAudioMasterVolume`
Gets the current master volume for the audio device. Returns a value between 0.0 (silent) and 1.0 (full volume).

```c
float SituationGetAudioMasterVolume(void);
```

**Returns:** Current master volume (0.0 to 1.0)

**Usage Example:**
```c
// Display volume slider
float volume = SituationGetAudioMasterVolume();
DrawVolumeSlider(volume);

// Save volume to settings
float current_volume = SituationGetAudioMasterVolume();
SaveSetting("audio.master_volume", current_volume);

// Toggle mute
static float saved_volume = 1.0f;
if (SituationIsKeyPressed(SIT_KEY_M)) {
    float current = SituationGetAudioMasterVolume();
    if (current > 0.0f) {
        saved_volume = current;
        SituationSetAudioMasterVolume(0.0f);
    } else {
        SituationSetAudioMasterVolume(saved_volume);
    }
}
```

**Notes:**
- Returns 0.0 if audio device not ready
- Use with `SituationSetAudioMasterVolume()` for volume controls
- Affects all sounds globally

---
#### `SituationSetAudioMasterVolume`
Sets the master volume for the entire audio device, from `0.0` (silent) to `1.0` (full volume). Affects all currently playing and future sounds.

```c
SituationError SituationSetAudioMasterVolume(float volume);
```

**Parameters:**
- `volume` - Master volume (0.0 = silent, 1.0 = full)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Volume slider
float volume = GetSliderValue();  // 0.0 to 1.0
SituationSetAudioMasterVolume(volume);

// Fade out audio
for (float v = 1.0f; v >= 0.0f; v -= 0.01f) {
    SituationSetAudioMasterVolume(v);
    SituationSleep(16);  // ~60 FPS
}

// Mute when window loses focus
if (!SituationHasWindowFocus()) {
    SituationSetAudioMasterVolume(0.0f);
} else {
    SituationSetAudioMasterVolume(settings.master_volume);
}
```

**Notes:**
- Clamps to 0.0-1.0 range
- Affects all sounds immediately
- Multiplies with individual sound volumes

---
#### `SituationGetAudioPlaybackSampleRate`
Gets the sample rate of the current audio device in Hz (e.g., 44100, 48000).

```c
int SituationGetAudioPlaybackSampleRate(void);
```

**Returns:** Sample rate in Hz, or 0 if device not ready

**Usage Example:**
```c
// Display audio info
int sample_rate = SituationGetAudioPlaybackSampleRate();
printf("Audio device: %d Hz\n", sample_rate);

// Verify sample rate
int rate = SituationGetAudioPlaybackSampleRate();
if (rate != 48000) {
    printf("Warning: Expected 48kHz, got %dHz\n", rate);
}

// Calculate buffer size
int sample_rate = SituationGetAudioPlaybackSampleRate();
int buffer_size = sample_rate / 60;  // 1 frame at 60 FPS
```

**Notes:**
- Common rates: 44100 (CD quality), 48000 (professional)
- Returns 0 if audio device not initialized
- Set during initialization, read-only at runtime

---
#### `SituationSetAudioPlaybackSampleRate`
Re-initializes the audio device with a new sample rate. This stops all currently playing sounds and may cause a brief audio interruption.

```c
SituationError SituationSetAudioPlaybackSampleRate(int sample_rate);
```

**Parameters:**
- `sample_rate` - New sample rate in Hz (e.g., 44100, 48000)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Change sample rate in settings menu
if (user_selected_48khz) {
    // Stop all sounds first
    SituationStopAllLoadedSounds();
    
    // Change sample rate
    if (SituationSetAudioPlaybackSampleRate(48000) == SITUATION_SUCCESS) {
        printf("Sample rate changed to 48kHz\n");
    } else {
        printf("Failed to change sample rate\n");
    }
}

// Try high-quality audio, fallback to standard
if (SituationSetAudioPlaybackSampleRate(96000) != SITUATION_SUCCESS) {
    printf("96kHz not supported, using 48kHz\n");
    SituationSetAudioPlaybackSampleRate(48000);
}
```

**Notes:**
- Stops all currently playing sounds
- May fail if hardware doesn't support the rate
- Common rates: 44100, 48000, 96000
- Causes brief audio interruption

---
#### `SituationGetAudioDevices`
Enumerates all available audio playback devices on the system. This is useful for providing the user with a choice of audio output devices.
```c
SituationAudioDeviceInfo* SituationGetAudioDevices(int* count);
```
**Usage Example:**
```c
int device_count;
SituationAudioDeviceInfo* devices = SituationGetAudioDevices(&device_count);
printf("Available Audio Devices:\n");
for (int i = 0; i < device_count; i++) {
    printf("- %s %s\n", devices[i].name, devices[i].is_default ? "(Default)" : "");
}
// Note: The returned array's memory is managed by the library and should not be freed.
```

---
#### `SituationSetAudioDevice`
Sets the active audio playback device by its ID. This should be called before loading any sounds.
```c
SituationError SituationSetAudioDevice(int device_id);
```

---
#### `SituationSetAudioMasterVolume`
Sets the master volume for the entire audio device. See full documentation in the Audio Device Management section above.

```c
SituationError SituationSetAudioMasterVolume(float volume);
```

---
#### `SituationSuspendAudioContext` / `SituationResumeAudioContext`
Suspends or resumes the entire audio context, stopping or restarting all sounds.
```c
SituationError SituationSuspendAudioContext(void);
SituationError SituationResumeAudioContext(void);
```
---
#### Sound Loading and Management
---
#### `SituationLoadSoundFromFile` / `SituationUnloadSound`
Loads a sound from a file (WAV, MP3, OGG, FLAC). The `mode` parameter determines whether to decode fully to RAM (`SITUATION_AUDIO_LOAD_FULL`, `AUTO`) or stream from disk (`SITUATION_AUDIO_LOAD_STREAM`). `SituationUnloadSound` frees the sound's memory.
```c
SituationError SituationLoadSoundFromFile(const char* file_path, SituationAudioLoadMode mode, bool looping, SituationSound* out_sound);
void SituationUnloadSound(SituationSound* sound);
```
**Usage Example:**
```c
// At init:
SituationSound jump_sound;
SituationLoadSoundFromFile("sounds/jump.wav", SITUATION_AUDIO_LOAD_AUTO, false, &jump_sound);

// During gameplay:
if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
    SituationPlayLoadedSound(&jump_sound);
}

// At shutdown:
SituationUnloadSound(&jump_sound);
```
---
#### `SituationLoadSoundFromStream`
Initializes a sound for playback by streaming it from a custom data source. This is highly memory-efficient and the preferred method for long music tracks. You provide callbacks to read and seek in your custom data stream.

```c
SituationError SituationLoadSoundFromStream(SituationStreamReadCallback on_read, SituationStreamSeekCallback on_seek, void* user_data, const SituationAudioFormat* format, bool looping, SituationSound* out_sound);
```

**Parameters:**
- `on_read` - Callback to read audio data from your source
- `on_seek` - Callback to seek to a position in your source
- `user_data` - User data passed to callbacks
- `format` - Audio format specification (sample rate, channels, format)
- `looping` - Whether the sound should loop
- `out_sound` - Pointer to receive the sound handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Custom stream from file
typedef struct {
    FILE* file;
    long file_size;
} FileStream;

// Read callback
size_t FileStreamRead(void* user_data, void* buffer, size_t bytes_to_read) {
    FileStream* stream = (FileStream*)user_data;
    return fread(buffer, 1, bytes_to_read, stream->file);
}

// Seek callback
bool FileStreamSeek(void* user_data, size_t byte_offset) {
    FileStream* stream = (FileStream*)user_data;
    return fseek(stream->file, byte_offset, SEEK_SET) == 0;
}

// Load music from custom stream
FileStream stream = {
    .file = fopen("music/background.ogg", "rb"),
    .file_size = GetFileSize("music/background.ogg")
};

SituationAudioFormat format = {
    .sample_rate = 44100,
    .channels = 2,
    .format = SITUATION_AUDIO_FORMAT_F32
};

SituationSound music;
if (SituationLoadSoundFromStream(FileStreamRead, FileStreamSeek, &stream, &format, true, &music) == SITUATION_SUCCESS) {
    SituationPlayLoadedSound(&music);
}
```

**Advanced Example (Network Stream):**
```c
// Stream audio from network
typedef struct {
    int socket;
    uint8_t buffer[8192];
    size_t buffer_pos;
} NetworkStream;

size_t NetworkStreamRead(void* user_data, void* buffer, size_t bytes) {
    NetworkStream* stream = (NetworkStream*)user_data;
    return recv(stream->socket, buffer, bytes, 0);
}

bool NetworkStreamSeek(void* user_data, size_t offset) {
    // Network streams typically don't support seeking
    return false;
}
```

**Notes:**
- Ideal for music tracks and long audio files
- Uses minimal memory (only buffers small chunks)
- Callbacks are called from audio thread - keep them fast
- Seeking may not be supported by all stream types

---
#### `SituationLoadSoundFromMemory`
Loads a sound from a data buffer already in memory. Useful for embedded audio data or audio downloaded from network.

```c
SituationError SituationLoadSoundFromMemory(const char* file_type, const unsigned char* data, int data_size, bool looping, SituationSound* out_sound);
```

**Parameters:**
- `file_type` - File format extension (e.g., "wav", "ogg", "mp3")
- `data` - Pointer to audio file data in memory
- `data_size` - Size of the data in bytes
- `looping` - Whether the sound should loop
- `out_sound` - Pointer to receive the sound handle

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Load embedded audio data
extern const unsigned char embedded_sound_data[];
extern const int embedded_sound_size;

SituationSound embedded_sound;
if (SituationLoadSoundFromMemory("wav", embedded_sound_data, embedded_sound_size, false, &embedded_sound) == SITUATION_SUCCESS) {
    SituationPlayLoadedSound(&embedded_sound);
}

// Load audio from network
uint8_t* downloaded_audio = DownloadAudio("https://example.com/sound.ogg", &size);
SituationSound network_sound;
SituationLoadSoundFromMemory("ogg", downloaded_audio, size, false, &network_sound);
free(downloaded_audio);  // Can free after loading
```

**Advanced Example (Asset Pack):**
```c
// Load sounds from a packed asset file
typedef struct {
    const char* name;
    size_t offset;
    size_t size;
} AssetEntry;

void LoadSoundsFromPack(const char* pack_file) {
    // Read entire pack into memory
    size_t pack_size;
    uint8_t* pack_data = LoadFile(pack_file, &pack_size);
    
    // Read asset table
    AssetEntry* entries = (AssetEntry*)pack_data;
    int entry_count = *(int*)(pack_data + sizeof(AssetEntry));
    
    // Load each sound
    for (int i = 0; i < entry_count; i++) {
        SituationSound sound;
        const uint8_t* sound_data = pack_data + entries[i].offset;
        
        if (SituationLoadSoundFromMemory("ogg", sound_data, entries[i].size, false, &sound) == SITUATION_SUCCESS) {
            RegisterSound(entries[i].name, sound);
        }
    }
}
```

**Notes:**
- Data must remain valid until sound is loaded (then can be freed)
- File type determines decoder (wav, ogg, mp3, flac)
- Entire file is decoded into memory
- Good for embedded assets or downloaded audio
---
#### Playback Control
---
#### `SituationPlayLoadedSound` / `SituationStopLoadedSound`
Begins or stops playback of a specific loaded sound.
```c
SituationError SituationPlayLoadedSound(SituationSound* sound);
SituationError SituationStopLoadedSound(SituationSound* sound);
```
**Usage Example:**
```c
if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
    SituationPlayLoadedSound(&jump_sound);
}
```

---
#### `SituationIsSoundPlaying`
Checks if a specific sound is currently playing. Returns `true` if the sound is actively playing, `false` if it has stopped, finished, or was never started.
```c
bool SituationIsSoundPlaying(SituationSound* sound);
```
**Parameters:**
- `sound`: Pointer to the sound to check

**Returns:**
- `true` if sound is playing
- `false` if sound is stopped or finished

**Usage Example:**
```c
// Only play if not already playing
if (!SituationIsSoundPlaying(&background_music)) {
    SituationPlayLoadedSound(&background_music);
}

// Wait for sound to finish before continuing
SituationPlayLoadedSound(&dialogue_sound);
while (SituationIsSoundPlaying(&dialogue_sound)) {
    SituationPollInputEvents();
    SituationUpdateTimers();
    // Update but don't advance game logic
}

// Stop sound if playing too long
static float play_time = 0.0f;
if (SituationIsSoundPlaying(&alarm_sound)) {
    play_time += SituationGetFrameTime();
    if (play_time > 5.0f) {
        SituationStopLoadedSound(&alarm_sound);
        play_time = 0.0f;
    }
}

// Restart looping sound if it stopped unexpectedly
if (!SituationIsSoundPlaying(&ambient_loop) && should_be_playing) {
    SituationPlayLoadedSound(&ambient_loop);
}
```
**Notes:**
- Returns `false` for one-shot sounds that have finished
- Returns `true` for looping sounds until explicitly stopped
- Useful for preventing overlapping sound effects
- Can be used to detect when a sound has finished

---
#### `SituationSetSoundVolume`
Sets the volume for a specific, individual sound. This is multiplied with the master volume.

```c
SituationError SituationSetSoundVolume(SituationSound* sound, float volume);
```

**Parameters:**
- `sound` - Sound to modify
- `volume` - Volume level (0.0 = silent, 1.0 = full)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Set volume before playing
SituationSound explosion;
SituationLoadSoundFromFile("sounds/explosion.wav", SITUATION_AUDIO_LOAD_AUTO, false, &explosion);
SituationSetSoundVolume(&explosion, 0.7f);  // 70% volume
SituationPlayLoadedSound(&explosion);

// Fade out effect
for (float v = 1.0f; v >= 0.0f; v -= 0.01f) {
    SituationSetSoundVolume(&music, v);
    SituationSleep(16);  // ~60 FPS
}

// Distance-based volume
float distance = CalculateDistance(player, sound_source);
float volume = 1.0f / (1.0f + distance * 0.1f);  // Inverse distance
SituationSetSoundVolume(&ambient_sound, volume);

// Category-based volume
SituationSetSoundVolume(&sfx_sound, settings.sfx_volume);
SituationSetSoundVolume(&music_sound, settings.music_volume);
SituationSetSoundVolume(&voice_sound, settings.voice_volume);
```

**Notes:**
- Multiplied with master volume
- Clamped to 0.0-1.0 range
- Affects currently playing sound immediately
- Persists across play/stop cycles

---
#### `SituationSetSoundPan`
Sets the stereo panning for a sound. Controls left/right speaker balance.

```c
SituationError SituationSetSoundPan(SituationSound* sound, float pan);
```

**Parameters:**
- `sound` - Sound to modify
- `pan` - Pan value (-1.0 = full left, 0.0 = center, 1.0 = full right)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Pan sound based on position
float player_x = 400.0f;  // Screen center
float sound_x = 600.0f;   // Right of center
float pan = (sound_x - player_x) / 400.0f;  // -1.0 to 1.0
SituationSetSoundPan(&sound, pan);

// Stereo effect - alternate left/right
static bool left_side = true;
SituationSetSoundPan(&footstep, left_side ? -0.5f : 0.5f);
left_side = !left_side;
SituationPlayLoadedSound(&footstep);

// Panning animation
float time = SituationGetTime();
float pan = sinf(time * 2.0f);  // Oscillate -1.0 to 1.0
SituationSetSoundPan(&ambient, pan);

// Positional audio (simple)
float relative_x = sound_pos.x - listener_pos.x;
float pan = glm_clamp(relative_x / 100.0f, -1.0f, 1.0f);
SituationSetSoundPan(&sound, pan);
```

**Notes:**
- -1.0 = full left speaker
- 0.0 = center (both speakers equal)
- 1.0 = full right speaker
- Clamped to -1.0 to 1.0 range
- Useful for positional audio

---
#### `SituationSetSoundPitch`
Sets the playback pitch for a sound by resampling (`1.0` is normal pitch, `0.5` is one octave lower, `2.0` is one octave higher).
```c
SituationError SituationSetSoundPitch(SituationSound* sound, float pitch);
```
**Usage Example:**
```c
// Make the sound effect's pitch slightly random
float random_pitch = 1.0f + ((rand() % 200) - 100) / 1000.0f; // Range 0.9 to 1.1
SituationSetSoundPitch(&jump_sound, random_pitch);
SituationPlayLoadedSound(&jump_sound);
```

---
#### Querying Sound State
---
#### `SituationIsSoundLooping`
Checks if a sound is set to loop.
```c
bool SituationIsSoundLooping(SituationSound* sound);
```
**Usage Example:**
```c
if (SituationIsSoundLooping(&music)) {
    printf("The music track is set to loop.\n");
}
```

---
#### `SituationGetSoundLength`
Gets the total length of a sound in seconds.
```c
double SituationGetSoundLength(SituationSound* sound);
```
**Usage Example:**
```c
double length = SituationGetSoundLength(&music);
printf("Music track length: %.2f seconds\n", length);
```

---
#### `SituationGetSoundCursor`
Gets the current playback position of a sound in seconds.
```c
double SituationGetSoundCursor(SituationSound* sound);
```
**Usage Example:**
```c
double position = SituationGetSoundCursor(&music);
printf("Music is currently at %.2f seconds\n", position);
```

---
#### `SituationSetSoundCursor`
Sets the current playback position of a sound in seconds.
```c
void SituationSetSoundCursor(SituationSound* sound, double seconds);
```
**Usage Example:**
```c
// Skip 30 seconds into the music track
SituationSetSoundCursor(&music, 30.0);
```
---
#### Effects and Custom Processing
---
#### `SituationSetSoundFilter`
Applies a low-pass or high-pass filter to a sound's effects chain.
```c
SituationError SituationSetSoundFilter(SituationSound* sound, SituationFilterType type, float cutoff_hz, float q_factor);
```
**Usage Example:**
```c
// To simulate sound coming from another room, apply a low-pass filter.
SituationSetSoundFilter(&music, SIT_FILTER_LOW_PASS, 800.0f, 1.0f); // Cut off frequencies above 800 Hz
```

---
#### `SituationSetSoundReverb`
Applies a reverb effect to a sound.
```c
SituationError SituationSetSoundReverb(SituationSound* sound, bool enabled, float room_size, float damping, float wet_mix, float dry_mix);
```

---
#### `SituationAttachAudioProcessor`
Attaches a custom DSP (Digital Signal Processing) processor to a sound's effect chain for real-time audio processing. Useful for visualization, custom effects, or audio analysis.

```c
SituationError SituationAttachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* userData);
```

**Parameters:**
- `sound` - Pointer to the sound to attach processor to
- `processor` - Callback function that processes audio samples
- `userData` - User data passed to the callback

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Callback Signature:**
```c
typedef void (*SituationAudioProcessorCallback)(void* buffer, unsigned int frames, void* userData);
```

**Usage Example:**
```c
// Audio visualization processor
void VisualizationProcessor(void* buffer, unsigned int frames, void* userData) {
    float* samples = (float*)buffer;
    float* peak_level = (float*)userData;
    
    // Calculate peak level for visualization
    float max_sample = 0.0f;
    for (unsigned int i = 0; i < frames * 2; i++) {  // Stereo
        float abs_sample = fabsf(samples[i]);
        if (abs_sample > max_sample) {
            max_sample = abs_sample;
        }
    }
    *peak_level = max_sample;
}

// Attach to music for waveform display
float music_peak = 0.0f;
SituationAttachAudioProcessor(&background_music, VisualizationProcessor, &music_peak);

// In render loop, use music_peak for visualization
DrawWaveform(music_peak);
```

**Advanced Example (Custom Distortion Effect):**
```c
// Distortion processor
void DistortionProcessor(void* buffer, unsigned int frames, void* userData) {
    float* samples = (float*)buffer;
    float drive = *(float*)userData;
    
    for (unsigned int i = 0; i < frames * 2; i++) {
        // Apply soft clipping distortion
        float sample = samples[i] * drive;
        samples[i] = tanhf(sample);  // Soft clip
    }
}

float distortion_drive = 2.0f;
SituationAttachAudioProcessor(&guitar_sound, DistortionProcessor, &distortion_drive);
```

**Notes:**
- Processor is called from audio thread - keep it fast!
- Avoid allocations, locks, or heavy computations
- Multiple processors can be attached to the same sound
- Processors are called in attachment order

---
#### `SituationDetachAudioProcessor`
Detaches a custom DSP processor from a sound's effect chain.

```c
SituationError SituationDetachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor);
```

**Parameters:**
- `sound` - Pointer to the sound
- `processor` - The processor callback to detach

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Attach processor
SituationAttachAudioProcessor(&music, VisualizationProcessor, &peak_data);

// Later, detach when no longer needed
SituationDetachAudioProcessor(&music, VisualizationProcessor);

// Toggle effect on/off
if (effect_enabled) {
    SituationAttachAudioProcessor(&sound, EffectProcessor, &effect_params);
} else {
    SituationDetachAudioProcessor(&sound, EffectProcessor);
}
```

**Notes:**
- Must pass the same function pointer used in `AttachAudioProcessor()`
- Safe to call even if processor is not attached
- Detaching during playback is safe

---
#### `SituationSetSoundEcho`
Applies an echo/delay effect to a sound with configurable delay time, feedback, and wet/dry mix.

```c
SituationError SituationSetSoundEcho(SituationSound* sound, bool enabled, float delay_sec, float feedback, float wet_mix);
```

**Parameters:**
- `sound` - Pointer to the sound
- `enabled` - Whether echo effect is enabled
- `delay_sec` - Delay time in seconds (e.g., 0.3 for 300ms)
- `feedback` - Feedback amount (0.0 to 1.0, controls number of repeats)
- `wet_mix` - Wet/dry mix (0.0 = dry only, 1.0 = wet only, 0.5 = 50/50)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Apply subtle echo to voice
SituationSetSoundEcho(&voice_sound, true, 0.15f, 0.3f, 0.25f);

// Cave/canyon echo effect
SituationSetSoundEcho(&footstep_sound, true, 0.5f, 0.6f, 0.4f);

// Disable echo
SituationSetSoundEcho(&sound, false, 0.0f, 0.0f, 0.0f);

// Dynamic echo based on environment
void UpdateEnvironmentEcho(SituationSound* sound, EnvironmentType env) {
    switch (env) {
        case ENV_CAVE:
            SituationSetSoundEcho(sound, true, 0.8f, 0.7f, 0.5f);
            break;
        case ENV_CANYON:
            SituationSetSoundEcho(sound, true, 1.2f, 0.5f, 0.4f);
            break;
        case ENV_ROOM:
            SituationSetSoundEcho(sound, true, 0.1f, 0.2f, 0.15f);
            break;
        case ENV_OUTDOOR:
            SituationSetSoundEcho(sound, false, 0.0f, 0.0f, 0.0f);
            break;
    }
}
```

**Parameter Guidelines:**
- **Delay:** 0.05-0.2s (slapback), 0.2-0.5s (echo), 0.5-2.0s (long delay)
- **Feedback:** 0.0-0.3 (single repeat), 0.3-0.6 (multiple repeats), 0.6-0.9 (many repeats)
- **Wet Mix:** 0.1-0.3 (subtle), 0.3-0.5 (noticeable), 0.5-0.8 (prominent)

**Notes:**
- Echo effect is applied in real-time during playback
- Higher feedback values create more repeats
- Wet mix controls the balance between original and echoed signal
- Can be changed dynamically during playback

---
#### `SituationGetAudioPlaybackSampleRate`
Gets the sample rate of the current audio device. See full documentation in the Audio Device Management section above.

```c
int SituationGetAudioPlaybackSampleRate(void);
```

---
#### `SituationSetAudioPlaybackSampleRate`
Re-initializes the audio device with a new sample rate. See full documentation in the Audio Device Management section above.

```c
SituationError SituationSetAudioPlaybackSampleRate(int sample_rate);
```

---
#### `SituationGetAudioMasterVolume`
Gets the current master volume for the audio device. See full documentation in the Audio Device Management section above.

```c
float SituationGetAudioMasterVolume(void);
```

---
#### `SituationIsAudioDevicePlaying`
Checks if the audio device is currently playing any sounds. See full documentation in the Audio Device Management section above.

```c
bool SituationIsAudioDevicePlaying(void);
```

---
#### `SituationPauseAudioDevice`
Pauses audio playback on the device.
```c
SituationError SituationPauseAudioDevice(void);
```

---
#### `SituationResumeAudioDevice`
Resumes audio playback on the device.
```c
SituationError SituationResumeAudioDevice(void);
```

---
#### `SituationStopLoadedSound`
Stops a specific sound that is currently playing. If the sound is not playing, this function has no effect. The sound remains loaded in memory and can be played again.
```c
SituationError SituationStopLoadedSound(SituationSound* sound);
```
**Parameters:**
- `sound`: Pointer to the sound to stop

**Returns:**
- `SITUATION_SUCCESS` on success
- Error code if sound is invalid

**Usage Example:**
```c
// Stop background music when entering menu
if (game_state == STATE_MENU) {
    SituationStopLoadedSound(&background_music);
}

// Stop sound effect on collision
void on_collision(void) {
    if (SituationIsSoundPlaying(&explosion_sound)) {
        SituationStopLoadedSound(&explosion_sound);
    }
    // Play new explosion sound
    SituationPlayLoadedSound(&explosion_sound);
}

// Stop looping ambient sound
if (player_left_area) {
    SituationStopLoadedSound(&ambient_loop);
}
```
**Notes:**
- Sound remains loaded and can be replayed
- Does nothing if sound is not currently playing
- For looping sounds, stops the loop immediately
- Use `SituationUnloadSound()` to free memory after stopping

---
#### `SituationStopAllLoadedSounds`
Stops all currently playing sounds immediately. This is useful for scene transitions, pause menus, or emergency audio cutoff. Sounds remain loaded in memory.
```c
SituationError SituationStopAllLoadedSounds(void);
```
**Returns:**
- `SITUATION_SUCCESS` on success

**Usage Example:**
```c
// Stop all audio when pausing game
void pause_game(void) {
    SituationStopAllLoadedSounds();
    game_paused = true;
}

// Stop all audio on scene transition
void load_new_scene(const char* scene_name) {
    SituationStopAllLoadedSounds();
    unload_current_scene();
    load_scene(scene_name);
}

// Emergency audio cutoff
if (SituationIsKeyPressed(SIT_KEY_M)) {  // Mute key
    SituationStopAllLoadedSounds();
    audio_muted = true;
}

// Stop all before cleanup
void shutdown_audio_system(void) {
    SituationStopAllLoadedSounds();
    // Now safe to unload all sounds
    for (int i = 0; i < sound_count; i++) {
        SituationUnloadSound(&sounds[i]);
    }
}
```
**Notes:**
- Stops ALL sounds, including music and effects
- Sounds remain loaded in memory
- Does not affect master volume setting
- Consider fading out instead for smoother transitions

---
#### `SituationSoundCopy`
Creates a new sound by making a deep copy of the raw PCM audio data from a source sound. Useful for creating variations or backups.

```c
SituationError SituationSoundCopy(const SituationSound* source, SituationSound* out_destination);
```

**Parameters:**
- `source` - The source sound to copy from
- `out_destination` - Pointer to receive the new sound copy

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create a copy for pitch variation
SituationSound original_explosion;
SituationLoadAudio("explosion.wav", false, &original_explosion);

// Create multiple variations with different pitches
SituationSound explosion_low, explosion_high;
SituationSoundCopy(&original_explosion, &explosion_low);
SituationSoundCopy(&original_explosion, &explosion_high);

SituationSetSoundPitch(&explosion_low, 0.8f);
SituationSetSoundPitch(&explosion_high, 1.2f);

// Play random variation
int variation = rand() % 3;
if (variation == 0) SituationPlayLoadedSound(&original_explosion);
else if (variation == 1) SituationPlayLoadedSound(&explosion_low);
else SituationPlayLoadedSound(&explosion_high);
```

**Advanced Example (Sound Pool):**
```c
// Create a pool of identical sounds for overlapping playback
#define SOUND_POOL_SIZE 8
SituationSound laser_pool[SOUND_POOL_SIZE];

SituationSound laser_original;
SituationLoadAudio("laser.wav", false, &laser_original);

for (int i = 0; i < SOUND_POOL_SIZE; i++) {
    SituationSoundCopy(&laser_original, &laser_pool[i]);
}

// Play from pool (allows overlapping)
int pool_index = 0;
void PlayLaser() {
    SituationPlayLoadedSound(&laser_pool[pool_index]);
    pool_index = (pool_index + 1) % SOUND_POOL_SIZE;
}
```

**Notes:**
- Creates a complete copy of PCM data
- Both sounds are independent after copying
- Remember to unload both original and copy
- Useful for creating sound variations without reloading

---
#### `SituationSoundCrop`
Crops a sound's PCM data in-place to a specific frame range. This permanently modifies the sound data.

```c
SituationError SituationSoundCrop(SituationSound* sound, uint64_t initFrame, uint64_t finalFrame);
```

**Parameters:**
- `sound` - Pointer to the sound to crop (modified in-place)
- `initFrame` - Starting frame (inclusive)
- `finalFrame` - Ending frame (exclusive)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Load a long audio file
SituationSound long_audio;
SituationLoadAudio("long_recording.wav", false, &long_audio);

// Extract just the intro (first 2 seconds at 44100 Hz)
SituationSound intro;
SituationSoundCopy(&long_audio, &intro);
SituationSoundCrop(&intro, 0, 44100 * 2);  // 0 to 2 seconds

// Extract middle section (2 to 5 seconds)
SituationSound middle;
SituationSoundCopy(&long_audio, &middle);
SituationSoundCrop(&middle, 44100 * 2, 44100 * 5);

// Extract outro (last 3 seconds)
uint64_t total_frames = GetSoundFrameCount(&long_audio);
SituationSound outro;
SituationSoundCopy(&long_audio, &outro);
SituationSoundCrop(&outro, total_frames - (44100 * 3), total_frames);
```

**Advanced Example (Remove Silence):**
```c
// Trim silence from beginning and end
void TrimSilence(SituationSound* sound, float threshold) {
    float* samples = GetSoundSamples(sound);
    uint64_t frame_count = GetSoundFrameCount(sound);
    
    // Find first non-silent frame
    uint64_t start_frame = 0;
    for (uint64_t i = 0; i < frame_count; i++) {
        if (fabsf(samples[i * 2]) > threshold || fabsf(samples[i * 2 + 1]) > threshold) {
            start_frame = i;
            break;
        }
    }
    
    // Find last non-silent frame
    uint64_t end_frame = frame_count;
    for (uint64_t i = frame_count - 1; i > start_frame; i--) {
        if (fabsf(samples[i * 2]) > threshold || fabsf(samples[i * 2 + 1]) > threshold) {
            end_frame = i + 1;
            break;
        }
    }
    
    // Crop to non-silent region
    SituationSoundCrop(sound, start_frame, end_frame);
}
```

**Notes:**
- Modifies the sound in-place - original data is lost
- Frame numbers are in audio frames (not bytes or samples)
- For stereo, one frame = 2 samples (left + right)
- Use `SituationSoundCopy()` first if you need to preserve original

---
#### `SituationSoundExportAsWav`
Exports a sound's raw PCM data to a WAV file. Useful for saving procedurally generated audio or processed sounds.

```c
bool SituationSoundExportAsWav(const SituationSound* sound, const char* fileName);
```

**Parameters:**
- `sound` - The sound to export
- `fileName` - Output file path (e.g., "output.wav")

**Returns:** `true` on success, `false` on failure

**Usage Example:**
```c
// Export processed audio
SituationSound voice;
SituationLoadAudio("voice.wav", false, &voice);

// Apply effects
SituationSetSoundPitch(&voice, 0.8f);
SituationSetSoundEcho(&voice, true, 0.3f, 0.4f, 0.3f);

// Export the processed version
if (SituationSoundExportAsWav(&voice, "voice_processed.wav")) {
    printf("Exported processed audio\n");
}
```

**Advanced Example (Procedural Audio Generation):**
```c
// Generate a sine wave tone
SituationSound GenerateTone(float frequency, float duration, int sample_rate) {
    int frame_count = (int)(duration * sample_rate);
    float* samples = malloc(frame_count * 2 * sizeof(float));  // Stereo
    
    for (int i = 0; i < frame_count; i++) {
        float t = (float)i / sample_rate;
        float sample = sinf(2.0f * M_PI * frequency * t) * 0.5f;
        samples[i * 2] = sample;      // Left
        samples[i * 2 + 1] = sample;  // Right
    }
    
    SituationSound tone = CreateSoundFromSamples(samples, frame_count, sample_rate, 2);
    free(samples);
    return tone;
}

// Generate and export a 440Hz A note
SituationSound a_note = GenerateTone(440.0f, 2.0f, 44100);
SituationSoundExportAsWav(&a_note, "a_note.wav");

// Generate chord
SituationSound c_note = GenerateTone(261.63f, 2.0f, 44100);
SituationSound e_note = GenerateTone(329.63f, 2.0f, 44100);
SituationSound g_note = GenerateTone(392.00f, 2.0f, 44100);

// Mix and export
SituationSound chord = MixSounds(&c_note, &e_note, &g_note);
SituationSoundExportAsWav(&chord, "c_major_chord.wav");
```

**Notes:**
- Exports in standard WAV format (PCM)
- Preserves sample rate and channel count
- Useful for saving procedurally generated audio
- Can export sounds after applying effects
- File can be reloaded with `SituationLoadAudio()`

---
#### `SituationGetSoundVolume`
Gets the current volume level of a specific sound. Returns a value between 0.0 (silent) and 1.0 (full volume).
```c
float SituationGetSoundVolume(SituationSound* sound);
```
**Parameters:**
- `sound`: Pointer to the sound

**Returns:**
- Volume level (0.0 to 1.0)

**Usage Example:**
```c
// Check current volume before adjusting
float current_volume = SituationGetSoundVolume(&music);
if (current_volume > 0.5f) {
    SituationSetSoundVolume(&music, 0.3f);  // Lower it
}

// Fade out effect
void fade_out_sound(SituationSound* sound, float duration) {
    float start_volume = SituationGetSoundVolume(sound);
    float elapsed = 0.0f;
    
    while (elapsed < duration) {
        float t = elapsed / duration;
        float volume = start_volume * (1.0f - t);
        SituationSetSoundVolume(sound, volume);
        elapsed += SituationGetFrameTime();
    }
    SituationStopLoadedSound(sound);
}

// Display volume in UI
float volume = SituationGetSoundVolume(&sfx_sound);
printf("SFX Volume: %.0f%%\n", volume * 100.0f);
```

---
#### `SituationGetSoundPan`
Gets the current stereo panning of a sound. Returns -1.0 (full left), 0.0 (center), or 1.0 (full right).
```c
float SituationGetSoundPan(SituationSound* sound);
```
**Parameters:**
- `sound`: Pointer to the sound

**Returns:**
- Pan value (-1.0 to 1.0)

**Usage Example:**
```c
// Check current pan position
float pan = SituationGetSoundPan(&footstep_sound);
printf("Sound is panned: %s\n", pan < 0 ? "left" : pan > 0 ? "right" : "center");

// Gradually pan sound based on object position
float object_x = get_object_screen_x();
float screen_center = SituationGetScreenWidth() / 2.0f;
float pan = (object_x - screen_center) / screen_center;  // -1 to 1
SituationSetSoundPan(&sound, pan);

// Reset pan to center
if (SituationIsKeyPressed(SIT_KEY_C)) {
    SituationSetSoundPan(&sound, 0.0f);
}
```

---
#### `SituationGetSoundPitch`
Gets the current pitch multiplier of a sound. Returns 1.0 for normal pitch, < 1.0 for lower pitch, > 1.0 for higher pitch.
```c
float SituationGetSoundPitch(SituationSound* sound);
```
**Parameters:**
- `sound`: Pointer to the sound

**Returns:**
- Pitch multiplier (typically 0.5 to 2.0)

**Usage Example:**
```c
// Check current pitch
float pitch = SituationGetSoundPitch(&engine_sound);
printf("Engine pitch: %.2fx\n", pitch);

// Gradually increase pitch based on speed
float speed = get_vehicle_speed();
float target_pitch = 0.8f + (speed / max_speed) * 1.2f;  // 0.8 to 2.0
float current_pitch = SituationGetSoundPitch(&engine_sound);
float new_pitch = lerp(current_pitch, target_pitch, 0.1f);
SituationSetSoundPitch(&engine_sound, new_pitch);

// Reset to normal pitch
if (SituationIsKeyPressed(SIT_KEY_R)) {
    SituationSetSoundPitch(&sound, 1.0f);
}
```

---
#### `SituationSetSoundEcho`
Applies an echo/delay effect to a sound with configurable parameters. Creates repeating copies of the sound that decay over time.

```c
SituationError SituationSetSoundEcho(SituationSound* sound, bool enabled, float delay_sec, float feedback, float wet_mix);
```

**Parameters:**
- `sound` - Sound to apply echo to
- `enabled` - Enable/disable the echo effect
- `delay_sec` - Delay time in seconds between echoes (e.g., 0.3)
- `feedback` - Amount of echo feedback (0.0-1.0, higher = more repeats)
- `wet_mix` - Dry/wet mix (0.0 = original only, 1.0 = echo only)

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Cave/canyon echo effect
SituationSound voice;
SituationLoadSoundFromFile("sounds/voice.wav", SITUATION_AUDIO_LOAD_AUTO, false, &voice);
SituationSetSoundEcho(&voice, true, 0.4f, 0.6f, 0.5f);
SituationPlayLoadedSound(&voice);

// Subtle echo for ambience
SituationSetSoundEcho(&ambient, true, 0.2f, 0.3f, 0.2f);

// Disable echo
SituationSetSoundEcho(&sound, false, 0.0f, 0.0f, 0.0f);

// Dynamic echo based on environment
if (player_in_cave) {
    SituationSetSoundEcho(&footstep, true, 0.5f, 0.7f, 0.6f);  // Strong echo
} else if (player_in_room) {
    SituationSetSoundEcho(&footstep, true, 0.1f, 0.3f, 0.2f);  // Subtle echo
} else {
    SituationSetSoundEcho(&footstep, false, 0.0f, 0.0f, 0.0f);  // No echo (outdoors)
}

// Rhythmic delay effect
SituationSetSoundEcho(&drum, true, 0.25f, 0.5f, 0.4f);  // Quarter note delay at 120 BPM
```

**Notes:**
- delay_sec: Time between echoes (0.1-2.0 typical)
- feedback: Controls decay (0.0 = one echo, 0.9 = many echoes)
- wet_mix: Balance between original and effect (0.5 = 50/50)
- Higher feedback values can create infinite echo
- Use sparingly for performance

---
#### `SituationUnloadSound`
Unloads a sound and frees its resources. Always call this when done with a sound to prevent memory leaks.

```c
void SituationUnloadSound(SituationSound* sound);
```

**Parameters:**
- `sound` - Pointer to sound to unload

**Usage Example:**
```c
// Load and use sound
SituationSound jump_sound;
SituationLoadSoundFromFile("sounds/jump.wav", SITUATION_AUDIO_LOAD_AUTO, false, &jump_sound);

// Use the sound
SituationPlayLoadedSound(&jump_sound);

// Cleanup when done
SituationUnloadSound(&jump_sound);

// Proper resource management
void LoadGameSounds() {
    SituationLoadSoundFromFile("sounds/jump.wav", SITUATION_AUDIO_LOAD_AUTO, false, &game_sounds.jump);
    SituationLoadSoundFromFile("sounds/shoot.wav", SITUATION_AUDIO_LOAD_AUTO, false, &game_sounds.shoot);
    SituationLoadSoundFromFile("sounds/music.ogg", SITUATION_AUDIO_LOAD_STREAM, true, &game_sounds.music);
}

void UnloadGameSounds() {
    SituationUnloadSound(&game_sounds.jump);
    SituationUnloadSound(&game_sounds.shoot);
    SituationUnloadSound(&game_sounds.music);
}

// Safe to call multiple times
SituationSound sound;
SituationLoadSoundFromFile("test.wav", SITUATION_AUDIO_LOAD_AUTO, false, &sound);
SituationUnloadSound(&sound);
SituationUnloadSound(&sound);  // Safe, but unnecessary

// Array of sounds
#define MAX_SOUNDS 10
SituationSound sounds[MAX_SOUNDS];
int sound_count = 0;

// Load sounds
for (int i = 0; i < sound_count; i++) {
    SituationLoadSoundFromFile(sound_files[i], SITUATION_AUDIO_LOAD_AUTO, false, &sounds[i]);
}

// Cleanup all
for (int i = 0; i < sound_count; i++) {
    SituationUnloadSound(&sounds[i]);
}
```

**Notes:**
- Stops sound if currently playing
- Frees audio data from memory
- Safe to call multiple times (idempotent)
- Always call before application exit
- Use handle-based API (`SituationUnloadAudio`) for simpler management

---
#### Audio Handle API
These functions operate on the new `SituationSoundHandle` system, which simplifies audio management by using opaque handles instead of structs. This is the recommended API for new code.

---
#### `SituationLoadAudio`
Loads an audio file and returns an opaque handle for simplified audio management. This is the modern, handle-based API that's easier to use than the struct-based API.
```c
SituationSoundHandle SituationLoadAudio(const char* file_path, SituationAudioLoadMode mode, bool looping);
```
**Parameters:**
- `file_path`: Path to audio file (WAV, OGG, MP3, FLAC)
- `mode`: `SITUATION_AUDIO_LOAD_AUTO`, `SITUATION_AUDIO_LOAD_MEMORY`, or `SITUATION_AUDIO_LOAD_STREAM`
- `looping`: `true` for looping playback, `false` for one-shot

**Returns:**
- Valid handle on success, invalid handle (0) on failure

**Usage Example:**
```c
// Load background music (streaming for memory efficiency)
SituationSoundHandle music = SituationLoadAudio(
    "assets/music/background.ogg",
    SITUATION_AUDIO_LOAD_STREAM,
    true  // Loop
);

// Load sound effect (in memory for low latency)
SituationSoundHandle jump_sfx = SituationLoadAudio(
    "assets/sfx/jump.wav",
    SITUATION_AUDIO_LOAD_MEMORY,
    false  // One-shot
);

// Auto mode (library decides based on file size)
SituationSoundHandle ambient = SituationLoadAudio(
    "assets/ambient.ogg",
    SITUATION_AUDIO_LOAD_AUTO,
    true
);

// Check if load succeeded
if (music == 0) {
    printf("Failed to load music!\n");
}
```
**Notes:**
- Use `SITUATION_AUDIO_LOAD_STREAM` for long music tracks
- Use `SITUATION_AUDIO_LOAD_MEMORY` for short sound effects
- Use `SITUATION_AUDIO_LOAD_AUTO` to let library decide
- Always check return value for 0 (invalid handle)

---
#### `SituationPlayAudio`
Plays audio using its handle. If the audio is already playing, it restarts from the beginning. For looping audio, this starts the loop.
```c
SituationError SituationPlayAudio(SituationSoundHandle handle);
```
**Parameters:**
- `handle`: Audio handle from `SituationLoadAudio()`

**Returns:**
- `SITUATION_SUCCESS` on success
- Error code if handle is invalid

**Usage Example:**
```c
// Play background music
SituationPlayAudio(background_music);

// Play sound effect on event
if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
    SituationPlayAudio(jump_sound);
}

// Play with volume control
SituationSetAudioVolume(explosion_sfx, 0.7f);
SituationPlayAudio(explosion_sfx);

// Restart currently playing sound
if (player_died) {
    SituationPlayAudio(death_sound);  // Restarts if already playing
}

// Play multiple instances (load multiple times)
SituationSoundHandle shot1 = SituationLoadAudio("shot.wav", SITUATION_AUDIO_LOAD_MEMORY, false);
SituationSoundHandle shot2 = SituationLoadAudio("shot.wav", SITUATION_AUDIO_LOAD_MEMORY, false);
SituationPlayAudio(shot1);
SituationPlayAudio(shot2);  // Plays simultaneously
```
**Notes:**
- Restarts sound if already playing
- For simultaneous playback, load the same file multiple times
- Looping sounds will loop until stopped
- No need to check if sound is playing first

---
#### `SituationUnloadAudio`
Unloads audio and frees its resources. The handle becomes invalid after this call. Always unload audio when no longer needed to free memory.
```c
void SituationUnloadAudio(SituationSoundHandle handle);
```
**Parameters:**
- `handle`: Audio handle to unload

**Usage Example:**
```c
// Unload when done
SituationUnloadAudio(menu_music);

// Unload all audio on scene change
void cleanup_scene_audio(void) {
    for (int i = 0; i < audio_count; i++) {
        SituationUnloadAudio(audio_handles[i]);
    }
    audio_count = 0;
}

// Unload at shutdown
void shutdown(void) {
    SituationUnloadAudio(background_music);
    SituationUnloadAudio(ambient_sound);
    for (int i = 0; i < sfx_count; i++) {
        SituationUnloadAudio(sfx_handles[i]);
    }
    SituationShutdown();
}
```
**Notes:**
- Always unload audio when done to free memory
- Automatically stops audio if playing
- Handle becomes invalid after unload
- Safe to call multiple times (no-op if already unloaded)

---
#### `SituationSetAudioVolume`
Sets the volume for an audio handle. Volume range is 0.0 (silent) to 1.0 (full volume). Values outside this range are clamped.
```c
SituationError SituationSetAudioVolume(SituationSoundHandle handle, float volume);
```
**Parameters:**
- `handle`: Audio handle
- `volume`: Volume level (0.0 to 1.0)

**Returns:**
- `SITUATION_SUCCESS` on success
- Error code if handle is invalid

**Usage Example:**
```c
// Set music volume from settings
SituationSetAudioVolume(music, user_settings.music_volume);

// Lower volume for background ambience
SituationSetAudioVolume(ambient, 0.3f);

// Fade in effect
void fade_in(SituationSoundHandle handle, float duration) {
    SituationSetAudioVolume(handle, 0.0f);
    SituationPlayAudio(handle);
    
    float elapsed = 0.0f;
    while (elapsed < duration) {
        float t = elapsed / duration;
        SituationSetAudioVolume(handle, t);
        elapsed += SituationGetFrameTime();
    }
}

// Dynamic volume based on distance
float distance = calculate_distance_to_source();
float volume = 1.0f / (1.0f + distance * 0.1f);  // Inverse distance
SituationSetAudioVolume(sound_handle, volume);
```
**Notes:**
- Values are clamped to 0.0-1.0 range
- Can be set before or during playback
- Combines with master volume setting
- Use for fade effects, distance attenuation, mixing

---
#### `SituationSetAudioPan`
Sets the stereo panning for an audio handle. Pan range is -1.0 (full left) to 1.0 (full right), with 0.0 being center.
```c
SituationError SituationSetAudioPan(SituationSoundHandle handle, float pan);
```
**Parameters:**
- `handle`: Audio handle
- `pan`: Pan position (-1.0 to 1.0)

**Returns:**
- `SITUATION_SUCCESS` on success
- Error code if handle is invalid

**Usage Example:**
```c
// Pan sound based on object position
float object_x = enemy.position.x;
float camera_x = camera.position.x;
float screen_width = SituationGetScreenWidth();
float relative_x = (object_x - camera_x) / (screen_width / 2.0f);
float pan = clamp(relative_x, -1.0f, 1.0f);
SituationSetAudioPan(enemy_sound, pan);

// Alternate between left and right speakers
static float pan_time = 0.0f;
pan_time += SituationGetFrameTime();
float pan = sinf(pan_time * 2.0f);  // Oscillate -1 to 1
SituationSetAudioPan(siren_sound, pan);

// Hard pan to left or right
SituationSetAudioPan(left_engine, -1.0f);   // Full left
SituationSetAudioPan(right_engine, 1.0f);   // Full right
SituationSetAudioPan(center_voice, 0.0f);   // Center
```
**Notes:**
- Values are clamped to -1.0 to 1.0 range
- 0.0 is center (equal in both speakers)
- Useful for spatial audio in 2D games
- Can create stereo effects and positional audio

---
#### `SituationSetAudioPitch`
Sets the pitch multiplier for an audio handle. Pitch of 1.0 is normal, < 1.0 is lower/slower, > 1.0 is higher/faster. Typical range is 0.5 to 2.0.
```c
SituationError SituationSetAudioPitch(SituationSoundHandle handle, float pitch);
```
**Parameters:**
- `handle`: Audio handle
- `pitch`: Pitch multiplier (typically 0.5 to 2.0)

**Returns:**
- `SITUATION_SUCCESS` on success
- Error code if handle is invalid

**Usage Example:**
```c
// Engine sound based on RPM
float rpm = engine.get_rpm();
float pitch = 0.5f + (rpm / max_rpm) * 1.5f;  // 0.5 to 2.0
SituationSetAudioPitch(engine_sound, pitch);

// Slow motion effect
if (game_in_slow_motion) {
    SituationSetAudioPitch(all_sounds, 0.7f);  // Slow and deep
} else {
    SituationSetAudioPitch(all_sounds, 1.0f);  // Normal
}

// Random pitch variation for variety
float random_pitch = 0.9f + (rand() / (float)RAND_MAX) * 0.2f;  // 0.9 to 1.1
SituationSetAudioPitch(footstep_sound, random_pitch);

// Musical intervals (semitones)
float semitones_up = 7;  // Perfect fifth
float pitch = powf(2.0f, semitones_up / 12.0f);
SituationSetAudioPitch(note_sound, pitch);
```
**Notes:**
- Also affects playback speed (higher pitch = faster)
- Typical range is 0.5 to 2.0 for natural sounds
- Use for engine sounds, slow-motion, musical effects
- Random variation adds realism to repeated sounds

---
#### `SituationPauseAudioDevice`
Pauses the entire audio device, stopping all audio playback. This is more efficient than stopping individual sounds when you need to pause everything (e.g., game pause menu).
```c
SituationError SituationPauseAudioDevice(void);
```
**Returns:**
- `SITUATION_SUCCESS` on success

**Usage Example:**
```c
// Pause all audio when game is paused
void pause_game(void) {
    SituationPauseAudioDevice();
    game_paused = true;
}

// Pause audio when window loses focus
void on_focus_changed(int focused, void* user_data) {
    if (!focused) {
        SituationPauseAudioDevice();
    } else {
        SituationResumeAudioDevice();
    }
}
SituationSetFocusCallback(on_focus_changed, NULL);

// Pause during loading screen
void show_loading_screen(void) {
    SituationPauseAudioDevice();
    load_assets();
    SituationResumeAudioDevice();
}
```
**Notes:**
- Pauses ALL audio, not individual sounds
- More efficient than stopping each sound individually
- Audio resumes from where it paused
- Use `SituationResumeAudioDevice()` to continue playback

---
#### `SituationResumeAudioDevice`
Resumes audio playback on the device after it was paused with `SituationPauseAudioDevice()`. All sounds continue from where they were paused.
```c
SituationError SituationResumeAudioDevice(void);
```
**Returns:**
- `SITUATION_SUCCESS` on success

**Usage Example:**
```c
// Resume audio when game is unpaused
void unpause_game(void) {
    SituationResumeAudioDevice();
    game_paused = false;
}

// Resume when window regains focus
void on_focus_changed(int focused, void* user_data) {
    if (focused) {
        SituationResumeAudioDevice();
    } else {
        SituationPauseAudioDevice();
    }
}

// Resume after loading
void finish_loading(void) {
    hide_loading_screen();
    SituationResumeAudioDevice();
}
```
**Notes:**
- Resumes ALL paused audio
- Sounds continue from where they were paused
- No effect if audio is not paused
- Pair with `SituationPauseAudioDevice()`

---
#### `SituationSetAudioDevice`
Sets the active audio playback device by its ID and format.
```c
SITAPI SituationError SituationSetAudioDevice(int situation_internal_id, const SituationAudioFormat* format);
```
**Usage Example:**
```c
int device_count;
SituationAudioDeviceInfo* devices = SituationGetAudioDevices(&device_count);
if (device_count > 0) {
    SituationAudioFormat format = { .channels = 2, .sample_rate = 44100, .bit_depth = 16 };
    SituationSetAudioDevice(devices[0].internal_id, &format);
}
```

---

#### `SituationSetSoundReverb`
Applies a reverb effect to a sound.
```c
SITAPI SituationError SituationSetSoundReverb(SituationSound* sound, bool enabled, float room_size, float damping, float wet_mix, float dry_mix);
```
**Usage Example:**
```c
SituationSound my_sound;
if (SituationLoadSoundFromFile("sounds/footstep.wav", SITUATION_AUDIO_LOAD_AUTO, false, &my_sound) == SITUATION_SUCCESS) {
    // Apply a reverb to simulate a large room
    SituationSetSoundReverb(&my_sound, true, 0.8f, 0.5f, 0.6f, 0.4f);
    SituationPlayLoadedSound(&my_sound);
}
```

---
#### Audio Capture
---
#### `SituationStartAudioCapture`
Initializes and starts capturing audio from the default microphone or recording device. The captured audio data is delivered via a callback that you provide.
```c
SITAPI SituationError SituationStartAudioCapture(SituationAudioCaptureCallback on_capture, void* user_data);
```
-   `on_capture`: A pointer to a function that will be called whenever a new buffer of audio data is available. The callback receives the raw audio buffer, the number of frames, and the user data pointer.
-   `user_data`: A custom pointer that will be passed to your `on_capture` callback.
**Usage Example:**
```c
// Define a callback to process the incoming audio data.
void MyAudioCaptureCallback(const float* frames, int frame_count, void* user_data) {
    // 'frames' is an interleaved buffer of 32-bit float samples.
    // For stereo, it would be [L, R, L, R, ...].
    printf("Captured %d audio frames.\n", frame_count);
    // You could write this data to a file, perform FFT, or visualize it.
}

// In your initialization code:
if (SituationStartAudioCapture(MyAudioCaptureCallback, NULL) != SIT_SUCCESS) {
    fprintf(stderr, "Failed to start audio capture: %s\n", SituationGetLastErrorMsg());
}
```

---
#### `SituationStopAudioCapture`
Stops the audio capture stream and releases the microphone device.
```c
SITAPI SituationError SituationStopAudioCapture(void);
```
**Usage Example:**
```c
// When the user clicks a "Stop Recording" button.
SituationStopAudioCapture();
printf("Audio capture stopped.\n");
```

---
#### `SituationIsAudioCapture`
Checks if the audio capture stream is currently active.
```c
SITAPI bool SituationIsAudioCapture(void);
```
**Usage Example:**
```c
if (SituationIsAudioCapture()) {
    // Update UI to show a "Recording" indicator.
}
```

---
#### Tone Generation and Synthesis
---
#### `SituationPlayToneEx`
Generates and plays a synthesized tone with full ADSR envelope control. Returns a handle that can be used to stop the tone early.
```c
SituationToneHandle SituationPlayToneEx(
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
```
**Parameters:**
- `type`: Waveform type (`SIT_WAVE_SINE`, `SIT_WAVE_SQUARE`, `SIT_WAVE_TRIANGLE`, `SIT_WAVE_SAWTOOTH`, `SIT_WAVE_NOISE`)
- `frequency`: Frequency in Hz (e.g., 440.0 for A4)
- `volume`: Volume from 0.0 to 1.0
- `pan`: Stereo panning from -1.0 (left) to 1.0 (right)
- `attack_sec`: Attack time in seconds
- `decay_sec`: Decay time in seconds
- `sustain_level`: Sustain level from 0.0 to 1.0
- `release_sec`: Release time in seconds
- `hold_sec`: How long to hold the sustain phase

**Usage Example:**
```c
// Play a 440Hz sine wave with a smooth envelope
SituationToneHandle tone = SituationPlayToneEx(
    SIT_WAVE_SINE,
    440.0f,      // A4 note
    0.5f,        // 50% volume
    0.0f,        // Center pan
    0.01f,       // 10ms attack
    0.1f,        // 100ms decay
    0.7f,        // 70% sustain level
    0.2f,        // 200ms release
    0.5f         // Hold for 500ms
);

// Optionally stop it early
if (user_cancelled) {
    SituationStopTone(tone);
}
```

---
#### `SituationPlayTone`
Simplified tone generation function without pan control. Useful for quick UI sounds and effects.
```c
void SituationPlayTone(
    SituationWaveType type,
    float frequency,
    float volume,
    float attack_sec,
    float decay_sec,
    float sustain_level,
    float release_sec,
    float hold_sec
);
```
**Usage Example:**
```c
// Play a quick UI beep
SituationPlayTone(SIT_WAVE_SINE, 800.0f, 0.3f, 0.01f, 0.05f, 0.0f, 0.05f, 0.1f);

// Play a retro game jump sound
SituationPlayTone(SIT_WAVE_SQUARE, 600.0f, 0.4f, 0.0f, 0.0f, 1.0f, 0.1f, 0.15f);
```

---
#### `SituationPlayMidiNote`
Plays a tone using MIDI note numbering (0-127, where 60 = Middle C). This is convenient for musical applications.
```c
void SituationPlayMidiNote(
    int note,
    SituationWaveType type,
    float volume,
    float attack_sec,
    float decay_sec,
    float sustain_level,
    float release_sec,
    float hold_sec
);
```
**Usage Example:**
```c
// Play Middle C (MIDI note 60) as a piano-like sound
SituationPlayMidiNote(60, SIT_WAVE_SINE, 0.5f, 0.01f, 0.2f, 0.6f, 0.3f, 1.0f);

// Play a C major chord
SituationPlayMidiNote(60, SIT_WAVE_SINE, 0.3f, 0.01f, 0.1f, 0.7f, 0.2f, 2.0f); // C
SituationPlayMidiNote(64, SIT_WAVE_SINE, 0.3f, 0.01f, 0.1f, 0.7f, 0.2f, 2.0f); // E
SituationPlayMidiNote(67, SIT_WAVE_SINE, 0.3f, 0.01f, 0.1f, 0.7f, 0.2f, 2.0f); // G
```

---
#### `SituationStopTone`
Stops a specific tone that was started with `SituationPlayToneEx()`.
```c
void SituationStopTone(SituationToneHandle handle);
```
**Usage Example:**
```c
SituationToneHandle alarm = SituationPlayToneEx(SIT_WAVE_SQUARE, 1000.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.1f, 5.0f);

// Later, when the alarm should stop
SituationStopTone(alarm);
```

---
#### `SituationStopAllTones`
Immediately stops all currently playing tones.
```c
void SituationStopAllTones(void);
```
**Usage Example:**
```c
// Emergency stop for all synthesized sounds
if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
    SituationStopAllTones();
}
```

</details>
<details>
<summary><h3>Filesystem Module</h3></summary>

**Overview:** The Filesystem module provides a robust, cross-platform, and UTF-8 aware API for interacting with the host's file system. It includes functions for checking file/directory existence, reading/writing files, and path manipulation. Use helpers like `SituationGetBasePath()` (for assets) and `SituationGetAppSavePath()` (for user data) for maximum portability.

### Functions

#### Path Management & Special Directories
---
#### `SituationGetBasePath`
Gets the full, absolute path to the directory containing the application's executable. This is the recommended way to locate asset files, as it is not affected by the current working directory. The returned string must be freed with `SituationFreeString()`.

```c
char* SituationGetBasePath(void);
```

**Returns:** Dynamically allocated string containing the executable's directory path (must be freed)

**Usage Example:**
```c
// Get the base path for loading assets relative to the executable
char* base_path = SituationGetBasePath();
printf("Application base path: %s\n", base_path);

// Build paths to asset files
char texture_path[512];
snprintf(texture_path, sizeof(texture_path), "%sassets/textures/player.png", base_path);

char shader_path[512];
snprintf(shader_path, sizeof(shader_path), "%sshaders/main.glsl", base_path);

// Load assets using absolute paths
SituationTexture player_tex = SituationLoadTexture(texture_path);
SituationShader main_shader = SituationLoadShader(shader_path);

// Always free the returned string
SituationFreeString(base_path);
```

**Alternative Pattern (Helper Function):**
```c
// Create a helper to build asset paths
char* GetAssetPath(const char* relative_path) {
    char* base = SituationGetBasePath();
    char* full_path = malloc(strlen(base) + strlen(relative_path) + 1);
    sprintf(full_path, "%s%s", base, relative_path);
    SituationFreeString(base);
    return full_path;
}

// Usage
char* texture_path = GetAssetPath("assets/textures/player.png");
SituationTexture tex = SituationLoadTexture(texture_path);
free(texture_path);
```

**Notes:**
- Returns the directory containing the executable, with trailing slash/backslash
- Not affected by the current working directory (unlike relative paths)
- Essential for distributing applications where working directory may vary
- The returned string must be freed with `SituationFreeString()`
- On macOS, returns the path to the .app bundle's Resources directory if applicable
---
#### `SituationGetAppSavePath`
Gets a platform-appropriate, user-specific directory for saving configuration files, save games, and other user data. For example, on Windows, this typically resolves to `%APPDATA%/YourAppName`. The returned string must be freed with `SituationFreeString`.
```c
char* SituationGetAppSavePath(const char* app_name);
```
**Usage Example:**
```c
char* save_path = SituationGetAppSavePath("MyCoolGame");
if (save_path) {
    char* config_file_path = SituationJoinPath(save_path, "config.ini");
    printf("Saving config to: %s\n", config_file_path);
    // ... save file ...
    SituationFreeString(config_file_path);
    SituationFreeString(save_path);
}
```
---
#### `SituationJoinPath`
Joins two path components with the correct OS separator.
```c
char* SituationJoinPath(const char* base_path, const char* file_or_dir_name);
```
**Usage Example:**
```c
char* base = SituationGetBasePath();
char* texture_path = SituationJoinPath(base, "textures/player.png");
// ... use texture_path ...
SituationFreeString(base);
SituationFreeString(texture_path);
```
---
#### File and Directory Queries
---
#### `SituationGetFileName`
Extracts the file name component from a path, including the extension.
```c
const char* SituationGetFileName(const char* file_path);
```
**Usage Example:**
```c
const char* path = "C:/assets/textures/player_avatar.png";
printf("File name: %s\n", SituationGetFileName(path)); // -> "player_avatar.png"
```

---
#### `SituationGetFileExtension`
Extracts the extension from a path, including the leading dot (`.`).
```c
const char* SituationGetFileExtension(const char* file_path);
```
**Usage Example:**
```c
const char* path = "C:/assets/textures/player_avatar.png";
printf("Extension: %s\n", SituationGetFileExtension(path)); // -> ".png"
```

---
#### `SituationGetBasePathFromFile`
Extracts the directory path from a full file path.
```c
char* SituationGetBasePathFromFile(const char* file_path);
```
**Usage Example:**
```c
const char* full_path = "/home/user/project/assets/texture.png";
char* base_path = SituationGetBasePathFromFile(full_path);
// base_path is now "/home/user/project/assets/"
SituationFreeString(base_path);
```

---
#### `SituationGetFileNameWithoutExt`
Extracts the file name from a path, excluding the extension. The caller is responsible for freeing the returned string with `SituationFreeString`.
```c
char* SituationGetFileNameWithoutExt(const char* file_path);
```
**Usage Example:**
```c
const char* path = "C:/assets/textures/player_avatar.png";
char* filename = SituationGetFileNameWithoutExt(path);
if (filename) {
    printf("File name without ext: %s\n", filename); // -> "player_avatar"
    SituationFreeString(filename);
}
```

---
#### `SituationIsFileExtension`
Checks if a file path has a specific extension (case-insensitive).
```c
bool SituationIsFileExtension(const char* file_path, const char* ext);
```
**Usage Example:**
```c
if (SituationIsFileExtension("my_archive.ZIP", ".zip")) {
    printf("This is a zip archive.\n");
}
```

---
#### `SituationFileExists`
Checks if a file exists at the given path and is accessible.
```c
bool SituationFileExists(const char* file_path);
```
**Usage Example:**
```c
// Before attempting to load a file, check if it exists.
if (SituationFileExists("settings.ini")) {
    LoadSettingsFromFile("settings.ini");
} else {
    CreateDefaultSettings();
}
```

---
#### `SituationDirectoryExists`
Checks if a directory exists at the given path and is accessible.
```c
bool SituationDirectoryExists(const char* dir_path);
```
**Usage Example:**
```c
// Check if a directory for save games exists before trying to list its files.
if (SituationDirectoryExists("save_games")) {
    // ... proceed to load a save file ...
}
```

---
#### `SituationIsPathFile`
Checks if a given path points to a file.
```c
bool SituationIsPathFile(const char* path);
```
**Usage Example:**
```c
// Differentiate between a file and a directory.
const char* path = "assets/player.png";
if (SituationIsPathFile(path)) {
    printf("%s is a file.\n", path);
} else if (SituationDirectoryExists(path)) {
    printf("%s is a directory.\n", path);
}
```
---
#### `SituationGetFileModTime`
Gets the last modification time of a file, returned as a Unix timestamp. Returns `-1` if the file does not exist. This is useful for hot-reloading assets.
```c
long SituationGetFileModTime(const char* file_path);
```
**Usage Example:**
```c
// In the update loop, check if a shader file has changed.
long mod_time = SituationGetFileModTime("shaders/main.frag");
if (mod_time > g_last_shader_load_time) {
    // Reload the shader.
    SituationUnloadShader(&g_main_shader);
    g_main_shader = SituationLoadShader("shaders/main.vert", "shaders/main.frag");
    g_last_shader_load_time = mod_time;
}
```
---
#### File I/O
---
#### `SituationLoadFileText`
Loads a text file as a null-terminated string. The caller is responsible for freeing the returned string with `SituationFreeString`.
```c
char* SituationLoadFileText(const char* file_path);
```
**Usage Example:**
```c
char* shader_code = SituationLoadFileText("shaders/main.glsl");
if (shader_code) {
    // ... use the shader code ...
    SituationFreeString(shader_code);
}
```

---
#### `SituationSaveFileText`
Saves a null-terminated string to a text file.
```c
SituationError SituationSaveFileText(const char* file_path, const char* text);
```
**Usage Example:**
```c
const char* settings = "[Graphics]\nwidth=1920\nheight=1080";
if (SituationSaveFileText("settings.ini", settings) == SITUATION_SUCCESS) {
    printf("Settings saved.\n");
}
```
---
#### `SituationLoadFileData`
Loads an entire file into a memory buffer. The caller is responsible for freeing the returned buffer with `SituationFreeString`.
```c
SituationError SituationLoadFileData(const char* file_path, unsigned int* out_bytes_read, unsigned char** out_data);
```
**Usage Example:**
```c
unsigned int data_size = 0;
unsigned char* file_data = NULL;
if (SituationLoadFileData("assets/level.dat", &data_size, &file_data) == SITUATION_SUCCESS) {
    // Process the loaded binary data.
    SituationFreeString((char*)file_data); // Cast and free the memory.
}
```

---
#### `SituationSaveFileData`
Saves a buffer of raw data to a file.
```c
SituationError SituationSaveFileData(const char* file_path, const void* data, unsigned int bytes_to_write);
```
**Usage Example:**
```c
// Assume 'player_data' is a struct containing game state.
PlayerState player_data = { .health = 100, .score = 5000 };
// Save the player state to a binary file.
if (SituationSaveFileData("save.dat", &player_data, sizeof(PlayerState)) == SITUATION_SUCCESS) {
    printf("Game saved successfully.\n");
}
```
---
#### `SituationFreeFileData`
Frees the memory for a data buffer returned by `SituationLoadFileData`.
```c
void SituationFreeFileData(unsigned char* data);
```
**Usage Example:**
```c
unsigned int data_size;
unsigned char* file_data = SituationLoadFileData("assets/level.dat", &data_size);
if (file_data) {
    // ... process data ...
    SituationFreeFileData(file_data); // Free the memory.
}
```
---
#### Directory Operations
---
#### `SituationCreateDirectory`
Creates a directory, optionally creating all parent directories in the path.
```c
SituationError SituationCreateDirectory(const char* dir_path, bool create_parents);
```
**Usage Example:**
```c
// Creates "assets", "assets/models", and "assets/models/player" if they don't exist.
SituationCreateDirectory("assets/models/player", true);
```
---
#### `SituationListDirectoryFiles`
Lists files and subdirectories in a path. The returned list must be freed with `SituationFreeDirectoryFileList`.
```c
SituationError SituationListDirectoryFiles(const char* dir_path, char*** out_files, int* out_count);
```
**Usage Example:**
```c
int file_count = 0;
char** files = NULL;
if (SituationListDirectoryFiles("assets", &files, &file_count) == SITUATION_SUCCESS) {
    for (int i = 0; i < file_count; ++i) {
        printf("Found file: %s\n", files[i]);
    }
    SituationFreeDirectoryFileList(files, file_count);
}
```

---
#### `SituationChangeDirectory` / `SituationGetWorkingDirectory`
Changes the application's current working directory or gets the current one.
```c
bool SituationChangeDirectory(const char* dir_path);
char* SituationGetWorkingDirectory(void);
```
**Usage Example:**
```c
char* initial_dir = SituationGetWorkingDirectory();
printf("Started in: %s\n", initial_dir);
if (SituationChangeDirectory("assets")) {
    printf("Changed dir to 'assets'\n");
}
SituationChangeDirectory(initial_dir); // Change back
SituationFreeString(initial_dir);
```

---
#### `SituationGetDirectoryPath`
Gets the directory path for a file path.
```c
char* SituationGetDirectoryPath(const char* file_path);
```
**Usage Example:**
```c
char* dir_path = SituationGetDirectoryPath("C:/assets/models/player.gltf");
// dir_path is now "C:/assets/models"
printf("The model's directory is: %s\n", dir_path);
SituationFreeString(dir_path);
```

---
#### `SituationCopyFile`
Copies a file from one location to another. The destination file will be created or overwritten if it already exists.

```c
SituationError SituationCopyFile(const char* source_path, const char* dest_path);
```

**Parameters:**
- `source_path` - Path to the source file to copy
- `dest_path` - Path where the copy should be created

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Create a backup of a save file
if (SituationCopyFile("saves/game.sav", "saves/game_backup.sav") == SITUATION_SUCCESS) {
    printf("Backup created successfully\n");
} else {
    printf("Failed to create backup\n");
}

// Copy a config file to a new location
if (SituationCopyFile("config/default.ini", "config/user.ini") == SITUATION_SUCCESS) {
    // Now modify the user config without affecting the default
    // ... modify user.ini ...
}
```

**Notes:**
- Overwrites the destination file if it already exists
- Preserves file contents but not necessarily metadata (timestamps, permissions)
- Works across different drives/volumes
- Returns an error if the source file doesn't exist or destination is not writable

---
#### `SituationDeleteFile`
Deletes a file from the filesystem. This operation is permanent and cannot be undone.

```c
SituationError SituationDeleteFile(const char* file_path);
```

**Parameters:**
- `file_path` - Path to the file to delete

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Delete temporary files on cleanup
if (SituationDeleteFile("temp/cache.tmp") == SITUATION_SUCCESS) {
    printf("Temporary file deleted\n");
}

// Delete old save files
const char* old_saves[] = {
    "saves/autosave_1.sav",
    "saves/autosave_2.sav",
    "saves/autosave_3.sav"
};

for (int i = 0; i < 3; i++) {
    if (SituationDeleteFile(old_saves[i]) != SITUATION_SUCCESS) {
        printf("Warning: Could not delete %s\n", old_saves[i]);
    }
}
```

**Notes:**
- Permanently deletes the file (not moved to recycle bin/trash)
- Returns an error if the file doesn't exist or cannot be deleted (e.g., in use, no permissions)
- Does not delete directories - use `SituationDeleteDirectory()` for that

---
#### `SituationMoveFile`
Moves or renames a file from one location to another. This works even across different drives on Windows.

```c
bool SituationMoveFile(const char* old_path, const char* new_path);
```

**Parameters:**
- `old_path` - Current path of the file
- `new_path` - New path for the file

**Returns:** `true` on success, `false` otherwise

**Usage Example:**
```c
// Rename a file in the same directory
if (SituationMoveFile("data/temp.dat", "data/final.dat")) {
    printf("File renamed successfully\n");
}

// Move a file to a different directory
if (SituationMoveFile("downloads/asset.png", "assets/textures/asset.png")) {
    printf("File moved to assets folder\n");
}

// Move across drives (Windows)
if (SituationMoveFile("C:/temp/data.bin", "D:/backup/data.bin")) {
    printf("File moved to different drive\n");
}
```

**Notes:**
- Can be used for both renaming (same directory) and moving (different directory)
- Works across different drives/volumes (uses copy+delete internally if needed)
- Overwrites the destination file if it already exists
- More efficient than copy+delete when on the same filesystem

---
#### `SituationRenameFile`
Alias for `SituationMoveFile()`. Renames or moves a file from one location to another.

```c
SituationError SituationRenameFile(const char* old_path, const char* new_path);
```

**Parameters:**
- `old_path` - Current path of the file
- `new_path` - New path for the file

**Returns:** `SITUATION_SUCCESS` on success, error code otherwise

**Usage Example:**
```c
// Rename a configuration file
if (SituationRenameFile("config.old", "config.ini") == SITUATION_SUCCESS) {
    printf("Config file renamed\n");
}
```

**Notes:**
- Functionally identical to `SituationMoveFile()` but returns `SituationError` instead of `bool`
- Use whichever function signature fits your error handling style better

---
#### `SituationDeleteDirectory`
Deletes a directory, optionally deleting all its contents.
```c
SituationError SituationDeleteDirectory(const char* dir_path, bool recursive);
```
**Usage Example:**
```c
// Delete the "temp_files" directory and everything inside it.
if (SituationDirectoryExists("temp_files")) {
    SituationDeleteDirectory("temp_files", true);
}
```

---

#### `SituationFreeDirectoryFileList`
Frees the memory for the list of file paths returned by `SituationListDirectoryFiles`.
```c
SITAPI void SituationFreeDirectoryFileList(char** files, int count);
```
**Usage Example:**
```c
int file_count = 0;
char** files = SituationListDirectoryFiles("assets", &file_count);
for (int i = 0; i < file_count; ++i) {
    printf("Found file: %s\n", files[i]);
}
SituationFreeDirectoryFileList(files, file_count);
```
</details>
<details>
<summary><h3>Threading Module</h3></summary>

**Overview:** The Threading module provides a high-performance **Generational Task System** for executing tasks asynchronously. It features a lock-minimized dual-queue architecture (High/Low priority) to prevent asset loading from stalling critical gameplay physics. The system uses O(1) generational IDs to prevent ABA problems and "Small Object Optimization" to avoid memory allocation for most jobs.

### Structs and Enums

#### `SituationJobFlags`
Flags to control job submission behavior, including priority and backpressure handling.
```c
typedef enum {
    SIT_SUBMIT_DEFAULT       = 0,       // Low Priority, Return 0 if full
    SIT_SUBMIT_HIGH_PRIORITY = 1 << 0,  // Use High Priority Queue (Physics, Audio)
    SIT_SUBMIT_BLOCK_IF_FULL = 1 << 1,  // Spin/Sleep until a slot opens
    SIT_SUBMIT_RUN_IF_FULL   = 1 << 2,  // Execute immediately on current thread if full
    SIT_SUBMIT_POINTER_ONLY  = 1 << 3   // Do not copy large data; user guarantees lifetime
} SituationJobFlags;
```

---
#### `SituationThreadPool`
Manages a pool of worker threads and dual priority queues.
```c
typedef struct SituationThreadPool {
    bool is_active;
    size_t thread_count;
    // ... internal state (queues, threads, synchronization) ...
} SituationThreadPool;
```

### Functions

---
#### `SituationCreateThreadPool`

Creates a thread pool with worker threads, job queues, and an optional dedicated I/O thread for background asset loading and hot-reload polling.

```c
bool SituationCreateThreadPool(
    SituationThreadPool* pool, 
    size_t num_threads, 
    size_t queue_size,
    double hot_reload_rate,
    bool disable_io
);
```

**Parameters:**
- `pool`: Pointer to the thread pool structure to initialize
- `num_threads`: Number of worker threads (0 = auto-detect CPU core count)
- `queue_size`: Maximum number of jobs per priority queue (High/Low)
- `hot_reload_rate`: Hot-reload polling frequency in seconds (0.0 = disable, default: 0.5)
- `disable_io`: If true, disables the dedicated I/O thread (useful for debugging or restricted environments)

**Returns:** `true` on success, `false` on failure

**New in v2.3.37:** Added `hot_reload_rate` and `disable_io` parameters for fine-grained control over the I/O subsystem.

**Usage Example:**
```c
SituationThreadPool pool;
// Create a pool with auto-detected threads, 1024 job slots, 
// hot-reload every 0.5 seconds, with I/O thread enabled
if (SituationCreateThreadPool(&pool, 0, 1024, 0.5, false)) {
    printf("Thread pool initialized with I/O thread.\n");
}

// For production builds, disable hot-reload and I/O thread:
if (SituationCreateThreadPool(&pool, 0, 1024, 0.0, true)) {
    printf("Thread pool initialized (production mode).\n");
}
```

---
#### `SituationDestroyThreadPool`
Shuts down the thread pool, stopping all worker threads and freeing resources. Blocks until all running jobs are finished.

```c
void SituationDestroyThreadPool(SituationThreadPool* pool);
```

**Parameters:**
- `pool` - Thread pool to destroy

**Usage Example:**
```c
// Create thread pool at startup
SituationThreadPool* pool = SituationCreateThreadPool(4, 1000, 0.0f, false);

// Use throughout application lifetime
for (int i = 0; i < 100; i++) {
    SituationSubmitJob(pool, ProcessTask, &tasks[i]);
}

// Wait for all jobs to complete
SituationWaitForAllJobs(pool);

// Shutdown at application exit
SituationDestroyThreadPool(pool);
pool = NULL;

// Proper shutdown sequence
void Shutdown() {
    // Stop submitting new jobs
    stop_flag = true;
    
    // Wait for pending jobs
    SituationWaitForAllJobs(thread_pool);
    
    // Destroy thread pool
    SituationDestroyThreadPool(thread_pool);
    thread_pool = NULL;
    
    // Continue with other cleanup...
}

// Graceful shutdown with timeout
void ShutdownWithTimeout() {
    // Try to finish jobs within 5 seconds
    double start_time = SituationGetTime();
    while (SituationGetJobCount(pool) > 0) {
        if (SituationGetTime() - start_time > 5.0) {
            printf("Warning: Forcing thread pool shutdown\n");
            break;
        }
        SituationSleep(10);
    }
    
    SituationDestroyThreadPool(pool);
}
```

**Notes:**
- **Blocks** until all running jobs complete
- Waits for jobs to finish, doesn't cancel them
- Safe to call with NULL pointer (no-op)
- Call before application exit
- Don't submit jobs after calling this

---
#### `SituationSubmitJobEx`
Submits a job with advanced control flags and embedded data.
```c
SituationJobId SituationSubmitJobEx(SituationThreadPool* pool, void (*func)(void*, void*), const void* data, size_t data_size, SituationJobFlags flags);
```
- `data`: Pointer to data to pass to the function. If `data_size` <= 64, it is copied into the job structure (zero-allocation). If larger, the pointer is passed directly.
- `flags`: Controls priority and behavior when the queue is full.

**Usage Example:**
```c
typedef struct { mat4 view; vec3 pos; } RenderData; // > 64 bytes

void ProcessRender(void* data, void* unused) {
    RenderData* rd = (RenderData*)data;
    // ...
}

RenderData my_data = { ... };
// Submit to High Priority queue, run immediately if full
SituationSubmitJobEx(&pool, ProcessRender, &my_data, sizeof(RenderData), SIT_SUBMIT_HIGH_PRIORITY | SIT_SUBMIT_RUN_IF_FULL);
```

---
#### `SituationSubmitJob`
Submits a job to the thread pool for asynchronous execution. This is a convenience macro that wraps `SituationSubmitJobEx()` with default flags.

```c
#define SituationSubmitJob(pool, func, user_ptr) SituationSubmitJobEx(pool, func, user_ptr, 0)
```

**Parameters:**
- `pool` - The thread pool to submit to
- `func` - Function pointer to execute (signature: `void func(void* user_data)`)
- `user_ptr` - User data pointer passed to the function

**Returns:** Job ID that can be used with `SituationWaitForJob()`

**Usage Example:**
```c
// Job function
void LoadAssetJob(void* user_data) {
    const char* filename = (const char*)user_data;
    printf("Loading %s on worker thread...\n", filename);
    // Load asset...
    printf("Finished loading %s\n", filename);
}

// Submit multiple jobs
SituationThreadPool* pool = SituationGetGlobalThreadPool();
SituationJobId job1 = SituationSubmitJob(pool, LoadAssetJob, "texture1.png");
SituationJobId job2 = SituationSubmitJob(pool, LoadAssetJob, "texture2.png");
SituationJobId job3 = SituationSubmitJob(pool, LoadAssetJob, "model.gltf");

// Continue main thread work while jobs execute
UpdateGameLogic();

// Wait for all assets to finish loading
SituationWaitForJob(pool, job1);
SituationWaitForJob(pool, job2);
SituationWaitForJob(pool, job3);

printf("All assets loaded!\n");
```

**Notes:**
- Jobs execute asynchronously on worker threads
- User pointer must remain valid until job completes
- For more control, use `SituationSubmitJobEx()` directly
- Jobs are executed in FIFO order per queue

---
#### `SituationWaitForJob`
Waits for a specific job to complete. Returns immediately if the job has already finished.

```c
bool SituationWaitForJob(SituationThreadPool* pool, SituationJobId job_id);
```

**Parameters:**
- `pool` - The thread pool the job was submitted to
- `job_id` - The job ID returned by `SituationSubmitJob()`

**Returns:** `true` if job completed successfully, `false` if job ID was invalid

**Usage Example:**
```c
// Submit a long-running job
typedef struct {
    int* data;
    int count;
    int result;
} ProcessData;

void ProcessJob(void* user_data) {
    ProcessData* pd = (ProcessData*)user_data;
    pd->result = 0;
    for (int i = 0; i < pd->count; i++) {
        pd->result += pd->data[i];
    }
}

ProcessData data = {
    .data = large_array,
    .count = 1000000,
    .result = 0
};

SituationJobId job = SituationSubmitJob(pool, ProcessJob, &data);

// Do other work while job runs
UpdateUI();
HandleInput();

// Wait for job to finish before using result
SituationWaitForJob(pool, job);
printf("Processing result: %d\n", data.result);
```

**Advanced Example (Conditional Wait):**
```c
// Submit job
SituationJobId job = SituationSubmitJob(pool, HeavyComputation, &data);

// Poll for completion without blocking
bool job_done = false;
while (!job_done) {
    // Try to wait with timeout (using custom polling)
    // Note: Situation doesn't have timeout, so we check periodically
    UpdateFrame();
    
    // Check if we need the result yet
    if (NeedResultNow()) {
        SituationWaitForJob(pool, job);
        job_done = true;
    }
}
```

**Notes:**
- Blocks the calling thread until job completes
- Returns immediately if job is already done
- Safe to call multiple times on the same job
- Invalid job IDs return `false`

---
#### `SituationWaitForAllJobs`
Blocks the calling thread until ALL jobs in the thread pool have completed. This ensures the thread pool is completely idle.

```c
void SituationWaitForAllJobs(SituationThreadPool* pool);
```

**Parameters:**
- `pool` - The thread pool to wait for

**Usage Example:**
```c
// Submit batch of jobs
SituationThreadPool* pool = SituationGetGlobalThreadPool();

for (int i = 0; i < 100; i++) {
    SituationSubmitJob(pool, ProcessItem, &items[i]);
}

printf("Submitted 100 jobs, waiting for completion...\n");

// Wait for all jobs to finish
SituationWaitForAllJobs(pool);

printf("All jobs complete!\n");

// Safe to access all results now
for (int i = 0; i < 100; i++) {
    printf("Item %d result: %d\n", i, items[i].result);
}
```

**Shutdown Example:**
```c
// Before shutting down, ensure all work is done
void Shutdown() {
    printf("Shutting down...\n");
    
    // Wait for all background jobs to finish
    SituationWaitForAllJobs(SituationGetGlobalThreadPool());
    
    // Now safe to destroy resources
    DestroyAllAssets();
    SituationShutdown();
}
```

**Notes:**
- Blocks until thread pool is completely idle
- More aggressive than waiting for individual jobs
- Use before shutdown or when you need to ensure all work is done
- Can cause performance hitches if called frequently - prefer `SituationWaitForJob()` for specific jobs

---
#### `SituationAddJobDependency`

Adds a dependency between two jobs, ensuring the prerequisite job completes before the dependent job starts. This is essential for building task graphs where jobs must execute in a specific order.

```c
bool SituationAddJobDependency(
    SituationThreadPool* pool,
    SituationJobId prerequisite_job,
    SituationJobId dependent_job
);
```

**Parameters:**
- `pool`: The thread pool managing the jobs
- `prerequisite_job`: The job that must complete first
- `dependent_job`: The job that depends on the prerequisite

**Returns:** `true` if the dependency was added successfully, `false` if either job ID is invalid

**Usage Example:**
```c
// Load texture, then create material (which needs the texture)
SituationJobId load_tex = SituationSubmitJobEx(&pool, LoadTextureTask, &tex_data, sizeof(tex_data), SIT_SUBMIT_DEFAULT);
SituationJobId create_mat = SituationSubmitJobEx(&pool, CreateMaterialTask, &mat_data, sizeof(mat_data), SIT_SUBMIT_DEFAULT);

// Ensure texture loads before material creation
SituationAddJobDependency(&pool, load_tex, create_mat);
```

---
#### `SituationAddJobDependencies`

Adds multiple prerequisite jobs for a single dependent job. This is more efficient than calling `SituationAddJobDependency` multiple times.

```c
bool SituationAddJobDependencies(
    SituationThreadPool* pool,
    SituationJobId* prerequisites,
    int count,
    SituationJobId dependent_job
);
```

**Parameters:**
- `pool`: The thread pool managing the jobs
- `prerequisites`: Array of job IDs that must complete first
- `count`: Number of prerequisite jobs in the array
- `dependent_job`: The job that depends on all prerequisites

**Returns:** `true` if all dependencies were added successfully

**Usage Example:**
```c
// Load multiple assets, then initialize the scene
SituationJobId asset_jobs[3];
asset_jobs[0] = SituationSubmitJobEx(&pool, LoadModelTask, &model_data, sizeof(model_data), SIT_SUBMIT_DEFAULT);
asset_jobs[1] = SituationSubmitJobEx(&pool, LoadTextureTask, &tex_data, sizeof(tex_data), SIT_SUBMIT_DEFAULT);
asset_jobs[2] = SituationSubmitJobEx(&pool, LoadAudioTask, &audio_data, sizeof(audio_data), SIT_SUBMIT_DEFAULT);

SituationJobId init_scene = SituationSubmitJobEx(&pool, InitSceneTask, &scene_data, sizeof(scene_data), SIT_SUBMIT_DEFAULT);

// Scene initialization waits for all assets to load
SituationAddJobDependencies(&pool, asset_jobs, 3, init_scene);
```

---
#### `SituationDumpTaskGraph`

Prints the current state of the task graph to a file stream for debugging and profiling. Can output in human-readable text or machine-readable JSON format.

```c
void SituationDumpTaskGraph(
    SituationThreadPool* pool,
    FILE* out_stream,
    bool json_mode
);
```

**Parameters:**
- `pool`: The thread pool to inspect
- `out_stream`: File stream to write to (use `stdout` or `stderr` for console output)
- `json_mode`: If `true`, outputs JSON format; if `false`, outputs human-readable text

**Usage Example:**
```c
// Debug: Print task graph to console
SituationDumpTaskGraph(&pool, stdout, false);

// Export task graph for analysis
FILE* graph_file = fopen("task_graph.json", "w");
if (graph_file) {
    SituationDumpTaskGraph(&pool, graph_file, true);
    fclose(graph_file);
}
```

---
#### `SituationGetIOQueueDepth`

Returns the current number of pending jobs in the Low Priority (I/O) queue. This is useful for monitoring background asset loading progress and implementing load balancing strategies.

```c
size_t SituationGetIOQueueDepth(void);
```

**Returns:** Number of pending I/O jobs in the queue

**New in v2.3.37:** Added as part of the Trinity Polish architecture update to expose I/O metrics.

**Usage Example:**
```c
// Check if background loading is complete
if (SituationGetIOQueueDepth() == 0) {
    printf("All assets loaded!\n");
    StartGame();
}

// Display loading progress
size_t pending = SituationGetIOQueueDepth();
printf("Loading... (%zu assets remaining)\n", pending);
```

### Async I/O Functions

These functions offload file operations to the dedicated I/O thread (Low Priority Queue).

---
#### `SituationLoadSoundFromFileAsync`
Asynchronously loads an audio file from disk in a background thread. It performs a full decode to RAM to avoid main-thread disk I/O.
```c
SituationJobId SituationLoadSoundFromFileAsync(SituationThreadPool* pool, const char* file_path, bool looping, SituationSound* out_sound);
```
**Usage Example:**
```c
SituationSound music;
SituationJobId job = SituationLoadSoundFromFileAsync(&pool, "music.mp3", true, &music);
// ... later ...
if (SituationWaitForJob(&pool, job)) {
    SituationPlayLoadedSound(&music);
}
```

---
#### `SituationLoadFileAsync`
Asynchronously loads a binary file from disk.
```c
SituationJobId SituationLoadFileAsync(SituationThreadPool* pool, const char* file_path, SituationFileLoadCallback callback, void* user_data);
```
**Usage Example:**
```c
void OnDataLoaded(void* data, size_t size, void* user) {
    printf("Loaded %zu bytes.\n", size);
    SIT_FREE(data); // You own the data!
}
SituationLoadFileAsync(&pool, "data.bin", OnDataLoaded, NULL);
```

---
#### `SituationLoadFileTextAsync`
Asynchronously loads a text file from disk on a background thread. The callback is invoked when loading completes.

```c
SituationJobId SituationLoadFileTextAsync(SituationThreadPool* pool, const char* file_path, SituationFileTextLoadCallback callback, void* user_data);
```

**Parameters:**
- `pool` - Thread pool to execute the load on
- `file_path` - Path to text file to load
- `callback` - Function called when load completes
- `user_data` - User data passed to callback

**Returns:** Job ID for tracking the operation

**Callback Signature:**
```c
typedef void (*SituationFileTextLoadCallback)(const char* path, const char* text, bool success, void* user_data);
```

**Usage Example:**
```c
// Callback when file loads
void OnConfigLoaded(const char* path, const char* text, bool success, void* user_data) {
    if (success) {
        printf("Loaded config: %s\n", path);
        ParseConfig(text);
        SituationFreeString((char*)text);  // Free the loaded text
    } else {
        printf("Failed to load: %s\n", path);
    }
}

// Load config file asynchronously
SituationThreadPool* pool = SituationCreateThreadPool(2, 100, 0.0f, false);
SituationLoadFileTextAsync(pool, "config.txt", OnConfigLoaded, NULL);

// Continue with other work while loading...
while (!SituationShouldClose()) {
    UpdateGame();
    RenderGame();
}

// Load multiple files
const char* files[] = {"level1.txt", "level2.txt", "level3.txt"};
for (int i = 0; i < 3; i++) {
    SituationLoadFileTextAsync(pool, files[i], OnLevelLoaded, (void*)(intptr_t)i);
}

// With user data
typedef struct {
    int level_id;
    char* level_name;
} LevelLoadContext;

LevelLoadContext* ctx = malloc(sizeof(LevelLoadContext));
ctx->level_id = 1;
ctx->level_name = "Forest";

SituationLoadFileTextAsync(pool, "levels/forest.txt", OnLevelLoaded, ctx);
```

**Notes:**
- Callback runs on background thread
- Must free loaded text with `SituationFreeString()`
- File path is copied, safe to free after call
- Returns immediately, doesn't block
- Use for config files, level data, etc.

---
#### `SituationSaveFileTextAsync`
Asynchronously saves text to a file on a background thread. The text is copied internally, so the caller can free their copy immediately.

```c
SituationJobId SituationSaveFileTextAsync(SituationThreadPool* pool, const char* file_path, const char* text, SituationFileSaveCallback callback, void* user_data);
```

**Parameters:**
- `pool` - Thread pool to execute the save on
- `file_path` - Path where file should be saved
- `text` - Null-terminated text to save
- `callback` - Optional callback when save completes (can be NULL)
- `user_data` - User data passed to callback

**Returns:** Job ID for tracking the operation

**Callback Signature:**
```c
typedef void (*SituationFileSaveCallback)(const char* path, bool success, void* user_data);
```

**Usage Example:**
```c
// Callback when save completes
void OnSaveComplete(const char* path, bool success, void* user_data) {
    if (success) {
        printf("Saved: %s\n", path);
    } else {
        printf("Failed to save: %s\n", path);
    }
}

// Save config asynchronously
char config_text[4096];
snprintf(config_text, sizeof(config_text), "volume=%.2f\nresolution=%dx%d\n",
    settings.volume, settings.width, settings.height);

SituationThreadPool* pool = SituationCreateThreadPool(2, 100, 0.0f, false);
SituationSaveFileTextAsync(pool, "config.txt", config_text, OnSaveComplete, NULL);

// Auto-save game state
void AutoSave() {
    char* save_data = SerializeGameState();
    SituationSaveFileTextAsync(pool, "autosave.txt", save_data, OnSaveComplete, NULL);
    free(save_data);  // Safe to free immediately
}

// Save high scores
char scores_text[1024];
FormatHighScores(scores_text, sizeof(scores_text));
SituationSaveFileTextAsync(pool, "highscores.txt", scores_text, NULL, NULL);  // No callback
```

**Notes:**
- Text is copied internally, safe to free after call
- Callback is optional (can be NULL)
- Returns immediately, doesn't block
- Creates parent directories if needed
- Use for save files, logs, exports

---
#### `SituationSaveFileAsync`
Asynchronously saves binary data to a file on a background thread. The data is copied to a temporary buffer, so the caller can free their copy immediately without waiting for the write to complete.

```c
SituationJobId SituationSaveFileAsync(SituationThreadPool* pool, const char* file_path, const void* data, size_t size, SituationFileSaveCallback callback, void* user_data);
```

**Parameters:**
- `pool` - The thread pool to execute the save operation on
- `file_path` - Path where the file should be saved
- `data` - Pointer to the binary data to save
- `size` - Size of the data in bytes
- `callback` - Optional callback function called when save completes (can be NULL)
- `user_data` - Optional user data passed to the callback (can be NULL)

**Returns:** Job ID that can be used to track or cancel the operation

**Usage Example:**
```c
// Callback to notify when save completes
void OnSaveComplete(const char* path, bool success, void* user_data) {
    if (success) {
        printf("Successfully saved: %s\n", path);
    } else {
        printf("Failed to save: %s\n", path);
    }
}

// Save game state asynchronously
typedef struct {
    int level;
    float player_x, player_y;
    int score;
} GameState;

GameState state = { .level = 5, .player_x = 100.0f, .player_y = 200.0f, .score = 12345 };

// Start async save - data is copied internally
SituationJobId job = SituationSaveFileAsync(
    SituationGetGlobalThreadPool(),
    "saves/quicksave.dat",
    &state,
    sizeof(GameState),
    OnSaveComplete,
    NULL
);

// Can immediately modify or free the original data
state.level = 6; // This won't affect the save operation

// Continue game without waiting for save to complete
```

**Notes:**
- Data is copied internally, so the source pointer doesn't need to remain valid
- Non-blocking - game continues running while file is being written
- Callback is called on the worker thread, not the main thread
- Useful for autosaves, screenshots, and large file exports
- Returns a job ID that can be used with `SituationWaitForJob()` if needed

---
#### `SituationSaveFileTextAsync`
Asynchronously saves a string to a text file. See full documentation in the Filesystem Module section above.

```c
SituationJobId SituationSaveFileTextAsync(SituationThreadPool* pool, const char* file_path, const char* text, SituationFileSaveCallback callback, void* user_data);
```

</details>
<details>
<summary><h3>Miscellaneous Module</h3></summary>

**Overview:** This module includes powerful utilities like the Temporal Oscillator System for rhythmic timing, a suite of color space conversion functions (RGBA, HSV, YPQA), and essential memory management helpers for data allocated by the library.

### Structs and Enums

#### `ColorRGBA`
Represents a color in the Red, Green, Blue, Alpha color space. Each component is an 8-bit unsigned integer (0-255).
```c
typedef struct ColorRGBA {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} ColorRGBA;
```

---
#### `ColorHSVA`
Represents a color in the Hue, Saturation, Value, Alpha color space.
```c
typedef struct ColorHSVA {
    float h; // Hue (0-360)
    float s; // Saturation (0-1)
    float v; // Value (0-1)
    float a; // Alpha (0-1)
} ColorHSVA;
```

---
#### `ColorYPQA`
Represents a color in a custom YPQA color space (Luma, Phase, Quadrature, Alpha).
```c
typedef struct ColorYPQA {
    float y; // Luma
    float p; // Phase
    float q; // Quadrature
    float a; // Alpha
} ColorYPQA;
```

### Functions

#### Temporal Oscillator System
---
#### `SituationTimerHasOscillatorUpdated`
Checks if an oscillator has just completed a cycle in the current frame. This is a single-frame trigger, ideal for synchronizing events to a rhythmic beat.
```c
bool SituationTimerHasOscillatorUpdated(int oscillator_id);
```
**Usage Example:**
```c
// In Init, create an oscillator with a 0.5s period (120 BPM).
double period = 0.5;
init_info.oscillator_count = 1;
init_info.oscillator_periods = &period;

// In the Update loop, check for the beat.
if (SituationTimerHasOscillatorUpdated(0)) {
    printf("Beat!\n");
    // Play a drum sound, flash a light, or trigger a game event.
}
```
---
#### `SituationSetTimerOscillatorPeriod`
Sets a new period for an oscillator at runtime, allowing you to change the tempo of rhythmic events dynamically.
```c
SituationError SituationSetTimerOscillatorPeriod(int oscillator_id, double period_seconds);
```
---
#### `SituationTimerGetOscillatorValue` / `SituationTimerGetOscillatorPhase`
Gets the current value (typically 0.0 to 1.0) or phase (0.0 to 2*PI) of an oscillator.
```c
double SituationTimerGetOscillatorValue(int oscillator_id);
double SituationTimerGetOscillatorPhase(int oscillator_id);
```
**Usage Example:**
```c
// Use an oscillator to create a smooth pulsing animation
double pulse = SituationTimerGetOscillatorValue(0); // This will smoothly go 0 -> 1 -> 0
float scale = 1.0f + (float)pulse * 0.2f;
// Apply 'scale' to a transform
```

---
#### `SituationTimerGetTime`
`double SituationTimerGetTime(void);`
Gets the total time elapsed since the library was initialized. This function returns the master application time, updated once per frame by `SituationUpdateTimers()`. It serves as the high-resolution monotonic clock for the entire application and is the basis for all other timing functions, including the Temporal Oscillator system.

**Usage Example:**
```c
// Get the total elapsed time to drive a procedural animation.
double totalTime = SituationTimerGetTime();
float pulse = sinf((float)totalTime * 2.0f); // A simple pulsing effect over time.
// Use 'pulse' to modify an object's color, size, etc.
```
---
#### Color Space Conversions
---
#### `SituationRgbToHsv` / `SituationHsvToRgb`
Converts a color between RGBA and HSV (Hue, Saturation, Value) color spaces.
```c
ColorHSV SituationRgbToHsv(ColorRGBA rgb);
ColorRGBA SituationHsvToRgb(ColorHSV hsv);
```
**Usage Example:**
```c
// Create a rainbow effect by cycling the hue
ColorHSV hsv_color = {.h = fmodf(SituationTimerGetTime() * 50.0f, 360.0f), .s = 1.0, .v = 1.0};
ColorRGBA final_color = SituationHsvToRgb(hsv_color);
```

---
#### `SituationTimerGetOscillatorState`
Gets the current binary state (0 or 1) of an oscillator. Oscillators toggle between 0 and 1 at their configured period.

```c
bool SituationTimerGetOscillatorState(int oscillator_id);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)

**Returns:** Current state (false = 0, true = 1)

**Usage Example:**
```c
// Blinking light effect
bool blink_state = SituationTimerGetOscillatorState(0);  // 1Hz oscillator
if (blink_state) {
    DrawLight(position, RED);
}

// Alternating animation
if (SituationTimerGetOscillatorState(1)) {
    DrawSprite(sprite_frame_a);
} else {
    DrawSprite(sprite_frame_b);
}

// Rhythmic game mechanic
if (SituationTimerGetOscillatorState(2)) {
    // Beat is "on"
    EnablePowerup();
} else {
    // Beat is "off"
    DisablePowerup();
}
```

**Notes:**
- Returns current state, not edge detection
- Use `SituationTimerHasOscillatorUpdated()` for edge detection
- State toggles at configured period

---
#### `SituationTimerGetPreviousOscillatorState`
Gets the previous frame's state of an oscillator. Useful for detecting state transitions (edges).

```c
bool SituationTimerGetPreviousOscillatorState(int oscillator_id);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)

**Returns:** Previous frame's state (false = 0, true = 1)

**Usage Example:**
```c
// Detect rising edge (0 -> 1 transition)
bool current = SituationTimerGetOscillatorState(0);
bool previous = SituationTimerGetPreviousOscillatorState(0);

if (current && !previous) {
    // Rising edge detected
    PlayBeatSound();
    TriggerEffect();
}

// Detect falling edge (1 -> 0 transition)
if (!current && previous) {
    // Falling edge detected
    StopEffect();
}

// Detect any change
if (current != previous) {
    // State changed
    OnBeatChange();
}
```

**Notes:**
- Use with current state for edge detection
- Alternative: use `SituationTimerHasOscillatorUpdated()`
- Updated once per frame

---
#### `SituationTimerPingOscillator`
Checks if an oscillator's period has elapsed since the last ping. This is a "manual" oscillator that only advances when you check it.

```c
bool SituationTimerPingOscillator(int oscillator_id);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)

**Returns:** `true` if period elapsed since last ping, `false` otherwise

**Usage Example:**
```c
// Spawn enemy every 2 seconds
if (SituationTimerPingOscillator(5)) {  // 0.5Hz = 2 second period
    SpawnEnemy();
}

// Auto-save every 30 seconds
if (SituationTimerPingOscillator(6)) {  // 1/30 Hz
    AutoSave();
}

// Periodic update that only runs when needed
if (SituationTimerPingOscillator(7)) {
    UpdateSlowSystem();
}

// Get progress between pings
double progress = SituationTimerGetPingProgress(7);
DrawProgressBar(progress);  // 0.0 to 1.0
```

**Notes:**
- Only advances when you call it
- Returns true once per period
- Use `SituationTimerGetPingProgress()` for interpolation
- Different from automatic oscillators

---
#### `SituationTimerGetOscillatorTriggerCount`
Gets the total number of times an oscillator has triggered (transitioned to state 1) since initialization.

```c
uint64_t SituationTimerGetOscillatorTriggerCount(int oscillator_id);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)

**Returns:** Total trigger count

**Usage Example:**
```c
// Count beats in a rhythm game
uint64_t beat_count = SituationTimerGetOscillatorTriggerCount(0);
printf("Beat: %llu\n", beat_count);

// Trigger event every N beats
if (beat_count % 4 == 0) {
    // Every 4th beat
    PlayAccentSound();
}

// Track total game time in beats
uint64_t total_beats = SituationTimerGetOscillatorTriggerCount(1);
double game_time_in_beats = (double)total_beats;

// Reset detection (if oscillator was reset)
static uint64_t last_count = 0;
uint64_t current_count = SituationTimerGetOscillatorTriggerCount(2);
if (current_count < last_count) {
    printf("Oscillator was reset\n");
}
last_count = current_count;
```

**Notes:**
- Increments on each 0->1 transition
- Never decreases (unless oscillator is reset)
- Useful for beat counting and rhythm games

---
#### `SituationTimerGetOscillatorPeriod`
Gets the period of an oscillator in seconds. The period is the time for one complete cycle (0->1->0).

```c
double SituationTimerGetOscillatorPeriod(int oscillator_id);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)

**Returns:** Period in seconds

**Usage Example:**
```c
// Display BPM from oscillator period
double period = SituationTimerGetOscillatorPeriod(0);
double bpm = 60.0 / period;
printf("Tempo: %.1f BPM\n", bpm);

// Calculate frequency
double frequency = 1.0 / period;
printf("Frequency: %.2f Hz\n", frequency);

// Sync animation speed to oscillator
double period = SituationTimerGetOscillatorPeriod(1);
float animation_speed = 1.0f / (float)period;
UpdateAnimation(animation_speed);
```

**Notes:**
- Returns period in seconds
- Frequency = 1.0 / period
- Use `SituationSetTimerOscillatorPeriod()` to change

---
#### `SituationTimerGetPingProgress`
Gets progress [0.0 to 1.0] of the interval since the last successful ping. Useful for interpolation between ping events.

```c
double SituationTimerGetPingProgress(int oscillator_id);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)

**Returns:** Progress from 0.0 (just pinged) to 1.0 (about to ping)

**Usage Example:**
```c
// Smooth progress bar for auto-save
double progress = SituationTimerGetPingProgress(5);
DrawProgressBar(progress);  // 0% to 100%

// Interpolate between spawn points
double t = SituationTimerGetPingProgress(6);
if (SituationTimerPingOscillator(6)) {
    SpawnEnemy();
}
// Show spawn warning as progress approaches 1.0
if (t > 0.8) {
    DrawSpawnWarning(t);  // Fade in warning
}

// Breathing animation
double breath = SituationTimerGetPingProgress(7);
float scale = 1.0f + 0.1f * sinf(breath * 2.0f * PI);
DrawSprite(position, scale);
```

**Notes:**
- Returns 0.0 immediately after ping
- Approaches 1.0 as next ping nears
- Can exceed 1.0 if ping is missed
- Use for smooth interpolation

---
#### `SituationSetTimerOscillatorPeriod`
Sets a new period for an oscillator at runtime, allowing you to change the tempo or frequency dynamically.

```c
void SituationSetTimerOscillatorPeriod(int oscillator_id, double period_seconds);
```

**Parameters:**
- `oscillator_id` - Oscillator index (0-15)
- `period_seconds` - New period in seconds

**Usage Example:**
```c
// Change tempo based on game state
if (boss_fight_started) {
    // Speed up to 140 BPM
    double period = 60.0 / 140.0;  // ~0.428 seconds
    SituationSetTimerOscillatorPeriod(0, period);
}

// Gradual tempo increase
static double current_bpm = 120.0;
current_bpm += 0.1;  // Increase by 0.1 BPM per frame
double period = 60.0 / current_bpm;
SituationSetTimerOscillatorPeriod(1, period);

// Sync to music tempo
double music_bpm = GetCurrentMusicBPM();
double beat_period = 60.0 / music_bpm;
SituationSetTimerOscillatorPeriod(2, beat_period);

// Slow down over time
double current_period = SituationTimerGetOscillatorPeriod(3);
SituationSetTimerOscillatorPeriod(3, current_period * 1.01);  // 1% slower
```

**Notes:**
- Changes take effect immediately
- Does not reset trigger count
- Period must be > 0.0
- Use for dynamic tempo changes

---
#### `SituationConvertColorToVec4`
Converts an 8-bit RGBA color (0-255 per channel) to a normalized vec4 (0.0-1.0 per channel). Useful for passing colors to shaders.

```c
void SituationConvertColorToVec4(ColorRGBA c, vec4 out_normalized_color);
```

**Parameters:**
- `c` - Input color with 8-bit channels (0-255)
- `out_normalized_color` - Output vec4 with normalized channels (0.0-1.0)

**Usage Example:**
```c
// Convert color for shader uniform
ColorRGBA tint_color = {255, 128, 64, 255};  // Orange
vec4 normalized_color;
SituationConvertColorToVec4(tint_color, normalized_color);

// Pass to shader
SituationSetShaderUniform(shader, "u_tint_color", normalized_color, SITUATION_UNIFORM_VEC4);

// Or use in push constant
struct PushConstants {
    vec4 color;
    mat4 mvp;
} pc;
SituationConvertColorToVec4(tint_color, pc.color);
SituationCmdSetPushConstant(cmd, 0, &pc, sizeof(pc));
```

**Notes:**
- Converts from [0, 255] to [0.0, 1.0] range
- Alpha channel is also normalized
- Useful for shader uniforms and GPU data

---
#### `SituationHsvToRgb`
Converts a color from HSV (Hue, Saturation, Value) color space to RGB color space. HSV is more intuitive for color manipulation and animation.

```c
ColorRGBA SituationHsvToRgb(ColorHSV hsv);
```

**Parameters:**
- `hsv` - Input color in HSV space (H: 0-360°, S: 0.0-1.0, V: 0.0-1.0)

**Returns:** Color in RGBA space (0-255 per channel)

**Usage Example:**
```c
// Create a rainbow effect by animating hue
float time = SituationGetTime();
ColorHSV hsv_color = {
    .h = fmodf(time * 60.0f, 360.0f),  // Cycle through hues
    .s = 1.0f,                          // Full saturation
    .v = 1.0f                           // Full brightness
};
ColorRGBA rgb_color = SituationHsvToRgb(hsv_color);
DrawRectangle(100, 100, 200, 200, rgb_color);

// Create color variations
ColorHSV base_hsv = {180.0f, 0.8f, 0.9f};  // Cyan-ish

// Lighter version (increase value)
ColorHSV light_hsv = base_hsv;
light_hsv.v = 1.0f;
ColorRGBA light_color = SituationHsvToRgb(light_hsv);

// Darker version (decrease value)
ColorHSV dark_hsv = base_hsv;
dark_hsv.v = 0.5f;
ColorRGBA dark_color = SituationHsvToRgb(dark_hsv);

// Desaturated version (decrease saturation)
ColorHSV gray_hsv = base_hsv;
gray_hsv.s = 0.2f;
ColorRGBA gray_color = SituationHsvToRgb(gray_hsv);
```

**Notes:**
- HSV is more intuitive for color picking and animation
- Hue wraps around (360° = 0°)
- Saturation 0.0 = grayscale, 1.0 = full color
- Value 0.0 = black, 1.0 = full brightness

---
#### `SituationColorToYPQ`
Converts a standard RGBA color to the YPQA (Luma, Phase, Quadrature, Alpha) color space. This is a custom color space useful for signal processing-like effects and color manipulation.

```c
ColorYPQA SituationColorToYPQ(ColorRGBA color);
```

**Parameters:**
- `color` - Input color in RGBA space

**Returns:** Color in YPQA space

**Usage Example:**
```c
// Convert to YPQ for manipulation
ColorRGBA original = {200, 100, 50, 255};
ColorYPQA ypq = SituationColorToYPQ(original);

// Adjust luma (brightness) without affecting hue
ypq.y *= 1.5f;  // Increase brightness

// Convert back to RGB
ColorRGBA brightened = SituationColorFromYPQ(ypq);

// Or manipulate phase/quadrature for color shifts
ColorYPQA ypq2 = SituationColorToYPQ(original);
ypq2.p += 0.2f;  // Shift phase
ypq2.q -= 0.1f;  // Shift quadrature
ColorRGBA shifted = SituationColorFromYPQ(ypq2);
```

**Notes:**
- Y = Luma (brightness)
- P = Phase (color component)
- Q = Quadrature (color component)
- Useful for advanced color grading and effects
- Less common than HSV but useful for specific effects

---
#### `SituationColorFromYPQ`
Converts a color from YPQA (Luma, Phase, Quadrature, Alpha) color space back to standard RGBA color space.

```c
ColorRGBA SituationColorFromYPQ(ColorYPQA ypq_color);
```

**Parameters:**
- `ypq_color` - Input color in YPQA space

**Returns:** Color in RGBA space (0-255 per channel)

**Usage Example:**
```c
// Create a color effect by manipulating YPQ components
void ApplyRetroEffect(ColorRGBA* pixels, int count) {
    for (int i = 0; i < count; i++) {
        // Convert to YPQ
        ColorYPQA ypq = SituationColorToYPQ(pixels[i]);
        
        // Reduce luma for darker look
        ypq.y *= 0.8f;
        
        // Shift phase for color tint
        ypq.p += 0.1f;
        
        // Convert back
        pixels[i] = SituationColorFromYPQ(ypq);
    }
}

// Use in image processing
SituationImage img = SituationLoadImage("photo.png");
ApplyRetroEffect((ColorRGBA*)img.data, img.width * img.height);
SituationSaveImage(img, "photo_retro.png");
```

**Notes:**
- Inverse of `SituationColorToYPQ()`
- Useful for color grading pipelines
- Allows independent manipulation of brightness and color

---
#### `SituationRgbToYpqa` / `SituationYpqaToRgb`
Converts a color between RGBA and the YPQA color space (a custom space for signal processing-like effects).
```c
ColorYPQA SituationRgbToYpqa(ColorRGBA rgb);
ColorRGBA SituationYpqaToRgb(ColorYPQA ypqa);
```

---
#### `SituationConvertColorToVector4`
Converts an 8-bit RGBA color to a normalized Vector4 (float values 0.0-1.0). This is useful for passing colors to shaders or graphics APIs that expect normalized values.
```c
void SituationConvertColorToVector4(ColorRGBA c, Vector4* out_normalized_color);
```
**Usage Example:**
```c
ColorRGBA color = {255, 128, 64, 255}; // Orange color
Vector4 normalized;
SituationConvertColorToVector4(color, &normalized);
// normalized is now {1.0f, 0.5f, 0.25f, 1.0f}

// Use in shader uniform
SituationSetShaderUniform(shader, "u_color", &normalized, sizeof(Vector4));
```

---
#### Rendering Utilities
---
#### `SituationDrawMetricsOverlay`
Draws a debug overlay showing FPS, frame time, memory usage, and other performance metrics. This is invaluable for development and profiling.
```c
void SituationDrawMetricsOverlay(SituationCommandBuffer cmd, Vector2 position, ColorRGBA color);
```
**Usage Example:**
```c
// Draw performance metrics in the top-left corner
if (SituationAcquireFrameCommandBuffer()) {
    SituationRenderPassInfo pass = {/* ... */};
    SituationCmdBeginRenderPass(cmd, &pass);
    
    // Draw your scene...
    
    // Add debug overlay
    #ifdef DEBUG
        Vector2 pos = {10, 10};
        ColorRGBA green = {0, 255, 0, 255};
        SituationDrawMetricsOverlay(cmd, pos, green);
    #endif
    
    SituationCmdEndRenderPass(cmd);
    SituationEndFrame();
}
```

---
#### `SituationIsFeatureSupported`
Checks if a specific graphics feature is supported and enabled on the current hardware. Use this to conditionally enable advanced rendering features.
```c
bool SituationIsFeatureSupported(SituationRenderFeature feature);
```
**Usage Example:**
```c
// Check for compute shader support before using them
if (SituationIsFeatureSupported(SIT_FEATURE_COMPUTE_SHADERS)) {
    printf("Compute shaders are supported!\n");
    // Enable compute-based post-processing
} else {
    printf("Compute shaders not available, using fallback\n");
    // Use traditional rendering path
}

// Check for bindless textures
if (SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
    // Use texture arrays and bindless rendering
}
```

---
#### Memory Management Helpers
---
#### `SituationFreeString`
Frees memory for a string allocated and returned by the library. Always call this for strings returned by functions like `SituationGetBasePath()`, `SituationGetUserDirectory()`, etc.

```c
void SituationFreeString(char* str);
```

**Parameters:**
- `str` - String pointer to free (can be NULL)

**Usage Example:**
```c
// Get base path (library allocates memory)
char* base_path = SituationGetBasePath();
printf("Base path: %s\n", base_path);

// Build asset path
char asset_path[512];
snprintf(asset_path, sizeof(asset_path), "%sassets/texture.png", base_path);

// Always free the string when done
SituationFreeString(base_path);

// Another example with user directory
char* user_dir = SituationGetUserDirectory();
printf("User directory: %s\n", user_dir);
SituationFreeString(user_dir);

// Safe to call with NULL
SituationFreeString(NULL);  // Does nothing
```

**Notes:**
- Must be called to avoid memory leaks
- Safe to call with NULL pointer
- Only use for strings returned by Situation functions
- Do not use `free()` directly - use this function instead

---
#### `SituationFreeDisplays`
Frees the memory allocated for the display list returned by `SituationGetDisplays()`. Always call this after you're done using the display information.

```c
void SituationFreeDisplays(SituationDisplayInfo* displays, int count);
```

**Parameters:**
- `displays` - Pointer to the display array to free
- `count` - Number of displays in the array

**Usage Example:**
```c
// Get display information
int display_count;
SituationDisplayInfo* displays = SituationGetDisplays(&display_count);

// Use the display info
for (int i = 0; i < display_count; i++) {
    printf("Display %d: %dx%d @ %dHz\n",
        i,
        displays[i].width,
        displays[i].height,
        displays[i].refresh_rate);
}

// Always free when done
SituationFreeDisplays(displays, display_count);
```

**Notes:**
- Must be called to avoid memory leaks
- Pass the same count returned by `SituationGetDisplays()`
- Safe to call with NULL pointer (does nothing)

---
#### `SituationFreeDirectoryFileList`
Frees the memory for the list of file paths returned by `SituationListDirectoryFiles()`. Always call this after processing the file list.

```c
void SituationFreeDirectoryFileList(char** files, int count);
```

**Parameters:**
- `files` - Array of file path strings to free
- `count` - Number of files in the array

**Usage Example:**
```c
// List all files in a directory
int file_count;
char** files = SituationListDirectoryFiles("assets/textures", &file_count);

if (files != NULL) {
    printf("Found %d files:\n", file_count);
    
    // Process each file
    for (int i = 0; i < file_count; i++) {
        printf("  %s\n", files[i]);
        
        // Load texture if it's a PNG
        if (strstr(files[i], ".png") != NULL) {
            SituationTexture tex = SituationLoadTexture(files[i]);
            // Use texture...
        }
    }
    
    // Always free the list when done
    SituationFreeDirectoryFileList(files, file_count);
}
```

**Advanced Example (Asset Discovery):**
```c
// Discover and load all shaders in a directory
void LoadAllShaders(const char* shader_dir) {
    int file_count;
    char** files = SituationListDirectoryFiles(shader_dir, &file_count);
    
    if (files == NULL) {
        printf("Failed to list directory: %s\n", shader_dir);
        return;
    }
    
    for (int i = 0; i < file_count; i++) {
        // Check if it's a vertex shader
        if (strstr(files[i], ".vert") != NULL) {
            // Find corresponding fragment shader
            char frag_path[512];
            strcpy(frag_path, files[i]);
            char* ext = strstr(frag_path, ".vert");
            strcpy(ext, ".frag");
            
            // Load shader pair
            SituationShader shader;
            if (SituationLoadShader(files[i], frag_path, &shader) == SITUATION_SUCCESS) {
                printf("Loaded shader: %s\n", files[i]);
            }
        }
    }
    
    // Free the file list
    SituationFreeDirectoryFileList(files, file_count);
}
```

**Notes:**
- Must be called to avoid memory leaks
- Frees both the array and all individual strings
- Safe to call with NULL pointer
- Pass the same count returned by `SituationListDirectoryFiles()`
</details>
<details>
<summary><h3>Hot-Reloading Module</h3></summary>

**Overview:** The Hot-Reloading module provides a powerful suite of functions to dynamically reload assets like shaders, textures, and models while the application is running. This significantly accelerates development by allowing for instant iteration on visual and computational resources without needing to restart the application. The system works by monitoring the last modification time of source files and triggering a reload when a change is detected.

### Functions

---
#### `SituationCheckHotReloads`
Checks all registered resources for changes and reloads them if necessary. This is the main entry point for the hot-reloading system and should be called once per frame in your main update loop.
```c
SITAPI void SituationCheckHotReloads(void);
```
**Usage Example:**
```c
// In the main application loop, after polling for input and updating timers.
while (!SituationWindowShouldClose()) {
    SITUATION_BEGIN_FRAME();

    // Check for any modified assets and reload them automatically.
    SituationCheckHotReloads();

    // ... proceed with application logic and rendering ...
}
```

---
#### `SituationReloadShader`
Forces an immediate reload of a specific graphics shader from its source files. This function is useful for targeted reloads or when you want to trigger a reload manually, for example, via a debug console command.
```c
SITAPI SituationError SituationReloadShader(SituationShader* shader);
```
**Usage Example:**
```c
// Assume 'g_main_shader' is the handle to your primary shader.
// In a debug input handler:
if (SituationIsKeyPressed(SIT_KEY_R)) {
    if (SituationReloadShader(&g_main_shader) == SIT_SUCCESS) {
        printf("Main shader reloaded successfully.\n");
    } else {
        printf("Failed to reload main shader: %s\n", SituationGetLastErrorMsg());
    }
}
```

---
#### `SituationReloadComputePipeline`
Forces an immediate reload of a specific compute pipeline from its source file.
```c
SITAPI SituationError SituationReloadComputePipeline(SituationComputePipeline* pipeline);
```
**Usage Example:**
```c
// Assume 'g_particle_sim' is your compute pipeline for particle physics.
// Reload it when 'F5' is pressed.
if (SituationIsKeyPressed(SIT_KEY_F5)) {
    SituationReloadComputePipeline(&g_particle_sim);
}
```

---
#### `SituationReloadTexture`
Forces an immediate reload of a specific texture from its source file. Note that the texture must have been originally created from a file for this to work.
```c
SITAPI SituationError SituationReloadTexture(SituationTexture* texture);
```
**Usage Example:**
```c
// In a material editor, when the user saves changes to a texture file.
void OnTextureFileSaved(const char* filepath) {
    // Find the texture associated with this filepath and reload it.
    SituationTexture* texture_to_reload = FindTextureByFilepath(filepath);
    if (texture_to_reload) {
        SituationReloadTexture(texture_to_reload);
    }
}
```

---
#### `SituationReloadModel`
Forces an immediate reload of a 3D model, including all its meshes and materials, from its source file (e.g., GLTF).
```c
SITAPI SituationError SituationReloadModel(SituationModel* model);
```
**Usage Example:**
```c
// In a 3D modeling workflow, reload the main scene model when requested.
if (UserRequestedModelReload()) {
    SituationReloadModel(&g_main_scene_model);
}
```
</details>

<details>
<summary><h3>Logging Module</h3></summary>

**Overview:** This module provides simple and direct functions for logging messages to the console. It allows for different log levels, enabling you to control the verbosity of the output for debugging and informational purposes.

### Functions

---
#### `SituationLog`
Prints a formatted log message to the console with a specified log level (e.g., `SIT_LOG_INFO`, `SIT_LOG_WARNING`, `SIT_LOG_ERROR`). It uses a `printf`-style format string.
```c
void SituationLog(int msgType, const char* text, ...);
```
**Usage Example:**
```c
int score = 100;
// Logs an informational message.
SituationLog(SIT_LOG_INFO, "Player reached a score of %d", score);

// Logs a warning if a value is unexpected.
if (score > 9000) {
    SituationLog(SIT_LOG_WARNING, "Score is over 9000!");
}
```

---
#### `SituationSetTraceLogLevel`
Sets the minimum log level to be displayed. Any message with a lower severity will be ignored. For example, setting the level to `SIT_LOG_WARNING` will show warnings, errors, and fatal messages, but hide info, debug, and trace messages.
```c
void SituationSetTraceLogLevel(int logType);
```
**Usage Example:**
```c
// During development, show all log messages for detailed debugging.
SituationSetTraceLogLevel(SIT_LOG_ALL);

// For a release build, only show warnings and errors to reduce noise.
#ifdef NDEBUG
    SituationSetTraceLogLevel(SIT_LOG_WARNING);
#endif
```
</details>

<details>
<summary><h3>Compute Shaders</h3></summary>

### Overview & Capabilities
Compute shaders enable developers to harness the parallel processing power of the GPU for general-purpose computations that are independent of the traditional graphics rendering pipeline. This includes tasks like physics simulations, AI calculations, image/video processing, procedural generation, and more. `situation.h` provides a unified, backend-agnostic API to create, manage, and execute compute shaders using either OpenGL Compute Shaders or Vulkan Compute Pipelines.

### Initialization Prerequisites
- The core `situation.h` library must be successfully initialized using `SituationInit`.
- Define `SITUATION_ENABLE_SHADER_COMPILER` in your build. This is **mandatory** for the Vulkan backend and highly recommended for OpenGL if you are providing GLSL source code. It enables the inclusion and use of the `shaderc` library for runtime compilation of GLSL to SPIR-V bytecode.
- For Vulkan: Ensure that the selected physical device (GPU) supports compute capabilities. This is checked during `SituationInit` if a Vulkan backend is chosen.

### Creating Compute Pipelines
#### From GLSL Source Code (SituationCreateComputePipelineFromMemory)
This is the primary function for creating a compute pipeline.
- **Signature:** `SITAPI SituationError SituationCreateComputePipelineFromMemory(const char* compute_shader_source, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline);`
- **Parameters:**
    - `compute_shader_source`: A null-terminated string containing the GLSL compute shader source code.
- **Process:**
    1.  Validates that the library is initialized and the source is not NULL.
    2.  If `SITUATION_ENABLE_SHADER_COMPILER` is defined:
        a.  Invokes `shaderc` to compile the provided GLSL source into SPIR-V bytecode.
        b.  Handles compilation errors and reports them via the error system.
    3.  Backend-Specific Creation:
        - **OpenGL**: Uses the SPIR-V (if compiled) or directly the GLSL source (if `ARB_gl_spirv` is not used/available) to create and link an OpenGL Compute Program (`glCreateProgram`, `glCreateShader(GL_COMPUTE_SHADER)`, `glLinkProgram`).
        - **Vulkan**: Uses the compiled SPIR-V bytecode to create a `VkShaderModule`, then a `VkPipelineLayout` (handling push constants), and finally the `VkComputePipeline` object.
    4.  Generates a unique `id` for the `SituationComputePipeline` handle.
    5.  Stores backend-specific handles internally (e.g., `gl_program_id`, `vk_pipeline`, `vk_pipeline_layout`).
    6.  Adds the pipeline to an internal tracking list for resource management and leak detection.
- **Return Value:**
    - Returns `SITUATION_SUCCESS` on success.
    - On success, `out_pipeline->id` will be a non-zero value.
    - On failure, returns an error code and `out_pipeline->id` will be 0.

#### Backend Compilation (OpenGL SPIR-V, Vulkan Runtime)
- The use of `shaderc` via `SITUATION_ENABLE_SHADER_COMPILER` standardizes the input (GLSL) and the intermediate representation (SPIR-V) for both backends, making the API consistent.
- OpenGL traditionally uses GLSL directly, but `ARB_gl_spirv` allows using SPIR-V. The library abstracts this choice.
- Vulkan *requires* SPIR-V, making runtime compilation with `shaderc` essential unless pre-compiled SPIR-V is used (which this function doesn't directly support, but the underlying Vulkan creation could be adapted).

### Using Compute Pipelines
Once a `SituationComputePipeline` is created, it can be used within a command buffer to perform computations.

#### Binding a Compute Pipeline (SituationCmdBindComputePipeline)
- **Signature:** `SITAPI void SituationCmdBindComputePipeline(SituationCommandBuffer cmd, SituationComputePipeline pipeline);`
- **Parameters:**
    - `cmd`: The command buffer obtained from `SituationAcquireFrameCommandBuffer` or `SituationBeginVirtualDisplayFrame`.
    - `pipeline`: The `SituationComputePipeline` handle returned by `SituationCreateComputePipelineFromMemory`.
- **Process:**
    1.  Validates the command buffer and pipeline handle.
    2.  Records the command to bind the pipeline state (program/pipeline object) to the command buffer for subsequent compute operations.
    3.  Backend-Specific:
        - **OpenGL**: Calls `glUseProgram(pipeline.gl_program_id)`.
        - **Vulkan**: Calls `vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.vk_pipeline)`.

#### Binding Resources (Buffers, Images)
Compute shaders often read from and write to GPU resources like Shader Storage Buffer Objects (SSBOs) or Images.
- `SITAPI void SituationCmdBindComputeBuffer(SituationCommandBuffer cmd, SituationBuffer buffer, uint32_t binding);`
    - Binds a `SituationBuffer` (created with appropriate usage flags like `SITUATION_BUFFER_USAGE_STORAGE_BUFFER`) to a specific binding point in the currently bound compute shader.
    - **Backend-Specific:**
        - **OpenGL**: Calls `glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer.gl_buffer_id)`.
        - **Vulkan**: Allocates a temporary descriptor set (or uses a pre-allocated one) that describes the buffer binding, then records `vkCmdBindDescriptorSets` for that set.

#### Dispatching Work (SituationCmdDispatch)
- **Signature:** `SITAPI void SituationCmdDispatch(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);`
- **Parameters:**
    - `cmd`: The command buffer.
    - `group_count_x/y/z`: The number of local work groups to dispatch in each dimension. The total number of invocations is `group_count_x * group_count_y * group_count_z * local_size_x * local_size_y * local_size_z` (where local size is defined in the shader).
- **Process:**
    1.  Validates the command buffer.
    2.  Records the command to dispatch the compute work.
    3.  Backend-Specific:
        - **OpenGL**: Calls `glDispatchCompute(group_count_x, group_count_y, group_count_z)`.
        - **Vulkan**: Calls `vkCmdDispatch(cmd, group_count_x, group_count_y, group_count_z)`.

### Synchronization & Memory Barriers
#### Importance of Synchronization in Compute
GPU operations, including compute shaders, can execute asynchronously and out-of-order relative to CPU commands and other GPU operations. Memory barriers are crucial to ensure that reads happen after writes, and that dependencies between operations are correctly observed.

#### Using SituationCmdPipelineBarrier
The primary tool for synchronization is `SituationCmdPipelineBarrier`. It provides fine-grained control by explicitly defining the source of a memory dependency (the producer stage) and the destination (the consumer stage). This allows the driver to create a much more efficient barrier than a coarse, "sledgehammer" approach.

- **Signature:** `SITAPI void SituationCmdPipelineBarrier(SituationCommandBuffer cmd, uint32_t src_flags, uint32_t dst_flags);`
- **Parameters:**
    - `cmd`: The command buffer to record the barrier into.
    - `src_flags`: A bitmask of `SituationBarrierSrcFlags`.
    - `dst_flags`: A bitmask of `SituationBarrierDstFlags`.

**Source Flags (`SituationBarrierSrcFlags`):**
| Flag | Description |
|---|---|
| `SITUATION_BARRIER_VERTEX_SHADER_WRITE` | Vertex shader wrote to an SSBO or Image. |
| `SITUATION_BARRIER_FRAGMENT_SHADER_WRITE` | Fragment shader wrote to an SSBO, Image, or Color Attachment. |
| `SITUATION_BARRIER_COMPUTE_SHADER_WRITE` | Compute shader wrote to a Storage Buffer or Image. |
| `SITUATION_BARRIER_TRANSFER_WRITE` | A transfer operation (Copy, Blit, Fill) wrote data. |

**Destination Flags (`SituationBarrierDstFlags`):**
| Flag | Description |
|---|---|
| `SITUATION_BARRIER_VERTEX_SHADER_READ` | Vertex shader will read data (SSBO, Image, Vertex Attribute). |
| `SITUATION_BARRIER_FRAGMENT_SHADER_READ` | Fragment shader will read data. |
| `SITUATION_BARRIER_COMPUTE_SHADER_READ` | Compute shader will read data. |
| `SITUATION_BARRIER_TRANSFER_READ` | A transfer operation will read data. |
| `SITUATION_BARRIER_INDIRECT_COMMAND_READ` | Data will be read as indirect command parameters. |

- **Process:** This function maps the abstract source and destination flags to the precise stage and access masks required by the underlying API (`vkCmdPipelineBarrier` in Vulkan or a combination of flags for `glMemoryBarrier` in OpenGL).
- **Example: GPU Particle Simulation**
A common use case is updating particle positions in a compute shader and then immediately rendering them. A barrier is required between the dispatch and the draw call.
```c
// 1. Dispatch compute shader to update particle data in an SSBO
SituationCmdBindComputePipeline(cmd, particle_update_pipeline);
SituationCmdBindComputeBuffer(cmd, 0, particle_data_ssbo);
SituationCmdDispatch(cmd, PARTICLE_GROUPS, 1, 1);

// 2. *** CRITICAL BARRIER ***
//    Ensure the compute shader's writes to the SSBO are finished and visible
//    before the vertex shader stage attempts to read that data as vertex attributes.
SituationCmdPipelineBarrier(cmd,
                          SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
                          SITUATION_BARRIER_VERTEX_SHADER_READ);

// 3. Draw the particles using a graphics pipeline
SituationCmdBindPipeline(cmd, particle_render_pipeline);
// The same SSBO is now used as the source for vertex data
SituationCmdBindVertexBuffer(cmd, particle_data_ssbo);
SituationCmdDraw(cmd, PARTICLE_COUNT, 1, 0, 0);
```
- **Note on `SituationMemoryBarrier`:**
The library also provides a simpler, deprecated function `SituationMemoryBarrier(cmd, barrier_bits)`. This function is less optimal as it creates a very coarse barrier. It is provided for backward compatibility or extremely simple cases. For all new development, **`SituationCmdPipelineBarrier` is strongly recommended.**

### Destroying Compute Pipelines (SituationDestroyComputePipeline)
- **Signature:** `SITAPI void SituationDestroyComputePipeline(SituationComputePipeline* pipeline);`
- **Parameters:**
    - `pipeline`: A pointer to the `SituationComputePipeline` handle to be destroyed. The handle's `id` field will be set to 0 upon successful destruction.
- **Process:**
    1.  Validates the input pointer and that the pipeline has a non-zero `id`.
    2.  Removes the pipeline from the internal tracking list.
    3.  Backend-Specific Cleanup:
        - **OpenGL**: Calls `glDeleteProgram(pipeline->gl_program_id)`.
        - **Vulkan**:
            a. Waits for the device to be idle (`vkDeviceWaitIdle`) to ensure no commands using the pipeline are pending.
            b. Destroys the Vulkan objects: `vkDestroyPipeline`, `vkDestroyPipelineLayout`.
    4.  Invalidates the handle by setting `pipeline->id = 0`.

### Compute Presentation (SituationCmdPresent)
- **Signature:** `SITAPI SituationError SituationCmdPresent(SituationCommandBuffer cmd, SituationTexture texture);`
- **Description:** Submits a command to copy a texture to the main window's swapchain. This is specifically designed for **Compute-Only** applications where there is no standard render pass (e.g., ray tracing or terminal emulators) and you need to display the result of a compute shader (a storage image) directly to the screen.

</details>
<details>
<summary><h3>Text Rendering</h3></summary>

### Simple Text Drawing (SituationDrawTextSimple)
- **Signature:** `SITAPI void SituationDrawTextSimple(const char* text, float x, float y, float scale, ColorRGBA color);`
- Draws text character by character using a simple, built-in font (often 8x8 or similar bitmap).
- Parameters define position (`x`, `y`), size (`scale`), and color.
- **Note:** As indicated in the library code comments, this method is intentionally slow and intended for debugging or simple UI elements where performance is not critical.

### Styled Text Rendering (SituationDrawTextStyled)
- **Signature:** `SITAPI void SituationDrawTextStyled(SituationFont font, const char* text, float x, float y, float font_size, ColorRGBA color);`
- Draws high-quality text using pre-rendered font atlases (textures) and Signed Distance Fields (SDF).
- Requires a `SituationFont` handle, obtained via font loading functions.
- Offers superior performance and visual quality (smooth scaling, sharp edges) compared to `SituationDrawTextSimple`.
- Parameters define the font, text string, position, size (`font_size`), and color.

### Font Loading & Management
- `SITAPI SituationError SituationLoadFont(const char* fileName, SituationFont* out_font);`
    - Loads a TrueType Font (TTF) file.
    - Internally uses `stb_truetype` to parse the font and generate SDF data for an atlas texture.
    - Returns `SITUATION_SUCCESS` on success.
- `SITAPI void SituationUnloadFont(SituationFont font);`
    - Destroys a loaded font, freeing the associated atlas texture and `stbtt_fontinfo` data.

### GPU Text Drawing (Command Buffer)
For best performance and integration with the rendering pipeline, use these command-buffer variants.

- **`SituationCmdDrawText`**
    - **Signature:** `SITAPI SituationError SituationCmdDrawText(SituationCommandBuffer cmd, SituationFont font, const char* text, Vector2 pos, ColorRGBA color);`
    - Draws a text string using GPU-accelerated textured quads. This is the preferred method for rendering text within a render pass.

- **`SituationCmdDrawTextEx`**
    - **Signature:** `SITAPI SituationError SituationCmdDrawTextEx(SituationCommandBuffer cmd, SituationFont font, const char* text, Vector2 pos, float fontSize, float spacing, ColorRGBA color);`
    - Advanced version of `SituationCmdDrawText` that allows for custom scaling (`fontSize`) and character spacing (`spacing`).

</details>
<details>
<summary><h3>2D Rendering & Drawing</h3></summary>

### Overview
While "Situation" is a powerful 3D rendering library, it also provides a comprehensive and high-performance suite of tools specifically for classic 2D drawing. This is ideal for building user interfaces, debugging overlays, data visualizations, or complete 2D games. All 2D drawing functions operate within the Command Buffer model.

### 2D Coordinate System & Camera
For all 2D drawing commands, "Situation" automatically establishes a 2D orthographic coordinate system. The origin (0, 0) is at the **top-left** corner of the current render target (either the main window or a Virtual Display). The X-axis increases to the right, and the Y-axis increases downwards. You do not need to set up a 3D camera or projection matrix; the library manages this internally for all `SituationCmdDraw*` 2D functions.

### Drawing Basic Shapes
The library provides commands for rendering primitive geometric shapes, which form the building blocks of any 2D scene.

#### Rectangles (SituationCmdDrawQuad)
This is the primary function for drawing solid-colored rectangles. It uses the library's internal, optimized quad renderer.
- **Signature:** `SITAPI SituationError SituationCmdDrawQuad(SituationCommandBuffer cmd, mat4 model, Vector4 color);`
- `model`: A `mat4` transformation matrix used to set the rectangle's position, size, and rotation. Use `cglm` helpers (`glm_translate`, `glm_scale`, `glm_rotate`) to build this matrix.
- `color`: A normalized `Vector4` representing the RGBA color of the quad.

#### Lines & Circles (Concept)
While not yet implemented, the API is designed to easily accommodate high-level commands for drawing other primitives like lines (`SituationCmdDrawLine`), circles (`SituationCmdDrawCircle`), and polygons.

### Drawing Textures (Sprites)
This is the core of 2D rendering, allowing you to draw images and sprite sheets to the screen.

#### Loading Textures
First, load your image file into a `SituationTexture` handle using the functions described in section `4.6.3`.
- `SituationTexture my_sprite = SituationCreateTexture(SituationLoadImage("assets/player.png"), true);`

#### Drawing a Texture (SituationCmdDrawTexture)
This unified high-level command draws a texture (or part of it) with full control over source region, destination rectangle, rotation, origin, and color tint.
- **Signature:** `SITAPI SituationError SituationCmdDrawTexture(SituationCommandBuffer cmd, SituationTexture texture, Rectangle source, Rectangle dest, Vector2 origin, float rotation, ColorRGBA tint);`
- `texture`: The `SituationTexture` to draw.
- `source`: The rectangular region of the texture to draw (for sprite sheets). Use a full rect `{0,0,w,h}` for the whole texture.
- `dest`: The destination rectangle on the screen, defining position and size.
- `origin`: The rotation pivot point, relative to the top-left of the destination rectangle. `(0,0)` pivots from the top-left corner.
- `rotation`: The rotation in degrees (clockwise).
- `tint`: A `ColorRGBA` multiplier. White `{255,255,255,255}` draws the texture with its original colors.

### Text Rendering
The library includes a powerful text rendering system suitable for UI, HUDs, and any in-game text. For a full API reference, see section `4.9`.
- **High-Quality Styled Text:** Use `SituationDrawTextStyled` for crisp, anti-aliased text with support for TrueType fonts (.ttf). This is the recommended function for all user-facing text.
- **Simple Debug Text:** Use `SituationDrawTextSimple` for quick, unstyled text output, ideal for debugging information where performance and visual quality are not critical.

### UI & Layer Management
"Situation" provides two key features that are essential for building complex 2D UIs and managing render layers.

#### Scissor/Clipping (SituationCmdSetScissor)
The scissor command restricts all subsequent drawing to a specific rectangular area on the screen. This is indispensable for creating UI elements like scrollable lists, text boxes, or windows where content must be clipped to a boundary.
- See section `4.6.8.5` for the full API reference.
- **Example workflow:**
  1. Call `SituationCmdSetScissor(cmd, panel_x, panel_y, panel_width, panel_height);`
  2. Draw all content that should appear inside the panel.
  3. Disable the scissor by setting it to the full screen size.

#### Virtual Displays as UI Layers
The Virtual Display system (see `4.6.5`) is a perfect tool for 2D layer management. You can render an entire UI screen or game layer to an off-screen Virtual Display first. This allows you to:
- Apply shaders and post-processing effects to the entire UI layer at once.
- Scale a low-resolution UI to a high-resolution screen with pixel-perfect filtering (`SITUATION_SCALING_INTEGER`).
- Easily manage render order using the `z_order` property when compositing the layers back to the main window.
</details>

---
---

# Examples & Tutorials

<details>
<summary><h3>Examples & Tutorials</h3></summary>

### Terminal Module
The "Terminal" module is a comprehensive extension subsystem that provides a complete VT100/xterm-compatible terminal emulator. It is located in `sit/terminal/` and integrates deeply with the Situation rendering and input pipeline.

#### Overview
The terminal library emulates a wide range of standards (VT52 through VT420/xterm) and supports modern features like True Color (24-bit), mouse tracking (SGR), and bracketed paste. It uses a **Compute Shader** based rendering pipeline (`SIT_COMPUTE_LAYOUT_TERMINAL`) to efficiently render the character grid, attributes, and colors on the GPU.

#### Key APIs
-   `InitTerminal()`: Initializes the terminal state and compute resources.
-   `UpdateTerminal()`: Processes input, updates state, and manages the host-terminal pipeline.
-   `DrawTerminal()`: Renders the terminal to the screen using a compute shader dispatch.
-   `PipelineWrite...()`: Functions to send data (text, escape sequences) to the terminal emulation.
-   `SetVTLevel()`: Configures the emulation compliance level.

#### Integration
The terminal module relies on the `SIT_COMPUTE_LAYOUT_TERMINAL` layout defined in `situation.h`. This layout configures the descriptor sets required by the terminal's compute shader:
1.  **Set 0 (SSBO):** Contains the terminal grid data (`GPUCell` array).
2.  **Set 1 (Storage Image):** The target image for the rendered output.
3.  **Set 2 (Sampler):** The font atlas texture.

For detailed documentation, see the [Terminal Technical Reference](sit/terminal/terminal.md) and [Terminal README](sit/terminal/README.md).

### Basic Triangle Rendering
This example demonstrates the minimal steps required to render a single, colored triangle using `situation.h`. It covers window setup, shader creation, mesh definition, and the core rendering loop.

The full source code for this example can be found in `examples/basic_triangle.c`.

### Loading and Rendering a 3D Model
This example shows how to load a 3D model from a file (e.g., Wavefront .OBJ) and render it using `situation.h`. It assumes the existence of a function like `SituationLoadModelFromObj` (based on library snippets) or a similar model loading mechanism.

The full source code for this example can be found in `examples/loading_and_rendering_a_3d_model.c`.

### Playing Background Music
This example demonstrates how to load and play a sound file (e.g., WAV, OGG) in a continuous loop using the audio capabilities of `situation.h`.

The full source code for this example can be found in `examples/playing_background_music.c`.

### Handling Keyboard and Mouse Input
This example shows how to query the state of keyboard keys and the mouse position within the main application loop, and how to use this input to control simple application behavior (e.g., moving an on-screen element or closing the window).

The full source code for this example can be found in `examples/handling_keyboard_and_mouse_input.c`.

### Compute Shader Example: Image Processing (SSBO Version - Updated for Persistent Descriptor Sets)
This example demonstrates using compute shaders with `situation.h` to perform a simple image processing task (inverting colors) by reading from and writing to Shader Storage Buffer Objects (SSBOs). This approach uses the confirmed API function `SituationCmdBindComputeBuffer`, which now correctly implements the high-performance, persistent descriptor set model for Vulkan.

The full source code for this example can be found in `examples/compute_shader_image_processing.c`.

#### Problem Definition (Updated)
We want to take the pixel data of an image (already loaded into a `SituationBuffer` configured as an SSBO) and invert its colors using the GPU compute shader. The result will be written to another `SituationBuffer`.
**Crucial Note on Performance:** The library now ensures that binding these buffers for the compute shader is highly efficient. When a `SituationBuffer` is created using `SituationCreateBuffer`, the Vulkan backend internally:
1.  Allocates a `VkBuffer` and `VmaAllocation`.
2.  **Crucially:** Allocates a *persistent* `VkDescriptorSet` from a dedicated pool (`sit_gs.vk.persistent_descriptor_pool`).
3.  Immediately populates this descriptor set with the buffer's `VkBuffer` handle.
4.  Stores this `VkDescriptorSet` within the `SituationBuffer` struct (`buffer.descriptor_set`).
This means that subsequent binding operations are extremely fast, as they do not involve any runtime allocation or update of descriptor sets.

#### Compute Shader Code (GLSL using SSBOs)
The shader reads RGBA values from an input SSBO, inverts them, and writes the result to an output SSBO. Each invocation processes one pixel.

#### Host Code Walkthrough (Init, Create, Bind Buffers, Dispatch, Sync, Destroy)
This C code shows how to prepare data, create buffers, load the shader, bind resources, dispatch the compute job, synchronize, and clean up using the *existing* `situation.h` API.

### Example: GPU Particle Simulation and Rendering (Concept)
This example concept demonstrates a fundamental and powerful technique: combining compute and graphics pipelines within a single frame. It illustrates how to use a compute shader to update simulation data (like particle positions and velocities) stored in GPU buffers, and then immediately use a standard graphics pipeline to render the results in the same frame.

A conceptual implementation for this example can be found in `examples/gpu_particle_simulation.c`.

#### Scenario
The core idea is to perform calculations on the GPU (using a compute shader) and then visualize the results (using a graphics shader) without stalling the pipeline or introducing race conditions.

1. **Compute Shader:** A compute shader operates on a buffer of particle data (e.g., struct { vec2 position; vec2 velocity; }). It reads the current state,
applies simulation logic (e.g., physics updates like velocity integration, applying forces), and writes the new state back to the same or a different buffer.
2. **Graphics Shader:** A vertex shader (potentially using instancing) reads the updated particle data from the buffer and uses it to position geometry
(e.g., a quad or sprite) for each particle on the screen.
3. **Synchronization:** The critical aspect is ensuring the compute shader's writes are globally visible and finished before the vertex shader attempts
to read that data. This requires explicit synchronization.

#### Key APIs Demonstrated
This example concept highlights the interaction between several situation.h APIs:

- **SituationCreateBuffer / SituationDestroyBuffer:** Used to create the GPU buffer(s) that will store the particle simulation data (positions, velocities).
These buffers must be created with appropriate usage flags (e.g., SITUATION_BUFFER_USAGE_STORAGE_BUFFER for compute read/write, potentially
SITUATION_BUFFER_USAGE_VERTEX_BUFFER if used as such in the graphics pipeline, or bound via SituationCmdBindUniformBuffer if accessed as an SSBO).
- **SituationCreateComputePipelineFromMemory / SituationDestroyComputePipeline:**
Used to create the compute pipeline that will execute the particle update logic.
- **SituationCmdBindComputePipeline:** Binds the compute pipeline for subsequent dispatch commands.
- **SituationCmdBindComputeBuffer:** Binds the particle data buffer to a specific binding point within the compute shader's descriptor set.
- **SituationCmdDispatch:** Launches the compute shader work groups to perform the particle simulation update.
- **SituationMemoryBarrier:** Crucially, this function is used after the compute dispatch and before the graphics draw call. It inserts a memory and execution
barrier to ensure all compute shader invocations have completed their writes (SITUATION_BARRIER_COMPUTE_SHADER_STORAGE_WRITE) and that these writes are
visible to the subsequent graphics pipeline stages that will read the data (SITUATION_BARRIER_VERTEX_SHADER_STORAGE_READ or similar). Without this
barrier, the graphics pipeline might read stale or partially updated data.
- **SituationCmdBindPipeline (Graphics):** Binds the graphics pipeline used for rendering the particles.
- **SituationCmdBindVertexBuffer / SituationCmdBindIndexBuffer:** Binds the mesh data (e.g., a simple quad) used for instanced rendering of particles.
- **SituationCmdBindUniformBuffer / SituationCmdBindTexture:** Binds resources needed by the graphics shaders (e.g., the particle data buffer if accessed as an SSBO, textures for particle appearance).
- **SituationCmdDrawIndexedInstanced / SituationCmdDrawInstanced:** Renders the particle geometry, typically using instancing where the instance count equals
the number of particles, and the instance ID is used in the vertex shader to fetch data from the particle buffer.

#### Purpose
This conceptual example should clarify the intended workflow for integrating compute-generated data into subsequent graphics rendering passes.
It emphasizes the essential role of SituationMemoryBarrier for correctness when sharing data between different pipeline types within the same command stream.
This bridges the gap between the existing separate compute and graphics examples, showing how they can be combined effectively.
</details>

---
---

# FAQ & Support

<details>
<summary><h3>Frequently Asked Questions (FAQ) & Troubleshooting</h3></summary>

### Common Initialization Failures
- **GLFW Init Failed:** Check GLFW installation, system libraries (X11 on Linux).
- **OpenGL Loader Failed:** Ensure \`GLAD\` is compiled and linked correctly when using \`SITUATION_USE_OPENGL\`.
- **Vulkan Instance/Device Failed:** Verify Vulkan SDK installation, compatible driver. Enable validation layers (\`init_info.enable_vulkan_validation = true;\`) for detailed errors.
- **Audio Device Failed:** Check system audio settings, permissions.

### "Resource Invalid" Errors
- Occur when trying to use a resource handle (Shader, Mesh, Texture, Buffer, ComputePipeline) that hasn't been created successfully (\`id == 0\`) or has already been destroyed.

### Performance Considerations
- Minimize state changes (binding different shaders, textures) within a single command buffer recording.
- Batch similar draw calls.
- Use \`SituationDrawTextStyled\` instead of \`SituationDrawTextSimple\` for significant text rendering.
- Profile your application to identify bottlenecks.

### Backend-Specific Issues (OpenGL vs. Vulkan)
- OpenGL might be easier to set up initially but can have driver-specific quirks.
- Vulkan offers more explicit control and potentially better performance but has a steeper learning curve and more verbose setup.

### Debugging Tips (Validation Layers, Error Messages)
- Always check the return value of \`SituationInit\` and resource creation functions.
- Use \`SituationGetLastErrorMsg()\` to get detailed error descriptions.
- For Vulkan, enable validation layers during development (\`init_info.enable_vulkan_validation = true;\`) to catch API misuse.
</details>

---

## License (MIT)

"Situation" is licensed under the permissive MIT License. In simple terms, this means you are free to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the software for both commercial and private projects. The only requirement is that you include the original copyright and license notice in any substantial portion of the software or derivative work you distribute. This library is provided "as is", without any warranty.

---

Copyright (c) 2025 Jacques Morel

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
