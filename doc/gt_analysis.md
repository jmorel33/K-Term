# Grid Terminal Analysis vs K-Term v2.4

## Section A: Comparative Analysis

The following table compares the features of the legacy "Grid Terminal" engine with the modern K-Term v2.4 library.

### 1. Core Architecture & Terminal Standards
| Feature | Grid Terminal | K-Term v2.4 | Notes |
| :--- | :--- | :--- | :--- |
| **Engine Basis** | Tile-based "DOS" like environment | Tile-based Grid (`EnhancedTermChar`) | Both use a grid-based character cell model. K-Term is a headless library adaptable to any engine. |
| **Standards** | Custom parser, partial VT500 | **Full VT525 Compliance** | K-Term supports VT52, VT100, VT220, VT320, VT420, VT520, VT525, and xterm. |
| **Color Depth** | 16/256 colors, Per-page palettes | **24-bit TrueColor** + Palettes | K-Term supports ANSI 16, 256-color indexed, and full RGB TrueColor. Palettes are per-session. |
| **Integration** | Blitz3D Overlay | Embeddable Library | K-Term is designed to be embedded in any C/C++ application (Game Engines, OS, Tools). |

### 2. Display & Rendering System
| Feature | Grid Terminal | K-Term v2.4 | Notes |
| :--- | :--- | :--- | :--- |
| **Virtual Res** | Floating point virtual resolution | **GPU Scalable** | K-Term uses `DEFAULT_WINDOW_SCALE` and vector-based rendering (Compute Shaders) for resolution independence. |
| **Page System** | 32 pages, 4 split screens | **4 Sessions**, Recursive Panes | K-Term supports 4 active sessions (`MAX_SESSIONS`) and infinite recursive split-panes via `KTermLayout`. |
| **Page Sizing** | Zoom (0.5x, 2.0x) | **Resize & Reflow** | K-Term supports dynamic resizing with scrollback reflow (`KTERM_OP_RESIZE_GRID`). |
| **Positioning** | Movable windows (X/Y) | **Tiling Layout** | K-Term uses a tiling window manager approach (`KTermPane`). |
| **Scrolling** | Forward/Reverse | **Full Scrolling** | Supports standard scrolling, jump/smooth scroll modes (`DECSCLM`), and margins (`DECSTBM`). |
| **Fonts** | 8 internal fonts, Big Chars | **Soft Fonts (DECDLD)** | Supports loading soft fonts dynamically, TrueType via `stb_truetype`, and Double Width/Height (`DECDWL`/`DECDHL`). |
| **Transparency** | Alpha transparency | **Alpha Channel** | Full alpha support in `RGB_KTermColor`. |

### 3. Advanced Graphics & Effects
| Feature | Grid Terminal | K-Term v2.4 | Notes |
| :--- | :--- | :--- | :--- |
| **Line Draw** | Custom 1-byte chars | **DEC Special Graphics** | Full support for DEC line drawing characters and Unicode block elements. |
| **Per-Char FX** | Zoom, Rotate, Offset, Mask | **Attributes Only** | K-Term supports Bold, Faint, Italic, Underline (Styles), Blink, Reverse, Strike, Conceal. Rotation/Offset not supported. |
| **Shadows** | Drop shadows | **CRT Effects** | K-Term implements CRT curvature and scanlines via shaders, but not per-character drop shadows. |
| **3D Integration**| Text-to-3D entities | ❌ Not Supported | K-Term is a 2D terminal emulator; 3D projection is up to the host engine. |

### 4. Input & Interaction
| Feature | Grid Terminal | K-Term v2.4 | Notes |
| :--- | :--- | :--- | :--- |
| **Mouse** | Pointer, Click Masks | **Full Mouse Tracking** | Supports X10, VT200, SGR, URXVT, and Pixel tracking modes. Reports clicks/movement to host. |
| **Keyboard** | Buffering, Modes | **Enhanced Keyboard** | Full buffering, programmable keys (`DECUDK`), application modes (`DECCKM`), and auto-repeat. |
| **Modes** | Custom `TRMOD_*` | **DEC/ANSI Modes** | Standardized mode handling (Insert/Replace, Auto-Wrap, Origin, etc.). |

### 5. Data, Networking & Synchronization
| Feature | Grid Terminal | K-Term v2.4 | Notes |
| :--- | :--- | :--- | :--- |
| **Transfer** | `LOAD`/`UNLOAD` | **DCS/OSC Protocols** | Uses standard sequences for resource loading (Soft Fonts via `DECDLD`, Keys via `DECUDK`). |
| **Security** | Encryption | ❌ Not Supported | Transport security is out of scope for the terminal emulator library. |
| **Sync** | Timer Sync | **Internal Timers** | Internal blink/animation timers. Host sync via standard flow control (`XON`/`XOFF`). |
| **State** | `sts` command | **DECSTR / Reports** | Supports Soft Terminal Reset (`DECSTR`) and extensive Device Status Reports (`DSR`). |

### 6. Scripting & Logic
| Feature | Grid Terminal | K-Term v2.4 | Notes |
| :--- | :--- | :--- | :--- |
| **Scripting** | Full Language (Registers) | **Macros (DECDMAC)** | Supports recording and replaying macro sequences, but no Turing-complete scripting language. |
| **Modifiers** | 4800 visual modifiers | ❌ Not Supported | Visual effects are shader-based (Global) or Attribute-based (Per-cell), not logical "modifiers". |

### 7. Development & Debugging Tools
| Feature | Grid Terminal | K-Term v2.4 | Notes |
| :--- | :--- | :--- | :--- |
| **Trace** | Debug Page | **Debug Logging** | Configurable error/debug callbacks (`KTerm_EnableDebug`, `KTerm_LogUnsupportedSequence`). |
| **Self-Test** | Built-in | **DECTST** | Full VT confidence tests supported. |
| **DSR** | Resolution, Mouse | **Extensive DSR** | Reports Cursor, Printer, Keyboard, Macros, Checksums (`DECRQCRA`), and Session status. |

### 8. Performance & Optimization
| Feature | Grid Terminal | K-Term v2.4 | Notes |
| :--- | :--- | :--- | :--- |
| **Rendering** | Inline optimized loops | **Compute Shaders** | GPU-accelerated rendering pipeline (Vulkan/OpenGL) with dirty rect optimization. |
| **Memory** | Banked tokens | **Dynamic Atlas** | Efficient glyph caching and op-queue based grid management. |
| **FPS** | ~900 FPS (2007 hardware) | **High Performance** | Designed for modern GPUs with minimized CPU footprint. |

---

## Section B: Original Analysis

This document provides a comprehensive report on the features and technical specifications of "Grid Terminal" (also referred to as "Grid World" in some contexts), based on the development logs and documentation. Grid Terminal is a highly advanced, tile-based terminal emulator and engine designed to run as an overlay within a 3D environment (specifically using the Blitz3D engine).

### 1. Core Architecture & Terminal Standards
*   **Engine Basis:** Tile-based engine functioning as a "DOS" like environment.
*   **Standards Compliance:** Implements a custom parser supporting ANSI graphics and various VT terminal standards.
    *   Partial support for VT52, VT100, VT220, VT420, and VT500 escape sequences.
    *   Custom extensions to the standard VT command set.
*   **Color Support:**
    *   Supports 16-color palettes (standard).
    *   Supports 256-color palettes.
    *   Every page can use a different color palette.
    *   Palette rotation capabilities.
*   **Integration:** Designed to act as an overlay for a 3D scene (Blitz3D), allowing for HUDs, menus, and consoles.

### 2. Display & Rendering System
*   **Virtual Resolution:** Implements a virtual resolution system, allowing the terminal to scale correctly to any physical screen resolution.
*   **Page System:**
    *   Supports up to 32 pages.
    *   **Split Screen:** Capable of displaying up to 4 pages simultaneously (split horizontally or vertically).
    *   **Page Sizing:** Defined by width and height (FLOAT) and character counts. `CSI_set_page_size` allows for global zooming of a page (e.g., 0.5x, 2.0x).
    *   **Positioning:** Pages can be dragged along X and Y axes (movable windows).
    *   **Scrolling:** Supports scrolling (including reverse linefeeds).
    *   **Priorities:** Page display order list and priorities (focus system).
    *   **Margins:** Configurable margins via `CSI_set_margins`.
*   **Fonts:**
    *   Supports multiple internal fonts (up to 8 in memory).
    *   Supports double, triple, and quadruple width and height for characters.
    *   `CSI_write_big_character` for filling blocks with character pixels.
*   **Transparency:** Alpha transparency supported for pages to show the 3D world behind them.

### 3. Advanced Graphics & Effects
*   **Line Drawing:**
    *   Dedicated 1 byte per character for line drawing capabilities.
    *   Supports: Underline, Strikeout (horizontal center), Top line, Vertical center, 45° cross angle upwards, 45° cross angle downwards.
*   **Special Features (Per-Character):**
    The "Special Features" segment allows individual control over the following properties for *every single character*:
    *   **Alpha Transparency:** Dynamic transparency levels.
    *   **Zoom:** Independent X and Y zooming (up to 4x).
    *   **Rotation:** 0° to 360° rotation.
    *   **Offsets:** X and Y pixel offsets (-128 to 127).
    *   **Masks:** Mouse over mask and Mouse click mask (for interactive UI).
    *   **Shading:** Character shading in 4 directions (color and alpha) - picking next color/alpha in the sequence.
*   **Shadows:**
    *   Character shadows.
    *   Page shadow system (isometric angle shadow behind pages).
*   **3D Integration:** Functions to convert text to 3D entities (Text-to-3D) for use in the 3D scene.

### 4. Input & Interaction
*   **Mouse Support:**
    *   Mouse pointer rendering (animatable).
    *   Mouse can move pages and scroll the terminal display.
    *   **Interaction:** Detects "Mouse Over" and "Mouse Click" events.
    *   **Visual Feedback:** Uses masks to change character appearance on hover or click.
    *   **Reporting:** Reports clicked item ID, coordinates (page, col, row) to the calling application via Device Status Report (DSR).
*   **Keyboard:**
    *   Keyboard buffering (moved from console to terminal).
    *   Character repeat support.
    *   Modes to control if key entries send CSIs.
*   **Modes:** Various terminal modes including `TRMOD_WRITE_PROTECT`, `TRMOD_AUTOPAGE`, `TRMOD_LOCK_CURSOR_LINE`, etc.

### 5. Data, Networking & Synchronization
*   **Data Transfer:**
    *   `LOAD` and `UNLOAD` commands for pages, palettes, fonts, etc.
    *   Standardized `term_receive_byte()` and `term_send_byte()` pipeline.
*   **Security:** Encryption scheme for data transfer (network play security).
*   **Synchronization:**
    *   `CSI_set_timer` to sync terminal counters/blinking to an external source.
    *   Answerback FIFO queue.
*   **State Management:**
    *   User configuration file system (`terminal.cfg`).
    *   `sts` (Set Terminal State) command for saving/restoring states.
    *   Page clipboard and backup/restore functions.

### 6. Scripting & Logic
*   **Scripting Ambition:** Goal to turn the ANSI/VT parser into a full scripting language.
*   **Registers:** 16 general-purpose 32-bit registers for storing and shuffling data.
*   **Modifiers:**
    *   Up to 4800 modifiers (special effect triggers).
    *   Can perform operations (XOR, OR, AND, INC, DEC) on page zones automatically.
    *   Intended for creating visual effects (plasma, scrolls) without heavy CPU load.

### 7. Development & Debugging Tools
*   **Trace System:**
    *   **Page 4 (Overlay):** A reserved "immediate page" that acts as a trace display/prompter, always active.
    *   **CSI Tracing:** Dumps bad CSIs and arguments to a debug page.
    *   **Error Codes:** 5 levels of debug reporting.
*   **Self-Test:** Built-in self-test mode.
*   **Device Status Report (DSR):** Reports resolution, cursor position, mouse status, active pages, etc.

### 8. Performance & Optimization
*   **Rendering:** Optimized render loops (removed function calls in favor of inline code).
*   **Memory:** Use of banks instead of types for tokens to improve tokenizer speed.
*   **Efficiency:** Capable of high framerates (e.g., ~900 FPS empty, ~160 FPS complex ANSI) on contemporary hardware (circa 2007).
*   **Virtualization:** Virtual resolution allows consistent display across different screen sizes.

### 9. Planned/Unfinished Features (as of logs)
*   Full implementation of the scripting language (CMP, BNE, loops).
*   Menu management system (pull-down menus).
*   Stencil shadow optimization for 3D engine.
*   PhysX integration (mentioned as a goal).
*   Completion of the manual.
