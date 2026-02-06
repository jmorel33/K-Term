<div align="center">
  <img src="K-Term.PNG" alt="K-Term Logo" width="933">
</div>

# K-Term Emulation Library v2.4.23
(c) 2026 Jacques Morel

For a comprehensive guide, please refer to [doc/kterm.md](doc/kterm.md).

<details>
<summary>Table of Contents</summary>

1.  [Description](#description)
2.  [Key Features](#key-features)
3.  [How It Works](#how-it-works)
    1.  [3.1. Main Loop and Initialization](#31-main-loop-and-initialization)
    2.  [3.2. Input Pipeline and Character Processing](#32-input-pipeline-and-character-processing)
    3.  [3.3. Escape Sequence Parsing](#33-escape-sequence-parsing)
    4.  [3.4. Keyboard and Mouse Handling](#34-keyboard-and-mouse-handling)
    5.  [3.5. GPU Rendering Pipeline](#35-gpu-rendering-pipeline)
    6.  [3.6. Callbacks](#36-callbacks)
4.  [How to Use](#how-to-use)
    1.  [4.1. Basic Setup](#41-basic-setup)
    2.  [4.2. Sending Data to the KTerm (Simulating Host Output)](#42-sending-data-to-the-terminal-simulating-host-output)
    3.  [4.3. Receiving Responses and Key Events from the Terminal](#43-receiving-responses-and-key-events-from-the-terminal)
    4.  [4.4. Configuring KTerm Behavior](#44-configuring-terminal-behavior)
    5.  [4.5. Advanced Features](#45-advanced-features)
    6.  [4.6. Window and Title Management](#46-window-and-title-management)
    7.  [4.7. Diagnostics and Testing](#47-diagnostics-and-testing)
    8.  [4.8. Scripting API](#48-scripting-api)
    9.  [4.9. Post-Flush Grid Access](#49-post-flush-grid-access)
5.  [Configuration Constants](#configuration-constants)
6.  [Key Data Structures](#key-data-structures)
7.  [Implementation Model](#implementation-model)
8.  [Dependencies](#dependencies)
9.  [License](#license)

</details>

## Description

**K-Term** (`kterm.h`) is a production-ready, single-header C library delivering the most comprehensive and faithful terminal emulation available today — spanning the full DEC lineage from VT52 to VT525, classic xterm behavior, and cutting-edge modern extensions.

With museum-grade legacy compliance, full Kitty graphics protocol support (animations, transparency, z-index layering, pane clipping), recursive arbitrary pane multiplexing (tmux-style tree layouts with dynamic splits and compositing), GPU-accelerated ReGIS/Tektronix/Sixel graphics, rich styling (curly/dotted/dashed underlines, colored underline/strikethrough, attribute stacking), independent blink rates, and authentic CRT effects (phosphor glow, scanlines, curvature), K-Term is unmatched for both historical accuracy and modern capability.

Designed for seamless embedding in embedded systems, development tools, IDE plugins, remote access clients, retro emulators, and GPU-accelerated applications, it leverages the **Situation** framework for cross-platform hardware-accelerated rendering and input while providing a thread-safe, lock-free architecture for massive throughput.

For a detailed compliance review, see [doc/DEC_COMPLIANCE_REVIEW.md](doc/DEC_COMPLIANCE_REVIEW.md).

**New in v2.4.23: Gateway Grid Banner**
*   **Gateway Extensions**: Added `banner` subcommand to the `grid` extension (`EXT;grid;banner;...`).
    *   **Direct Plotting**: Plots large, scaled text directly onto the grid using the built-in 8x8 bitmap font.
    *   **Features**: Supports integer scaling, text alignment (Left/Center/Right), masked styling, and intelligent kerning (`kern=1`).
    *   **Performance**: Bypasses escape sequence parsing for high-performance dashboard/overlay construction.

**New in v2.4.22: Gateway Grid Shapes**
*   **Gateway Extensions**: Expanded `grid` extension with `fill_circle` and `fill_line` subcommands.
    *   `fill_circle`: Draws filled circles using the midpoint algorithm.
    *   `fill_line`: Draws linear spans in cardinal directions (h, v) with optional wrapping for horizontal lines.
*   **Fixes**: Fixed `fill` subcommand to correctly handle 0-width/height arguments (defaulting to 1).

**New in v2.4.10: Shader-based Bold & Italic Simulation**
*   **Visuals:** Implemented shader-based simulation for **Bold** (smear effect) and **Italic** (coordinate skew) text attributes in `shaders/terminal.comp`.
*   **Accuracy:** Added bounds checking to texture sampling to prevent atlas bleeding when distorting glyph coordinates.
*   **Completeness:** These attributes are now visually distinct even when using the standard bitmap font, improving readability and styling support.

**New in v2.4.9: Session Routing, Queries, Macros, State Snapshot**
*   **Per-Session Response Routing:** Fixed critical data contention issues in multi-session environments. Responses (`DSR`, `OSC`) are now routed exclusively to the originating session's ring buffer (`response_ring`) instead of the active session, ensuring 100% reliable output handling for background tasks.
*   **Expanded Query Handlers:** Added support for `CSI ?10n` (Graphics Capabilities), `?20n` (Macro Storage), and `?30n` (Session State). Fixed `CSI 98n` (Error Reporting) to comply with standard syntax.
*   **Macro Acknowledgements:** Implemented robust success/error reporting for DCS macro definitions (`\x1BP1$sOK\x1B\` / `\x1BP1$sERR\x1B\`), enabling reliable upload flows for host applications.
*   **State Snapshot:** Added `DECRQSS` (`DCS $ q`) support for state snapshots (`state` argument), returning a packed, parsable string of the terminal's cursor, modes, and attributes for session persistence or debugging.

**New in v2.4.8: Parser Hardening & Fuzzing Support**
*   **Malicious Input Resilience:** Introduced configurable limits for Sixel graphics (`max_sixel_width`/`height`), Kitty graphics (`max_kitty_image_pixels`), and grid operations (`max_ops_per_flush`) to prevent DoS attacks via memory exhaustion or CPU hogging.
*   **Continuous Fuzzing:** Added `tests/libfuzzer_target.c` and GitHub Actions workflow for continuous regression fuzzing with libFuzzer/AddressSanitizer, targeting parser robustness against malformed sequences.
*   **Strict Mode:** Exposed `strict_mode` in `KTermConfig` to enforce stricter parsing rules.

**New in v2.4.7: Memory & Sanitizer Hardening**
*   **ReGIS Memory Safety:** Fixed memory leaks in ReGIS macro handling by properly freeing existing macros before re-initialization or reset commands (`RESET;REGIS`).
*   **Shutdown Cleanup:** Enhanced `KTerm_Cleanup` to ensure all internal buffers (e.g., `row_dirty`, `row_scratch_buffer`) are freed, eliminating leaks on session destruction.
*   **Buffer Hardening:** Mitigated buffer overflow risks in `KTerm_SetLevel` by enforcing correct size limits on the answerback buffer.
*   **Sanitizer Compliance:** The codebase is now verified clean under AddressSanitizer (ASan) and Valgrind, ensuring robust operation in CI environments.

**New in v2.4.6: Conversational UI Hardening**
*   **Synchronized Output (DECSET 2026):** Implemented support for synchronized updates (`\x1B[?2026h` ... `\x1B[?2026l`), preventing visual tearing during batched high-speed text output (e.g., LLM streaming).
*   **Focus Tracking:** Added Focus In/Out event reporting (`\x1B[I` / `\x1B[O`) via `KTerm_SetFocus`, enabling host applications to dim interfaces or pause updates when inactive.
*   **Clipboard Hardening:** Increased `MAX_COMMAND_BUFFER` to 256KB to robustly handle large `OSC 52` clipboard copy/paste payloads and improved verification tests for malicious base64 inputs.

**New in v2.4.5: Hardened Conversational Interface**
*   **Kitty Keyboard Protocol:** Full implementation of the progressive keyboard enhancement protocol (`CSI > 1 u`), enabling unambiguous key reporting (distinguishing `Tab` vs `Ctrl+I`, `Enter` vs `Ctrl+Enter`), explicit modifier reporting (Shift, Alt, Ctrl, Super), and key release events.
*   **Input Hardening:** Introduced `KTermKey` enum for standardized key codes and hardened the `ParseCSI` logic to correctly handle modern prefix parameters (e.g., `>`).
*   **Ring Buffer Integration:** Finalized the per-session `response_ring` integration for robust, lock-free output interleaving in high-throughput conversational scenarios (AI streaming, chat UIs).

**New in v2.4.4: Conversational Completeness & Output Refactor**
*   **Per-Session Output Ring:** Replaced the legacy linear output buffer with a lock-free, per-session ring buffer (`response_ring`). This ensures thread-safe, non-blocking response generation in multi-session environments and prevents data drops under load.
*   **Expanded Query Support:** Added new DSR queries for inspecting internal state:
    *   `CSI ? 10 n`: Graphics capabilities bitmask (Sixel, ReGIS, Kitty, etc.).
    *   `CSI ? 20 n`: Macro storage usage (slots used, free bytes).
    *   `CSI ? 30 n`: Session state (active ID, max sessions, scroll region).
    *   `CSI 98 n`: Internal error count.
*   **Macro Acknowledgments:** Macro definitions now reply with `DCS > ID ; Status ST` (0=Success, 1=Full, 2=Error), allowing hosts to verify uploads.
*   **Gateway State Snapshot:** New `GET;STATE` command returns a packed, comprehensive snapshot of the terminal's state (cursor, modes, attributes, scroll region) for seamless reattachment or debugging.

**New in v2.4.3: Multi-Session Graphics & Lifecycle Fixes**
*   **Tektronix Isolation:** Moved Tektronix graphics state from global storage to per-session storage, ensuring that vector graphics in one session do not bleed into others.
*   **Kitty Lifecycle:** Fixed Kitty graphics lifecycle issues during resize events. Images now correctly track their logical position in the scrollback buffer when the grid is resized, preventing visual artifacts or lost images.
*   **Initialization:** Standardized graphics subsystem initialization (Sixel, ReGIS, Tektronix, Kitty) within `KTerm_InitSession`.

**v2.4.1 Core Advancement: ReGIS Per-Session Architecture**
Moved ReGIS graphics state (cursor position, macros, colors) from the global `KTerm` struct to the per-session `KTermSession` struct. This ensures that in multi-session environments (e.g., split panes or background sessions), each terminal session maintains its own independent graphics context.

**v2.4.0 Core Advancement: Mandatory Op Queue Decoupling**
All grid mutations are batched atomically via a lock-free queue — direct writes have been eliminated for absolute thread-safety and efficiency. The post-flush grid is pure and directly addressable, ready for external simulation layers, bytecode hooks, or custom extensions.

**New in v2.4.0:** Major Architecture Overhaul, Safety Hardening, and Feature Consolidation.
This release represents the culmination of the decoupling architecture, delivering a fully thread-safe and robust engine.

*   **Mandatory Op Queue:** All mutations (text, scroll, rect ops, vertical ops, resize) are unconditionally queued via `KTermOpQueue`. This atomic batching ensures thread safety and removes all direct mutation paths.
*   **Grid Post-Flush Purity:** The cells array is guaranteed to be consistent and addressable after `KTerm_Update` (flush), allowing for safe external logic (simulation, bytecode, extensions) to read/write the grid.
*   **JIT Text Shaping:** Dynamic grouping of characters (e.g., base + combining marks) for correct rendering without complex grid storage.
*   **Sink Output Pattern:** Zero-copy, high-throughput data output via `KTerm_SetOutputSink`.
*   **Layout Engine:** Multiplexer logic decoupled into `kt_layout.h`, separating geometry management from the emulation core.
*   **Security & Stability:** Hardened parser using `StreamScanner`, memory safety wrappers, and structured error reporting.

*   **DEC Compliance & Emulation:**
    *   **Full VT500/VT525 Support:** Implemented esoteric features (DECRQTSR, DECRQUPSS, DECARR, DECRQDE), Soft Fonts (DECDLD), and Rectangular Operations (DECCRA, DECFRA, etc.).
    *   **Protected Cells:** Full support for DECSCA protected attributes in all destructive operations.
    *   **Unicode:** Opt-in `enable_wide_chars` mode with correct `wcwidth` logic for CJK and emoji.
    *   **Enhanced Modes:** Added support for Column Mode (DECCOLM), Page Mode, Sixel Display Mode, and Infinite Scrollback.

*   **Gateway Protocol & Visuals:**
    *   **Protocol Expansion:** Added commands for dynamic resizing (`SET;WIDTH`), session targeting (`SET;SESSION`), banners, and pipes.
    *   **Graphics:** Enhanced Sixel, ReGIS, and Kitty protocol support with robust reset mechanisms and session routing.
    *   **Styling:** Added SGR 51 (Framed), 52 (Encircled), curly/dotted underlines, and extended True Color support.

**v2.3 Major Update:** This release consolidates the extensive stability, thread-safety, and compliance improvements.

*   **Thread Safety & Architecture:**
    *   **Robust Locking:** Implemented Phase 3 Coarse-Grained Locking with `pthread` mutexes for `KTerm` and `KTermSession`, fixing race conditions in `KTerm_Resize` and `KTerm_Update`.
    *   **Lock-Free Input:** Converted the input pipeline to a lock-free Single-Producer Single-Consumer (SPSC) queue using C11 atomics (Phase 2), supporting high-throughput injection.
    *   **Session Isolation:** Decoupled background session processing from the global `active_session` state and refactored internal update logic for thread safety.
    *   **Memory Safety:** Added robust OOM checks during initialization and resizing.
    *   **Optimization:** Refactored character attributes to use bit flags (`uint32_t`), reducing memory footprint per cell.

*   **DEC Compliance & Emulation:**
    *   **Printing:** Implemented DEC Print Form Feed (DECPFF) and Printer Extent (DECPEX) modes.
    *   **Geometry:** Implemented Allow 80/132 Column Mode (Mode 40), DECCOLM (Mode 3) resizing, Left/Right Margins (DECLRMM), and No Clear on Switch (DECNCSM).
    *   **Sixel:** Added Sixel Display Mode (DECSDM) and Sixel Cursor Mode (Private Mode 8452).
    *   **Input/Editing:** Implemented Backarrow Key Mode (DECBKM), Extended Edit Mode (DECEDM), DEC Locator Enable (DECELR), and Numeric Keypad Mode (DECNKM).
    *   **Legacy:** Added ANSI/VT52 Mode Switching (DECANM) and IBM DOS ANSI Mode (ANSI.SYS emulation).
    *   **State:** Implemented Alt Screen Cursor Save Mode (Mode 1041) and fixed Tab Stop logic with dynamic allocation.

*   **Visuals, Graphics & Typography:**
    *   **Rich Styling:** Added support for Curly, Dotted, and Dashed underlines (SGR 4:x), plus extended colors for Underline and Strikethrough (SGR 58).
    *   **Attribute Stack:** Implemented `XTPUSHSGR`/`XTPOPSGR` to push/pop attributes.
    *   **Fonts:** Added `KTermFontMetric` for precise per-font ink bounds, OSC 50 font loading, and automatic glyph centering.
    *   **Blink:** Implemented independent Fast, Slow, and Background blink flavors.
    *   **Vector Graphics:** Fixed aspect ratio scaling for ReGIS vectors.

*   **Multiplexer & Session Management:**
    *   **Pane Management:** Implemented correct pane closure logic (`KTerm_ClosePane`) with tree pruning, and added `Ctrl+B x` keybinding.
    *   **Split Screen:** Corrected initialization visibility for split panes.
    *   **Performance:** Optimized resize operations to only copy populated history rows.

*   **Gateway Protocol & API:**
    *   **Modular Architecture:** The Gateway Protocol logic has been extracted to a separate `kt_gateway.h` module (Phase 1-3 completion). This reduces core complexity and allows the feature to be fully disabled via `KTERM_DISABLE_GATEWAY` for minimal builds.
    *   **Banners:** Added `PIPE;BANNER` command to generate ASCII-art banners with gradients, alignment, and font selection.
    *   **Configuration:** Added `SET;ATTR` (runtime attributes), `SET;GRID` (debug grid), `SET;CONCEAL`, `SET;OUTPUT`, and `GET;OUTPUT`.
    *   **Controls:** Added `SET`/`RESET` commands for global attributes and blink rates.
    *   **API:** Introduced `auto_process` mode for manual input handling and `KTerm_SetResponseEnabled`.

**v2.2.0 Major Update:** This release marks a significant milestone, delivering the complete **Multiplexer & Graphics Update**, consolidating all features developed throughout the v2.1.x cycle.
*   **Multiplexer & Compositor:**
    *   **Recursive Pane Layout:** Full support for arbitrary split-screen layouts using a recursive `KTermPane` tree structure.
    *   **Dynamic Resizing:** Robust resizing logic with `SessionResizeCallback` integration.
    *   **Compositor Engine:** Optimized recursive rendering pipeline with persistent scratch buffers and transparency support for background layering.
    *   **Session Management:** Support for up to 4 independent sessions (VT525 standard) with `Ctrl+B` keybindings for splitting (`%`, `"`), and navigation (`o`, `n`).
*   **Kitty Graphics Protocol:**
    *   **Full Implementation:** Complete support for the protocol, including chunked transmission (`m=1`), placement (`a=p`), and deletion (`a=d`).
    *   **Animation:** Automatic animation of multi-frame images (`a=f`) with frame delay handling.
    *   **Z-Index & Composition:** Proper layering with `z<0` (behind text) and `z>=0` (above text) support, including correct transparency handling.
    *   **Integration:** Features scrolling images, clipping to split panes, and intelligent default placement.
*   **Graphics & Rendering Improvements:**
    *   **ReGIS Resolution Independence:** The graphics engine now supports the ReGIS `S` (Screen) command with `E` (Erase) and `A` (Addressing) options for flexible logical coordinate systems.
    *   **Sixel Improvements:** Added HLS color support.
    *   **Correctness:** Improved string terminator logic for OSC/DCS/APC sequences and accurate CP437 mapping for the default font.
*   **Production Ready:** This release consolidates all Phase 1-4 features into a stable, high-performance terminal emulation solution.

**v2.1.0 Major Update:** This release focuses on **Architecture Decoupling** and **Visual Accuracy**, consolidating features that were incrementally released during the v2.0.x cycle.
*   **Architectural Decoupling:** The library has been significantly refactored to separate concerns, enabling easier porting.
    *   **Input Decoupling:** Input handling is now isolated in `kt_io_sit.h` with a hardened `KTermEvent` system, allowing the core logic to be window-system agnostic.
    *   **Render Decoupling:** Rendering logic now relies on the `kt_render_sit.h` abstraction layer, removing direct dependencies on the Situation library from the core header.
*   **Museum-Grade Visuals:** The default font has been updated to an authentic DEC VT220 8x10 font with precise G0/G1 charset mapping and Special Graphics character support for pixel-perfect historical accuracy.
*   **Enhanced Flexibility:** Support for flexible window resizing and restored printer controller functionality.

The library implements a complete set of historical and modern terminal standards, ensuring compatibility with **VT52, VT100, VT220, VT320, VT340, VT420, VT510, VT520, VT525, and xterm**.

Beyond standard text emulation, `kterm.h` features a **GPU-accelerated rendering pipeline** using Compute Shaders, enabling advanced visual capabilities such as:
*   **Vector Graphics:** Full support for Tektronix 4010/4014 and ReGIS (Remote Graphics Instruction Set).
*   **Raster Graphics:** Sixel bitmap graphics.
*   **Modern Visuals:** True Color (24-bit) support, dynamic Unicode glyph caching, and retro CRT effects.
*   **Multi-Session Management:** Simultaneous support for independent sessions (up to 4, depending on VT level) with split-screen compositing.

The library processes a stream of input characters (typically from a host application or PTY) and updates an internal screen buffer. This buffer, representing the terminal display, is then rendered to the screen via the GPU. It handles a wide range of escape sequences to control cursor movement, text attributes, colors, screen clearing, scrolling, and various terminal modes.

## Key Features

-   **Mandatory Op Queue Decoupling:** Atomic batching for all mutations — thread-safe, efficient, and robust.
-   **Post-Flush Grid Access:** Direct, safe poking for custom logic after the update cycle.
-   **Compute Shader Rendering:** High-performance SSBO-based text rendering pipeline.
-   **Multi-Session Support:** Independent terminal sessions (up to 4) with split-screen compositing and dynamic pane layouts (tmux-style).
-   **Gateway Protocol:** Runtime introspection and configuration via `DCS GATE` sequences (hardened in v2.3.24).
-   **Vector Graphics Engine:** GPU-accelerated Tektronix 4010/4014 and ReGIS graphics support with "storage tube" phosphor glow simulation.
-   **Sixel Graphics:** Full implementation including HLS color support.
-   **Kitty Graphics Protocol:** Complete support for animations, transparency, z-index layering, and pane clipping.
-   **Soft Fonts:** DECDLD (Down-Line Loadable) font support for custom typefaces.
-   **Rectangular Operations:** Full VT420 suite (DECCRA, DECFRA, DECERA, DECSERA, DECCARA, DECRARA).
-   **VT Compliance:** VT52, VT100, VT102, VT220, VT320, VT340, VT420, VT510, VT520, VT525, and xterm compatibility.
-   **True Color:** 24-bit True Color (RGB) support for text and styling.
-   **Internationalization:** ISO 2022, NRCS (National Replacement Character Sets), and UTF-8 with overlong encoding protection.
-   **Typography:** Accurate double-width/double-height lines, curly/dotted/dashed underlines, strikethrough, and colored attributes.
-   **Input Handling:** Software keyboard repeater, bracketed paste, and User-Defined Keys (DECUDK).
-   **Esoteric Features:** DECRQTSR, DECRQUPSS, DECARR, DECRQDE, DECST8C, DECXRLM, DECSNLS.
-   **Retro CRT Effects:** Configurable screen curvature, scanlines, and visual bell.
-   **Safety:** Hardened against buffer overflows and integer exploits using `StreamScanner` and safe parsing primitives.
-   **Diagnostics:** Built-in test suite and verbose debug logging.

## How It Works

The library operates around a central `Terminal` structure (`KTerm`) that manages global resources (GPU buffers, textures) and a set of `KTermSession` structures, each maintaining independent state for distinct terminal sessions. The API is instance-based (`KTerm*`), supporting multiple terminal instances within a single application.

```mermaid
graph TD
    UserApp["User Application"] -->|"KTerm_Create"| TerminalInstance
    UserApp -->|"KTerm_Update(term)"| UpdateLoop
    UserApp -->|"KTerm_ProcessEvent"| EventDispatcher

    subgraph Terminal_Internals ["KTerm Library Internals"]
        TerminalInstance["KTerm Instance (Heap)"]
        EventDispatcher["Event Dispatcher"]

        EventDispatcher -->|"BYTES"| InputPipeline["SPSC Input Queue (1MB)<br/>(Overflow/Backpressure)"]
        EventDispatcher -->|"KEY"| EventQueue["Key Queue"]
        EventDispatcher -->|"MOUSE/RESIZE/FOCUS"| EventQueue

        subgraph Layout_System ["Multiplexer & Session Management"]
            LayoutTree["KTermPane Layout Tree"]
            Sessions["Sessions (0..3)"]
            ActiveSession["Active Session Pointer"]

            LayoutTree -.->|"Manages"| Sessions
        end

        TerminalInstance --> Layout_System

        subgraph Update_Cycle ["Update Cycle"]
            UpdateLoop["KTerm_Update Loop"]
            InputProc["Input Processor"]
            KeyProc["Key Processor"]
            Parser["VT/ANSI/Gateway Parser"]
            DirectInput["Direct Input Logic"]
            OpQueue["Op Queue Batching"]
            StateMod["State Modification"]

            UpdateLoop -->|"For Each Session"| InputProc
            UpdateLoop -->|"Process Keys"| KeyProc

            InputPipeline -->|"Batch Pop"| InputProc
            EventQueue --> KeyProc

            InputProc -->|"Byte Stream"| Parser
            InputProc -->|"RAWDUMP"| DumpLogic["Raw Dump Mirror"]
            DumpLogic -->|"Queue Ops"| OpQueue

            KeyProc -->|"Standard Mode"| KeyTranslate["KTerm_TranslateKey"]
            KeyProc -->|"Direct Mode"| DirectInput

            KeyTranslate -->|"Generate Sequence"| ResponseRing["Per-Session Response Ring"]
            Parser -->|"Queue Response"| ResponseRing

            Parser -->|"DCS GATE"| GatewayRouter["Gateway Router"]
            GatewayRouter -->|"Route"| GatewayExt["Extensions & Callbacks"]
            GatewayExt -->|"Response"| ResponseRing
            GatewayExt -->|"Config"| StateMod

            ResponseRing -->|"Drain"| Response["Response Callback (To Host)"]
            ResponseRing -->|"Drain (Zero-Copy)"| OutputSink["Output Sink (Optional)"]

            Parser -->|"Queue Ops"| OpQueue
            DirectInput -->|"Queue Ops"| OpQueue

            OpQueue -->|"Flush & Apply"| StateMod
            StateMod -->|"Update"| ScreenGrid["Screen Buffer"]
            StateMod -->|"Update"| Cursor["Cursor State"]
            StateMod -->|"Update"| Sixel["Sixel State (Per-Session)"]
            StateMod -->|"Update"| ReGIS["ReGIS State (Per-Session)"]
            StateMod -->|"Update"| Tektronix["Tektronix State (Per-Session)"]
            StateMod -->|"Update"| Kitty["Kitty Graphics State (Per-Session)"]
        end

        subgraph Rendering_Pipeline ["GPU Rendering Pipeline"]
            DrawCall["KTerm_Draw"]
            SSBO_Upload["KTerm_UpdateSSBO"]
            Compute["Text Compute Shader"]
            TextureBlit["Texture Blit Shader (Kitty)"]

            DrawCall --> SSBO_Upload
            SSBO_Upload -->|"Recursive Walk"| LayoutTree
            SSBO_Upload -->|"Upload"| GPU_SSBO["GPU SSBO Buffer"]

            DrawCall --> TextureBlit
            DrawCall --> Compute

            GPU_SSBO --> Compute
            FontTex["Dynamic Font Atlas"] --> Compute
            SixelTex["Sixel Texture"] --> Compute
            VectorTex["Vector Layer"] --> Compute

            TextureBlit -->|"Compositing (Images)"| OutputImage["Output Image"]
            Compute -->|"Compositing (Text/Sixel/Vector)"| OutputImage
        end
    end

    OutputImage -->|"Present"| Screen["Screen / Window"]
```

### 3.1. Main Loop and Initialization

-   `KTerm_Create()`: Allocates and initializes a new `Terminal` instance. It sets up memory for `KTermSession`s and configures GPU resources via the **Situation** backend (Compute Shaders, SSBOs, Textures). It initializes the dynamic glyph cache and default session states.
-   `KTerm_Update(term)`: The main heartbeat. It iterates through active sessions, processing the **Input Pipeline** (host data) and **Event Queue** (local input). Critically, it flushes the **Op Queue**, ensuring all batched grid mutations are applied atomically before rendering. It also manages timers and flushes the response buffer.
-   `KTerm_Draw(term)`: Executes the rendering pipeline. It updates the SSBO with the current frame's cell data and dispatches Compute Shaders to render text, graphics, and effects.

### 3.2. Input Pipeline (Host to Terminal)

-   Data from the host (PTY or application) is written to a session-specific input buffer using `KTerm_WriteChar(term, ...)` or `KTerm_WriteCharToSession(term, ...)`.
-   `KTerm_ProcessEvents(term)` consumes bytes from this buffer.
-   `KTerm_ProcessChar()` acts as the primary state machine dispatcher, routing characters based on the current parsing state.
-   Mutations (e.g., printing text, scrolling) are not applied immediately; instead, they are pushed to the `KTermOpQueue`. This decoupling ensures thread safety and allows for optimization.

### 3.3. Escape Sequence Parsing

-   The library implements a robust state machine compatible with VT500-series standards.
-   **CSI sequences** (e.g., `CSI H`) are parsed by accumulating parameters into `escape_params`.
-   **DCS sequences** (Device Control Strings) trigger specialized parsers for Sixel, ReGIS, Soft Fonts, and the **Gateway Protocol**.
-   **OSC sequences** handle window title changes and palette manipulation.

### 3.4. Keyboard and Mouse Handling (Local to Host)

-   **Decoupled Architecture:** KTerm does not poll hardware directly. The application pushes events using `KTerm_QueueInputEvent(term, event)`.
-   **Situation Adapter:** For applications using the Situation framework, `kt_io_sit.h` provides `KTermSit_ProcessInput(term)`, which automatically captures keyboard/mouse state, translates it to `KTermEvent`s, and queues them.
-   **Translation:** Events are translated into standard VT escape sequences (e.g., `ESC [ A` for Up Arrow) based on current modes (DECCKM, Keypad Mode). These sequences are sent to the host via the `ResponseCallback`.

### 3.5. GPU Rendering Pipeline

-   **SSBO Upload (`KTerm_PrepareRenderBuffer`)**: The CPU gathers visible rows from the active session(s). In split-screen mode, it composites rows from the layout tree into a single GPU-accessible buffer (`KTermBuffer`).
-   **Compute Shaders (`shaders/terminal.comp`)**:
    -   **KTerm Shader:** Renders the text grid, sampling the **Dynamic Font Atlas** and mixing in Sixel/Vector layers. Applies attributes (bold, underline, blink) and CRT effects.
    -   **Vector Shader:** Renders Tektronix and ReGIS vector graphics using a "storage tube" accumulation technique.
    -   **Sixel Shader:** Renders Sixel strips to a dedicated texture.
-   **Dynamic Atlas:** Uses `stb_truetype` to rasterize Unicode glyphs on-the-fly into a texture atlas.

### 3.6. Callbacks

-   `ResponseCallback`: Sends data back to the "host" application (e.g., keystrokes, mouse reports, DA responses).
-   `TitleCallback`: Invoked when window/icon title changes (OSC 0/1/2).
-   `BellCallback`: Called on BEL (0x07).
-   `GatewayCallback`: Hook for handling custom or unhandled Gateway Protocol commands.

## How to Use

This library is designed as a single-header library.

### 4.1. Basic Setup

-   In one of your C files, define implementations. If using the Situation framework, also include the IO adapter:
    ```c
    #define KTERM_IMPLEMENTATION
    #include "kterm.h"

    // Optional: Input adapter for Situation
    #define KTERM_IO_SIT_IMPLEMENTATION
    #include "kt_io_sit.h"
    ```
-   In other files, just `#include "kterm.h"` (and `kt_io_sit.h` if needed).
-   Initialize Situation: `InitWindow(...)`.
-   Initialize the terminal:
    ```c
    KTermConfig config = {
        .width = 132,
        .height = 50,
        .response_callback = MyResponseCallback
    };
    KTerm* term = KTerm_Create(config);

    // Optional: Set a direct output sink for high-throughput zero-copy output
    // KTerm_SetOutputSink(term, MySinkCallback, my_context_ptr);
    ```
-   Set target FPS for Situation: `SetTargetFPS(60)`.
-   Optionally, set terminal performance: `KTerm_SetPipelineTargetFPS(term, 60)`, `KTerm_SetPipelineTimeBudget(term, 0.5)`.
-   In your main application loop:
    ```c
    // Process host inputs and terminal updates
    KTermSit_ProcessInput(term); // Translates Situation key/mouse events (kt_io_sit.h)
    KTerm_Update(term);          // Processes pipeline, timers, and callbacks

    // Render the terminal
    BeginDrawing();
        ClearBackground(BLACK); // Or your desired background color
        KTerm_Draw(term);
    EndDrawing();
    ```
-   On exit: `KTerm_Destroy(term)` and `CloseWindow()`.

### 4.2. Sending Data to the KTerm (Simulating Host Output)

-   `KTerm_WriteChar(term, unsigned char ch)`: Send a single byte.
-   `KTerm_WriteString(term, const char* str)`: Send a null-terminated string.
-   `KTerm_WriteFormat(term, const char* format, ...)`: Send a printf-style formatted string.

These functions add data to an internal buffer, which `KTerm_Update(term)` processes.

### 4.3. Receiving Responses and Key Events from the Terminal

-   `KTerm_SetResponseCallback(term, ResponseCallback callback)`: Register a function like `void my_response_handler(KTerm* term, const char* response, int length)` to receive data
    that the terminal emulator needs to send back (e.g., status reports, DA). This uses an internal ring buffer.
-   `KTerm_SetOutputSink(term, KTermOutputSink sink, void* ctx)`: Register a direct sink callback `void my_sink(void* ctx, const char* data, size_t len)`. This bypasses the internal buffer for zero-copy output, ideal for high-throughput scenarios. Setting a sink automatically flushes any data remaining in the legacy buffer.
-   `KTerm_GetKey(term, VTKeyEvent* event)`: Retrieve a fully processed `VTKeyEvent` from the keyboard buffer. The `event->sequence` field contains the string
    to be sent to the host or processed by a local application.
    > **Note:** Applications can call `KTerm_GetKey` before `KTerm_Update` to intercept and consume input events locally (e.g., for local editing or hotkeys) before they are sent to the terminal pipeline.

### 4.4. Configuring KTerm Behavior

Configuration can be applied via the C API (during setup) or the **Gateway Protocol** (runtime).

#### Programmatic Control (C API)
-   **VT Compliance Level:**
    -   `KTerm_SetLevel(term, session, VTLevel level)` (e.g., `VT_LEVEL_XTERM`, `VT_LEVEL_525`).
    -   Controls available feature sets, DA responses, and emulation quirks.
-   **Terminal Modes:**
    -   `KTerm_SetMode(term, const char* mode, bool enable)`: Manually toggle DEC/ANSI modes (e.g., "application_cursor", "origin_mode").
    -   Most modes are typically controlled by the host via `CSI ? h/l` sequences.
-   **Visuals:**
    -   `KTerm_SetCursorShape(term, CursorShape shape)`: Block, Underline, Bar (Steady/Blink).
    -   `KTerm_SetCursorColor(term, ExtendedColor color)`.
-   **Mouse Tracking:**
    -   `KTerm_SetMouseTracking(term, MouseTrackingMode mode)` (e.g., `MOUSE_TRACKING_SGR`).
    -   Also settable via `CSI ? Pn h/l` (e.g., `CSI ? 1000 h`, `CSI ? 1006 h`).

#### Runtime Control (Gateway Protocol)
The **Gateway Protocol** enables configuration via `DCS` sequences sent to the terminal.
**Sequence Format:** `DCS GATE KTERM;<ID>;<COMMAND>;<PARAMS> ST`

**Examples:**
-   **Resize:** `SET;SIZE;132;43` or `SET;WIDTH;80`
-   **Keyboard:** `SET;KEYBOARD;REPEAT_RATE=30;DELAY=250`
-   **Debugging:** `SET;DEBUG;ON` (Enables verbose logging)
-   **Styling:** `SET;ATTR;BOLD=1;FG=1` (Forces attributes)
-   **Reset:** `RESET;GRAPHICS` (Clears Sixel/ReGIS/Kitty state)
-   **Query:** `GET;VERSION` (Returns `DCS ... REPORT;VERSION=... ST`)

### 4.5. Advanced Features

-   **Graphics Protocols:**
    -   **Sixel:** Bitmap graphics (DEC standard). Send via `DCS q ... ST`.
    -   **ReGIS:** Vector graphics (DEC standard). Send via `DCS p ... ST`. Supports command sets P, V, C, T, W, S, L, @.
    -   **Kitty:** Modern graphics protocol for high-performance images and animations. Send via `APC G ... ST`.
    -   **Tektronix:** Storage tube emulation (4010/4014).
-   **Soft Fonts (DECDLD):**
    -   Load custom bitmap fonts into the terminal for specific character sets via `DCS`.
-   **Multiplexer & Sessions:**
    -   Built-in split-screen (tmux-like) capability.
    -   Control via C API (`KTerm_SplitPane`, `KTerm_ClosePane`) or user keybindings (Ctrl+B %, Ctrl+B ").
    -   Gateway Target: `DCS GATE KTERM;0;SET;SESSION;1 ST` switches context for subsequent commands.
-   **Banners & Text Effects:**
    -   Generate ASCII/ANSI art banners via Gateway:
        `DCS GATE KTERM;0;PIPE;BANNER;TEXT=Hello;FONT=Block ST`
-   **Programmable Keys (DECUDK):**
    -   Host-programmable function keys (F1-F20) via `DCS | ... ST`.
-   **Rectangular Operations:**
    -   Manipulate rectangular screen areas (Copy, Fill, Erase, Selective Erase) via VT420 sequences (DECCRA, etc.).

### 4.6. Window and Title Management

-   `KTerm_SetWindowTitle(term, const char* title)`, `KTerm_SetIconTitle(term, const char* title)`.
-   `KTerm_GetWindowTitle(term)`, `KTerm_GetIconTitle(term)`.
-   `KTerm_SetTitleCallback(term, TitleCallback callback)`: Register `void my_title_handler(KTerm* term, const char* title, bool is_icon)` to be notified of title changes (e.g., from OSC sequences `ESC ]0;...ST`).

### 4.7. Diagnostics and Testing

-   `KTerm_EnableDebug(term, bool enable)`: Toggles verbose logging of unsupported/unknown sequences.
-   `KTerm_GetStatus(term)`: Returns `KTermStatus` with pipeline buffer usage, key buffer usage, and overflow flags.
-   `KTerm_RunTest(term, const char* test_name)`: Executes built-in test sequences to verify terminal functionality.
    -   Available tests: `"cursor"`, `"colors"`, `"charset"`, `"mouse"`, `"modes"`, `"all"`.
-   `KTerm_ShowInfo(term)`: Prints current terminal state (dimensions, active modes, conformance level) to the terminal screen.

### 4.8. Scripting API

-   A set of `Script_` functions provide simple wrappers for common operations:
    `KTerm_Script_PutChar(term, ...)`, `KTerm_Script_Print(term, ...)`, `KTerm_KTerm_Script_Printf(term, ...)`, `KTerm_Script_Cls(term)`, `KTerm_Script_SetColor(term, ...)`.

### 4.9. Post-Flush Grid Access

Thanks to the **Mandatory Op Queue**, the grid state is guaranteed to be stable and consistent immediately after `KTerm_Update` returns. This allows external logic (e.g., test harnesses, game simulations, or accessibility tools) to safely read or modify the grid directly.

```c
// Process inputs and flush operations
KTerm_Update(term);

// Safe direct access to the grid (post-flush)
if (term->session) {
    int x = 10, y = 5;
    KTermCell* cell = &term->session->cells[y * term->width + x];

    // Read cell
    if (cell->rune == L'@') {
        printf("Player found at %d,%d\n", x, y);
    }

    // Modify cell (thread-safe until next Update)
    cell->rune = L'#';
    cell->fg_color = KTERM_COLOR_RED;

    // Mark region as dirty to ensure GPU upload
    KTermRect rect = {x, y, 1, 1};
    KTerm_MarkRegionDirty(term, rect);
}
```

## Configuration Constants & Macros

The library uses several compile-time constants defined at the beginning of this
file (e.g., `DEFAULT_TERM_WIDTH`, `DEFAULT_TERM_HEIGHT`, `DEFAULT_CHAR_WIDTH`, `DEFAULT_CHAR_HEIGHT`, `DEFAULT_WINDOW_SCALE`,
`KTERM_OUTPUT_PIPELINE_SIZE`) to set default terminal dimensions, font size, rendering scale,
and buffer sizes. These can be modified before compilation.

### Feature Toggles
You can define the following macros to enable/disable specific subsystems:

*   `KTERM_DISABLE_GATEWAY`: Disables the Gateway Protocol (runtime introspection/configuration). Reduces code size and removes the overhead of parsing `DCS GATE` sequences.
*   `KTERM_ENABLE_MT_ASSERTS`: Enables runtime checks to ensure main-thread-only functions are called correctly (Debug only).

## Key Data Structures

-   `Terminal`: The main struct encapsulating the entire terminal state. The API is instance-based, meaning all functions take a `KTerm*` pointer.
-   `EnhancedTermChar`: Represents a single character cell on the screen, including its Unicode codepoint, foreground/background `ExtendedColor`, and text attributes
    (bold, italic, underline, etc.).
-   `VTParseState`: Enum tracking the current state of the escape sequence parser.
-   `VTLevel`: Enum defining the VT compatibility level (e.g., VT100, VT220, VT420, XTERM).
-   `ExtendedColor`: Struct for representing colors, supporting both standard ANSI palette indices and 24-bit RGB true color values.
-   `VTKeyboard`: Struct managing keyboard input state, modifier keys, application modes (cursor keys, keypad), and a buffer for processed `VTKeyEvent`s.
-   `DECModes`, `ANSIModes`: Structs containing boolean flags for various DEC private and ANSI standard modes.
-   `EnhancedCursor`: Struct detailing cursor properties like position, visibility, shape (`CursorShape`), and blink status.
-   `CharsetState`: Manages the G0-G3 character sets and the active GL/GR mappings.
-   `MouseTrackingMode`: Enum for the various mouse reporting protocols.
-   `SixelGraphics`, `SoftFont`, `BracketedPaste`, `ProgrammableKeys`: Structs for managing state related to these advanced features.

## Implementation Model

This is a single-header library. To include the implementation, define `KTERM_IMPLEMENTATION` in exactly one C source file before including this header:
```c
#define KTERM_IMPLEMENTATION
#include "kterm.h"
```
Other source files can simply include "kterm.h" for declarations.

## Dependencies

-   **Backend Framework:** **Situation** (version 2.3.x or later) is required for windowing, input, and GPU context creation.
-   **Helper Headers:** The library is distributed as a set of headers:
    -   `kterm.h`: Main API.
    -   `kt_gateway.h`: Gateway Protocol implementation.
    -   `kt_render_sit.h`: Rendering abstraction layer for Situation.
    -   `kt_io_sit.h`: Input adapter for Situation.
    -   `font_data.h`: Built-in bitmap fonts.
    -   `stb_truetype.h`: Font rasterization (bundled/vendored).
-   **Standard Libraries:** C11 standard library (`stdio.h`, `stdlib.h`, `string.h`, `stdbool.h`, `ctype.h`, `stdarg.h`, `math.h`, `time.h`).
-   **Runtime Resources:** The `shaders/` directory containing `terminal.comp`, `vector.comp`, and `sixel.comp` must be present in the application's working directory (or the path configured via `KTERM_TERMINAL_SHADER_PATH` etc.).

## License

MIT License
