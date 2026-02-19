// kterm.h - K-Term Terminal Emulation Library
// Comprehensive emulation of VT52, VT100, VT220, VT320, VT420, VT520, and xterm standards
// with modern extensions including truecolor, Sixel/ReGIS/Tektronix graphics, Kitty protocol,
// GPU-accelerated rendering, recursive multiplexing, and rich text styling.

/**********************************************************************************************
*
*   K-Term - High-Performance Terminal Emulation Library
*   (c) 2026 Jacques Morel
*
*   DESCRIPTION:
*       K-Term is a single-header C library providing exhaustive terminal emulation for legacy
*       DEC VT series terminals (VT52 through VT525) and xterm, while incorporating modern
*       extensions such as 24-bit truecolor, Sixel/ReGIS/Tektronix vector graphics, full Kitty
*       graphics protocol (animations, compositing, transparency), advanced mouse tracking,
*       bracketed paste, and rich text attributes (colored underline/strikethrough with styles,
*       attribute stacking, conceal replacement, debug grid, etc.).
*
*       Designed for seamless embedding in applications requiring robust text-based interfaces
*       (game engines, GPU-based operating systems, tools, IDEs, remote clients), it uses a
*       compute-shader GPU pipeline for rendering and the Situation framework for cross-platform
*       windowing, input, and acceleration.
*
*       Input is processed as a byte stream (e.g., from PTY or host application), updating an
*       internal screen buffer that supports multiple sessions, recursive pane layouts, scrolling
*       regions, and alternate screens. Responses (keyboard, mouse, reports) are queued via
*       configurable callbacks.
*
*   KEY FEATURES:
*       • Maximal VT compatibility with strict/permissive modes
*       • GPU-accelerated graphics and effects (CRT curvature, scanlines, glow)
*       • Gateway Protocol for runtime configuration and introspection
*       • Embeddable single-header design
*
*   LIMITATIONS:
*       • Unicode: Full UTF-8 decoding; glyph cache covers BMP (Basic Multilingual Plane)
*       • BiDi: Bidirectional text support is currently stubbed/unimplemented
*       • Platform: Relies on Situation backend (Vulkan/OpenGL/Metal compute shaders)
*
*   LICENSE: MIT License
*
**********************************************************************************************/
#ifndef KTERM_H
#define KTERM_H

// --- Debug Control ---
#ifndef KTERM_ENABLE_DEBUG_OUTPUT
#define KTERM_ENABLE_DEBUG_OUTPUT 0  // Set to 1 to enable debug fprintf statements
#endif

#if KTERM_ENABLE_DEBUG_OUTPUT
#define KTERM_DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define KTERM_DEBUG_PRINT(...) ((void)0)
#endif

// --- Version Macros ---
#define KTERM_VERSION_MAJOR 2
#define KTERM_VERSION_MINOR 6
#define KTERM_VERSION_PATCH 23
#define KTERM_VERSION_STRING "2.6.23"

// --- DLL Export/Import ---
#if defined(_WIN32)
    #if defined(KTERM_BUILD_SHARED)
        #define KTERM_API __declspec(dllexport)
    #elif defined(KTERM_USE_SHARED)
        #define KTERM_API __declspec(dllimport)
    #else
        #define KTERM_API
    #endif
#else
    #define KTERM_API
#endif

// Default to enabling Gateway Protocol unless explicitly disabled
#ifndef KTERM_DISABLE_GATEWAY
    #define KTERM_ENABLE_GATEWAY
#endif

#include "kt_render_sit.h"
#include "kt_layout.h"
#ifndef KTERM_DISABLE_NET
#include "kt_net.h"
#endif


// Safe Allocation Wrappers
void* KTerm_Malloc(size_t size);
void* KTerm_Calloc(size_t nmemb, size_t size);
void* KTerm_Realloc(void* ptr, size_t size);
KTERM_API void KTerm_Free(void* ptr);

// --- Threading Support Configuration ---
#if !defined(__STDC_NO_THREADS__)
    #include <threads.h>
    #include <stdatomic.h>
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
        #include <stdalign.h> // For alignas/_Alignas
    #endif
    typedef mtx_t kterm_mutex_t;
    typedef thrd_t kterm_thread_t;
    #define KTERM_MUTEX_INIT(m) mtx_init(&(m), mtx_plain)
    #define KTERM_MUTEX_LOCK(m) mtx_lock(&(m))
    #define KTERM_MUTEX_UNLOCK(m) mtx_unlock(&(m))
    #define KTERM_MUTEX_DESTROY(m) mtx_destroy(&(m))
    #define KTERM_THREAD_CURRENT() thrd_current()
    #define KTERM_THREAD_EQUAL(a, b) thrd_equal(a, b)
#else
    #include <pthread.h>
    #include <stdatomic.h>
    typedef pthread_mutex_t kterm_mutex_t;
    typedef pthread_t kterm_thread_t;
    #define KTERM_MUTEX_INIT(m) pthread_mutex_init(&(m), NULL)
    #define KTERM_MUTEX_LOCK(m) pthread_mutex_lock(&(m))
    #define KTERM_MUTEX_UNLOCK(m) pthread_mutex_unlock(&(m))
    #define KTERM_MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))
    #define KTERM_THREAD_CURRENT() pthread_self()
    #define KTERM_THREAD_EQUAL(a, b) pthread_equal(a, b)
#endif

// Enable runtime main-thread asserts (debug only)
#ifdef KTERM_ENABLE_MT_ASSERTS
    #define KTERM_ASSERT_MAIN_THREAD(term) _KTermAssertMainThread(term, __FILE__, __LINE__)
#else
    #define KTERM_ASSERT_MAIN_THREAD(term) do {} while(0)
#endif

// =============================================================================
// TERMINAL CONFIGURATION CONSTANTS
// =============================================================================
#define REGIS_WIDTH 800
#define REGIS_HEIGHT 480
#define DEFAULT_TERM_WIDTH 132
#define DEFAULT_TERM_HEIGHT 50
#define KTERM_MAX_COLS 2048
#define KTERM_MAX_ROWS 2048
#define DEFAULT_CHAR_WIDTH 10
#define DEFAULT_CHAR_HEIGHT 10
#define DEFAULT_WINDOW_SCALE 1 // Scale factor for the window and font rendering
#define DEFAULT_WINDOW_WIDTH (DEFAULT_TERM_WIDTH * DEFAULT_CHAR_WIDTH * DEFAULT_WINDOW_SCALE)
#define MAX_SESSIONS 4
#define DEFAULT_WINDOW_HEIGHT (DEFAULT_TERM_HEIGHT * DEFAULT_CHAR_HEIGHT * DEFAULT_WINDOW_SCALE)
#define MAX_ESCAPE_PARAMS 32
#define MAX_COMMAND_BUFFER 262144 // General purpose buffer for commands, OSC, DCS etc.
#define MAX_TAB_STOPS 256 // Max columns for tab stops, ensure it's >= DEFAULT_TERM_WIDTH
#define MAX_TITLE_LENGTH 256
#define MAX_RECT_OPERATIONS 16
#define KEY_EVENT_BUFFER_SIZE 65536
#define KTERM_OUTPUT_PIPELINE_SIZE 16384
#define KTERM_INPUT_PIPELINE_SIZE (1024 * 1024) // 1MB buffer for high-throughput graphics
#define MAX_SCROLLBACK_LINES 1000

// Oscillator Slots for Blink Rates
#define KTERM_OSC_SLOT_FAST_BLINK 30 // ~250ms
#define KTERM_OSC_SLOT_SLOW_BLINK 35 // ~500ms

// =============================================================================
// GLOBAL VARIABLES DECLARATIONS
// =============================================================================
// Callbacks for application to handle terminal events
// Forward declaration
typedef struct KTermSession_T KTermSession;
typedef struct KTerm_T KTerm;
typedef enum {
    KTERM_LOG_DEBUG = 0,
    KTERM_LOG_INFO,
    KTERM_LOG_WARNING,
    KTERM_LOG_ERROR,
    KTERM_LOG_FATAL
} KTermErrorLevel;

typedef enum {
    KTERM_SOURCE_API = 0,
    KTERM_SOURCE_PARSER,
    KTERM_SOURCE_RENDER,
    KTERM_SOURCE_SYSTEM
} KTermErrorSource;

typedef void (*KTermErrorCallback)(KTerm* term, KTermErrorLevel level, KTermErrorSource source, const char* msg, void* user_data);

// Response callback typedef
typedef void (*ResponseCallback)(KTerm* term, const char* response, int length); // For sending data back to host
typedef void (*PrinterCallback)(KTerm* term, const char* data, size_t length);   // For Printer Controller Mode
typedef void (*TitleCallback)(KTerm* term, const char* title, bool is_icon);    // For GUI window title changes
typedef void (*BellCallback)(KTerm* term);                                 // For audible bell
typedef void (*NotificationCallback)(KTerm* term, const char* message);          // For sending notifications (OSC 9)
typedef void (*KTermOutputSink)(void* user_data, KTermSession* session, const char* data, size_t len); // Direct Output Sink
#ifdef KTERM_ENABLE_GATEWAY
typedef void (*GatewayCallback)(KTerm* term, const char* class_id, const char* id, const char* command, const char* params); // Gateway Protocol
// Gateway Extensions
typedef void (*GatewayResponseCallback)(KTerm* term, KTermSession* session, const char* msg);
typedef void (*GatewayExtHandler)(KTerm* term, KTermSession* session, const char* id, const char* args, GatewayResponseCallback respond);
#endif
typedef void (*SessionResizeCallback)(KTerm* term, int session_index, int cols, int rows); // Notification of session resize

// =============================================================================
// ENHANCED COLOR SYSTEM
// =============================================================================
typedef enum {
    COLOR_BLACK = 0, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
    COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE,
    COLOR_BRIGHT_BLACK, COLOR_BRIGHT_RED, COLOR_BRIGHT_GREEN, COLOR_BRIGHT_YELLOW,
    COLOR_BRIGHT_BLUE, COLOR_BRIGHT_MAGENTA, COLOR_BRIGHT_CYAN, COLOR_BRIGHT_WHITE
} AnsiKTermColor; // Standard 16 ANSI colors

typedef struct RGB_KTermColor_T {
    unsigned char r, g, b, a;
} RGB_KTermColor; // True color representation
typedef enum {
    VT_LEVEL_52 = 52,
    VT_LEVEL_100 = 100,
    VT_LEVEL_102 = 102,
    VT_LEVEL_132 = 132,
    VT_LEVEL_220 = 220,
    VT_LEVEL_320 = 320,
    VT_LEVEL_340 = 340,
    VT_LEVEL_420 = 420,
    VT_LEVEL_510 = 510,
    VT_LEVEL_520 = 520,
    VT_LEVEL_525 = 525,
    VT_LEVEL_K95 = 95,
    VT_LEVEL_XTERM = 1000,
    VT_LEVEL_TT = 1001,
    VT_LEVEL_PUTTY = 1002,
    VT_LEVEL_ANSI_SYS = 1003,
    VT_LEVEL_COUNT = 15 // Update this if more levels are added
} VTLevel;

typedef enum {
    GRAPHICS_RESET_ALL = 0,
    GRAPHICS_RESET_KITTY = 1 << 0,
    GRAPHICS_RESET_REGIS = 1 << 1,
    GRAPHICS_RESET_TEK   = 1 << 2,
    GRAPHICS_RESET_SIXEL = 1 << 3,
} GraphicsResetFlags;


// =============================================================================
// PARSE STATES
// =============================================================================
typedef enum {
    VT_PARSE_NORMAL,
    VT_PARSE_ESCAPE,
    PARSE_CSI,          // Control Sequence Introducer (ESC [)
    PARSE_OSC,          // Operating System Command (ESC ])
    PARSE_DCS,          // Device Control String (ESC P)
    PARSE_APC,          // Application Program Command (ESC _)
    PARSE_PM,           // Privacy Message (ESC ^)
    PARSE_SOS,          // Start of String (ESC X)
    PARSE_STRING_TERMINATOR, // Expecting ST (ESC \) to terminate a string
    PARSE_CHARSET,      // Selecting character set (ESC ( C, ESC ) C etc.)
    PARSE_HASH,         // DEC Line Attributes (ESC #)
    PARSE_PERCENT,      // Select Character Set (ESC %)
    PARSE_VT52,         // In VT52 compatibility mode
    PARSE_SIXEL,        // Parsing Sixel graphics data (ESC P q ... ST)
    PARSE_SIXEL_ST,
    PARSE_TEKTRONIX,    // Tektronix 4010/4014 vector graphics mode
    PARSE_REGIS,        // ReGIS graphics mode (ESC P p ... ST)
    PARSE_KITTY,        // Kitty Graphics Protocol (ESC _ G ... ST)
    /*
     * PARSE_nF: Corresponds to the "Escape Intermediate" state in standard DEC/ANSI parsing (ECMA-35/ISO 2022).
     * It handles Escape sequences where ESC is followed by one or more Intermediate Bytes (0x20-0x2F)
     * before a Final Byte (0x30-0x7E).
     * Example: S7C1T is 'ESC SP F' (0x1B 0x20 0x46).
     *   1. ESC transitions to VT_PARSE_ESCAPE.
     *   2. SP (0x20) transitions to PARSE_nF.
     *   3. Any further 0x20-0x2F bytes loop in PARSE_nF.
     *   4. F (0x46) executes the command and returns to VT_PARSE_NORMAL.
     */
    PARSE_nF            // nF Escape Sequences (ESC SP ...)
} VTParseState;

// Extended color support
typedef struct {
    int color_mode;          // 0=indexed (palette), 1=RGB
    union {
        int index;           // 0-255 palette index
        RGB_KTermColor rgb;       // True color
    } value;
} ExtendedKTermColor;

// =============================================================================
// VT TERMINAL MODES AND STATES
// =============================================================================

// DEC Private Modes
#define KTERM_MODE_DECCKM           (1 << 0)  // DECCKM (set with CSI ? 1 h/l)
#define KTERM_MODE_DECOM            (1 << 1)  // DECOM (set with CSI ? 6 h/l) - cursor relative to scroll region
#define KTERM_MODE_DECAWM           (1 << 2)  // DECAWM (set with CSI ? 7 h/l)
#define KTERM_MODE_DECTCEM          (1 << 3)  // DECTCEM (set with CSI ? 25 h/l)
#define KTERM_MODE_ALTSCREEN        (1 << 4)  // DECSET 47/1047/1049 (uses alt_screen buffer)
#define KTERM_MODE_INSERT           (1 << 5)  // DECSET 4 (IRM in ANSI modes, also CSI ? 4 h/l)
#define KTERM_MODE_LOCALECHO        (1 << 6)  // Not a specific DEC mode, usually application controlled
#define KTERM_MODE_LNM              (1 << 7)  // DECSET 20 (LNM in ANSI modes) - LF implies CR
#define KTERM_MODE_DECCOLM          (1 << 8)  // DECSET 3 (DECCOLM) - switches to 132 columns
#define KTERM_MODE_DECSCLM          (1 << 9)  // DECSET 4 (DECSCLM) - smooth vs jump scroll
#define KTERM_MODE_DECSCNM          (1 << 10) // DECSET 5 (DECSCNM) - reverses fg/bg for whole screen
#define KTERM_MODE_RELATIVE_ORIGIN  KTERM_MODE_DECOM // DECOM (same as origin_mode)
#define KTERM_MODE_DECARM           (1 << 11) // DECSET 8 (DECARM) - (usually OS controlled)
#define KTERM_MODE_X10MOUSE         (1 << 12) // DECSET 9 - X10 mouse reporting
#define KTERM_MODE_TOOLBAR          (1 << 13) // DECSET 10 - (relevant for GUI terminals)
#define KTERM_MODE_BLINKCURSOR      (1 << 14) // DECSET 12 - (cursor style, often linked with DECSCUSR)
#define KTERM_MODE_DECPFF           (1 << 15) // DECSET 18 - (printer control)
#define KTERM_MODE_DECPEX           (1 << 16) // DECSET 19 - (printer control)
#define KTERM_MODE_BDSM             (1 << 17) // BDSM (CSI ? 8246 h/l) - Bi-Directional Support Mode
#define KTERM_MODE_DECLRMM          (1 << 18) // DECSET 69 - Left Right Margin Mode
#define KTERM_MODE_DECNCSM          (1 << 19) // DECSET 95 - DECNCSM (No Clear Screen on Column Change)
#define KTERM_MODE_VT52             (1 << 20) // DECANM (Mode 2) - true=VT52, false=ANSI
#define KTERM_MODE_DECBKM           (1 << 21) // DECBKM (Mode 67) - true=BS, false=DEL
#define KTERM_MODE_DECSDM           (1 << 22) // DECSDM (Mode 80) - true=Scrolling Enabled, false=Discard
#define KTERM_MODE_DECEDM           (1 << 23) // DECEDM (Mode 45) - Extended Edit Mode
#define KTERM_MODE_SIXEL_CURSOR     (1 << 24) // Mode 8452 - Sixel Cursor Position
#define KTERM_MODE_DECECR           (1 << 25) // DECECR (CSI z) - Enable Checksum Reporting
#define KTERM_MODE_ALLOW_80_132     (1 << 26) // Mode 40 - Allow 80/132 Column Switching
#define KTERM_MODE_ALT_CURSOR_SAVE  (1 << 27) // Mode 1041 - Save/Restore Cursor on Alt Screen Switch
#define KTERM_MODE_DECHDPXM         (1 << 28) // DECHDPXM (Mode 103) - Half-Duplex Mode
#define KTERM_MODE_DECKBUM          (1 << 29) // DECKBUM (Mode 68) - Keyboard Usage Mode
#define KTERM_MODE_DECESKM          (1 << 30) // DECESKM (Mode 104) - Secondary Keyboard Language Mode
#define KTERM_MODE_DECXRLM          (1U << 31) // DECXRLM (Mode 88) - Transmit XOFF/XON on Receive Limit

typedef uint32_t DECModes;

// Forms Mode Home Behavior
typedef enum {
    HOME_MODE_ABSOLUTE = 0, // (0,0)
    HOME_MODE_FIRST_UNPROTECTED, // First unprotected cell in grid
    HOME_MODE_FIRST_UNPROTECTED_LINE, // First unprotected cell in current line
    HOME_MODE_LAST_FOCUSED // Last focused unprotected cell (requires tracking)
} KTermHomeMode;

// ANSI Modes
typedef struct {
    // bool keyboard_action;           // KAM - Keyboard Action Mode (typically locked)
    bool insert_replace;            // IRM - Insert/Replace Mode (CSI 4 h/l)
    // bool send_receive;              // SRM - Send/Receive Mode (echo)
    bool line_feed_new_line;        // LNM - Line Feed/New Line Mode (CSI 20 h/l)
} ANSIModes;

// Mouse tracking mode enumeration
typedef enum {
    MOUSE_TRACKING_OFF, // No mouse tracking
    MOUSE_TRACKING_X10, // X10 mouse tracking
    MOUSE_TRACKING_VT200, // VT200 mouse tracking
    MOUSE_TRACKING_VT200_HIGHLIGHT, // VT200 highlight tracking
    MOUSE_TRACKING_BTN_EVENT, // Button-event tracking
    MOUSE_TRACKING_ANY_EVENT, // Any-event tracking
    MOUSE_TRACKING_SGR, // SGR mouse reporting
    MOUSE_TRACKING_URXVT, // URXVT mouse reporting
    MOUSE_TRACKING_PIXEL // Pixel position mouse reporting
} MouseTrackingMode;

// =============================================================================
// CURSOR SHAPES AND STYLES
// =============================================================================
typedef enum {
    CURSOR_BLOCK = 0,
    CURSOR_BLOCK_BLINK = 1,
    CURSOR_UNDERLINE = 2,
    CURSOR_UNDERLINE_BLINK = 3,
    CURSOR_BAR = 4, // Typically a vertical bar
    CURSOR_BAR_BLINK = 5
} CursorShape;

typedef struct {
    int x, y;
    bool visible;
    bool blink_enabled;     // Overall blink setting for the shape
    bool blink_state;       // Current on/off state of blink
    double blink_timer;     // Timer for blink interval
    CursorShape shape;
    ExtendedKTermColor color;    // Cursor color (often inverse of cell or specific color)
} EnhancedCursor;

// =============================================================================
// TAB STOP MANAGEMENT
// =============================================================================
typedef struct {
    bool* stops;               // Dynamic array indicating tab stop presence at each column
    int capacity;              // Allocated capacity of the stops array
    int count;                 // Number of active tab stops
    int default_width;         // Default tab width (usually 8)
} TabStops;

// =============================================================================
// CHARACTER SET HANDLING
// =============================================================================
typedef enum {
    CHARSET_ASCII = 0,      // Standard US ASCII
    CHARSET_DEC_SPECIAL,    // DEC Special Graphics (line drawing)
    CHARSET_UK,             // UK National character set
    CHARSET_DEC_MULTINATIONAL, // DEC Multinational Character Set (MCS)
    CHARSET_ISO_LATIN_1,    // ISO 8859-1 Latin-1
    CHARSET_UTF8,           // UTF-8 (requires multi-byte processing)
    // NRCS (National Replacement Character Sets)
    CHARSET_DUTCH,
    CHARSET_FINNISH,
    CHARSET_FRENCH,
    CHARSET_FRENCH_CANADIAN,
    CHARSET_GERMAN,
    CHARSET_ITALIAN,
    CHARSET_NORWEGIAN_DANISH,
    CHARSET_SPANISH,
    CHARSET_SWEDISH,
    CHARSET_SWISS,
    CHARSET_DRCS,
    CHARSET_COUNT // Must be < 32
} CharacterSet;

typedef struct {
    CharacterSet g0, g1, g2, g3; // The four designated character sets
    CharacterSet *gl, *gr;      // Pointers to active left/right sets (for 7-bit/8-bit env)
    bool single_shift_2;         // Next char from G2 (SS2)
    bool single_shift_3;         // Next char from G3 (SS3)
    // bool locking_shift;       // LS0/LS1/LS2/LS3 state (not typically used in modern terminals like GL/GR)
} CharsetState;

// =============================================================================
// ATTRIBUTE BIT FLAGS
// =============================================================================
// Shared GPU Attributes (0-15) - Must match shaders/terminal.comp
#define KTERM_ATTR_BOLD               (1 << 0)
#define KTERM_ATTR_FAINT              (1 << 1)
#define KTERM_ATTR_ITALIC             (1 << 2)
#define KTERM_ATTR_UNDERLINE          (1 << 3)
#define KTERM_ATTR_BLINK              (1 << 4)
#define KTERM_ATTR_REVERSE            (1 << 5)
#define KTERM_ATTR_STRIKE             (1 << 6)
#define KTERM_ATTR_DOUBLE_WIDTH       (1 << 7)
#define KTERM_ATTR_DOUBLE_HEIGHT_TOP  (1 << 8)
#define KTERM_ATTR_DOUBLE_HEIGHT_BOT  (1 << 9)
#define KTERM_ATTR_CONCEAL            (1 << 10)
#define KTERM_ATTR_OVERLINE           (1 << 11) // xterm extension
#define KTERM_ATTR_DOUBLE_UNDERLINE   (1 << 12) // ECMA-48
#define KTERM_ATTR_BLINK_BG           (1 << 13) // Background Blink
#define KTERM_ATTR_BLINK_SLOW         (1 << 14) // Slow Blink (Independent Speed)
#define KTERM_ATTR_FAINT_BG           (1 << 15) // Halfbrite Background

// Logical / Internal Attributes (16-31)
#define KTERM_ATTR_FRAMED             (1 << 16) // SGR 51
#define KTERM_ATTR_ENCIRCLED          (1 << 17) // SGR 52
#define KTERM_ATTR_GRID               (1 << 18) // Debug Grid
#define KTERM_ATTR_SUPERSCRIPT        (1 << 19) // SGR 73
#define KTERM_ATTR_UL_STYLE_MASK      (7 << 20) // Bits 20-22 for Underline Style
#define KTERM_ATTR_UL_STYLE_NONE      (0 << 20)
#define KTERM_ATTR_UL_STYLE_SINGLE    (1 << 20)
#define KTERM_ATTR_UL_STYLE_DOUBLE    (2 << 20)
#define KTERM_ATTR_UL_STYLE_CURLY     (3 << 20)
#define KTERM_ATTR_UL_STYLE_DOTTED    (4 << 20)
#define KTERM_ATTR_UL_STYLE_DASHED    (5 << 20)
#define KTERM_ATTR_SUBSCRIPT          (1 << 23) // SGR 74

// Relocated Private Entries
#define KTERM_ATTR_PROTECTED          (1 << 28) // DECSCA (Was 16)
#define KTERM_ATTR_SOFT_HYPHEN        (1 << 29) // (Was 17)

#define KTERM_DIRTY_FRAMES            2         // Number of frames a dirty row persists (Double Buffering)
#define KTERM_FLAG_DIRTY              (1 << 30) // Cell needs redraw
#define KTERM_FLAG_COMBINING          (1 << 31) // Unicode combining char

// =============================================================================
// ENHANCED TERMINAL CHARACTER
// =============================================================================
typedef struct {
    unsigned int ch;             // Unicode codepoint (or ASCII/charset specific value)
    ExtendedKTermColor fg_color;
    ExtendedKTermColor bg_color;
    ExtendedKTermColor ul_color;
    ExtendedKTermColor st_color;
    uint32_t flags;              // Consolidated attributes
} EnhancedTermChar;

#include "kt_ops.h"

// Grid Mask Constants
#define GRID_MASK_CH    (1 << 0)
#define GRID_MASK_FG    (1 << 1)
#define GRID_MASK_BG    (1 << 2)
#define GRID_MASK_UL    (1 << 3)
#define GRID_MASK_ST    (1 << 4)
#define GRID_MASK_FLAGS (1 << 5)

// =============================================================================
// TEXT RUN (JIT SHAPING)
// =============================================================================
typedef struct {
    int start_index;      // Index in the row buffer
    int length;           // Number of chars (base + combining)
    int visual_width;     // Visual width (usually 1, or 2 for wide chars)
    uint32_t codepoints[8]; // Simple static buffer for now. 0=Base, 1..=Marks
    int codepoint_count;
} KTermTextRun;

// =============================================================================
// BRACKETED PASTE MODE
// =============================================================================
typedef struct {
    bool enabled;       // Is CSI ? 2004 h active?
    bool active;        // Is a paste sequence currently being received? (between 200~ and 201~)
    char *buffer;       // Buffer for paste data (if needed, usually directly piped)
    size_t buffer_size;
    size_t buffer_pos;
    // double start_time; // For timeout logic if buffering
    // double timeout;
} BracketedPaste;

// =============================================================================
// PROGRAMMABLE KEYS
// =============================================================================
typedef struct {
    int key_code;           // Situation key code that triggers this
    char *sequence;         // String to send to host
    size_t sequence_length;
    bool active;            // Is this definition active
} ProgrammableKey;

typedef struct {
    ProgrammableKey *keys;
    bool udk_locked;            // Tracks UDK lock status for CSI ?25 n
    size_t count;
    size_t capacity;
} ProgrammableKeys;

typedef struct {
    int id;
    char* content; // The macro sequence
    size_t length;
    int encoding; // 0=Text, 1=Hex
} StoredMacro;

typedef struct {
    StoredMacro* macros;
    size_t count;
    size_t capacity;
    size_t total_memory_used;
} StoredMacros;

// =============================================================================
// RECTANGULAR OPERATIONS
// =============================================================================
typedef struct {
    int top, left, bottom, right; // 0-indexed inclusive coordinates
    bool active;                  // Is a rectangular operation defined/active
} VTRectangle;

typedef enum {
    RECT_OP_COPY,    // DECCRA
    RECT_OP_MOVE,    // (Not standard VT, but common concept)
    RECT_OP_FILL,    // DECFRA
    RECT_OP_ERASE,   // DECERA
    RECT_OP_SELECT   // (For selection, not a VT command)
} RectOperation;

typedef struct {
    VTRectangle area;
    RectOperation operation;
    EnhancedTermChar fill_char; // Character used for fill/erase
    EnhancedTermChar *data;     // Buffer for copy/move operations
    size_t data_size;
} RectangularOperation; // This might be for managing ongoing ops, VT ops are usually immediate

// =============================================================================
// SIXEL GRAPHICS SUPPORT
// =============================================================================

// Included via kt_composite_sit.h
#include "kt_composite_sit.h"

typedef struct {
    unsigned char* data;
    int width;
    int height;
    int x, y;
    bool active;
    int pos_x, pos_y;
    int max_x, max_y;
    int color_index;
    int repeat_count;
    int params[MAX_ESCAPE_PARAMS];
    int param_count;
    bool dirty;
    RGB_KTermColor palette[256];
    int parse_state; // 0=Normal, 1=Repeat, 2=Color, 3=Raster
    int param_buffer[8]; // For color definitions #Pc;Pu;Px;Py;Pz etc.
    int param_buffer_idx;
    GPUSixelStrip* strips;
    size_t strip_count;
    size_t strip_capacity;
    bool scrolling; // Controls if image scrolls with text
    bool transparent_bg; // From DECGRA (P2)
    int logical_start_row; // Row index where the image starts (relative to screen_head)
    int last_y_shift; // Track last shift to optimize redraws
} SixelGraphics;

#define SIXEL_STATE_NORMAL 0
#define SIXEL_STATE_REPEAT 1
#define SIXEL_STATE_COLOR  2
#define SIXEL_STATE_RASTER 3

// =============================================================================
// KITTY GRAPHICS PROTOCOL
// =============================================================================

#define KTERM_KITTY_MEMORY_LIMIT (64 * 1024 * 1024) // 64MB Limit per session

typedef struct {
    unsigned char* data;
    size_t size;
    size_t capacity;
    int width;
    int height;
    KTermTexture texture;
    int delay_ms;
} KittyFrame;

typedef struct {
    uint32_t id;

    // Frames
    KittyFrame* frames;
    int frame_count;
    int frame_capacity;

    // Animation State
    int current_frame;
    double frame_timer;

    // Placement
    int x; // Screen coordinates (relative to session)
    int y;
    int z_index;
    int start_row; // Logical row index (screen_head) when image was placed
    bool visible;
    bool complete; // Is the image upload complete?
} KittyImageBuffer;

typedef struct {
    // Parsing state
    int state; // 0=KEY, 1=VALUE, 2=PAYLOAD
    char key_buffer[32];
    int key_len;
    char val_buffer[128];
    int val_len;
    bool continuing; // Flag for chunked transmission (m=1)

    // Current Command Parameters
    struct {
        char action; // 'a' value: 't', 'q', 'p', 'd'
        char delete_action; // 'd' value: 'a', 'i', 'p', etc.
        char format; // 'f' value: 32, 24, 100(PNG)
        uint32_t id; // 'i'
        uint32_t placement_id; // 'p'
        int width; // 's'
        int height; // 'v'
        int x; // 'x'
        int y; // 'y'
        int z_index; // 'z'
        int transmission_type; // 't'
        int medium; // 'm' (0 or 1)
        bool quiet; // 'q'
        bool has_x; // 'x' key present
        bool has_y; // 'y' key present
    } cmd;

    // Base64 State
    uint32_t b64_accumulator;
    int b64_bits;

    // Active upload buffer
    KittyImageBuffer* active_upload;

    // Storage for images (Simple array for Phase 3.1)
    // We will use a dynamic list later, or just a few slots for testing
    KittyImageBuffer* images; // Array of stored images
    int image_count;
    int image_capacity;
    size_t current_memory_usage; // Bytes used by image data

} KittyGraphics;

// =============================================================================
// SOFT FONTS
// =============================================================================
typedef struct {
    uint8_t width;      // Advance width or bitmap width
    uint8_t begin_x;    // First pixel column with data
    uint8_t end_x;      // Last pixel column with data
} KTermFontMetric;

typedef struct {
    unsigned char font_data[256][32]; // Storage for 256 characters, 32 bytes each (e.g., 16x16 monochrome)
    int char_width;                   // Width of characters in this font
    int char_height;                  // Height of characters in this font
    bool loaded[256];                 // Which characters in this set are loaded
    bool active;                      // Is a soft font currently selected?
    bool dirty;                       // Font data has changed and needs upload
    KTermFontMetric metrics[256];     // Per-character metrics
    char name[4];                     // Designated Set Name (Dscs)
} SoftFont;

// =============================================================================
// VT CONFORMANCE AND FEATURE MANAGEMENT
// =============================================================================
#define KTERM_FEATURE_VT52_MODE           (1U << 0)
#define KTERM_FEATURE_VT100_MODE          (1U << 1)
#define KTERM_FEATURE_VT102_MODE          (1U << 2)
#define KTERM_FEATURE_VT132_MODE          (1U << 3)
#define KTERM_FEATURE_VT220_MODE          (1U << 4)
#define KTERM_FEATURE_VT320_MODE          (1U << 5)
#define KTERM_FEATURE_VT340_MODE          (1U << 6)
#define KTERM_FEATURE_VT420_MODE          (1U << 7)
#define KTERM_FEATURE_VT510_MODE          (1U << 8)
#define KTERM_FEATURE_VT520_MODE          (1U << 9)
#define KTERM_FEATURE_VT525_MODE          (1U << 10)
#define KTERM_FEATURE_K95_MODE            (1U << 11)
#define KTERM_FEATURE_XTERM_MODE          (1U << 12)
#define KTERM_FEATURE_TT_MODE             (1U << 13)
#define KTERM_FEATURE_PUTTY_MODE          (1U << 14)
#define KTERM_FEATURE_SIXEL_GRAPHICS      (1U << 15)
#define KTERM_FEATURE_REGIS_GRAPHICS      (1U << 16)
#define KTERM_FEATURE_RECT_OPERATIONS     (1U << 17)
#define KTERM_FEATURE_SELECTIVE_ERASE     (1U << 18)
#define KTERM_FEATURE_USER_DEFINED_KEYS   (1U << 19)
#define KTERM_FEATURE_SOFT_FONTS          (1U << 20)
#define KTERM_FEATURE_NATIONAL_CHARSETS   (1U << 21)
#define KTERM_FEATURE_MOUSE_TRACKING      (1U << 22)
#define KTERM_FEATURE_ALTERNATE_SCREEN    (1U << 23)
#define KTERM_FEATURE_TRUE_COLOR          (1U << 24)
#define KTERM_FEATURE_WINDOW_MANIPULATION (1U << 25)
#define KTERM_FEATURE_LOCATOR             (1U << 26)
#define KTERM_FEATURE_MULTI_SESSION_MODE  (1U << 27)
#define KTERM_FEATURE_LEFT_RIGHT_MARGIN   (1U << 28)

typedef uint32_t VTFeatures;

typedef struct {
    VTLevel level;        // Current conformance level (e.g., VT220)
    bool strict_mode;     // Enforce strict conformance? (vs. permissive)

    VTFeatures features;  // Feature flags derived from the level
    int max_session_count;        // Maximum number of sessions supported

    // Compliance tracking for diagnostics
    struct {
        int unsupported_sequences;
        int partial_implementations;
        int extensions_used;
        char last_unsupported[64]; // Last unsupported sequence string
    } compliance;
} VTConformance;

// =============================================================================
// ENHANCED KEYBOARD WITH FULL VT SUPPORT
// =============================================================================

// Standardized Key Codes for KTerm Input
typedef enum {
    KTERM_KEY_UNKNOWN = 0,
    KTERM_KEY_SPACE = 32,
    KTERM_KEY_APOSTROPHE = 39,
    KTERM_KEY_COMMA = 44,
    KTERM_KEY_MINUS = 45,
    KTERM_KEY_PERIOD = 46,
    KTERM_KEY_SLASH = 47,
    KTERM_KEY_0 = 48, KTERM_KEY_1, KTERM_KEY_2, KTERM_KEY_3, KTERM_KEY_4,
    KTERM_KEY_5, KTERM_KEY_6, KTERM_KEY_7, KTERM_KEY_8, KTERM_KEY_9,
    KTERM_KEY_SEMICOLON = 59,
    KTERM_KEY_EQUAL = 61,
    KTERM_KEY_A = 65, KTERM_KEY_B, KTERM_KEY_C, KTERM_KEY_D, KTERM_KEY_E,
    KTERM_KEY_F, KTERM_KEY_G, KTERM_KEY_H, KTERM_KEY_I, KTERM_KEY_J,
    KTERM_KEY_K, KTERM_KEY_L, KTERM_KEY_M, KTERM_KEY_N, KTERM_KEY_O,
    KTERM_KEY_P, KTERM_KEY_Q, KTERM_KEY_R, KTERM_KEY_S, KTERM_KEY_T,
    KTERM_KEY_U, KTERM_KEY_V, KTERM_KEY_W, KTERM_KEY_X, KTERM_KEY_Y,
    KTERM_KEY_Z,
    KTERM_KEY_LEFT_BRACKET = 91,
    KTERM_KEY_BACKSLASH = 92,
    KTERM_KEY_RIGHT_BRACKET = 93,
    KTERM_KEY_GRAVE_ACCENT = 96,
    KTERM_KEY_ESCAPE = 256,
    KTERM_KEY_ENTER,
    KTERM_KEY_TAB,
    KTERM_KEY_BACKSPACE,
    KTERM_KEY_INSERT,
    KTERM_KEY_DELETE,
    KTERM_KEY_RIGHT,
    KTERM_KEY_LEFT,
    KTERM_KEY_DOWN,
    KTERM_KEY_UP,
    KTERM_KEY_PAGE_UP,
    KTERM_KEY_PAGE_DOWN,
    KTERM_KEY_HOME,
    KTERM_KEY_END,
    KTERM_KEY_CAPS_LOCK,
    KTERM_KEY_SCROLL_LOCK,
    KTERM_KEY_NUM_LOCK,
    KTERM_KEY_PRINT_SCREEN,
    KTERM_KEY_PAUSE,
    KTERM_KEY_F1, KTERM_KEY_F2, KTERM_KEY_F3, KTERM_KEY_F4, KTERM_KEY_F5,
    KTERM_KEY_F6, KTERM_KEY_F7, KTERM_KEY_F8, KTERM_KEY_F9, KTERM_KEY_F10,
    KTERM_KEY_F11, KTERM_KEY_F12, KTERM_KEY_F13, KTERM_KEY_F14, KTERM_KEY_F15,
    KTERM_KEY_F16, KTERM_KEY_F17, KTERM_KEY_F18, KTERM_KEY_F19, KTERM_KEY_F20,
    KTERM_KEY_F21, KTERM_KEY_F22, KTERM_KEY_F23, KTERM_KEY_F24,
    KTERM_KEY_KP_0, KTERM_KEY_KP_1, KTERM_KEY_KP_2, KTERM_KEY_KP_3, KTERM_KEY_KP_4,
    KTERM_KEY_KP_5, KTERM_KEY_KP_6, KTERM_KEY_KP_7, KTERM_KEY_KP_8, KTERM_KEY_KP_9,
    KTERM_KEY_KP_DECIMAL,
    KTERM_KEY_KP_DIVIDE,
    KTERM_KEY_KP_MULTIPLY,
    KTERM_KEY_KP_SUBTRACT,
    KTERM_KEY_KP_ADD,
    KTERM_KEY_KP_ENTER,
    KTERM_KEY_KP_EQUAL,
    KTERM_KEY_LEFT_SHIFT,
    KTERM_KEY_LEFT_CONTROL,
    KTERM_KEY_LEFT_ALT,
    KTERM_KEY_LEFT_SUPER,
    KTERM_KEY_RIGHT_SHIFT,
    KTERM_KEY_RIGHT_CONTROL,
    KTERM_KEY_RIGHT_ALT,
    KTERM_KEY_RIGHT_SUPER,
    KTERM_KEY_MENU
} KTermKey;

typedef enum {
    KEY_PRIORITY_LOW = 0,
    KEY_PRIORITY_NORMAL = 1,
    KEY_PRIORITY_HIGH = 2,
    KEY_PRIORITY_CRITICAL = 3
} KeyPriority; // For prioritizing events in the buffer (e.g., Ctrl+C)

typedef struct {
    int key_code;           // KTermKey enum or compatible
    bool ctrl, shift, alt, meta;
    bool is_repeat;
    KeyPriority priority;
    double timestamp;
    char sequence[32];      // Generated escape sequence
} KTermKeyEvent;

typedef struct {
    int x, y;
    int button; // 0=Left, 1=Middle, 2=Right
    bool ctrl, shift, alt, meta;
    bool is_drag;
    bool is_release;
    float wheel_delta;
} KTermMouseEvent;

typedef enum {
    KTERM_EVENT_BYTES,
    KTERM_EVENT_KEY,
    KTERM_EVENT_MOUSE,
    KTERM_EVENT_RESIZE,
    KTERM_EVENT_FOCUS,
    KTERM_EVENT_PASTE
} KTermEventType;

typedef struct {
    KTermEventType type;
    union {
        struct { const uint8_t* data; size_t len; } bytes;
        KTermKeyEvent key;
        KTermMouseEvent mouse;
        struct { int w, h; } resize;
        bool focused;
        struct { const char* text; size_t len; } paste;
    };
} KTermEvent;

typedef struct {
    bool keypad_application_mode; // DECKPAM/DECKPNM
    bool meta_sends_escape;
    bool backarrow_sends_bs;
    bool delete_sends_del;
    int keyboard_dialect;
    int keyboard_variant; // For DECSKCV (0-15)
    char function_keys[24][32];
    bool auto_process;

    // Kitty Keyboard Protocol
    int kitty_keyboard_flags;   // Current active flags
    int kitty_keyboard_stack[16]; // Stack for push/pop
    int kitty_keyboard_stack_depth;

    // Event Buffer
    KTermKeyEvent buffer[KEY_EVENT_BUFFER_SIZE];
    atomic_int buffer_head;
    atomic_int buffer_tail;

    bool use_8bit_controls; // S7C1T / S8C1T state
    // buffer_count removed for thread safety
    atomic_int total_events;
    atomic_int dropped_events;
} KTermInputConfig;

/*
typedef struct {
    bool application_mode;      // General application mode for some keys (not DECCKM)
    bool cursor_key_mode;       // DECCKM: Application Cursor Keys (ESC OA vs ESC [ A)
    bool keypad_mode;           // DECKPAM/DECKPNM: Application/Numeric Keypad
    bool meta_sends_escape;   // Does Alt/Meta key prefix char with ESC?
    bool delete_sends_del;    // DEL key sends DEL (0x7F) or BS (0x08)
    bool backarrow_sends_bs;  // Backarrow key sends BS (0x08) or DEL (0x7F)

    int keyboard_dialect;        // Tracks NRCS dialect for CSI ?26 n (1=North American, 2=British, etc.)

    // Function key definitions (programmable or standard)
    char function_keys[24][32];  // F1-F24 sequences (can be overridden by DECUDK)

    // Key mapping table (example, might not be fully used if KTerm_GenerateVTSequence is comprehensive)
    // struct {
    //     int Situation_key;
    //     char normal[16];
    //     char shift[16];
    //     char ctrl[16];
    //     char alt[16];
    //     char app[16]; // For application modes
    // } key_mappings[256]; // Max Situation key codes

    // Buffered input for key events
    VTKeyEvent buffer[512]; // Circular buffer for key events
    int buffer_head, buffer_tail, buffer_count;

    // Statistics
    size_t total_events;
    size_t dropped_events;      // If buffer overflows
    // size_t priority_overrides; // If high priority event preempts
} VTKeyboard;
*/

// =============================================================================
// TITLE AND ICON MANAGEMENT
// =============================================================================
typedef struct {
    char window_title[MAX_TITLE_LENGTH];
    char icon_title[MAX_TITLE_LENGTH];
    char terminal_name[64]; // Name reported by some terminal identification sequences
    bool title_changed;     // Flag for GUI update
    bool icon_changed;      // Flag for GUI update
} TitleManager;

// =============================================================================
// TERMINAL STATUS
// =============================================================================
typedef struct {
    size_t pipeline_usage;      // Bytes currently in input_pipeline
    size_t key_usage;           // Events currently in vt_keyboard.buffer
    bool overflow_detected;     // Was input_pipeline overflowed recently?
    double avg_process_time;    // Average time to process one char from pipeline (diagnostics)
} KTermStatus;

// =============================================================================
// TERMINAL COMPUTE SHADER & GPU STRUCTURES
// =============================================================================

// GPU Structures moved to kt_composite_sit.h

// --- Shader Code ---
#ifndef KTERM_TERMINAL_SHADER_PATH
#define KTERM_TERMINAL_SHADER_PATH "sit/k-term/shaders/terminal.comp"
#endif
#ifndef KTERM_VECTOR_SHADER_PATH
#define KTERM_VECTOR_SHADER_PATH "sit/k-term/shaders/vector.comp"
#endif
#ifndef KTERM_SIXEL_SHADER_PATH
#define KTERM_SIXEL_SHADER_PATH "sit/k-term/shaders/sixel.comp"
#endif


// --- DUAL BACKEND SHADER PREAMBLES ---
// Terminal Compute Shader Preamble
#if defined(SITUATION_USE_VULKAN)
    static const char* terminal_compute_preamble =
    "#version 460\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "#define VULKAN_BACKEND\n"
    "layout(local_size_x = 8, local_size_y = 16, local_size_z = 1) in;\n"
    "// Vulkan: Individual texture bindings (not array)\n"
    "layout(set = 2, binding = 0) uniform sampler2D u_font_texture;\n"
    "layout(set = 3, binding = 0) uniform sampler2D u_sixel_texture;\n"
    "#define GET_SAMPLER_2D(h) u_font_texture\n"  // Ignore handle, always use font texture
    "#define GET_SIXEL_SAMPLER() u_sixel_texture\n"
    "struct GPUCell { uint char_code; uint fg_color; uint bg_color; uint flags; uint ul_color; uint st_color; };\n"
    "layout(buffer_reference, scalar) buffer KTermBuffer { GPUCell cells[]; };\n"
    "layout(set = 1, binding = 0, rgba8) uniform image2D output_image;\n"
    "layout(buffer_reference, scalar) buffer ConfigBuffer { float crt_curvature; float scanline_intensity; float glow_intensity; float noise_intensity; float visual_bell_intensity; uint flags; uint font_cell_width; uint font_cell_height; uint font_data_width; uint font_data_height; };\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    vec2 screen_size; vec2 char_size; vec2 grid_size; float time;\n"
    "    uint cursor_index; uint cursor_blink_state; uint text_blink_state;\n"
    "    uint sel_start; uint sel_end; uint sel_active; uint mouse_cursor_index;\n"
    "    uint64_t terminal_buffer_addr; uint64_t vector_buffer_addr;\n"
    "    uint64_t font_texture_handle; uint64_t sixel_texture_handle; uint64_t vector_texture_handle;\n"
    "    uint64_t shader_config_addr; uint atlas_cols; uint vector_count;\n"
    "    int sixel_y_offset; uint grid_color; uint conceal_char_code;\n"
    "} pc;\n";
#else
    static const char* terminal_compute_preamble =
    "#version 460\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "#extension GL_ARB_bindless_texture : require\n"
    "#extension GL_GOOGLE_include_directive : require\n"
    "layout(local_size_x = 8, local_size_y = 16, local_size_z = 1) in;\n"
    "// OpenGL: Bindless texture handles\n"
    "#define GET_SAMPLER_2D(h) sampler2D(h)\n"
    "struct GPUCell { uint char_code; uint fg_color; uint bg_color; uint flags; uint ul_color; uint st_color; };\n"
    "layout(buffer_reference, scalar) buffer KTermBuffer { GPUCell cells[]; };\n"
    "layout(binding = 1, rgba8) uniform image2D output_image;\n"
    "layout(buffer_reference, scalar) buffer ConfigBuffer { float crt_curvature; float scanline_intensity; float glow_intensity; float noise_intensity; float visual_bell_intensity; uint flags; uint font_cell_width; uint font_cell_height; uint font_data_width; uint font_data_height; };\n"
    "layout(scalar, binding = 0) uniform PushConstants {\n"
    "    vec2 screen_size; vec2 char_size; vec2 grid_size; float time;\n"
    "    uint cursor_index; uint cursor_blink_state; uint text_blink_state;\n"
    "    uint sel_start; uint sel_end; uint sel_active; uint mouse_cursor_index;\n"
    "    uint64_t terminal_buffer_addr; uint64_t vector_buffer_addr;\n"
    "    uint64_t font_texture_handle; uint64_t sixel_texture_handle; uint64_t vector_texture_handle;\n"
    "    uint64_t shader_config_addr; uint atlas_cols; uint vector_count;\n"
    "    int sixel_y_offset; uint grid_color; uint conceal_char_code;\n"
    "} pc;\n";
#endif

// Vector Compute Shader Preamble
#if defined(SITUATION_USE_VULKAN)
    static const char* vector_compute_preamble =
    "#version 460\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "#extension GL_EXT_nonuniform_qualifier : require\n"
    "layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n"
    "struct GPUVectorLine { vec2 start; vec2 end; uint color; float intensity; uint mode; float _pad; };\n"
    "layout(buffer_reference, scalar) buffer VectorBuffer { GPUVectorLine data[]; };\n"
    "layout(buffer_reference, scalar) buffer ConfigBuffer { float crt_curvature; float scanline_intensity; float glow_intensity; float noise_intensity; float visual_bell_intensity; uint flags; uint font_cell_width; uint font_cell_height; uint font_data_width; uint font_data_height; };\n"
    "layout(set = 1, binding = 0, rgba8) uniform image2D output_image;\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    vec2 screen_size; vec2 char_size; vec2 grid_size; float time;\n"
    "    uint cursor_index; uint cursor_blink_state; uint text_blink_state;\n"
    "    uint sel_start; uint sel_end; uint sel_active; uint mouse_cursor_index;\n"
    "    uint64_t terminal_buffer_addr; uint64_t vector_buffer_addr;\n"
    "    uint64_t font_texture_handle; uint64_t sixel_texture_handle; uint64_t vector_texture_handle;\n"
    "    uint64_t shader_config_addr; uint atlas_cols; uint vector_count;\n"
    "    int sixel_y_offset; uint grid_color; uint conceal_char_code;\n"
    "} pc;\n";
#else
    static const char* vector_compute_preamble =
    "#version 460\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "#extension GL_ARB_bindless_texture : require\n"
    "layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n"
    "struct GPUVectorLine { vec2 start; vec2 end; uint color; float intensity; uint mode; float _pad; };\n"
    "layout(buffer_reference, scalar) buffer VectorBuffer { GPUVectorLine data[]; };\n"
    "layout(buffer_reference, scalar) buffer ConfigBuffer { float crt_curvature; float scanline_intensity; float glow_intensity; float noise_intensity; float visual_bell_intensity; uint flags; uint font_cell_width; uint font_cell_height; uint font_data_width; uint font_data_height; };\n"
    "layout(binding = 1, rgba8) uniform image2D output_image;\n"
    "layout(scalar, binding = 0) uniform PushConstants {\n"
    "    vec2 screen_size; vec2 char_size; vec2 grid_size; float time;\n"
    "    uint cursor_index; uint cursor_blink_state; uint text_blink_state;\n"
    "    uint sel_start; uint sel_end; uint sel_active; uint mouse_cursor_index;\n"
    "    uint64_t terminal_buffer_addr; uint64_t vector_buffer_addr;\n"
    "    uint64_t font_texture_handle; uint64_t sixel_texture_handle; uint64_t vector_texture_handle;\n"
    "    uint64_t shader_config_addr; uint atlas_cols; uint vector_count;\n"
    "    int sixel_y_offset; uint grid_color; uint conceal_char_code;\n"
    "} pc;\n";
#endif

// Sixel Compute Shader Preamble
#if defined(SITUATION_USE_VULKAN)
    static const char* sixel_compute_preamble =
    "#version 460\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "#extension GL_EXT_nonuniform_qualifier : require\n"
    "#define VULKAN_BACKEND\n"
    "layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n"
    "struct GPUSixelStrip { uint x; uint y; uint pattern; uint color_index; };\n"
    "layout(buffer_reference, scalar) buffer SixelBuffer { GPUSixelStrip data[]; };\n"
    "layout(buffer_reference, scalar) buffer PaletteBuffer { uint colors[]; };\n"
    "layout(buffer_reference, scalar) buffer ConfigBuffer { float crt_curvature; float scanline_intensity; float glow_intensity; float noise_intensity; float visual_bell_intensity; uint flags; uint font_cell_width; uint font_cell_height; uint font_data_width; uint font_data_height; };\n"
    "layout(set = 1, binding = 0, rgba8) uniform image2D output_image;\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    vec2 screen_size; vec2 char_size; vec2 grid_size; float time;\n"
    "    uint cursor_index; uint cursor_blink_state; uint text_blink_state;\n"
    "    uint sel_start; uint sel_end; uint sel_active; uint mouse_cursor_index;\n"
    "    uint64_t terminal_buffer_addr; uint64_t vector_buffer_addr;\n"
    "    uint64_t font_texture_handle; uint64_t sixel_texture_handle; uint64_t vector_texture_handle;\n"
    "    uint64_t shader_config_addr; uint atlas_cols; uint vector_count;\n"
    "    int sixel_y_offset; uint grid_color; uint conceal_char_code;\n"
    "} pc;\n";
#else
    static const char* sixel_compute_preamble =
    "#version 460\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "#extension GL_ARB_bindless_texture : require\n"
    "layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n"
    "struct GPUSixelStrip { uint x; uint y; uint pattern; uint color_index; };\n"
    "layout(buffer_reference, scalar) buffer SixelBuffer { GPUSixelStrip data[]; };\n"
    "layout(buffer_reference, scalar) buffer PaletteBuffer { uint colors[]; };\n"
    "layout(buffer_reference, scalar) buffer ConfigBuffer { float crt_curvature; float scanline_intensity; float glow_intensity; float noise_intensity; float visual_bell_intensity; uint flags; uint font_cell_width; uint font_cell_height; uint font_data_width; uint font_data_height; };\n"
    "layout(binding = 1, rgba8) uniform image2D output_image;\n"
    "layout(scalar, binding = 0) uniform PushConstants {\n"
    "    vec2 screen_size; vec2 char_size; vec2 grid_size; float time;\n"
    "    uint cursor_index; uint cursor_blink_state; uint text_blink_state;\n"
    "    uint sel_start; uint sel_end; uint sel_active; uint mouse_cursor_index;\n"
    "    uint64_t terminal_buffer_addr; uint64_t vector_buffer_addr;\n"
    "    uint64_t font_texture_handle; uint64_t sixel_texture_handle; uint64_t vector_texture_handle;\n"
    "    uint64_t shader_config_addr; uint atlas_cols; uint vector_count;\n"
    "    int sixel_y_offset; uint grid_color; uint conceal_char_code;\n"
    "} pc;\n";
#endif

// Texture Blit Compute Shader Preamble
#if defined(SITUATION_USE_VULKAN)
    static const char* blit_compute_preamble =
    "#version 460\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "#define VULKAN_BACKEND\n"
    "layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;\n"
    "// Vulkan: Descriptor array for bindless textures\n"
    "layout(set = 2, binding = 0) uniform sampler2D u_textures[4096];\n"
    "#define BINDLESS_SAMPLER2D(handle) u_textures[uint(handle)]\n"
    "layout(set = 1, binding = 0, rgba8) uniform image2D dstImage;\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    ivec2 dest_pos; ivec2 src_size;\n"
    "    uint64_t src_texture_handle; ivec4 clip_rect;\n"
    "} pc;\n";
#else
    static const char* blit_compute_preamble =
    "#version 460\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "#extension GL_ARB_bindless_texture : require\n"
    "layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;\n"
    "#define BINDLESS_SAMPLER2D(handle) sampler2D(handle)\n"
    "layout(binding = 1, rgba8) uniform image2D dstImage;\n"
    "layout(scalar, binding = 0) uniform PushConstants {\n"
    "    ivec2 dest_pos; ivec2 src_size;\n"
    "    uint64_t src_texture_handle; ivec4 clip_rect;\n"
    "} pc;\n";
#endif


// =============================================================================
// VT COMPLIANCE LEVELS
// =============================================================================
KTERM_API void KTerm_ResetGraphics(KTerm* term, KTermSession* session, GraphicsResetFlags flags);

#ifdef KTERM_ENABLE_GATEWAY
#include "kt_gateway.h"
#endif

// =============================================================================
// MULTIPLEXER LAYOUT STRUCTURES
// =============================================================================

// Typedef for the command execution callback to accept session
typedef void (*ExecuteCommandCallback)(KTerm* term, KTermSession* session);


// =============================================================================
// CORE API FUNCTIONS
// =============================================================================

// THREAD SAFETY NOTE:
// This library is NOT thread-safe. All KTerm_* functions (especially KTerm_WriteChar,
// KTerm_Update, and KTerm_Draw) must be called from the same thread or
// externally synchronized by the application.
// term->active_session and other global states are shared across functions.

typedef struct {
    int width;
    int height;
    ResponseCallback response_callback;

    // Hardening / Limits
    int max_sixel_width;       // Default: 0 (Unlimited/Term Width)
    int max_sixel_height;      // Default: 0 (Unlimited/Term Height)
    int max_kitty_image_pixels;// Default: 0 (Unlimited/Memory Limit applies)
    int max_ops_per_flush;     // Default: 0 (Unlimited)
    bool strict_mode;          // Enable strict parsing mode
} KTermConfig;

KTERM_API KTerm* KTerm_Create(KTermConfig config);
KTERM_API void KTerm_Destroy(KTerm* term);

// Session Management
KTERM_API void KTerm_SetActiveSession(KTerm* term, int index);
KTERM_API void KTerm_SetSplitScreen(KTerm* term, bool active, int row, int top_idx, int bot_idx);
KTERM_API void KTerm_WriteCharToSession(KTerm* term, int session_index, unsigned char ch);
KTERM_API void KTerm_SetResponseEnabled(KTerm* term, int session_index, bool enable);
KTERM_API bool KTerm_InitSession(KTerm* term, int index);

// KTerm lifecycle
KTERM_API bool KTerm_Init(KTerm* term);
KTERM_API void KTerm_Cleanup(KTerm* term);
KTERM_API void KTerm_Update(KTerm* term);  // Process events, update states (e.g., cursor blink)
KTERM_API void KTerm_Draw(KTerm* term);    // Render the terminal state to screen

// VT compliance and identification
KTERM_API bool KTerm_GetKey(KTerm* term, KTermKeyEvent* event); // Retrieve buffered event
KTERM_API void KTerm_QueueInputEvent(KTerm* term, KTermKeyEvent event); // Push event to buffer
KTERM_API void KTerm_SetLevel(KTerm* term, KTermSession* session, VTLevel level);
VTLevel KTerm_GetLevel(KTerm* term);
// void EnableVTFeature(const char* feature, bool enable); // e.g., "sixel", "DECCKM" - Deprecated by KTerm_SetLevel
// bool IsVTFeatureSupported(const char* feature); - Deprecated by direct struct access
void GetDeviceAttributes(KTerm* term, char* primary, char* secondary, size_t buffer_size);

// Enhanced pipeline management (for host input)
KTERM_API bool KTerm_ProcessEvent(KTerm* term, KTermSession* session, const KTermEvent* event);
KTERM_API bool KTerm_WriteChar(KTerm* term, unsigned char ch);
KTERM_API size_t KTerm_PushInput(KTerm* term, const void* data, size_t length);
KTERM_API bool KTerm_WriteString(KTerm* term, const char* str);
KTERM_API bool KTerm_WriteFormat(KTerm* term, const char* format, ...);
// bool PipelineWriteUTF8(const char* utf8_str); // Requires UTF-8 decoding logic
KTERM_API void KTerm_ProcessEvents(KTerm* term); // Process characters from the pipeline
KTERM_API void KTerm_ClearEvents(KTerm* term);
KTERM_API int KTerm_GetPendingEventCount(KTerm* term);
KTERM_API bool KTerm_IsEventOverflow(KTerm* term);

// Performance management
KTERM_API void KTerm_SetPipelineTargetFPS(KTerm* term, int fps);    // Helps tune processing budget
KTERM_API void KTerm_SetPipelineTimeBudget(KTerm* term, double pct); // Percentage of frame time for pipeline

// Mouse support (enhanced)
KTERM_API void KTerm_SetMouseTracking(KTerm* term, MouseTrackingMode mode); // Explicitly set a mouse mode
KTERM_API void KTerm_EnableMouseFeature(KTerm* term, const char* feature, bool enable); // e.g., "focus", "sgr"
// void KTerm_UpdateMouse(KTerm* term); // Removed in v2.1
// void KTerm_UpdateKeyboard(KTerm* term); // Removed in v2.1
// void UpdateKeyboard(KTerm* term);  // Removed in v2.1
// bool GetKeyEvent(KTerm* term, KeyEvent* event);  // Removed in v2.1
KTERM_API void KTerm_SetKeyboardMode(KTerm* term, const char* mode, bool enable); // "application_cursor", "keypad_numeric"
KTERM_API void KTerm_SetFocus(KTerm* term, bool focused); // Report focus state (CSI I/O)
KTERM_API void KTerm_DefineFunctionKey(KTerm* term, int key_num, const char* sequence); // Program F1-F24

// KTerm control and modes
KTERM_API void KTerm_SetMode(KTerm* term, const char* mode, bool enable); // Generic mode setting by name
KTERM_API void KTerm_SetCursorShape(KTerm* term, CursorShape shape);
KTERM_API void KTerm_SetCursorKTermColor(KTerm* term, ExtendedKTermColor color);

// Character sets and encoding
KTERM_API void KTerm_SelectCharacterSet(KTerm* term, int gset, CharacterSet charset); // Designate G0-G3
KTERM_API void KTerm_SetCharacterSet(KTerm* term, CharacterSet charset); // Set current GL (usually G0)
unsigned int TranslateCharacter(KTerm* term, unsigned char ch, CharsetState* state); // Translate based on active CS

// Tab stops
KTERM_API void KTerm_SetTabStop(KTerm* term, int column);
KTERM_API void KTerm_ClearTabStop(KTerm* term, int column);
KTERM_API void KTerm_ClearAllTabStops(KTerm* term);
int NextTabStop(KTerm* term, int current_column);
int PreviousTabStop(KTerm* term, int current_column); // Added for CBT

// Bracketed paste
KTERM_API void KTerm_EnableBracketedPaste(KTerm* term, bool enable); // Enable/disable CSI ? 2004 h/l
bool IsBracketedPasteActive(KTerm* term);
void ProcessPasteData(KTerm* term, const char* data, size_t length); // Handle pasted data

// Rectangular operations (VT420+)
KTERM_API void KTerm_DefineRectangle(KTerm* term, int top, int left, int bottom, int right); // (DECSERA, DECFRA, DECCRA)
KTERM_API void KTerm_ExecuteRectangularOperation(KTerm* term, RectOperation op, const EnhancedTermChar* fill_char);
KTERM_API void KTerm_CopyRectangle(KTerm* term, VTRectangle src, int dest_x, int dest_y);
KTERM_API void KTerm_ExecuteRectangularOps(KTerm* term, KTermSession* session);  // DECCRA Implementation
KTERM_API void KTerm_ExecuteRectangularOps2(KTerm* term, KTermSession* session); // DECRQCRA Implementation

// Sixel graphics
KTERM_API void KTerm_InitSixelGraphics(KTerm* term, KTermSession* session);
KTERM_API void KTerm_ProcessSixelData(KTerm* term, KTermSession* session, const char* data, size_t length); // Process raw Sixel string
KTERM_API void KTerm_DrawSixelGraphics(KTerm* term); // Render current Sixel image

// Soft fonts
KTERM_API void KTerm_LoadSoftFont(KTerm* term, const unsigned char* font_data, int char_start, int char_count); // DECDLD
KTERM_API void KTerm_SelectSoftFont(KTerm* term, bool enable); // Enable/disable use of loaded soft font

// Title management
KTERM_API void KTerm_SetWindowTitle(KTerm* term, const char* title); // Set window title (OSC 0, OSC 2)
KTERM_API void KTerm_SetIconTitle(KTerm* term, const char* title);   // Set icon title (OSC 1)
KTERM_API const char* KTerm_GetWindowTitle(KTerm* term);
KTERM_API const char* KTerm_GetIconTitle(KTerm* term);

// Callbacks
KTERM_API void KTerm_SetResponseCallback(KTerm* term, ResponseCallback callback);
KTERM_API void KTerm_SetOutputSink(KTerm* term, KTermOutputSink sink, void* ctx);
KTERM_API void KTerm_SetPrinterCallback(KTerm* term, PrinterCallback callback);
KTERM_API void KTerm_SetTitleCallback(KTerm* term, TitleCallback callback);
KTERM_API void KTerm_SetBellCallback(KTerm* term, BellCallback callback);
KTERM_API void KTerm_SetNotificationCallback(KTerm* term, NotificationCallback callback);
KTERM_API void KTerm_SetErrorCallback(KTerm* term, KTermErrorCallback callback, void* user_data);
#ifdef KTERM_ENABLE_GATEWAY
KTERM_API void KTerm_SetGatewayCallback(KTerm* term, GatewayCallback callback);
KTERM_API void KTerm_RegisterGatewayExtension(KTerm* term, const char* name, GatewayExtHandler handler);
#endif
KTERM_API void KTerm_SetSessionResizeCallback(KTerm* term, SessionResizeCallback callback);

// Testing and diagnostics
KTERM_API void KTerm_RunTest(KTerm* term, const char* test_name); // Run predefined test sequences
KTERM_API void KTerm_ShowInfo(KTerm* term);           // Display current terminal state/info
KTERM_API void KTerm_EnableDebug(KTerm* term, bool enable);     // Toggle verbose debug logging
KTERM_API void KTerm_LogUnsupportedSequence(KTerm* term, const char* sequence); // Log an unsupported sequence
KTERM_API void KTerm_ReportError(KTerm* term, KTermErrorLevel level, KTermErrorSource source, const char* format, ...);
KTermStatus KTerm_GetStatus(KTerm* term);
KTERM_API void KTerm_ShowDiagnostics(KTerm* term);      // Display buffer usage info

// Screen buffer management
KTERM_API void KTerm_SwapScreenBuffer(KTerm* term); // Handles 1047/1049 logic

KTERM_API void KTerm_LoadFont(KTerm* term, const char* filepath);
KTERM_API void KTerm_CalculateFontMetrics(const void* data, int count, int width, int height, int stride, bool is_16bit, KTermFontMetric* metrics_out);

// Clipboard
KTERM_API void KTerm_CopySelectionToClipboard(KTerm* term);

// Direct Write (Bypasses Input Queue - For High-Throughput Graphics)
KTERM_API void KTerm_WriteRawGraphics(KTerm* term, int session_index, const char* data, size_t len);

// Helper to allocate a glyph index in the dynamic atlas for any Unicode codepoint
KTERM_API uint32_t KTerm_AllocateGlyph(KTerm* term, uint32_t codepoint);

// Forward declaration for SGR helper
int ProcessExtendedKTermColor(KTerm* term, ExtendedKTermColor* color, int param_index);

// Resize the terminal grid and window texture
KTERM_API void KTerm_Resize(KTerm* term, int cols, int rows);

// Multiplexer API
KTermPane* KTerm_SplitPane(KTerm* term, KTermPane* target_pane, KTermPaneType split_type, float ratio);
KTERM_API void KTerm_ClosePane(KTerm* term, KTermPane* pane);

// Internal rendering/parsing functions (potentially exposed for advanced use or testing)
KTERM_API void KTerm_CreateFontTexture(KTerm* term);

// Internal helper forward declaration
KTERM_API void KTerm_InitCompute(KTerm* term);
KTERM_API void KTerm_PrepareRenderBuffer(KTerm* term);

// Low-level char processing (called by KTerm_ProcessEvents via KTerm_ProcessChar)
KTERM_API void KTerm_ProcessChar(KTerm* term, KTermSession* session, unsigned char ch); // Main dispatcher for character processing
KTERM_API void KTerm_ProcessPrinterControllerChar(KTerm* term, KTermSession* session, unsigned char ch); // Handle Printer Controller Mode
KTERM_API void KTerm_ProcessNormalChar(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessEscapeChar(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessCSIChar(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessOSCChar(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessDCSChar(KTerm* term, KTermSession* session, unsigned char ch);
void ProcessMacroDefinition(KTerm* term, KTermSession* session, const char* data);
void ExecuteInvokeMacro(KTerm* term, KTermSession* session);
void ExecuteDECSRFR(KTerm* term, KTermSession* session);
KTERM_API void KTerm_ProcessAPCChar(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessPMChar(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessSOSChar(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessVT52Char(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessKittyChar(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessSixelChar(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessSixelSTChar(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessControlChar(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessStringTerminator(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessCharsetCommand(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessHashChar(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessPercentChar(KTerm* term, KTermSession* session, unsigned char ch);
KTERM_API void KTerm_ProcessnFChar(KTerm* term, KTermSession* session, unsigned char ch);


// Grid Access (Post-Flush / Simulation)
EnhancedTermChar* KTerm_GetCell(KTerm* term, int x, int y);
KTERM_API void KTerm_SetCellDirect(KTerm* term, int x, int y, EnhancedTermChar c);
KTERM_API void KTerm_MarkRegionDirty(KTerm* term, KTermRect rect);

// Screen manipulation internals
KTERM_API void KTerm_ScrollUpRegion(KTerm* term, int top, int bottom, int lines);
KTERM_API void KTerm_InsertLinesAt(KTerm* term, int row, int count); // Added IL
KTERM_API void KTerm_DeleteLinesAt(KTerm* term, int row, int count); // Added DL
KTERM_API void KTerm_InsertCharactersAt(KTerm* term, int row, int col, int count); // Added ICH
KTERM_API void KTerm_DeleteCharactersAt(KTerm* term, int row, int col, int count); // Added DCH
KTERM_API void KTerm_InsertCharacterAtCursor(KTerm* term, unsigned int ch); // Handles character placement and insert mode
KTERM_API void KTerm_ScrollDownRegion(KTerm* term, int top, int bottom, int lines);

KTERM_API void KTerm_ExecuteSaveCursor(KTerm* term, KTermSession* session);
static void KTerm_ExecuteSaveCursor_Internal(KTermSession* session);
KTERM_API void KTerm_ExecuteRestoreCursor(KTerm* term, KTermSession* session);
static void KTerm_ExecuteRestoreCursor_Internal(KTermSession* session);

KTERM_API void KTerm_FlushOps(KTerm* term, KTermSession* session); // Flush pending ops to grid

// Response and parsing helpers
KTERM_API void KTerm_QueueResponse(KTerm* term, const char* response); // Add string to answerback_buffer
KTERM_API void KTerm_QueueResponseBytes(KTerm* term, const char* data, size_t len);
KTERM_API void KTerm_DispatchSequence(KTerm* term, KTermSession* session, VTParseState type);
#ifdef KTERM_ENABLE_GATEWAY
static void KTerm_ParseGatewayCommand(KTerm* term, KTermSession* session, const char* data, size_t len); // Gateway Protocol Parser
#endif
KTERM_API int KTerm_ParseCSIParams(KTerm* term, const char* params, int* out_params, int max_params); // Parses CSI parameter string into escape_params
KTERM_API int KTerm_GetCSIParam(KTerm* term, KTermSession* session, int index, int default_value); // Gets a parsed CSI parameter
KTERM_API void KTerm_ExecuteCSICommand(KTerm* term, KTermSession* session, unsigned char command);
KTERM_API void KTerm_ExecuteOSCCommand(KTerm* term, KTermSession* session);
KTERM_API void KTerm_ExecuteDCSCommand(KTerm* term, KTermSession* session);
KTERM_API void KTerm_ExecuteAPCCommand(KTerm* term, KTermSession* session);
KTERM_API void KTerm_ExecuteKittyCommand(KTerm* term, KTermSession* session);
KTERM_API void KTerm_ExecutePMCommand(KTerm* term, KTermSession* session);
KTERM_API void KTerm_ExecuteSOSCommand(KTerm* term, KTermSession* session);
KTERM_API void KTerm_ExecuteDCSAnswerback(KTerm* term, KTermSession* session);

// Cell and attribute helpers
KTERM_API void KTerm_ClearCell(KTerm* term, EnhancedTermChar* cell); // Clears a cell with current attributes
KTERM_API void KTerm_ResetAllAttributes(KTerm* term, KTermSession* session);          // Resets current text attributes to default

// Character set translation helpers
unsigned int KTerm_TranslateDECSpecial(KTerm* term, unsigned char ch);
unsigned int KTerm_TranslateDECMultinational(KTerm* term, unsigned char ch);

// Keyboard sequence generation helpers (Removed in v2.1)

// Scripting API functions
KTERM_API void KTerm_Script_PutChar(KTerm* term, unsigned char ch);
KTERM_API void KTerm_Script_Print(KTerm* term, const char* text);
KTERM_API void KTerm_Script_Printf(KTerm* term, const char* format, ...);
KTERM_API void KTerm_Script_Cls(KTerm* term);
KTERM_API void KTerm_Script_SetKTermColor(KTerm* term, int fg, int bg);


// KTermPushConstants and GPU_ATTR macros moved to kt_composite_sit.h

#ifdef KTERM_ENABLE_GATEWAY
typedef struct {
    char name[32];
    GatewayExtHandler handler;
} KTermGatewayExtension;
#endif

// =============================================================================
// MAIN ENHANCED TERMINAL STRUCTURE
// =============================================================================

// Saved Cursor State (DECSC/DECRC)
typedef struct {
    int x, y;
    bool origin_mode;
    bool auto_wrap_mode;

    // Attributes
    ExtendedKTermColor fg_color;
    ExtendedKTermColor bg_color;
    uint32_t attributes;

    // Charset
    CharsetState charset;
} SavedCursorState;

// SGR Stack State (XTPUSHSGR/XTPOPSGR)
typedef struct {
    ExtendedKTermColor fg_color;
    ExtendedKTermColor bg_color;
    ExtendedKTermColor ul_color;
    ExtendedKTermColor st_color;
    uint32_t attributes;
} SavedSGRState;


#endif // KTERM_H
