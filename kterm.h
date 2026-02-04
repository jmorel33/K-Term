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

// --- Version Macros ---
#define KTERM_VERSION_MAJOR 2
#define KTERM_VERSION_MINOR 4
#define KTERM_VERSION_PATCH 16
#define KTERM_VERSION_REVISION ""
#define KTERM_VERSION_STRING "2.4.16 (Shader Config Refactor)"

// Default to enabling Gateway Protocol unless explicitly disabled
#ifndef KTERM_DISABLE_GATEWAY
    #define KTERM_ENABLE_GATEWAY
#endif

#include "kt_render_sit.h"
#include "kt_layout.h"
#include "font_data.h"


// Safe Allocation Wrappers
void* KTerm_Malloc(size_t size);
void* KTerm_Calloc(size_t nmemb, size_t size);
void* KTerm_Realloc(void* ptr, size_t size);
void KTerm_Free(void* ptr);

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
#define DEFAULT_CHAR_WIDTH 8
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
typedef void (*KTermOutputSink)(void* user_data, const char* data, size_t len); // Direct Output Sink
#ifdef KTERM_ENABLE_GATEWAY
typedef void (*GatewayCallback)(KTerm* term, const char* class_id, const char* id, const char* command, const char* params); // Gateway Protocol
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
    KTermEvent buffer[KEY_EVENT_BUFFER_SIZE];
    atomic_int buffer_head;
    atomic_int buffer_tail;

    bool use_8bit_controls; // S7C1T / S8C1T state
    // buffer_count removed for thread safety
    atomic_int total_events;
    atomic_int dropped_events;

    // Rate Limiting (DECARR)
    int last_key_code;
    double last_key_time;
    int repeat_state; // 0=None, 1=WaitDelay, 2=Repeating
    bool use_software_repeat;
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
#define KTERM_TERMINAL_SHADER_PATH "shaders/terminal.comp"
#endif
#ifndef KTERM_VECTOR_SHADER_PATH
#define KTERM_VECTOR_SHADER_PATH "shaders/vector.comp"
#endif
#ifndef KTERM_SIXEL_SHADER_PATH
#define KTERM_SIXEL_SHADER_PATH "shaders/sixel.comp"
#endif


#if defined(SITUATION_USE_VULKAN)
    // --- VULKAN DEFINITIONS ---
    static const char* terminal_compute_preamble =
    "#version 460\n"
    "#define VULKAN_BACKEND\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "#extension GL_ARB_bindless_texture : require\n"
    "struct GPUCell { uint char_code; uint fg_color; uint bg_color; uint flags; uint ul_color; uint st_color; };\n"
    "layout(buffer_reference, scalar) buffer KTermBuffer { GPUCell cells[]; };\n"
    "layout(set = 1, binding = 0, rgba8) uniform image2D output_image;\n"
    "layout(buffer_reference, scalar) buffer ConfigBuffer { float crt_curvature; float scanline_intensity; float glow_intensity; float noise_intensity; float visual_bell_intensity; uint flags; };\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    vec2 screen_size;\n"
    "    vec2 char_size;\n"
    "    vec2 grid_size;\n"
    "    float time;\n"
    "    uint cursor_index;\n"
    "    uint cursor_blink_state;\n"
    "    uint text_blink_state;\n"
    "    uint sel_start;\n"
    "    uint sel_end;\n"
    "    uint sel_active;\n"
    "    uint mouse_cursor_index;\n"
    "    uint64_t terminal_buffer_addr;\n"
    "    uint64_t vector_buffer_addr;\n"
    "    uint64_t font_texture_handle;\n"
    "    uint64_t sixel_texture_handle;\n"
    "    uint64_t vector_texture_handle;\n"
    "    uint64_t shader_config_addr;\n"
    "    uint atlas_cols;\n"
    "    uint vector_count;\n"
    "    int sixel_y_offset;\n"
    "    uint grid_color;\n"
    "    uint conceal_char_code;\n"
    "} pc;\n";

    static const char* vector_compute_preamble =
    "#version 460\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n"
    "struct GPUVectorLine { vec2 start; vec2 end; uint color; float intensity; uint mode; float _pad; };\n"
    "layout(buffer_reference, scalar) buffer VectorBuffer { GPUVectorLine data[]; };\n"
    "layout(buffer_reference, scalar) buffer ConfigBuffer { float crt_curvature; float scanline_intensity; float glow_intensity; float noise_intensity; float visual_bell_intensity; uint flags; };\n"
    "layout(set = 1, binding = 0, rgba8) uniform image2D output_image;\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    vec2 screen_size;\n"
    "    vec2 char_size;\n"
    "    vec2 grid_size;\n"
    "    float time;\n"
    "    uint cursor_index;\n"
    "    uint cursor_blink_state;\n"
    "    uint text_blink_state;\n"
    "    uint sel_start;\n"
    "    uint sel_end;\n"
    "    uint sel_active;\n"
    "    uint mouse_cursor_index;\n"
    "    uint64_t terminal_buffer_addr;\n"
    "    uint64_t vector_buffer_addr;\n"
    "    uint64_t font_texture_handle;\n"
    "    uint64_t sixel_texture_handle;\n"
    "    uint64_t vector_texture_handle;\n"
    "    uint64_t shader_config_addr;\n"
    "    uint atlas_cols;\n"
    "    uint vector_count;\n"
    "    int sixel_y_offset;\n"
    "    uint grid_color;\n"
    "    uint conceal_char_code;\n"
    "} pc;\n";

    static const char* sixel_compute_preamble =
    "#version 460\n"
    "#define VULKAN_BACKEND\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "#extension GL_ARB_bindless_texture : require\n"
    "layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n"
    "struct GPUSixelStrip { uint x; uint y; uint pattern; uint color_index; };\n"
    "layout(buffer_reference, scalar) buffer SixelBuffer { GPUSixelStrip data[]; };\n"
    "layout(buffer_reference, scalar) buffer PaletteBuffer { uint colors[]; };\n"
    "layout(buffer_reference, scalar) buffer ConfigBuffer { float crt_curvature; float scanline_intensity; float glow_intensity; float noise_intensity; float visual_bell_intensity; uint flags; };\n"
    "layout(set = 1, binding = 0, rgba8) uniform image2D output_image;\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    vec2 screen_size;\n"
    "    vec2 char_size;\n"
    "    vec2 grid_size;\n"
    "    float time;\n"
    "    uint cursor_index;\n"
    "    uint cursor_blink_state;\n"
    "    uint text_blink_state;\n"
    "    uint sel_start;\n"
    "    uint sel_end;\n"
    "    uint sel_active;\n"
    "    uint mouse_cursor_index;\n"
    "    uint64_t terminal_buffer_addr;\n"
    "    uint64_t vector_buffer_addr;\n"
    "    uint64_t font_texture_handle;\n"
    "    uint64_t sixel_texture_handle;\n"
    "    uint64_t vector_texture_handle;\n"
    "    uint64_t shader_config_addr;\n"
    "    uint atlas_cols;\n"
    "    uint vector_count;\n"
    "    int sixel_y_offset;\n"
    "    uint grid_color;\n"
    "    uint conceal_char_code;\n"
    "} pc;\n";

    static const char* blit_compute_preamble =
    "#version 460\n"
    "#define VULKAN_BACKEND\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "#extension GL_ARB_bindless_texture : require\n"
    "layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;\n"
    "layout(set = 1, binding = 0, rgba8) uniform image2D dstImage;\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    ivec2 dest_pos;\n"
    "    ivec2 src_size;\n"
    "    uint64_t src_texture_handle;\n"
    "    ivec4 clip_rect;\n"
    "} pc;\n";

#else
    // --- OPENGL / DEFAULT DEFINITIONS ---
    static const char* terminal_compute_preamble =
    "#version 460\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "#extension GL_ARB_bindless_texture : require\n"
    "layout(local_size_x = 8, local_size_y = 16, local_size_z = 1) in;\n"
    "struct GPUCell { uint char_code; uint fg_color; uint bg_color; uint flags; uint ul_color; uint st_color; };\n"
    "layout(buffer_reference, scalar) buffer KTermBuffer { GPUCell cells[]; };\n"
    "layout(buffer_reference, scalar) buffer ConfigBuffer { float crt_curvature; float scanline_intensity; float glow_intensity; float noise_intensity; float visual_bell_intensity; uint flags; };\n"
    "layout(binding = 1, rgba8) uniform image2D output_image;\n"
    "layout(scalar, binding = 0) uniform PushConstants {\n"
    "    vec2 screen_size;\n"
    "    vec2 char_size;\n"
    "    vec2 grid_size;\n"
    "    float time;\n"
    "    uint cursor_index;\n"
    "    uint cursor_blink_state;\n"
    "    uint text_blink_state;\n"
    "    uint sel_start;\n"
    "    uint sel_end;\n"
    "    uint sel_active;\n"
    "    uint mouse_cursor_index;\n"
    "    uint64_t terminal_buffer_addr;\n"
    "    uint64_t vector_buffer_addr;\n"
    "    uint64_t font_texture_handle;\n"
    "    uint64_t sixel_texture_handle;\n"
    "    uint64_t vector_texture_handle;\n"
    "    uint64_t shader_config_addr;\n"
    "    uint atlas_cols;\n"
    "    uint vector_count;\n"
    "    int sixel_y_offset;\n"
    "    uint grid_color;\n"
    "    uint conceal_char_code;\n"
    "} pc;\n";

    static const char* vector_compute_preamble =
    "#version 460\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n"
    "struct GPUVectorLine { vec2 start; vec2 end; uint color; float intensity; uint mode; float _pad; };\n"
    "layout(buffer_reference, scalar) buffer VectorBuffer { GPUVectorLine data[]; };\n"
    "layout(buffer_reference, scalar) buffer ConfigBuffer { float crt_curvature; float scanline_intensity; float glow_intensity; float noise_intensity; float visual_bell_intensity; uint flags; };\n"
    "layout(binding = 1, rgba8) uniform image2D output_image;\n"
    "layout(scalar, binding = 0) uniform PushConstants {\n"
    "    vec2 screen_size;\n"
    "    vec2 char_size;\n"
    "    vec2 grid_size;\n"
    "    float time;\n"
    "    uint cursor_index;\n"
    "    uint cursor_blink_state;\n"
    "    uint text_blink_state;\n"
    "    uint sel_start;\n"
    "    uint sel_end;\n"
    "    uint sel_active;\n"
    "    uint mouse_cursor_index;\n"
    "    uint64_t terminal_buffer_addr;\n"
    "    uint64_t vector_buffer_addr;\n"
    "    uint64_t font_texture_handle;\n"
    "    uint64_t sixel_texture_handle;\n"
    "    uint64_t vector_texture_handle;\n"
    "    uint64_t shader_config_addr;\n"
    "    uint atlas_cols;\n"
    "    uint vector_count;\n"
    "    int sixel_y_offset;\n"
    "    uint grid_color;\n"
    "    uint conceal_char_code;\n"
    "} pc;\n";

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
    "layout(buffer_reference, scalar) buffer ConfigBuffer { float crt_curvature; float scanline_intensity; float glow_intensity; float noise_intensity; float visual_bell_intensity; uint flags; };\n"
    "layout(binding = 1, rgba8) uniform image2D output_image;\n"
    "layout(scalar, binding = 0) uniform PushConstants {\n"
    "    vec2 screen_size;\n"
    "    vec2 char_size;\n"
    "    vec2 grid_size;\n"
    "    float time;\n"
    "    uint cursor_index;\n"
    "    uint cursor_blink_state;\n"
    "    uint text_blink_state;\n"
    "    uint sel_start;\n"
    "    uint sel_end;\n"
    "    uint sel_active;\n"
    "    uint mouse_cursor_index;\n"
    "    uint64_t terminal_buffer_addr;\n"
    "    uint64_t vector_buffer_addr;\n"
    "    uint64_t font_texture_handle;\n"
    "    uint64_t sixel_texture_handle;\n"
    "    uint64_t vector_texture_handle;\n"
    "    uint64_t shader_config_addr;\n"
    "    uint atlas_cols;\n"
    "    uint vector_count;\n"
    "    int sixel_y_offset;\n"
    "    uint grid_color;\n"
    "    uint conceal_char_code;\n"
    "} pc;\n";

    static const char* blit_compute_preamble =
    "#version 460\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "#extension GL_ARB_bindless_texture : require\n"
    "layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;\n"
    "layout(binding = 1, rgba8) uniform image2D dstImage;\n"
    "layout(scalar, binding = 0) uniform PushConstants {\n"
    "    ivec2 dest_pos;\n"
    "    ivec2 src_size;\n"
    "    uint64_t src_texture_handle;\n"
    "    ivec4 clip_rect;\n"
    "} pc;\n";
#endif



// External declarations for users of the library (if not header-only)
// extern Texture2D font_texture; // Moved to struct
// extern RGB_KTermColor color_palette[256]; // Moved to struct
extern KTermColor ansi_colors[16];        // KTerm Color type for the 16 base ANSI colors
// extern unsigned char font_data[256 * 32]; // Defined in implementation

// =============================================================================
// VT COMPLIANCE LEVELS
// =============================================================================
void KTerm_ResetGraphics(KTerm* term, KTermSession* session, GraphicsResetFlags flags);

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

KTerm* KTerm_Create(KTermConfig config);
void KTerm_Destroy(KTerm* term);

// Session Management
void KTerm_SetActiveSession(KTerm* term, int index);
void KTerm_SetSplitScreen(KTerm* term, bool active, int row, int top_idx, int bot_idx);
void KTerm_WriteCharToSession(KTerm* term, int session_index, unsigned char ch);
void KTerm_SetResponseEnabled(KTerm* term, int session_index, bool enable);
bool KTerm_InitSession(KTerm* term, int index);

// KTerm lifecycle
bool KTerm_Init(KTerm* term);
void KTerm_Cleanup(KTerm* term);
void KTerm_Update(KTerm* term);  // Process events, update states (e.g., cursor blink)
void KTerm_Draw(KTerm* term);    // Render the terminal state to screen

// VT compliance and identification
bool KTerm_GetKey(KTerm* term, KTermEvent* event); // Retrieve buffered event
void KTerm_QueueInputEvent(KTerm* term, KTermEvent event); // Push event to buffer
void KTerm_SetLevel(KTerm* term, KTermSession* session, VTLevel level);
VTLevel KTerm_GetLevel(KTerm* term);
// void EnableVTFeature(const char* feature, bool enable); // e.g., "sixel", "DECCKM" - Deprecated by KTerm_SetLevel
// bool IsVTFeatureSupported(const char* feature); - Deprecated by direct struct access
void GetDeviceAttributes(KTerm* term, char* primary, char* secondary, size_t buffer_size);

// Enhanced pipeline management (for host input)
bool KTerm_WriteChar(KTerm* term, unsigned char ch);
bool KTerm_WriteString(KTerm* term, const char* str);
bool KTerm_WriteFormat(KTerm* term, const char* format, ...);
// bool PipelineWriteUTF8(const char* utf8_str); // Requires UTF-8 decoding logic
void KTerm_ProcessEvents(KTerm* term); // Process characters from the pipeline
void KTerm_ClearEvents(KTerm* term);
int KTerm_GetPendingEventCount(KTerm* term);
bool KTerm_IsEventOverflow(KTerm* term);

// Performance management
void KTerm_SetPipelineTargetFPS(KTerm* term, int fps);    // Helps tune processing budget
void KTerm_SetPipelineTimeBudget(KTerm* term, double pct); // Percentage of frame time for pipeline

// Mouse support (enhanced)
void KTerm_SetMouseTracking(KTerm* term, MouseTrackingMode mode); // Explicitly set a mouse mode
void KTerm_EnableMouseFeature(KTerm* term, const char* feature, bool enable); // e.g., "focus", "sgr"
// void KTerm_UpdateMouse(KTerm* term); // Removed in v2.1
// void KTerm_UpdateKeyboard(KTerm* term); // Removed in v2.1
// void UpdateKeyboard(KTerm* term);  // Removed in v2.1
// bool GetKeyEvent(KTerm* term, KeyEvent* event);  // Removed in v2.1
void KTerm_SetKeyboardMode(KTerm* term, const char* mode, bool enable); // "application_cursor", "keypad_numeric"
void KTerm_SetFocus(KTerm* term, bool focused); // Report focus state (CSI I/O)
void KTerm_DefineFunctionKey(KTerm* term, int key_num, const char* sequence); // Program F1-F24

// KTerm control and modes
void KTerm_SetMode(KTerm* term, const char* mode, bool enable); // Generic mode setting by name
void KTerm_SetCursorShape(KTerm* term, CursorShape shape);
void KTerm_SetCursorKTermColor(KTerm* term, ExtendedKTermColor color);

// Character sets and encoding
void KTerm_SelectCharacterSet(KTerm* term, int gset, CharacterSet charset); // Designate G0-G3
void KTerm_SetCharacterSet(KTerm* term, CharacterSet charset); // Set current GL (usually G0)
unsigned int TranslateCharacter(KTerm* term, unsigned char ch, CharsetState* state); // Translate based on active CS

// Tab stops
void KTerm_SetTabStop(KTerm* term, int column);
void KTerm_ClearTabStop(KTerm* term, int column);
void KTerm_ClearAllTabStops(KTerm* term);
int NextTabStop(KTerm* term, int current_column);
int PreviousTabStop(KTerm* term, int current_column); // Added for CBT

// Bracketed paste
void KTerm_EnableBracketedPaste(KTerm* term, bool enable); // Enable/disable CSI ? 2004 h/l
bool IsBracketedPasteActive(KTerm* term);
void ProcessPasteData(KTerm* term, const char* data, size_t length); // Handle pasted data

// Rectangular operations (VT420+)
void KTerm_DefineRectangle(KTerm* term, int top, int left, int bottom, int right); // (DECSERA, DECFRA, DECCRA)
void KTerm_ExecuteRectangularOperation(KTerm* term, RectOperation op, const EnhancedTermChar* fill_char);
void KTerm_CopyRectangle(KTerm* term, VTRectangle src, int dest_x, int dest_y);
void KTerm_ExecuteRectangularOps(KTerm* term, KTermSession* session);  // DECCRA Implementation
void KTerm_ExecuteRectangularOps2(KTerm* term, KTermSession* session); // DECRQCRA Implementation

// Sixel graphics
void KTerm_InitSixelGraphics(KTerm* term, KTermSession* session);
void KTerm_ProcessSixelData(KTerm* term, KTermSession* session, const char* data, size_t length); // Process raw Sixel string
void KTerm_DrawSixelGraphics(KTerm* term); // Render current Sixel image

// Soft fonts
void KTerm_LoadSoftFont(KTerm* term, const unsigned char* font_data, int char_start, int char_count); // DECDLD
void KTerm_SelectSoftFont(KTerm* term, bool enable); // Enable/disable use of loaded soft font

// Title management
void KTerm_SetWindowTitle(KTerm* term, const char* title); // Set window title (OSC 0, OSC 2)
void KTerm_SetIconTitle(KTerm* term, const char* title);   // Set icon title (OSC 1)
const char* KTerm_GetWindowTitle(KTerm* term);
const char* KTerm_GetIconTitle(KTerm* term);

// Callbacks
void KTerm_SetResponseCallback(KTerm* term, ResponseCallback callback);
void KTerm_SetOutputSink(KTerm* term, KTermOutputSink sink, void* ctx);
void KTerm_SetPrinterCallback(KTerm* term, PrinterCallback callback);
void KTerm_SetTitleCallback(KTerm* term, TitleCallback callback);
void KTerm_SetBellCallback(KTerm* term, BellCallback callback);
void KTerm_SetNotificationCallback(KTerm* term, NotificationCallback callback);
void KTerm_SetErrorCallback(KTerm* term, KTermErrorCallback callback, void* user_data);
#ifdef KTERM_ENABLE_GATEWAY
void KTerm_SetGatewayCallback(KTerm* term, GatewayCallback callback);
#endif
void KTerm_SetSessionResizeCallback(KTerm* term, SessionResizeCallback callback);

// Testing and diagnostics
void KTerm_RunTest(KTerm* term, const char* test_name); // Run predefined test sequences
void KTerm_ShowInfo(KTerm* term);           // Display current terminal state/info
void KTerm_EnableDebug(KTerm* term, bool enable);     // Toggle verbose debug logging
void KTerm_LogUnsupportedSequence(KTerm* term, const char* sequence); // Log an unsupported sequence
void KTerm_ReportError(KTerm* term, KTermErrorLevel level, KTermErrorSource source, const char* format, ...);
KTermStatus KTerm_GetStatus(KTerm* term);
void KTerm_ShowDiagnostics(KTerm* term);      // Display buffer usage info

// Screen buffer management
void KTerm_SwapScreenBuffer(KTerm* term); // Handles 1047/1049 logic

void KTerm_LoadFont(KTerm* term, const char* filepath);
void KTerm_CalculateFontMetrics(const void* data, int count, int width, int height, int stride, bool is_16bit, KTermFontMetric* metrics_out);

// Clipboard
void KTerm_CopySelectionToClipboard(KTerm* term);

// Helper to allocate a glyph index in the dynamic atlas for any Unicode codepoint
uint32_t KTerm_AllocateGlyph(KTerm* term, uint32_t codepoint);

// Forward declaration for SGR helper
int ProcessExtendedKTermColor(KTerm* term, ExtendedKTermColor* color, int param_index);

// Resize the terminal grid and window texture
void KTerm_Resize(KTerm* term, int cols, int rows);

// Multiplexer API
KTermPane* KTerm_SplitPane(KTerm* term, KTermPane* target_pane, KTermPaneType split_type, float ratio);
void KTerm_ClosePane(KTerm* term, KTermPane* pane);

// Internal rendering/parsing functions (potentially exposed for advanced use or testing)
void KTerm_CreateFontTexture(KTerm* term);

// Internal helper forward declaration
void KTerm_InitCompute(KTerm* term);
void KTerm_PrepareRenderBuffer(KTerm* term);

// Low-level char processing (called by KTerm_ProcessEvents via KTerm_ProcessChar)
void KTerm_ProcessChar(KTerm* term, KTermSession* session, unsigned char ch); // Main dispatcher for character processing
void KTerm_ProcessPrinterControllerChar(KTerm* term, KTermSession* session, unsigned char ch); // Handle Printer Controller Mode
void KTerm_ProcessNormalChar(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessEscapeChar(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessCSIChar(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessOSCChar(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessDCSChar(KTerm* term, KTermSession* session, unsigned char ch);
void ProcessMacroDefinition(KTerm* term, KTermSession* session, const char* data);
void ExecuteInvokeMacro(KTerm* term, KTermSession* session);
void ExecuteDECSRFR(KTerm* term, KTermSession* session);
void KTerm_ProcessAPCChar(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessPMChar(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessSOSChar(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessVT52Char(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessKittyChar(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessSixelChar(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessSixelSTChar(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessControlChar(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessStringTerminator(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessCharsetCommand(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessHashChar(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessPercentChar(KTerm* term, KTermSession* session, unsigned char ch);
void KTerm_ProcessnFChar(KTerm* term, KTermSession* session, unsigned char ch);


// Grid Access (Post-Flush / Simulation)
EnhancedTermChar* KTerm_GetCell(KTerm* term, int x, int y);
void KTerm_SetCellDirect(KTerm* term, int x, int y, EnhancedTermChar c);
void KTerm_MarkRegionDirty(KTerm* term, KTermRect rect);

// Screen manipulation internals
void KTerm_ScrollUpRegion(KTerm* term, int top, int bottom, int lines);
void KTerm_InsertLinesAt(KTerm* term, int row, int count); // Added IL
void KTerm_DeleteLinesAt(KTerm* term, int row, int count); // Added DL
void KTerm_InsertCharactersAt(KTerm* term, int row, int col, int count); // Added ICH
void KTerm_DeleteCharactersAt(KTerm* term, int row, int col, int count); // Added DCH
void KTerm_InsertCharacterAtCursor(KTerm* term, unsigned int ch); // Handles character placement and insert mode
void KTerm_ScrollDownRegion(KTerm* term, int top, int bottom, int lines);

void KTerm_ExecuteSaveCursor(KTerm* term, KTermSession* session);
static void KTerm_ExecuteSaveCursor_Internal(KTermSession* session);
void KTerm_ExecuteRestoreCursor(KTerm* term, KTermSession* session);
static void KTerm_ExecuteRestoreCursor_Internal(KTermSession* session);

void KTerm_FlushOps(KTerm* term, KTermSession* session); // Flush pending ops to grid

// Response and parsing helpers
void KTerm_QueueResponse(KTerm* term, const char* response); // Add string to answerback_buffer
void KTerm_QueueResponseBytes(KTerm* term, const char* data, size_t len);
void KTerm_DispatchSequence(KTerm* term, KTermSession* session, VTParseState type);
#ifdef KTERM_ENABLE_GATEWAY
static void KTerm_ParseGatewayCommand(KTerm* term, KTermSession* session, const char* data, size_t len); // Gateway Protocol Parser
#endif
int KTerm_ParseCSIParams(KTerm* term, const char* params, int* out_params, int max_params); // Parses CSI parameter string into escape_params
int KTerm_GetCSIParam(KTerm* term, KTermSession* session, int index, int default_value); // Gets a parsed CSI parameter
void KTerm_ExecuteCSICommand(KTerm* term, KTermSession* session, unsigned char command);
void KTerm_ExecuteOSCCommand(KTerm* term, KTermSession* session);
void KTerm_ExecuteDCSCommand(KTerm* term, KTermSession* session);
void KTerm_ExecuteAPCCommand(KTerm* term, KTermSession* session);
void KTerm_ExecuteKittyCommand(KTerm* term, KTermSession* session);
void KTerm_ExecutePMCommand(KTerm* term, KTermSession* session);
void KTerm_ExecuteSOSCommand(KTerm* term, KTermSession* session);
void KTerm_ExecuteDCSAnswerback(KTerm* term, KTermSession* session);

// Cell and attribute helpers
void KTerm_ClearCell(KTerm* term, EnhancedTermChar* cell); // Clears a cell with current attributes
void KTerm_ResetAllAttributes(KTerm* term, KTermSession* session);          // Resets current text attributes to default

// Character set translation helpers
unsigned int KTerm_TranslateDECSpecial(KTerm* term, unsigned char ch);
unsigned int KTerm_TranslateDECMultinational(KTerm* term, unsigned char ch);

// Keyboard sequence generation helpers (Removed in v2.1)

// Scripting API functions
void KTerm_Script_PutChar(KTerm* term, unsigned char ch);
void KTerm_Script_Print(KTerm* term, const char* text);
void KTerm_Script_Printf(KTerm* term, const char* format, ...);
void KTerm_Script_Cls(KTerm* term);
void KTerm_Script_SetKTermColor(KTerm* term, int fg, int bg);


#define FONT_DATA_IMPLEMENTATION
#include "font_data.h"
#undef FONT_DATA_IMPLEMENTATION

// KTermPushConstants and GPU_ATTR macros moved to kt_composite_sit.h

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

#ifdef KTERM_IMPLEMENTATION
// --- Implementation Includes ---
  #if !defined(SITUATION_IMPLEMENTATION) && !defined(STB_TRUETYPE_IMPLEMENTATION)
    #define STB_TRUETYPE_IMPLEMENTATION
  #endif
  #define KTERM_LAYOUT_IMPLEMENTATION
  #define FONT_DATA_IMPLEMENTATION
#include "font_data.h"
#undef FONT_DATA_IMPLEMENTATION
#include "stb_truetype.h"
#include "kt_parser.h"
#include "kt_layout.h"


#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

// --- Internal Struct Definitions (Early) ---
typedef struct {
    int state; // 0=Command, 1=Values, 2=Options, 3=Text String
    int x, y; // Current beam position (0-(REGIS_WIDTH - 1), 0-(REGIS_HEIGHT - 1))
    int screen_min_x, screen_min_y; // Logical screen extents
    int screen_max_x, screen_max_y;
    int save_x, save_y; // Saved position (stack depth 1)
    uint32_t color; // Current RGBA color
    int write_mode; // 0=Overlay(V), 1=Replace(R), 2=Erase(E), 3=Complement(C)
    char command; // Current command letter (P, V, C, T, W, etc.)
    int params[16]; // Numeric parameters for current command
    bool params_relative[16]; // True if parameter was explicitly signed (+/-)
    int param_count; // Number of parameters parsed so far
    bool has_comma; // Was a comma seen? (Parameter separator)
    bool has_bracket; // Was an opening bracket seen? (Option/Vector list)
    bool has_paren; // Was an opening parenthesis seen? (Options)
    char option_command; // Current option command being parsed
    bool data_pending; // Has new data arrived since last execution?

    // Robust Number Parsing
    int current_val;
    int current_sign;
    bool parsing_val;
    bool val_is_relative;

    // Text Command
    char text_buffer[256];
    int text_pos;
    char string_terminator;

    // Curve & Polygon support
    struct { int x, y; } point_buffer[64];
    int point_count;
    char curve_mode; // 'C'ircle, 'A'rc, 'B'spline (Interpolated), 'O'pen (Unclosed)

    // Extended Text Attributes
    float text_size;
    float text_angle; // Radians

    // Macrographs
    char* macros[26]; // Storage for @A through @Z
    bool recording_macro;
    int macro_index;
    char* macro_buffer;
    size_t macro_len;
    size_t macro_cap;
    int recursion_depth;

    // Load Alphabet (L) Support
    struct {
        char name[16]; // Name of the alphabet (e.g., "A")
        int current_char; // The character currently being defined (0-255)
        int pattern_byte_idx; // Current byte index in the character's pattern (0-31)
        int hex_nibble; // Parsing state for hex pairs (-1 = expecting high nibble)
    } load;
} KTermReGIS;

typedef struct {
    int state;
    int sub_state;
    int x, y;
    int holding_x, holding_y;
    int extra_byte;
    bool pen_down;
} KTermTektronix;

typedef struct KTermSession_T {

    // Operation Queue for Grid Mutations
    KTermOpQueue op_queue;
    KTermRect dirty_rect;

    // Screen management
    EnhancedTermChar* screen_buffer;       // Primary screen ring buffer
    EnhancedTermChar* alt_buffer;          // Alternate screen buffer
    int buffer_height;                     // Total rows in ring buffer
    int screen_head;                       // Index of the top visible row in the buffer (Ring buffer head)
    int history_rows_populated;            // Number of valid lines in scrollback history
    int alt_screen_head;                   // Stored head for alternative screen
    int view_offset;                       // Scrollback offset (0 = bottom/active view)
    int saved_view_offset;                 // Stored scrollback offset for main screen

    // Legacy member placeholders to be removed after refactor,
    // but kept as comments for now to indicate change.
    // EnhancedTermChar screen[term->height][term->width];
    // EnhancedTermChar alt_screen[term->height][term->width];

    int cols; // KTerm width in columns
    int rows; // KTerm height in rows (viewport)
    int lines_per_page; // DECSLPP (Logical Page Height)

    uint8_t* row_dirty; // Tracks dirty state of the VIEWPORT rows (0..rows-1)
    // EnhancedTermChar saved_screen[term->height][term->width]; // For DECSEL/DECSED if implemented

    // Enhanced cursor
    EnhancedCursor cursor;
    SavedCursorState saved_cursor; // For DECSC/DECRC and other save/restore ops
    bool saved_cursor_valid;

    // KTerm identification & conformance
    VTConformance conformance;
    char device_attributes[128];    // Primary DA string (e.g., CSI c)
    char secondary_attributes[128]; // Secondary DA string (e.g., CSI > c)

    // Mode management
    DECModes dec_modes;
    ANSIModes ansi_modes;

    // Current character attributes for new text
    ExtendedKTermColor current_fg;
    ExtendedKTermColor current_bg;
    ExtendedKTermColor current_ul_color;
    ExtendedKTermColor current_st_color;
    uint32_t current_attributes; // Mask of KTERM_ATTR_* applied to new chars
    uint32_t text_blink_state;  // Current blink states (Bit 0: Fast, Bit 1: Slow, Bit 2: Background)
    double text_blink_timer;    // Timer for text blink interval
    int fast_blink_rate;        // Oscillator Slot (Default KTERM_OSC_SLOT_FAST_BLINK)
    int slow_blink_rate;        // Oscillator Slot (Default KTERM_OSC_SLOT_SLOW_BLINK)
    int bg_blink_rate;          // Oscillator Slot (Default KTERM_OSC_SLOT_SLOW_BLINK)

    // Debug Grid
    bool grid_enabled;
    RGB_KTermColor grid_color;
    uint32_t conceal_char_code;

    // Scrolling and margins
    int scroll_top, scroll_bottom;  // Defines the scroll region (0-indexed)
    int left_margin, right_margin;  // Defines left/right margins (0-indexed, VT420+)

    // Character handling
    CharsetState charset;
    TabStops tab_stops;

    // Enhanced features
    BracketedPaste bracketed_paste;
    ProgrammableKeys programmable_keys; // For DECUDK
    StoredMacros stored_macros;         // For DECDMAC
    SixelGraphics sixel;
    KTermReGIS regis;
    KTermTektronix tektronix;
    KittyGraphics kitty;
    SoftFont soft_font;                 // For DECDLD
    TitleManager title;

    // Mouse support state
    struct {
        MouseTrackingMode mode; // Mouse tracking mode
        bool enabled; // Mouse input enabled
        bool buttons[3]; // Left, middle, right button states
        int last_x; // Last cell x position
        int last_y; // Last cell y position
        int last_pixel_x; // Last pixel x position
        int last_pixel_y; // Last pixel y position
        bool focused; // Tracks window focus state for CSI I/O
        bool focus_tracking; // Enables focus in/out reporting
        bool sgr_mode; // Enables SGR mouse reporting
        int cursor_x; // Current mouse cursor X cell position
        int cursor_y; // Current mouse cursor Y cell position
    } mouse;

// Helper functions for Ring Buffer Access

    // Missing fields restored
    KTermInputConfig input;
    int auto_repeat_rate;
    int auto_repeat_delay;
    bool response_enabled;
    bool enable_wide_chars;

    // Other fields that seem to be required by session logic
    bool session_open;
    int active_display;
    bool echo_enabled;
    bool input_enabled;
    bool password_mode;
    bool raw_mode;
    bool paused;

    bool printer_available;
    bool auto_print_enabled;
    bool printer_controller_enabled;
    bool locator_enabled;

    struct {
        bool report_button_down;
        bool report_button_up;
        bool report_on_request_only;
    } locator_events;

    struct {
        size_t used;
        size_t total;
    } macro_space;

    struct {
        int algorithm;
        uint32_t last_checksum;
    } checksum;

    char tertiary_attributes[128];
    double visual_bell_timer;

    struct {
        uint32_t codepoint;
        uint32_t min_codepoint;
        int bytes_remaining;
    } utf8;

    struct {
        int start_x, start_y;
        int end_x, end_y;
        bool active;
        bool dragging;
    } selection;

    unsigned int last_char;
    int last_cursor_y;
    unsigned char printer_buffer[8];
    int printer_buf_len;
    void* user_data;

    // Input pipeline specific to session?
    // Usually on KTerm, but if per-session...
    unsigned char input_pipeline[KTERM_INPUT_PIPELINE_SIZE];
    int input_pipeline_length;
    atomic_int pipeline_head;
    atomic_int pipeline_tail;
    int pipeline_count;
    atomic_bool pipeline_overflow;
    bool xoff_sent;

    // Performance
    struct {
        int chars_per_frame;
        double target_frame_time;
        double time_budget;
        double avg_process_time;
        bool burst_mode;
        int burst_threshold;
        bool adaptive_processing;
    } VTperformance;

    // Output Ring Buffer
    struct {
        char data[KTERM_OUTPUT_PIPELINE_SIZE];
        atomic_int head;
        atomic_int tail;
        atomic_bool overflow;
    } response_ring;

    char answerback_buffer[KTERM_OUTPUT_PIPELINE_SIZE];
    int response_length;

    VTParseState parse_state;
    VTParseState saved_parse_state;
    char escape_buffer[MAX_COMMAND_BUFFER];
    int escape_pos;
    int escape_params[MAX_ESCAPE_PARAMS];
    char escape_separators[MAX_ESCAPE_PARAMS];
    int param_count;
    SavedSGRState sgr_stack[10];
    int sgr_stack_depth;

    struct {
        int error_count;
        bool debugging;
    } status;

    struct {
        bool conformance_checking;
        bool vttest_mode;
        bool debug_sequences;
        bool log_unsupported;
    } options;

    kterm_mutex_t lock; // Session Lock (Phase 3)
    int preferred_supplemental;
    bool synchronized_update; // DECSET 2026
} KTermSession;
static inline EnhancedTermChar* GetScreenRow(KTermSession* session, int row) {
    // Access logical row 'row' (0 to HEIGHT-1 or -scrollback) relative to the visible screen top.
    // We adjust by view_offset (which allows looking back).
    // view_offset = 0 means looking at active screen.
    // view_offset > 0 means scrolling up (looking at history).

    // Logical top row of the visible window (including scrollback offset)
    // screen_head points to the top line of the active screen (logical row 0).
    // If we view back, we look at screen_head - view_offset.

    int logical_row_idx = session->screen_head + row - session->view_offset;

    // Handle wrap-around
    int actual_index = logical_row_idx % session->buffer_height;
    if (actual_index < 0) actual_index += session->buffer_height;

    return &session->screen_buffer[actual_index * session->cols];
}

static inline EnhancedTermChar* GetScreenCell(KTermSession* session, int y, int x) {
    if (x < 0 || x >= session->cols) return NULL; // Basic safety
    return &GetScreenRow(session, y)[x];
}

static inline EnhancedTermChar* GetActiveScreenRow(KTermSession* session, int row) {
    // Access logical row 'row' relative to the ACTIVE screen top (ignoring view_offset).
    // This is used for emulation commands that modify the screen state (insert, delete, scroll).

    int logical_row_idx = session->screen_head + row; // No view_offset subtraction

    // Handle wrap-around
    int actual_index = logical_row_idx % session->buffer_height;
    if (actual_index < 0) actual_index += session->buffer_height;

    return &session->screen_buffer[actual_index * session->cols];
}

static inline EnhancedTermChar* GetActiveScreenCell(KTermSession* session, int y, int x) {
    if (x < 0 || x >= session->cols) return NULL;
    return &GetActiveScreenRow(session, y)[x];
}

// Render structures moved to kt_composite_sit.h

typedef struct KTerm_T {
    KTermConfig config;
    KTermSession sessions[MAX_SESSIONS];
    KTermLayout* layout;
    int width;
    int height;
    int active_session;
    int pending_session_switch;
    bool split_screen_active;
    int split_row;
    int session_top;
    int session_bottom;
    ResponseCallback response_callback;
    KTermPipeline compute_pipeline;
    KTermPipeline texture_blit_pipeline;
    KTermBuffer terminal_buffer;
    KTermTexture output_texture;
    KTermTexture font_texture;
    KTermTexture sixel_texture;
    KTermTexture dummy_sixel_texture;
    KTermTexture clear_texture;
    bool compute_initialized;

    KTermCompositor compositor;

    KTermBuffer vector_buffer;
    KTermTexture vector_layer_texture;
    KTermPipeline vector_pipeline;
    uint32_t vector_count;
    GPUVectorLine* vector_staging_buffer;
    size_t vector_capacity;

    KTermBuffer sixel_buffer;
    KTermBuffer sixel_palette_buffer;
    KTermPipeline sixel_pipeline;

    KTermBuffer shader_config_buffer;

    struct {
        float curvature;
        float scanline_intensity;
        float glow_intensity;
        float noise_intensity;
        uint32_t flags;
    } visual_effects;
    bool vector_clear_request;

    uint16_t* glyph_map;
    uint32_t next_atlas_index;
    uint32_t atlas_clock_hand;
    unsigned char* font_atlas_pixels;
    bool font_atlas_dirty;
    uint32_t atlas_width;
    uint32_t atlas_height;
    uint32_t atlas_cols;

    NotificationCallback notification_callback;

    struct {
        bool loaded;
        unsigned char* file_buffer;
        stbtt_fontinfo info;
        float scale;
        int ascent;
        int descent;
        int line_gap;
        int baseline;
    } ttf;

    uint64_t* glyph_last_used;
    uint32_t* atlas_to_codepoint;
    uint64_t frame_count;

    int char_width;
    int char_height;
    int font_data_width;
    int font_data_height;
    const void* current_font_data;
    bool current_font_is_16bit;
    KTermFontMetric font_metrics[256];

    PrinterCallback printer_callback;
#ifdef KTERM_ENABLE_GATEWAY
    GatewayCallback gateway_callback;
#endif
    TitleCallback title_callback;
    BellCallback bell_callback;
    SessionResizeCallback session_resize_callback;
    KTermErrorCallback error_callback;
    void* error_user_data;

    RGB_KTermColor color_palette[256];
    uint32_t charset_lut[32][128];
    EnhancedTermChar* row_scratch_buffer;

    struct {
        bool active;
        int prefix_key_code;
    } mux_input;

    int gateway_target_session;
    int regis_target_session;
    int tektronix_target_session;
    int kitty_target_session;
    int sixel_target_session;

    double last_resize_time;

    KTermOutputSink output_sink;
    void* output_sink_ctx;

    unsigned char input_pipeline[KTERM_INPUT_PIPELINE_SIZE];
    int input_pipeline_length;
    atomic_int pipeline_head;
    atomic_int pipeline_tail;
    int pipeline_count;
    atomic_bool pipeline_overflow;
    bool xoff_sent;

    KTermInputConfig input;

    struct {
        int chars_per_frame;
        double target_frame_time;
        double time_budget;
        double avg_process_time;
        bool burst_mode;
        int burst_threshold;
        bool adaptive_processing;
    } VTperformance;

    // Output Ring Buffer
    struct {
        char data[KTERM_OUTPUT_PIPELINE_SIZE];
        atomic_int head;
        atomic_int tail;
        atomic_bool overflow;
    } response_ring;
    char answerback_message[256];
    bool response_enabled;

    VTParseState parse_state;
    VTParseState saved_parse_state;
    char escape_buffer[MAX_COMMAND_BUFFER];
    int escape_pos;
    int escape_params[MAX_ESCAPE_PARAMS];
    char escape_separators[MAX_ESCAPE_PARAMS];
    int param_count;
    SavedSGRState sgr_stack[10];
    int sgr_stack_depth;

    struct {
        int error_count;
        bool debugging;
    } status;

    struct {
        bool conformance_checking;
        bool vttest_mode;
        bool debug_sequences;
        bool log_unsupported;
    } options;

    bool session_open;
    int active_display;
    bool echo_enabled;
    bool input_enabled;
    bool password_mode;
    bool raw_mode;
    bool paused;
    bool printer_available;
    bool auto_print_enabled;
    bool printer_controller_enabled;
    struct {
        bool report_button_down;
        bool report_button_up;
        bool report_on_request_only;
    } locator_events;
    bool locator_enabled;
    struct {
        size_t used;
        size_t total;
    } macro_space;
    struct {
        int algorithm;
        uint32_t last_checksum;
    } checksum;
    char tertiary_attributes[128];
    double visual_bell_timer;
    struct {
        uint32_t codepoint;
        uint32_t min_codepoint;
        int bytes_remaining;
    } utf8;
    struct {
        int start_x, start_y;
        int end_x, end_y;
        bool active;
        bool dragging;
    } selection;
    unsigned int last_char;
    int last_cursor_y;
    unsigned char printer_buffer[8];
    int printer_buf_len;
    void* user_data;
    int auto_repeat_rate;
    int auto_repeat_delay;
    int preferred_supplemental;
    bool enable_wide_chars;

    kterm_mutex_t lock;
    kterm_thread_t main_thread_id;
} KTerm;

// --- Internal Forward Declarations ---

// Internal forward declares
static unsigned int KTerm_CalculateRectChecksum(KTerm *term, int top, int left, int bottom, int right);
#define GET_SESSION(term) (&(term)->sessions[(term)->active_session])

// =============================================================================
// IMPLEMENTATION BEGINS HERE
// =============================================================================

// Forward declarations for Op Queue helpers
void KTerm_QueueFillRect(KTermSession* session, KTermRect rect, EnhancedTermChar fill_char);
void KTerm_QueueCopyRect(KTermSession* session, KTermRect src, int dst_x, int dst_y);
void KTerm_QueueSetAttrRect(KTermSession* session, KTermRect rect, uint32_t attr_mask, uint32_t attr_values, uint32_t attr_xor_mask, bool set_fg, ExtendedKTermColor fg, bool set_bg, ExtendedKTermColor bg);
void KTerm_QueueInsertLines(KTermSession* session, int count, bool respect_protected);
void KTerm_QueueDeleteLines(KTermSession* session, int count, bool respect_protected);
void KTerm_QueueScrollRegion(KTermSession* session, KTermRect rect, int dy);
void KTerm_QueueResize(KTermSession* session, int cols, int rows, bool reflow);

// Internal Forward Declarations
void KTerm_CopyRectangle(KTerm* term, VTRectangle src, int dest_x, int dest_y);
void KTerm_ExecuteRectangularOps2(KTerm* term, KTermSession* session);
static unsigned int KTerm_CalculateRectChecksum(KTerm* term, int top, int left, int bottom, int right);
void KTerm_InitSixelGraphics(KTerm* term, KTermSession* session);
static void KTerm_ScrollUpRegion_Internal(KTerm* term, KTermSession* session, int top, int bottom, int lines);
static void KTerm_ScrollDownRegion_Internal(KTerm* term, KTermSession* session, int top, int bottom, int lines);
void ExecuteDECRQCRA(KTerm* term, KTermSession* session);
void ExecuteDECERA(KTerm* term, KTermSession* session);
void ExecuteDECFRA(KTerm* term, KTermSession* session);
void ExecuteDECRARA(KTerm* term, KTermSession* session);
void ExecuteDECECR(KTerm* term, KTermSession* session);
void ExecuteDECSASD(KTerm* term, KTermSession* session);
void ExecuteDECSN(KTerm* term, KTermSession* session);
void ExecuteDECSSDT(KTerm* term, KTermSession* session);
void ExecuteDECTST(KTerm* term, KTermSession* session);
void ExecuteDECSLE(KTerm* term, KTermSession* session);
void ExecuteDECRQLP(KTerm* term, KTermSession* session);
void ExecuteDECRQM(KTerm* term, KTermSession* session);
static void KTerm_ResizeSession(KTerm* term, int session_index, int cols, int rows);

// Safe Allocation Implementation
void* KTerm_Malloc(size_t size) {
    if (size == 0) return NULL;
    void* ptr = malloc(size);
    return ptr;
}

void* KTerm_Calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) return NULL;
    if (size > SIZE_MAX / nmemb) return NULL; // Overflow check
    void* ptr = calloc(nmemb, size);
    return ptr;
}

void* KTerm_Realloc(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    void* new_ptr = realloc(ptr, size);
    return new_ptr;
}

void KTerm_Free(void* ptr) {
    free(ptr);
}

// Fixed global variable definitions
//VTKeyboard vt_keyboard = {0};   // deprecated
// RGLTexture font_texture = {0};  // Moved to terminal struct
// Callbacks moved to struct

// Color mappings - Fixed initialization
// RGB_KTermColor color_palette[256]; // Moved to struct

KTermColor cga_colors[16] = { // Standard CGA/VGA Palette (for ANSI.SYS)
    {0x00, 0x00, 0x00, 0xFF}, // 0: Black
    {0xAA, 0x00, 0x00, 0xFF}, // 1: Red
    {0x00, 0xAA, 0x00, 0xFF}, // 2: Green
    {0xAA, 0x55, 0x00, 0xFF}, // 3: Yellow (Brown)
    {0x00, 0x00, 0xAA, 0xFF}, // 4: Blue
    {0xAA, 0x00, 0xAA, 0xFF}, // 5: Magenta
    {0x00, 0xAA, 0xAA, 0xFF}, // 6: Cyan
    {0xAA, 0xAA, 0xAA, 0xFF}, // 7: White (Light Gray)
    {0x55, 0x55, 0x55, 0xFF}, // 8: Bright Black (Dark Gray)
    {0xFF, 0x55, 0x55, 0xFF}, // 9: Bright Red
    {0x55, 0xFF, 0x55, 0xFF}, // 10: Bright Green
    {0xFF, 0xFF, 0x55, 0xFF}, // 11: Bright Yellow
    {0x55, 0x55, 0xFF, 0xFF}, // 12: Bright Blue
    {0xFF, 0x55, 0xFF, 0xFF}, // 13: Bright Magenta
    {0x55, 0xFF, 0xFF, 0xFF}, // 14: Bright Cyan
    {0xFF, 0xFF, 0xFF, 0xFF}  // 15: Bright White
};

KTermColor ansi_colors[16] = { // XTerm Palette (Standard ANSI)
    {0x00, 0x00, 0x00, 0xFF}, // 0
    {0xCD, 0x00, 0x00, 0xFF}, // 1
    {0x00, 0xCD, 0x00, 0xFF}, // 2
    {0xCD, 0xCD, 0x00, 0xFF}, // 3
    {0x00, 0x00, 0xEE, 0xFF}, // 4
    {0xCD, 0x00, 0xCD, 0xFF}, // 5
    {0x00, 0xCD, 0xCD, 0xFF}, // 6
    {0xE5, 0xE5, 0xE5, 0xFF}, // 7
    {0x7F, 0x7F, 0x7F, 0xFF}, // 8
    {0xFF, 0x00, 0x00, 0xFF}, // 9
    {0x00, 0xFF, 0x00, 0xFF}, // 10
    {0xFF, 0xFF, 0x00, 0xFF}, // 11
    {0x5C, 0x5C, 0xFF, 0xFF}, // 12
    {0xFF, 0x00, 0xFF, 0xFF}, // 13
    {0x00, 0xFF, 0xFF, 0xFF}, // 14
    {0xFF, 0xFF, 0xFF, 0xFF}, // 15
};

// Add missing function declaration
void KTerm_InitFontData(KTerm* term); // In case it's used elsewhere, though font_data is static

#include "font_data.h"

// Forward declarations of internal helpers
static void KTerm_ResizeSession_Internal(KTerm* term, KTermSession* session, int cols, int rows);
static void KTerm_ResizeSession(KTerm* term, int session_index, int cols, int rows);

static void KTerm_LayoutResizeCallback(void* user_data, int session_index, int cols, int rows) {
    KTerm* term = (KTerm*)user_data;
    KTerm_ResizeSession(term, session_index, cols, rows);
}

// =============================================================================
// SAFE PARSING PRIMITIVES
// =============================================================================
// Phase 7.1: Safe Parsing Primitives
// StreamScanner and parsing helpers moved to kt_parser.h

// Extended font data with larger character matrix for better rendering
/*unsigned char cp437_font__8x16[256 * 16 * 2] = {
    // 0x00 - NULL (empty)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};*/

// static uint32_t charset_lut[32][128]; // Moved to struct

void InitCharacterSetLUT(KTerm* term) {
    // 1. Initialize all to ASCII identity first
    for (int s = 0; s < 32; s++) {
        for (int c = 0; c < 128; c++) {
            term->charset_lut[s][c] = c;
        }
    }

    // 2. DEC Special Graphics
    for (int c = 0; c < 128; c++) {
        term->charset_lut[CHARSET_DEC_SPECIAL][c] = KTerm_TranslateDECSpecial(term, c);
    }

    // 3. National Replacement Character Sets (NRCS)
    // UK
    term->charset_lut[CHARSET_UK]['#'] = 0x00A3; // £

    // Dutch
    term->charset_lut[CHARSET_DUTCH]['#'] = 0x00A3; // £
    term->charset_lut[CHARSET_DUTCH]['@'] = 0x00BE; // ¾
    term->charset_lut[CHARSET_DUTCH]['['] = 0x0133; // ij (digraph) - approximated as ĳ
    term->charset_lut[CHARSET_DUTCH]['\\'] = 0x00BD; // ½
    term->charset_lut[CHARSET_DUTCH][']'] = 0x007C; // |
    term->charset_lut[CHARSET_DUTCH]['{'] = 0x00A8; // ¨
    term->charset_lut[CHARSET_DUTCH]['|'] = 0x0192; // f (florin)
    term->charset_lut[CHARSET_DUTCH]['}'] = 0x00BC; // ¼
    term->charset_lut[CHARSET_DUTCH]['~'] = 0x00B4; // ´

    // Finnish
    term->charset_lut[CHARSET_FINNISH]['['] = 0x00C4; // Ä
    term->charset_lut[CHARSET_FINNISH]['\\'] = 0x00D6; // Ö
    term->charset_lut[CHARSET_FINNISH][']'] = 0x00C5; // Å
    term->charset_lut[CHARSET_FINNISH]['^'] = 0x00DC; // Ü
    term->charset_lut[CHARSET_FINNISH]['`'] = 0x00E9; // é
    term->charset_lut[CHARSET_FINNISH]['{'] = 0x00E4; // ä
    term->charset_lut[CHARSET_FINNISH]['|'] = 0x00F6; // ö
    term->charset_lut[CHARSET_FINNISH]['}'] = 0x00E5; // å
    term->charset_lut[CHARSET_FINNISH]['~'] = 0x00FC; // ü

    // French
    term->charset_lut[CHARSET_FRENCH]['#'] = 0x00A3; // £
    term->charset_lut[CHARSET_FRENCH]['@'] = 0x00E0; // à
    term->charset_lut[CHARSET_FRENCH]['['] = 0x00B0; // °
    term->charset_lut[CHARSET_FRENCH]['\\'] = 0x00E7; // ç
    term->charset_lut[CHARSET_FRENCH][']'] = 0x00A7; // §
    term->charset_lut[CHARSET_FRENCH]['{'] = 0x00E9; // é
    term->charset_lut[CHARSET_FRENCH]['|'] = 0x00F9; // ù
    term->charset_lut[CHARSET_FRENCH]['}'] = 0x00E8; // è
    term->charset_lut[CHARSET_FRENCH]['~'] = 0x00A8; // ¨

    // French Canadian (Similar to French but with subtle differences in some standards, using VT standard map)
    term->charset_lut[CHARSET_FRENCH_CANADIAN]['@'] = 0x00E0; // à
    term->charset_lut[CHARSET_FRENCH_CANADIAN]['['] = 0x00E2; // â
    term->charset_lut[CHARSET_FRENCH_CANADIAN]['\\'] = 0x00E7; // ç
    term->charset_lut[CHARSET_FRENCH_CANADIAN][']'] = 0x00EA; // ê
    term->charset_lut[CHARSET_FRENCH_CANADIAN]['^'] = 0x00EE; // î
    term->charset_lut[CHARSET_FRENCH_CANADIAN]['`'] = 0x00F4; // ô
    term->charset_lut[CHARSET_FRENCH_CANADIAN]['{'] = 0x00E9; // é
    term->charset_lut[CHARSET_FRENCH_CANADIAN]['|'] = 0x00F9; // ù
    term->charset_lut[CHARSET_FRENCH_CANADIAN]['}'] = 0x00E8; // è
    term->charset_lut[CHARSET_FRENCH_CANADIAN]['~'] = 0x00FB; // û

    // German
    term->charset_lut[CHARSET_GERMAN]['@'] = 0x00A7; // §
    term->charset_lut[CHARSET_GERMAN]['['] = 0x00C4; // Ä
    term->charset_lut[CHARSET_GERMAN]['\\'] = 0x00D6; // Ö
    term->charset_lut[CHARSET_GERMAN][']'] = 0x00DC; // Ü
    term->charset_lut[CHARSET_GERMAN]['{'] = 0x00E4; // ä
    term->charset_lut[CHARSET_GERMAN]['|'] = 0x00F6; // ö
    term->charset_lut[CHARSET_GERMAN]['}'] = 0x00FC; // ü
    term->charset_lut[CHARSET_GERMAN]['~'] = 0x00DF; // ß

    // Italian
    term->charset_lut[CHARSET_ITALIAN]['#'] = 0x00A3; // £
    term->charset_lut[CHARSET_ITALIAN]['@'] = 0x00A7; // §
    term->charset_lut[CHARSET_ITALIAN]['['] = 0x00B0; // °
    term->charset_lut[CHARSET_ITALIAN]['\\'] = 0x00E7; // ç
    term->charset_lut[CHARSET_ITALIAN][']'] = 0x00E9; // é
    term->charset_lut[CHARSET_ITALIAN]['`'] = 0x00F9; // ù
    term->charset_lut[CHARSET_ITALIAN]['{'] = 0x00E0; // à
    term->charset_lut[CHARSET_ITALIAN]['|'] = 0x00F2; // ò
    term->charset_lut[CHARSET_ITALIAN]['}'] = 0x00E8; // è
    term->charset_lut[CHARSET_ITALIAN]['~'] = 0x00EC; // ì

    // Norwegian/Danish
    // Note: There are two variants (E and 6), usually identical.
    term->charset_lut[CHARSET_NORWEGIAN_DANISH]['@'] = 0x00C4; // Ä
    term->charset_lut[CHARSET_NORWEGIAN_DANISH]['['] = 0x00C6; // Æ
    term->charset_lut[CHARSET_NORWEGIAN_DANISH]['\\'] = 0x00D8; // Ø
    term->charset_lut[CHARSET_NORWEGIAN_DANISH][']'] = 0x00C5; // Å
    term->charset_lut[CHARSET_NORWEGIAN_DANISH]['^'] = 0x00DC; // Ü
    term->charset_lut[CHARSET_NORWEGIAN_DANISH]['`'] = 0x00E4; // ä
    term->charset_lut[CHARSET_NORWEGIAN_DANISH]['{'] = 0x00E6; // æ
    term->charset_lut[CHARSET_NORWEGIAN_DANISH]['|'] = 0x00F8; // ø
    term->charset_lut[CHARSET_NORWEGIAN_DANISH]['}'] = 0x00E5; // å
    term->charset_lut[CHARSET_NORWEGIAN_DANISH]['~'] = 0x00FC; // ü

    // Spanish
    term->charset_lut[CHARSET_SPANISH]['#'] = 0x00A3; // £
    term->charset_lut[CHARSET_SPANISH]['@'] = 0x00A7; // §
    term->charset_lut[CHARSET_SPANISH]['['] = 0x00A1; // ¡
    term->charset_lut[CHARSET_SPANISH]['\\'] = 0x00D1; // Ñ
    term->charset_lut[CHARSET_SPANISH][']'] = 0x00BF; // ¿
    term->charset_lut[CHARSET_SPANISH]['{'] = 0x00B0; // °
    term->charset_lut[CHARSET_SPANISH]['|'] = 0x00F1; // ñ
    term->charset_lut[CHARSET_SPANISH]['}'] = 0x00E7; // ç

    // Swedish
    term->charset_lut[CHARSET_SWEDISH]['@'] = 0x00C9; // É
    term->charset_lut[CHARSET_SWEDISH]['['] = 0x00C4; // Ä
    term->charset_lut[CHARSET_SWEDISH]['\\'] = 0x00D6; // Ö
    term->charset_lut[CHARSET_SWEDISH][']'] = 0x00C5; // Å
    term->charset_lut[CHARSET_SWEDISH]['^'] = 0x00DC; // Ü
    term->charset_lut[CHARSET_SWEDISH]['`'] = 0x00E9; // é
    term->charset_lut[CHARSET_SWEDISH]['{'] = 0x00E4; // ä
    term->charset_lut[CHARSET_SWEDISH]['|'] = 0x00F6; // ö
    term->charset_lut[CHARSET_SWEDISH]['}'] = 0x00E5; // å
    term->charset_lut[CHARSET_SWEDISH]['~'] = 0x00FC; // ü

    // Swiss
    term->charset_lut[CHARSET_SWISS]['#'] = 0x00F9; // ù
    term->charset_lut[CHARSET_SWISS]['@'] = 0x00E0; // à
    term->charset_lut[CHARSET_SWISS]['['] = 0x00E9; // é
    term->charset_lut[CHARSET_SWISS]['\\'] = 0x00E7; // ç
    term->charset_lut[CHARSET_SWISS][']'] = 0x00EA; // ê
    term->charset_lut[CHARSET_SWISS]['^'] = 0x00EE; // î
    term->charset_lut[CHARSET_SWISS]['_'] = 0x00E8; // è
    term->charset_lut[CHARSET_SWISS]['`'] = 0x00F4; // ô
    term->charset_lut[CHARSET_SWISS]['{'] = 0x00E4; // ä
    term->charset_lut[CHARSET_SWISS]['|'] = 0x00F6; // ö
    term->charset_lut[CHARSET_SWISS]['}'] = 0x00FC; // ü
    term->charset_lut[CHARSET_SWISS]['~'] = 0x00FB; // û
}

void KTerm_InitFontData(KTerm* term) {
    (void)term;
    // This function is currently empty.
    // The font_data array is initialized statically.
    // If font_data needed dynamic initialization or loading from a file,
    // it would happen here.
}

// CP437 to Unicode Mapping Table
static const uint16_t kCp437ToUnicode[256] = {
    // 0x00 - 0x1F
    0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
    0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
    0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8,
    0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC,
    // 0x20 - 0x7E (ASCII)
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x2302,
    // 0x80 - 0xFF (Extended)
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
    0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
    0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
    0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
    0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
    0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
    0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
    0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
    0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
};

static void KTerm_InitCP437Map(KTerm* term) {
    if (!term->glyph_map) return;
    for (int i = 0; i < 256; i++) {
        uint16_t u = kCp437ToUnicode[i];
        if (u != 0) {
            term->glyph_map[u] = (uint16_t)i;
        }
    }
}

// =============================================================================
// REST OF THE IMPLEMENTATION
// =============================================================================

void KTerm_InitKTermColorPalette(KTerm* term) {
    for (int i = 0; i < 16; i++) {
        term->color_palette[i] = (RGB_KTermColor){ ansi_colors[i].r, ansi_colors[i].g, ansi_colors[i].b, 255 };
    }
    int index = 16;
    for (int r = 0; r < 6; r++) {
        for (int g = 0; g < 6; g++) {
            for (int b = 0; b < 6; b++) {
                term->color_palette[index++] = (RGB_KTermColor){
                    (unsigned char)(r ? 55 + r * 40 : 0),
                    (unsigned char)(g ? 55 + g * 40 : 0),
                    (unsigned char)(b ? 55 + b * 40 : 0),
                    255
                };
            }
        }
    }
    for (int i = 0; i < 24; i++) {
        unsigned char gray = 8 + i * 10;
        term->color_palette[232 + i] = (RGB_KTermColor){gray, gray, gray, 255};
    }
}


// =============================================================================
// UNICODE WIDTH SUPPORT (wcwidth)
// =============================================================================
/*
 * This is an implementation of wcwidth() and wcswidth() (defined in
 * IEEE Std 1002.1-2001) for Unicode.
 * ... (Credits to Markus Kuhn and others)
 */

struct KTermInterval {
  int first;
  int last;
};

/* sorted list of non-overlapping intervals of non-spacing characters */
/* generated by "uniset +cat=Me +cat=Mn +cat=Cf -00AD +1160-11FF +200B c" */
// Replaced by the combining table from Vim.
static const struct KTermInterval kterm_combining_table[] = {
	{0X0300, 0X036F},
	{0X0483, 0X0489},
	{0X0591, 0X05BD},
	{0X05BF, 0X05BF},
	{0X05C1, 0X05C2},
	{0X05C4, 0X05C5},
	{0X05C7, 0X05C7},
	{0X0610, 0X061A},
	{0X064B, 0X065F},
	{0X0670, 0X0670},
	{0X06D6, 0X06DC},
	{0X06DF, 0X06E4},
	{0X06E7, 0X06E8},
	{0X06EA, 0X06ED},
	{0X0711, 0X0711},
	{0X0730, 0X074A},
	{0X07A6, 0X07B0},
	{0X07EB, 0X07F3},
	{0X07FD, 0X07FD},
	{0X0816, 0X0819},
	{0X081B, 0X0823},
	{0X0825, 0X0827},
	{0X0829, 0X082D},
	{0X0859, 0X085B},
	{0X08D3, 0X08E1},
	{0X08E3, 0X0903},
	{0X093A, 0X093C},
	{0X093E, 0X094F},
	{0X0951, 0X0957},
	{0X0962, 0X0963},
	{0X0981, 0X0983},
	{0X09BC, 0X09BC},
	{0X09BE, 0X09C4},
	{0X09C7, 0X09C8},
	{0X09CB, 0X09CD},
	{0X09D7, 0X09D7},
	{0X09E2, 0X09E3},
	{0X09FE, 0X09FE},
	{0X0A01, 0X0A03},
	{0X0A3C, 0X0A3C},
	{0X0A3E, 0X0A42},
	{0X0A47, 0X0A48},
	{0X0A4B, 0X0A4D},
	{0X0A51, 0X0A51},
	{0X0A70, 0X0A71},
	{0X0A75, 0X0A75},
	{0X0A81, 0X0A83},
	{0X0ABC, 0X0ABC},
	{0X0ABE, 0X0AC5},
	{0X0AC7, 0X0AC9},
	{0X0ACB, 0X0ACD},
	{0X0AE2, 0X0AE3},
	{0X0AFA, 0X0AFF},
	{0X0B01, 0X0B03},
	{0X0B3C, 0X0B3C},
	{0X0B3E, 0X0B44},
	{0X0B47, 0X0B48},
	{0X0B4B, 0X0B4D},
	{0X0B56, 0X0B57},
	{0X0B62, 0X0B63},
	{0X0B82, 0X0B82},
	{0X0BBE, 0X0BC2},
	{0X0BC6, 0X0BC8},
	{0X0BCA, 0X0BCD},
	{0X0BD7, 0X0BD7},
	{0X0C00, 0X0C04},
	{0X0C3E, 0X0C44},
	{0X0C46, 0X0C48},
	{0X0C4A, 0X0C4D},
	{0X0C55, 0X0C56},
	{0X0C62, 0X0C63},
	{0X0C81, 0X0C83},
	{0X0CBC, 0X0CBC},
	{0X0CBE, 0X0CC4},
	{0X0CC6, 0X0CC8},
	{0X0CCA, 0X0CCD},
	{0X0CD5, 0X0CD6},
	{0X0CE2, 0X0CE3},
	{0X0D00, 0X0D03},
	{0X0D3B, 0X0D3C},
	{0X0D3E, 0X0D44},
	{0X0D46, 0X0D48},
	{0X0D4A, 0X0D4D},
	{0X0D57, 0X0D57},
	{0X0D62, 0X0D63},
	{0X0D82, 0X0D83},
	{0X0DCA, 0X0DCA},
	{0X0DCF, 0X0DD4},
	{0X0DD6, 0X0DD6},
	{0X0DD8, 0X0DDF},
	{0X0DF2, 0X0DF3},
	{0X0E31, 0X0E31},
	{0X0E34, 0X0E3A},
	{0X0E47, 0X0E4E},
	{0X0EB1, 0X0EB1},
	{0X0EB4, 0X0EBC},
	{0X0EC8, 0X0ECD},
	{0X0F18, 0X0F19},
	{0X0F35, 0X0F35},
	{0X0F37, 0X0F37},
	{0X0F39, 0X0F39},
	{0X0F3E, 0X0F3F},
	{0X0F71, 0X0F84},
	{0X0F86, 0X0F87},
	{0X0F8D, 0X0F97},
	{0X0F99, 0X0FBC},
	{0X0FC6, 0X0FC6},
	{0X102B, 0X103E},
	{0X1056, 0X1059},
	{0X105E, 0X1060},
	{0X1062, 0X1064},
	{0X1067, 0X106D},
	{0X1071, 0X1074},
	{0X1082, 0X108D},
	{0X108F, 0X108F},
	{0X109A, 0X109D},
	{0X135D, 0X135F},
	{0X1712, 0X1714},
	{0X1732, 0X1734},
	{0X1752, 0X1753},
	{0X1772, 0X1773},
	{0X17B4, 0X17D3},
	{0X17DD, 0X17DD},
	{0X180B, 0X180D},
	{0X1885, 0X1886},
	{0X18A9, 0X18A9},
	{0X1920, 0X192B},
	{0X1930, 0X193B},
	{0X1A17, 0X1A1B},
	{0X1A55, 0X1A5E},
	{0X1A60, 0X1A7C},
	{0X1A7F, 0X1A7F},
	{0X1AB0, 0X1ABE},
	{0X1B00, 0X1B04},
	{0X1B34, 0X1B44},
	{0X1B6B, 0X1B73},
	{0X1B80, 0X1B82},
	{0X1BA1, 0X1BAD},
	{0X1BE6, 0X1BF3},
	{0X1C24, 0X1C37},
	{0X1CD0, 0X1CD2},
	{0X1CD4, 0X1CE8},
	{0X1CED, 0X1CED},
	{0X1CF4, 0X1CF4},
	{0X1CF7, 0X1CF9},
	{0X1DC0, 0X1DF9},
	{0X1DFB, 0X1DFF},
	{0X20D0, 0X20F0},
	{0X2CEF, 0X2CF1},
	{0X2D7F, 0X2D7F},
	{0X2DE0, 0X2DFF},
	{0X302A, 0X302F},
	{0X3099, 0X309A},
	{0XA66F, 0XA672},
	{0XA674, 0XA67D},
	{0XA69E, 0XA69F},
	{0XA6F0, 0XA6F1},
	{0XA802, 0XA802},
	{0XA806, 0XA806},
	{0XA80B, 0XA80B},
	{0XA823, 0XA827},
	{0XA880, 0XA881},
	{0XA8B4, 0XA8C5},
	{0XA8E0, 0XA8F1},
	{0XA8FF, 0XA8FF},
	{0XA926, 0XA92D},
	{0XA947, 0XA953},
	{0XA980, 0XA983},
	{0XA9B3, 0XA9C0},
	{0XA9E5, 0XA9E5},
	{0XAA29, 0XAA36},
	{0XAA43, 0XAA43},
	{0XAA4C, 0XAA4D},
	{0XAA7B, 0XAA7D},
	{0XAAB0, 0XAAB0},
	{0XAAB2, 0XAAB4},
	{0XAAB7, 0XAAB8},
	{0XAABE, 0XAABF},
	{0XAAC1, 0XAAC1},
	{0XAAEB, 0XAAEF},
	{0XAAF5, 0XAAF6},
	{0XABE3, 0XABEA},
	{0XABEC, 0XABED},
	{0XFB1E, 0XFB1E},
	{0XFE00, 0XFE0F},
	{0XFE20, 0XFE2F},
	{0X101FD, 0X101FD},
	{0X102E0, 0X102E0},
	{0X10376, 0X1037A},
	{0X10A01, 0X10A03},
	{0X10A05, 0X10A06},
	{0X10A0C, 0X10A0F},
	{0X10A38, 0X10A3A},
	{0X10A3F, 0X10A3F},
	{0X10AE5, 0X10AE6},
	{0X10D24, 0X10D27},
	{0X10F46, 0X10F50},
	{0X11000, 0X11002},
	{0X11038, 0X11046},
	{0X1107F, 0X11082},
	{0X110B0, 0X110BA},
	{0X11100, 0X11102},
	{0X11127, 0X11134},
	{0X11145, 0X11146},
	{0X11173, 0X11173},
	{0X11180, 0X11182},
	{0X111B3, 0X111C0},
	{0X111C9, 0X111CC},
	{0X1122C, 0X11237},
	{0X1123E, 0X1123E},
	{0X112DF, 0X112EA},
	{0X11300, 0X11303},
	{0X1133B, 0X1133C},
	{0X1133E, 0X11344},
	{0X11347, 0X11348},
	{0X1134B, 0X1134D},
	{0X11357, 0X11357},
	{0X11362, 0X11363},
	{0X11366, 0X1136C},
	{0X11370, 0X11374},
	{0X11435, 0X11446},
	{0X1145E, 0X1145E},
	{0X114B0, 0X114C3},
	{0X115AF, 0X115B5},
	{0X115B8, 0X115C0},
	{0X115DC, 0X115DD},
	{0X11630, 0X11640},
	{0X116AB, 0X116B7},
	{0X1171D, 0X1172B},
	{0X1182C, 0X1183A},
	{0X119D1, 0X119D7},
	{0X119DA, 0X119E0},
	{0X119E4, 0X119E4},
	{0X11A01, 0X11A0A},
	{0X11A33, 0X11A39},
	{0X11A3B, 0X11A3E},
	{0X11A47, 0X11A47},
	{0X11A51, 0X11A5B},
	{0X11A8A, 0X11A99},
	{0X11C2F, 0X11C36},
	{0X11C38, 0X11C3F},
	{0X11C92, 0X11CA7},
	{0X11CA9, 0X11CB6},
	{0X11D31, 0X11D36},
	{0X11D3A, 0X11D3A},
	{0X11D3C, 0X11D3D},
	{0X11D3F, 0X11D45},
	{0X11D47, 0X11D47},
	{0X11D8A, 0X11D8E},
	{0X11D90, 0X11D91},
	{0X11D93, 0X11D97},
	{0X11EF3, 0X11EF6},
	{0X16AF0, 0X16AF4},
	{0X16B30, 0X16B36},
	{0X16F4F, 0X16F4F},
	{0X16F51, 0X16F87},
	{0X16F8F, 0X16F92},
	{0X1BC9D, 0X1BC9E},
	{0X1D165, 0X1D169},
	{0X1D16D, 0X1D172},
	{0X1D17B, 0X1D182},
	{0X1D185, 0X1D18B},
	{0X1D1AA, 0X1D1AD},
	{0X1D242, 0X1D244},
	{0X1DA00, 0X1DA36},
	{0X1DA3B, 0X1DA6C},
	{0X1DA75, 0X1DA75},
	{0X1DA84, 0X1DA84},
	{0X1DA9B, 0X1DA9F},
	{0X1DAA1, 0X1DAAF},
	{0X1E000, 0X1E006},
	{0X1E008, 0X1E018},
	{0X1E01B, 0X1E021},
	{0X1E023, 0X1E024},
	{0X1E026, 0X1E02A},
	{0X1E130, 0X1E136},
	{0X1E2EC, 0X1E2EF},
	{0X1E8D0, 0X1E8D6},
	{0X1E944, 0X1E94A},
	{0XE0100, 0XE01EF}
};

/* auxiliary function for binary search in interval table */
static int KTerm_Bisearch(uint32_t ucs, const struct KTermInterval *table, int max) {
  int min = 0;
  int mid;

  if ((int)ucs < table[0].first || (int)ucs > table[max].last)
    return 0;
  while (max >= min) {
    mid = (min + max) / 2;
    if ((int)ucs > table[mid].last)
      min = mid + 1;
    else if ((int)ucs < table[mid].first)
      max = mid - 1;
    else
      return 1;
  }

  return 0;
}

static int KTerm_wcwidth(uint32_t ucs)
{
  /* test for 8-bit control characters */
  if (ucs == 0)
    return 0;
  if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0))
    return -1;

  /* binary search in table of non-spacing characters */
  if (KTerm_Bisearch(ucs, kterm_combining_table,
               sizeof(kterm_combining_table) / sizeof(struct KTermInterval) - 1))
    return 0;

  /* if we arrive here, ucs is not a combining or C0/C1 control character */

  return 1 +
    (ucs >= 0x1100 &&
     (ucs <= 0x115f ||                    /* Hangul Jamo init. consonants */
      ucs == 0x2329 || ucs == 0x232a ||
      (ucs >= 0x2e80 && ucs <= 0xa4cf &&
       ucs != 0x303f) ||                  /* CJK ... Yi */
      (ucs >= 0xac00 && ucs <= 0xd7a3) || /* Hangul Syllables */
      (ucs >= 0xf900 && ucs <= 0xfaff) || /* CJK Compatibility Ideographs */
      (ucs >= 0xfe10 && ucs <= 0xfe19) || /* Vertical forms */
      (ucs >= 0xfe30 && ucs <= 0xfe6f) || /* CJK Compatibility Forms */
      (ucs >= 0xff00 && ucs <= 0xff60) || /* Fullwidth Forms */
      (ucs >= 0xffe0 && ucs <= 0xffe6) ||
      (ucs >= 0x20000 && ucs <= 0x2fffd) ||
      (ucs >= 0x30000 && ucs <= 0x3fffd)));
}

#if 0
/* sorted list of non-overlapping intervals of East Asian Ambiguous
* characters, generated by "uniset +WIDTH-A -cat=Me -cat=Mn -cat=Cf c" */
static const struct KTermInterval kterm_ambiguous_table[] = {
{ 0x00A1, 0x00A1 }, { 0x00A4, 0x00A4 }, { 0x00A7, 0x00A8 },
{ 0x00AA, 0x00AA }, { 0x00AE, 0x00AE }, { 0x00B0, 0x00B4 },
{ 0x00B6, 0x00BA }, { 0x00BC, 0x00BF }, { 0x00C6, 0x00C6 },
{ 0x00D0, 0x00D0 }, { 0x00D7, 0x00D8 }, { 0x00DE, 0x00E1 },
{ 0x00E6, 0x00E6 }, { 0x00E8, 0x00EA }, { 0x00EC, 0x00ED },
{ 0x00F0, 0x00F0 }, { 0x00F2, 0x00F3 }, { 0x00F7, 0x00FA },
{ 0x00FC, 0x00FC }, { 0x00FE, 0x00FE }, { 0x0101, 0x0101 },
{ 0x0111, 0x0111 }, { 0x0113, 0x0113 }, { 0x011B, 0x011B },
{ 0x0126, 0x0127 }, { 0x012B, 0x012B }, { 0x0131, 0x0133 },
{ 0x0138, 0x0138 }, { 0x013F, 0x0142 }, { 0x0144, 0x0144 },
{ 0x0148, 0x014B }, { 0x014D, 0x014D }, { 0x0152, 0x0153 },
{ 0x0166, 0x0167 }, { 0x016B, 0x016B }, { 0x01CE, 0x01CE },
{ 0x01D0, 0x01D0 }, { 0x01D2, 0x01D2 }, { 0x01D4, 0x01D4 },
{ 0x01D6, 0x01D6 }, { 0x01D8, 0x01D8 }, { 0x01DA, 0x01DA },
{ 0x01DC, 0x01DC }, { 0x0251, 0x0251 }, { 0x0261, 0x0261 },
{ 0x02C4, 0x02C4 }, { 0x02C7, 0x02C7 }, { 0x02C9, 0x02CB },
{ 0x02CD, 0x02CD }, { 0x02D0, 0x02D0 }, { 0x02D8, 0x02DB },
{ 0x02DD, 0x02DD }, { 0x02DF, 0x02DF }, { 0x0391, 0x03A1 },
{ 0x03A3, 0x03A9 }, { 0x03B1, 0x03C1 }, { 0x03C3, 0x03C9 },
{ 0x0401, 0x0401 }, { 0x0410, 0x044F }, { 0x0451, 0x0451 },
{ 0x2010, 0x2010 }, { 0x2013, 0x2016 }, { 0x2018, 0x2019 },
{ 0x201C, 0x201D }, { 0x2020, 0x2022 }, { 0x2024, 0x2027 },
{ 0x2030, 0x2030 }, { 0x2032, 0x2033 }, { 0x2035, 0x2035 },
{ 0x203B, 0x203B }, { 0x203E, 0x203E }, { 0x2074, 0x2074 },
{ 0x207F, 0x207F }, { 0x2081, 0x2084 }, { 0x20AC, 0x20AC },
{ 0x2103, 0x2103 }, { 0x2105, 0x2105 }, { 0x2109, 0x2109 },
{ 0x2113, 0x2113 }, { 0x2116, 0x2116 }, { 0x2121, 0x2122 },
{ 0x2126, 0x2126 }, { 0x212B, 0x212B }, { 0x2153, 0x2154 },
{ 0x215B, 0x215E }, { 0x2160, 0x216B }, { 0x2170, 0x2179 },
{ 0x2190, 0x2199 }, { 0x21B8, 0x21B9 }, { 0x21D2, 0x21D2 },
{ 0x21D4, 0x21D4 }, { 0x21E7, 0x21E7 }, { 0x2200, 0x2200 },
{ 0x2202, 0x2203 }, { 0x2207, 0x2208 }, { 0x220B, 0x220B },
{ 0x220F, 0x220F }, { 0x2211, 0x2211 }, { 0x2215, 0x2215 },
{ 0x221A, 0x221A }, { 0x221D, 0x2220 }, { 0x2223, 0x2223 },
{ 0x2225, 0x2225 }, { 0x2227, 0x222C }, { 0x222E, 0x222E },
{ 0x2234, 0x2237 }, { 0x223C, 0x223D }, { 0x2248, 0x2248 },
{ 0x224C, 0x224C }, { 0x2252, 0x2252 }, { 0x2260, 0x2261 },
{ 0x2264, 0x2267 }, { 0x226A, 0x226B }, { 0x226E, 0x226F },
{ 0x2282, 0x2283 }, { 0x2286, 0x2287 }, { 0x2295, 0x2295 },
{ 0x2299, 0x2299 }, { 0x22A5, 0x22A5 }, { 0x22BF, 0x22BF },
{ 0x2312, 0x2312 }, { 0x2460, 0x24E9 }, { 0x24EB, 0x254B },
{ 0x2550, 0x2573 }, { 0x2580, 0x258F }, { 0x2592, 0x2595 },
{ 0x25A0, 0x25A1 }, { 0x25A3, 0x25A9 }, { 0x25B2, 0x25B3 },
{ 0x25B6, 0x25B7 }, { 0x25BC, 0x25BD }, { 0x25C0, 0x25C1 },
{ 0x25C6, 0x25C8 }, { 0x25CB, 0x25CB }, { 0x25CE, 0x25D1 },
{ 0x25E2, 0x25E5 }, { 0x25EF, 0x25EF }, { 0x2605, 0x2606 },
{ 0x2609, 0x2609 }, { 0x260E, 0x260F }, { 0x2614, 0x2615 },
{ 0x261C, 0x261C }, { 0x261E, 0x261E }, { 0x2640, 0x2640 },
{ 0x2642, 0x2642 }, { 0x2660, 0x2661 }, { 0x2663, 0x2665 },
{ 0x2667, 0x266A }, { 0x266C, 0x266D }, { 0x266F, 0x266F },
{ 0x273D, 0x273D }, { 0x2776, 0x277F }, { 0xE000, 0xF8FF },
{ 0xFFFD, 0xFFFD }, { 0xF0000, 0xFFFFD }, { 0x100000, 0x10FFFD }
};
#endif
void KTerm_InitVTConformance(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    GET_SESSION(term)->conformance.level = VT_LEVEL_XTERM;
    session->conformance.strict_mode = term->config.strict_mode;
    KTerm_SetLevel(term, session, session->conformance.level);
    session->conformance.compliance.unsupported_sequences = 0;
    session->conformance.compliance.partial_implementations = 0;
    session->conformance.compliance.extensions_used = 0;
    session->conformance.compliance.last_unsupported[0] = '\0';
}

void KTerm_InitTabStops(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    if (session->tab_stops.stops) {
        KTerm_Free(session->tab_stops.stops);
    }

    int capacity = term->width;
    if (capacity < MAX_TAB_STOPS) capacity = MAX_TAB_STOPS;

    session->tab_stops.stops = (bool*)KTerm_Calloc(capacity, sizeof(bool));
    session->tab_stops.capacity = capacity;
    session->tab_stops.count = 0;
    session->tab_stops.default_width = 8;

    for (int i = session->tab_stops.default_width; i < capacity; i += session->tab_stops.default_width) {
        session->tab_stops.stops[i] = true;
        session->tab_stops.count++;
    }
}

void KTerm_InitCharacterSets(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    session->charset.g0 = CHARSET_ASCII;
    session->charset.g1 = CHARSET_DEC_SPECIAL;
    session->charset.g2 = CHARSET_ASCII;
    session->charset.g3 = CHARSET_ASCII;
    session->charset.gl = &session->charset.g0;
    session->charset.gr = &session->charset.g1;
    session->charset.single_shift_2 = false;
    session->charset.single_shift_3 = false;
}

// Initialize Input State
void KTerm_InitInputState(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    // Initialize keyboard modes and flags
    // application_cursor_keys is in dec_modes
    session->input.keypad_application_mode = false; // Numeric keypad mode
    session->input.meta_sends_escape = true; // Alt sends ESC prefix
    session->input.delete_sends_del = true; // Delete sends DEL (\x7F)
    session->input.backarrow_sends_bs = true; // Backspace sends BS (\x08)
    session->input.keyboard_dialect = 1; // North American ASCII
    session->input.keyboard_variant = 0; // Standard
    session->input.use_software_repeat = true;

    // Kitty Keyboard Protocol Defaults
    session->input.kitty_keyboard_flags = 0;
    session->input.kitty_keyboard_stack_depth = 0;
    memset(session->input.kitty_keyboard_stack, 0, sizeof(session->input.kitty_keyboard_stack));

    // Initialize function key mappings
    const char* function_key_sequences[] = {
        "\x1BOP", "\x1BOQ", "\x1BOR", "\x1BOS", // F1–F4
        "\x1B[15~", "\x1B[17~", "\x1B[18~", "\x1B[19~", // F5–F8
        "\x1B[20~", "\x1B[21~", "\x1B[23~", "\x1B[24~", // F9–F12
        "\x1B[25~", "\x1B[26~", "\x1B[28~", "\x1B[29~", // F13–F16
        "\x1B[31~", "\x1B[32~", "\x1B[33~", "\x1B[34~", // F17–F20
        "", "", "", "" // F21–F24 (unused)
    };
    for (int i = 0; i < 24; i++) {
        strncpy(session->input.function_keys[i], function_key_sequences[i], 31);
        session->input.function_keys[i][31] = '\0';
    }
}

KTerm* KTerm_Create(KTermConfig config) {
    KTerm* term = (KTerm*)KTerm_Calloc(1, sizeof(KTerm));
    if (!term) return NULL;

    // Apply config
    term->config = config; // Store configuration

    if (config.width > 0) term->width = config.width;
    else term->width = DEFAULT_TERM_WIDTH;

    if (config.height > 0) term->height = config.height;
    else term->height = DEFAULT_TERM_HEIGHT;

    term->response_callback = config.response_callback;

    if (!KTerm_Init(term)) {
        KTerm_Cleanup(term);
        KTerm_Free(term);
        return NULL;
    }
    return term;
}

void KTerm_Destroy(KTerm* term) {
    if (!term) return;
    KTerm_Cleanup(term);
    KTerm_Free(term);
}

void KTerm_CalculateFontMetrics(const void* data, int count, int width, int height, int stride, bool is_16bit, KTermFontMetric* metrics_out) {
    if (!data || !metrics_out) return;
    if (stride == 0) stride = height;

    for (int i = 0; i < count; i++) {
        int min_x = width;
        int max_x = -1;
        const uint8_t* char_data_8 = (const uint8_t*)data + (i * stride);
        const uint16_t* char_data_16 = (const uint16_t*)data + (i * stride);

        for (int y = 0; y < height; y++) {
            uint16_t row_data = 0;
            if (is_16bit) {
                row_data = char_data_16[y];
            } else {
                row_data = char_data_8[y];
            }

            for (int x = 0; x < width; x++) {
                 bool set = false;
                 if (is_16bit) {
                     if ((row_data >> (width - 1 - x)) & 1) set = true;
                 } else {
                     if ((row_data >> (7 - x)) & 1) set = true;
                 }
                 if (set) {
                     if (x < min_x) min_x = x;
                     if (x > max_x) max_x = x;
                 }
            }
        }

        metrics_out[i].width = width;
        if (max_x == -1) {
            metrics_out[i].begin_x = 0;
            metrics_out[i].end_x = 0;
        } else {
            metrics_out[i].begin_x = min_x;
            metrics_out[i].end_x = max_x;
        }
    }
}


static void KTerm_InitReGIS(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    // Cleanup existing allocations to prevent leaks on reset
    for (int i = 0; i < 26; i++) {
        if (session->regis.macros[i]) {
            KTerm_Free(session->regis.macros[i]);
            session->regis.macros[i] = NULL;
        }
    }
    if (session->regis.macro_buffer) {
        KTerm_Free(session->regis.macro_buffer);
        session->regis.macro_buffer = NULL;
    }
    memset(&session->regis, 0, sizeof(session->regis));
    session->regis.screen_min_x = 0;
    session->regis.screen_min_y = 0;
    session->regis.screen_max_x = REGIS_WIDTH - 1;
    session->regis.screen_max_y = REGIS_HEIGHT - 1;

    // Clear Vectors (Graphics)
    // Note: Vector output is currently global (shared KTerm buffer).
    // Resetting any session's ReGIS clears the global vector screen.
    // Future work: Isolate vector output per-session or via viewports.
    term->vector_count = 0;
    term->vector_clear_request = true;
}

static void KTerm_InitTektronix(KTerm* term, KTermSession* session) {
    memset(&session->tektronix, 0, sizeof(session->tektronix));
    session->tektronix.extra_byte = -1;

    // Clear Vectors (Graphics) - Shared with ReGIS
    // Note: Vector output buffer is still global on KTerm, so clearing it affects all sessions.
    // Ideally, this should only clear if this session is active or we have per-session vector buffers.
    // For now, we keep the global clear behavior on reset.
    term->vector_count = 0;
    term->vector_clear_request = true;
}

static void KTerm_InitKitty(KTerm* term, KTermSession* session) {
    (void)term; // Unused for now, but kept for consistency/future-proofing
    // Free existing images
    if (session->kitty.images) {
        for (int k = 0; k < session->kitty.image_count; k++) {
            if (session->kitty.images[k].frames) {
                for (int f = 0; f < session->kitty.images[k].frame_count; f++) {
                    if (session->kitty.images[k].frames[f].data) {
                        KTerm_Free(session->kitty.images[k].frames[f].data);
                    }
                    if (session->kitty.images[k].frames[f].texture.id != 0) {
                        KTerm_DestroyTexture(&session->kitty.images[k].frames[f].texture);
                    }
                }
                KTerm_Free(session->kitty.images[k].frames);
            }
        }
        KTerm_Free(session->kitty.images);
        session->kitty.images = NULL;
    }
    session->kitty.image_count = 0;
    session->kitty.image_capacity = 0;
    session->kitty.current_memory_usage = 0;

    memset(&session->kitty.cmd, 0, sizeof(session->kitty.cmd));
    session->kitty.state = 0; // KEY
    session->kitty.key_len = 0;
    session->kitty.val_len = 0;
    session->kitty.b64_accumulator = 0;
    session->kitty.b64_bits = 0;
    session->kitty.active_upload = NULL;
    session->kitty.continuing = false;
    // Set defaults
    session->kitty.cmd.action = 't'; // Default action is transmit
    session->kitty.cmd.format = 32;  // Default format RGBA
    session->kitty.cmd.medium = 0;   // Default medium 0 (direct)
}

void KTerm_ResetGraphics(KTerm* term, KTermSession* session, GraphicsResetFlags flags) {
    if (!session) session = GET_SESSION(term);
    KTermSession* s = NULL;

    // If a target session is set, use it; otherwise use active/focused
    // ReGIS and Kitty are per-session. Tektronix is still global (for now).

    if (flags & GRAPHICS_RESET_KITTY && term->kitty_target_session >= 0) {
        s = &term->sessions[term->kitty_target_session];
    } else if (flags & GRAPHICS_RESET_REGIS && term->regis_target_session >= 0) {
        s = &term->sessions[term->regis_target_session];
    } else if (flags & GRAPHICS_RESET_TEK && term->tektronix_target_session >= 0) {
        s = &term->sessions[term->tektronix_target_session];
    }
    if (!s) s = session;  // Fallback

    if (flags == GRAPHICS_RESET_ALL || (flags & GRAPHICS_RESET_KITTY)) {
        KTerm_InitKitty(term, s);
    }
    if (flags == GRAPHICS_RESET_ALL || (flags & GRAPHICS_RESET_REGIS)) {
        // If regis_target_session is set, use it, otherwise use 's' (which is session or target)
        // logic above sets 's' based on flags.
        // But logic above is mutually exclusive?
        // if flags has KITTY, s is kitty target.
        // if flags has REGIS, s is regis target (elif).
        // if flags is ALL, s is session (fallback).
        // Let's rely on 's' which should be correct for the specific flag if single flag, or session if ALL.
        // However, for ALL, we want to reset ALL sessions? No, just the active one usually.
        // [v2.4.1] Updated to pass session pointer for isolation.
        KTerm_InitReGIS(term, s);
    }
    if (flags == GRAPHICS_RESET_ALL || (flags & GRAPHICS_RESET_TEK)) {
        KTerm_InitTektronix(term, s);
    }
    if (flags == GRAPHICS_RESET_ALL || (flags & GRAPHICS_RESET_SIXEL)) {
        KTermSession* sixel_s = session;
        if (term->sixel_target_session >= 0 && term->sixel_target_session < MAX_SESSIONS) {
            sixel_s = &term->sessions[term->sixel_target_session];
        }
        KTerm_InitSixelGraphics(term, sixel_s);
    }

    // Force dirty redraw
    for(int i = 0; i < s->rows; i++) {
        s->row_dirty[i] = KTERM_DIRTY_FRAMES;
    }
}

bool KTerm_Init(KTerm* term) {
    KTerm_InitFontData(term);
    KTerm_InitKTermColorPalette(term);

    // Init global members
    if (term->width == 0) term->width = DEFAULT_TERM_WIDTH;
    if (term->height == 0) term->height = DEFAULT_TERM_HEIGHT;

    // Initialize Session Locks (Phase 3) - Done once per terminal lifetime
    for (int i = 0; i < MAX_SESSIONS; i++) {
        KTERM_MUTEX_INIT(term->sessions[i].lock);
    }

    // Default Font
    term->char_width = DEFAULT_CHAR_WIDTH;
    term->char_height = DEFAULT_CHAR_HEIGHT;
    term->font_data_width = 8;
    term->font_data_height = 10;
    term->current_font_data = dec_vt220_cp437_8x10;
    term->current_font_is_16bit = false;
    KTerm_CalculateFontMetrics(term->current_font_data, 256, term->font_data_width, term->font_data_height, 0, false, term->font_metrics);
    term->active_session = 0;
    term->pending_session_switch = -1;
    term->split_screen_active = false;
    term->split_row = term->height / 2;
    term->session_top = 0;
    term->session_bottom = 1;
    term->visual_effects.curvature = 0.0f;
    term->visual_effects.scanline_intensity = 0.0f;
    term->visual_effects.glow_intensity = 0.0f;
    term->visual_effects.noise_intensity = 0.0f;
    term->visual_effects.flags = SHADER_FLAG_CRT | SHADER_FLAG_SCANLINE | SHADER_FLAG_GLOW | SHADER_FLAG_NOISE; // Enable all by default logic, controlled by intensity
    term->last_resize_time = -1.0; // Allow immediate first resize

    // Init Multiplexer
    term->mux_input.active = false;
    term->mux_input.prefix_key_code = 'B';

    // Init Gateway
    term->gateway_target_session = -1;
    term->regis_target_session = -1;
    term->tektronix_target_session = -1;
    term->kitty_target_session = -1;
    term->sixel_target_session = -1;

    // Init sessions
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!KTerm_InitSession(term, i)) return false;
        KTermSession* session = &term->sessions[i];

        // Context switch to use existing helper functions
        int saved = term->active_session;
        term->active_session = i;

        KTerm_InitVTConformance(term, session);
        KTerm_InitTabStops(term, session);
        KTerm_InitCharacterSets(term, session);
        KTerm_InitInputState(term, session);

        // Only the first session is active by default in the multiplexer
        if (i > 0) {
            term->sessions[i].session_open = false;
        }

        term->active_session = saved;
    }
    term->active_session = 0;

    // Initialize Layout Tree
    term->layout = KTermLayout_Create(term->width, term->height);
    if (!term->layout) return false;

    InitCharacterSetLUT(term);

    // Allocate full Unicode map
    if (term->glyph_map) KTerm_Free(term->glyph_map);
    term->glyph_map = (uint16_t*)KTerm_Calloc(0x110000, sizeof(uint16_t));
    if (!term->glyph_map) return false;

    // Initialize Dynamic Atlas dimensions before creation
    term->atlas_width = 1024;
    term->atlas_height = 1024;
    term->atlas_cols = 128;

    // Allocate LRU Cache
    size_t capacity = (term->atlas_width / DEFAULT_CHAR_WIDTH) * (term->atlas_height / DEFAULT_CHAR_HEIGHT);
    term->glyph_last_used = (uint64_t*)KTerm_Calloc(capacity, sizeof(uint64_t));
    if (!term->glyph_last_used) return false;
    term->atlas_to_codepoint = (uint32_t*)KTerm_Calloc(capacity, sizeof(uint32_t));
    if (!term->atlas_to_codepoint) return false;
    term->frame_count = 0;

    KTerm_InitCP437Map(term);

    KTerm_CreateFontTexture(term);

    // Initialize Compositor
    if (!KTermCompositor_Init(&term->compositor, term->width, term->height)) return false;

    KTerm_InitCompute(term);

    // Initialize KTerm Lock (Phase 3)
    KTERM_MUTEX_INIT(term->lock);
    term->main_thread_id = KTERM_THREAD_CURRENT();

    return true;
}

#ifdef KTERM_ENABLE_MT_ASSERTS
static void _KTermAssertMainThread(KTerm* term, const char* file, int line) {
    if (!KTERM_THREAD_EQUAL(KTERM_THREAD_CURRENT(), term->main_thread_id)) {
        fprintf(stderr, "KTerm Assertion Failed: Not on Main Thread at %s:%d\n", file, line);
        // abort(); // Optional: Crash on failure
    }
}
#endif



void KTerm_DispatchSequence(KTerm* term, KTermSession* session, VTParseState type) {
    // Ensure buffer is null-terminated
    if (session->escape_pos < MAX_COMMAND_BUFFER) {
        session->escape_buffer[session->escape_pos] = '\0';
    } else {
        session->escape_buffer[MAX_COMMAND_BUFFER - 1] = '\0';
    }

    switch (type) {
        case PARSE_OSC: KTerm_ExecuteOSCCommand(term, session); break;
        case PARSE_DCS: KTerm_ExecuteDCSCommand(term, session); break;
        case PARSE_APC: KTerm_ExecuteAPCCommand(term, session); break;
        case PARSE_PM:  KTerm_ExecutePMCommand(term, session); break;
        case PARSE_SOS: KTerm_ExecuteSOSCommand(term, session); break;
        case PARSE_KITTY:
            {
                KTermSession* target = session;
                if (term->kitty_target_session >= 0 && term->kitty_target_session < MAX_SESSIONS) {
                    target = &term->sessions[term->kitty_target_session];
                }
                KTerm_ExecuteKittyCommand(term, target);
            }
            break;
        default: break;
    }
}

// String terminator handler for ESC P, ESC _, ESC ^, ESC X
void KTerm_ProcessStringTerminator(KTerm* term, KTermSession* session, unsigned char ch) {
    // Expects ST (ESC \) to terminate.
    // If ch is '\', we terminate and execute.
    // If ch is NOT '\', we terminate (implicitly), execute, and then process ch as start of new sequence.

    bool is_st = (ch == '\\');

    // Execute the pending command based on saved state
    KTerm_DispatchSequence(term, session, session->saved_parse_state);

    // Reset buffer
    session->escape_pos = 0;

    if (is_st) {
        session->parse_state = VT_PARSE_NORMAL;
    } else {
        // Not a valid ST, but ESC was seen previously.
        // The previous string is terminated.
        // We now process 'ch' as the character following ESC.
        session->parse_state = VT_PARSE_ESCAPE;
        KTerm_ProcessEscapeChar(term, session, ch);
    }
}

void KTerm_ProcessCharsetCommand(KTerm* term, KTermSession* session, unsigned char ch) {
    if (session->escape_pos < MAX_COMMAND_BUFFER) {
        session->escape_buffer[session->escape_pos++] = ch;
    }

    // Intermediate bytes (0x20-0x2F) mean keep collecting
    if (ch >= 0x20 && ch <= 0x2F) return;

    if (session->escape_pos >= 2) {
        char designator = session->escape_buffer[0];

        // Construct full designator string (excluding the initial '(' etc.)
        char dscs[8] = {0};
        int len = session->escape_pos - 1;
        if (len > 4) len = 4; // Cap length
        memcpy(dscs, &session->escape_buffer[1], len);

        CharacterSet selected_cs = CHARSET_ASCII;

        // Check for Soft Font match
        if (session->soft_font.active && strncmp(dscs, session->soft_font.name, 4) == 0) {
            selected_cs = CHARSET_DRCS;
        } else {
            // Standard parsing
            char charset_char = session->escape_buffer[session->escape_pos - 1]; // Use final char

            switch (charset_char) {
                case 'A': selected_cs = CHARSET_UK; break;
                case 'B': selected_cs = CHARSET_ASCII; break;
                case '0': selected_cs = CHARSET_DEC_SPECIAL; break;
                case '1':
                case '2':
                    if (session->options.debug_sequences) {
                        KTerm_LogUnsupportedSequence(term, "DEC Alternate Character ROM not fully supported, using ASCII/DEC Special");
                    }
                    selected_cs = (charset_char == '1') ? CHARSET_ASCII : CHARSET_DEC_SPECIAL;
                    break;
                case '<': selected_cs = CHARSET_DEC_MULTINATIONAL; break;
                // NRCS Designators
                case '4': selected_cs = CHARSET_DUTCH; break;
                case 'C': case '5': selected_cs = CHARSET_FINNISH; break;
                case 'R': case 'f': selected_cs = CHARSET_FRENCH; break;
                case 'Q': selected_cs = CHARSET_FRENCH_CANADIAN; break;
                case 'K': selected_cs = CHARSET_GERMAN; break;
                case 'Y': selected_cs = CHARSET_ITALIAN; break;
                case 'E': case '6': selected_cs = CHARSET_NORWEGIAN_DANISH; break;
                case 'Z': selected_cs = CHARSET_SPANISH; break;
                case 'H': case '7': selected_cs = CHARSET_SWEDISH; break;
                case '=': selected_cs = CHARSET_SWISS; break;
                default:
                    // If intermediates were present but no soft font match, it's likely an unknown set
                    if (session->options.debug_sequences) {
                        char debug_msg[64];
                        snprintf(debug_msg, sizeof(debug_msg), "Unknown charset: %s for %c", dscs, designator);
                        KTerm_LogUnsupportedSequence(term, debug_msg);
                    }
                    break;
            }
        }

        switch (designator) {
            case '(': session->charset.g0 = selected_cs; break;
            case ')': session->charset.g1 = selected_cs; break;
            case '*': session->charset.g2 = selected_cs; break;
            case '+': session->charset.g3 = selected_cs; break;
        }

        session->parse_state = VT_PARSE_NORMAL;
        session->escape_pos = 0;
    }
}

// Stubs for APC/PM/SOS command execution
void KTerm_ExecuteAPCCommand(KTerm* term, KTermSession* session) {
    if (session->options.debug_sequences) KTerm_LogUnsupportedSequence(term, "APC sequence executed (no-op)");
    // session->escape_buffer contains the APC string data.
}
void KTerm_ExecutePMCommand(KTerm* term, KTermSession* session) {
    if (session->options.debug_sequences) KTerm_LogUnsupportedSequence(term, "PM sequence executed (no-op)");
    // session->escape_buffer contains the PM string data.
}
void KTerm_ExecuteSOSCommand(KTerm* term, KTermSession* session) {
    if (session->options.debug_sequences) KTerm_LogUnsupportedSequence(term, "SOS sequence executed (no-op)");
    // session->escape_buffer contains the SOS string data.
}

// Generic string processor for APC, PM, SOS
void KTerm_ProcessGenericStringChar(KTerm* term, KTermSession* session, unsigned char ch, VTParseState next_state_on_escape) {
    (void)next_state_on_escape;
    if ((size_t)session->escape_pos < sizeof(session->escape_buffer) - 1) {
        if (ch == '\x1B') {
            session->saved_parse_state = session->parse_state;
            session->parse_state = PARSE_STRING_TERMINATOR;
            return;
        }

        session->escape_buffer[session->escape_pos++] = ch;

        // BEL is not a standard terminator for these, ST is.
    } else { // Buffer overflow
        KTerm_DispatchSequence(term, session, session->parse_state);
        session->parse_state = VT_PARSE_NORMAL;
        session->escape_pos = 0;
        char log_msg[64];
        snprintf(log_msg, sizeof(log_msg), "String sequence (type %d) too long, truncated", (int)session->parse_state); // Log current state
        KTerm_LogUnsupportedSequence(term, log_msg);
    }
}

void KTerm_ProcessAPCChar(KTerm* term, KTermSession* session, unsigned char ch) {
    // Detect Kitty Graphics Protocol start
    if (session->escape_pos == 0 && ch == 'G') {
         // Determine Target
         KTermSession* target_session = session;
         if (term->kitty_target_session >= 0 && term->kitty_target_session < MAX_SESSIONS) {
             target_session = &term->sessions[term->kitty_target_session];
         }

         session->parse_state = PARSE_KITTY;
         // Initialize Kitty Parser for new command
         memset(&target_session->kitty.cmd, 0, sizeof(target_session->kitty.cmd));
         target_session->kitty.state = 0; // KEY
         target_session->kitty.key_len = 0;
         target_session->kitty.val_len = 0;
         target_session->kitty.b64_accumulator = 0;
         target_session->kitty.b64_bits = 0;

         // Only reset active_upload if NOT continuing a chunked transmission
         if (!target_session->kitty.continuing) {
             target_session->kitty.active_upload = NULL;
         }

         // Set defaults
         target_session->kitty.cmd.action = 't'; // Default action is transmit
         target_session->kitty.cmd.format = 32;  // Default format RGBA
         target_session->kitty.cmd.medium = 0;   // Default medium 0 (direct)
         return;
    }
    KTerm_ProcessGenericStringChar(term, session, ch, VT_PARSE_ESCAPE /* Fallback if ST is broken */);
}
void KTerm_ProcessPMChar(KTerm* term, KTermSession* session, unsigned char ch) { KTerm_ProcessGenericStringChar(term, session, ch, VT_PARSE_ESCAPE); }
void KTerm_ProcessSOSChar(KTerm* term, KTermSession* session, unsigned char ch) { KTerm_ProcessGenericStringChar(term, session, ch, VT_PARSE_ESCAPE); }

// Internal helper forward declaration
static void ProcessTektronixChar(KTerm* term, KTermSession* session, unsigned char ch);
static void ProcessReGISChar(KTerm* term, KTermSession* session, unsigned char ch);

// Process character when in Printer Controller Mode (pass-through)
void KTerm_ProcessPrinterControllerChar(KTerm* term, KTermSession* session, unsigned char ch) {
    // We must detect the exit sequence: CSI 4 i
    // CSI can be 7-bit (\x1B [) or 8-bit (\x9B)
    // Exit sequence:
    // 7-bit: ESC [ 4 i
    // 8-bit: CSI 4 i (\x9B 4 i)

    // Ensure session-specific buffering
    // Add to buffer
    if (session->printer_buf_len < 7) {
        session->printer_buffer[session->printer_buf_len++] = ch;
    } else {
        // Buffer full, flush oldest and shift
        if (term->printer_callback) {
            term->printer_callback(term, (const char*)&session->printer_buffer[0], 1);
        }
        memmove(session->printer_buffer, session->printer_buffer + 1, --session->printer_buf_len);
        session->printer_buffer[session->printer_buf_len++] = ch;
    }
    session->printer_buffer[session->printer_buf_len] = '\0';

    // Target Sequences
    const char* seq1 = "\x1B[4i"; // 7-bit CSI 4 i
    const char* seq2 = "\x9B""4i"; // 8-bit CSI 4 i

    // Sliding window check: Is the buffer a valid prefix or full match?
    // We check if the buffer *ends* with a valid prefix, but since we append one char at a time
    // and flush mismatches, we mainly check if the *start* matches.
    // However, we might have flushed part of a sequence if we aren't careful.
    // Actually, simple state buffering is:
    // 1. Append ch.
    // 2. Check if buffer matches or is prefix of targets.
    // 3. If exact match: Disable mode, Clear buffer.
    // 4. If prefix match: Return (keep buffering).
    // 5. If mismatch: Flush head until we have a prefix match or empty.

    while (session->printer_buf_len > 0) {
        // Check 7-bit
        bool match1 = true;
        for (int i = 0; i < session->printer_buf_len; i++) {
            if (i >= 4 || session->printer_buffer[i] != (unsigned char)seq1[i]) {
                match1 = false;
                break;
            }
        }
        if (match1 && session->printer_buf_len == 4) {
            session->printer_controller_enabled = false;
            session->printer_buf_len = 0;
            return;
        }

        // Check 8-bit
        bool match2 = true;
        for (int i = 0; i < session->printer_buf_len; i++) {
            if (i >= 3 || session->printer_buffer[i] != (unsigned char)seq2[i]) {
                match2 = false;
                break;
            }
        }
        if (match2 && session->printer_buf_len == 3) {
            session->printer_controller_enabled = false;
            session->printer_buf_len = 0;
            return;
        }

        if (match1 || match2) {
            // It's a valid prefix, wait for more data
            return;
        }

        // Mismatch: Flush the first character and retry
        if (term->printer_callback) {
            term->printer_callback(term, (const char*)&session->printer_buffer[0], 1);
        }
        memmove(session->printer_buffer, session->printer_buffer + 1, --session->printer_buf_len);
    }
}

void ExecuteDECSNLS(KTerm* term, KTermSession* session) {
    // CSI Ps * | - Set Number of Lines per Screen
    if (!session) session = GET_SESSION(term);
    int lines = KTerm_GetCSIParam(term, session, 0, 24);
    if (lines < 1) lines = 24;

    // Resize session visible height (rows)
    // Maintain current columns
    // Note: KTerm_ResizeSession_Internal expects lock to be held.
    // This is called from ProcessCSIChar which is called from ProcessEventsInternal which holds lock.
    KTerm_QueueResize(session, session->cols, lines, true);
}

void ExecuteDECRQPKU(KTerm* term, KTermSession* session) {
    // CSI ? 26 ; Ps u - Request Programmed Key
    if (!session) session = GET_SESSION(term);

    // Verify first param is 26 (part of the command signature)
    int p1 = KTerm_GetCSIParam(term, session, 0, 0);
    if (p1 != 26) return;

    // DECRQUPSS (CSI ? 26 u) has only one parameter (26).
    // DECRQPKU (CSI ? 26 ; Ps u) has two parameters.
    // KTerm_GetCSIParam returns default_value if the parameter is missing.
    // If we ask for param index 1, and it's missing (0), it's DECRQUPSS.
    // Note: The caller must ensure p1 == 26, which effectively distinguishes this
    // from other private 'u' sequences.

    int key_num = KTerm_GetCSIParam(term, session, 1, 0);

    // If key_num is 0 (missing), treat as DECRQUPSS (CSI ? 26 u)
    if (key_num == 0) {
        // DECRQUPSS - Report User Preferred Supplemental Set
        // Response: DCS Ps $ r ST (Ps = preferred set code)
        char response[64];
        snprintf(response, sizeof(response), "\x1BP%d$r\x1B\\", session->preferred_supplemental);
        KTerm_QueueResponse(term, response);
        return;
    }

    // Map VT key number to Situation Key Code to search the programmable_keys list.
    int sit_key = 0;
    // Map based on VT520 manual or standard usage:
    // F6=17, F7=18, F8=19, F9=20, F10=21
    // F11=23, F12=24, F13=25, F14=26
    // F15=28, F16=29
    // F17=31, F18=32, F19=33, F20=34

    switch(key_num) {
        case 17: sit_key = KTERM_KEY_F6; break;
        case 18: sit_key = KTERM_KEY_F7; break;
        case 19: sit_key = KTERM_KEY_F8; break;
        case 20: sit_key = KTERM_KEY_F9; break;
        case 21: sit_key = KTERM_KEY_F10; break;
        case 23: sit_key = KTERM_KEY_F11; break;
        case 24: sit_key = KTERM_KEY_F12; break;
        case 25: sit_key = KTERM_KEY_F13; break;
        case 26: sit_key = KTERM_KEY_F14; break;
        case 28: sit_key = KTERM_KEY_F15; break;
        case 29: sit_key = KTERM_KEY_F16; break;
        case 31: sit_key = KTERM_KEY_F17; break;
        case 32: sit_key = KTERM_KEY_F18; break;
        case 33: sit_key = KTERM_KEY_F19; break;
        case 34: sit_key = KTERM_KEY_F20; break;
        default: sit_key = 0; break;
    }

    char* seq = NULL;
    if (sit_key != 0) {
        for(size_t i=0; i<session->programmable_keys.count; i++) {
             if (session->programmable_keys.keys[i].key_code == sit_key && session->programmable_keys.keys[i].active) {
                 seq = session->programmable_keys.keys[i].sequence;
                 break;
             }
        }
    }

    // Response
    char response[1024]; // Safe buffer size
    snprintf(response, sizeof(response), "\x1BP%d;1;%s\x1B\\", key_num, seq ? seq : ""); // Pc=1 (Unshifted)
    KTerm_QueueResponse(term, response);
}

void ExecuteDECRQTSR(KTerm* term, KTermSession* session) {
    // CSI ? Ps $ u - Request Terminal State Report
    // Ps=1 (All), Ps=53 (Factory Defaults / DECRQDE)
    if (!session) session = GET_SESSION(term);
    int req = KTerm_GetCSIParam(term, session, 0, 1);

    // Build DCS Response: DCS Ps $ r ... ST
    // Example format: DCS 1 $ r <ModeBits>;<Columns>;<Rows>;<PageLength>;<ScrollTop>;<ScrollBot>;... ST

    char buffer[1024];
    int pos = 0;

    // Header
    // If DECRQDE (53), we report as Type 1 (All) but with default values, or as user requested DCS 1 $ r.
    // DECRQTSR (1) reports as Type 1.
    int report_type = (req == 53) ? 1 : req;
    int written = snprintf(buffer + pos, sizeof(buffer) - pos, "\x1BP%d$r", report_type);
    if (written > 0 && (size_t)written < sizeof(buffer) - pos) pos += written;

    if (req == 53) {
        // DECRQDE - Factory Defaults
        // Report hardcoded defaults
        // Modes (approx default): DECAWM, DECTCEM, DECBKM, DECECR
        uint32_t def_modes = KTERM_MODE_DECAWM | KTERM_MODE_DECTCEM | KTERM_MODE_DECBKM | KTERM_MODE_DECECR;
        written = snprintf(buffer + pos, sizeof(buffer) - pos, "%u;%d;%d;%d;%d;%d",
            def_modes, DEFAULT_TERM_WIDTH, DEFAULT_TERM_HEIGHT, 24, 1, 24);
        if (written > 0 && (size_t)written < sizeof(buffer) - pos) pos += written;
    } else {
        // DECRQTSR - Current State
        written = snprintf(buffer + pos, sizeof(buffer) - pos, "%u;%d;%d;%d;%d;%d",
            session->dec_modes, session->cols, session->rows, session->lines_per_page,
            session->scroll_top + 1, session->scroll_bottom + 1);
        if (written > 0 && (size_t)written < sizeof(buffer) - pos) pos += written;

        // Append margins if relevant
        if (session->dec_modes & KTERM_MODE_DECLRMM) {
            written = snprintf(buffer + pos, sizeof(buffer) - pos, ";%d;%d",
                session->left_margin + 1, session->right_margin + 1);
            if (written > 0 && (size_t)written < sizeof(buffer) - pos) pos += written;
        }
    }

    // Terminator
    if (pos < sizeof(buffer) - 2) {
        buffer[pos++] = '\x1B';
        buffer[pos++] = '\\';
        buffer[pos] = '\0';
    }

    KTerm_QueueResponse(term, buffer);
}

void ExecuteDECARR(KTerm* term, KTermSession* session) {
    // CSI Ps SP r - Set Auto Repeat Rate
    if (!session) session = GET_SESSION(term);
    int rate = KTerm_GetCSIParam(term, session, 0, 0); // Default 0? Or 30?
    if (rate < 0) rate = 0;
    if (rate > 31) rate = 31;
    session->auto_repeat_rate = rate;
}

void ExecuteDECSKCV(KTerm* term, KTermSession* session) {
    // CSI Ps SP = - Select Keyboard Variant
    if (!session) session = GET_SESSION(term);
    int variant = KTerm_GetCSIParam(term, session, 0, 0);
    session->input.keyboard_variant = variant;
}

void ExecuteDECSLPP(KTerm* term, KTermSession* session) {
    // CSI Ps * { - Set Lines Per Page
    if (!session) session = GET_SESSION(term);
    int lines = KTerm_GetCSIParam(term, session, 0, 24);
    if (lines < 1) lines = 24;
    session->lines_per_page = lines;
    KTerm_QueueResize(session, session->cols, lines, true);
}

void ExecuteDECSCPP(KTerm* term, KTermSession* session) {
    // CSI Pn $ |
    // Select 80 or 132 Columns per Page (DECSCPP)
    // Pn = 0 or 80 -> 80 columns
    // Pn = 132 -> 132 columns
    if (!session) session = GET_SESSION(term);

    // Check if switching is allowed (Mode 40)
    if (!(session->dec_modes & KTERM_MODE_ALLOW_80_132)) {
        return;
    }

    int cols = KTerm_GetCSIParam(term, session, 0, 80);
    if (cols == 0) cols = 80;

    if (cols == 80 || cols == 132) {
        // 1. Update DECCOLM mode bit
        if (cols == 132) {
             session->dec_modes |= KTERM_MODE_DECCOLM;
        } else {
             session->dec_modes &= ~KTERM_MODE_DECCOLM;
        }

        // 2. Resize
        // We use the internal resize function which does not take the lock (caller holds it)
        // We need the session index.
//         int session_idx = (int)(session - term->sessions);
        KTerm_QueueResize(session, cols, session->rows, true);

        // 3. Side effects (Clear screen, Home cursor, Reset margins)
        // Only if DECNCSM (Mode 95) is NOT set (i.e. default behavior is to clear)
        if (!(session->dec_modes & KTERM_MODE_DECNCSM)) {
             // Reset scrolling region
             session->scroll_top = 0;
             session->scroll_bottom = session->rows - 1;
             session->left_margin = 0;
             session->right_margin = cols - 1;

             // Home cursor
             session->cursor.x = 0;
             session->cursor.y = 0;

             // Clear screen buffer
             EnhancedTermChar default_char = {
                .ch = ' ',
                .fg_color = session->current_fg,
                .bg_color = session->current_bg,
                .flags = KTERM_FLAG_DIRTY
            };

             for(int i=0; i < session->buffer_height * session->cols; i++) {
                 session->screen_buffer[i] = default_char;
             }
             // Mark all rows dirty
             for(int r=0; r<session->rows; r++) session->row_dirty[r] = KTERM_DIRTY_FRAMES;
        }
    }
}

// Continue with enhanced character processing...
void KTerm_ProcessChar(KTerm* term, KTermSession* session, unsigned char ch) {
    if (session->printer_controller_enabled) {
        KTerm_ProcessPrinterControllerChar(term, session, ch);
        return;
    }

    switch (session->parse_state) {
        case VT_PARSE_NORMAL:              KTerm_ProcessNormalChar(term, session, ch); break;
        case VT_PARSE_ESCAPE:              KTerm_ProcessEscapeChar(term, session, ch); break;
        case PARSE_CSI:                 KTerm_ProcessCSIChar(term, session, ch); break;
        case PARSE_OSC:                 KTerm_ProcessOSCChar(term, session, ch); break;
        case PARSE_DCS:                 KTerm_ProcessDCSChar(term, session, ch); break;
        case PARSE_SIXEL_ST:            KTerm_ProcessSixelSTChar(term, session, ch); break;
        case PARSE_VT52:                KTerm_ProcessVT52Char(term, session, ch); break;
        case PARSE_TEKTRONIX:
            {
                KTermSession* target = session;
                if (term->tektronix_target_session != -1 && term->tektronix_target_session >= 0 && term->tektronix_target_session < MAX_SESSIONS) {
                    target = &term->sessions[term->tektronix_target_session];
                }
                ProcessTektronixChar(term, target, ch);
            }
            break;
        case PARSE_REGIS:
            {
                KTermSession* target = session;
                if (term->regis_target_session != -1 && term->regis_target_session >= 0 && term->regis_target_session < MAX_SESSIONS) {
                    target = &term->sessions[term->regis_target_session];
                }
                ProcessReGISChar(term, target, ch);
            }
            break;
        case PARSE_KITTY:
            {
                KTermSession* target = session;
                if (term->kitty_target_session != -1 && term->kitty_target_session >= 0 && term->kitty_target_session < MAX_SESSIONS) {
                    target = &term->sessions[term->kitty_target_session];
                }
                KTerm_ProcessKittyChar(term, target, ch);
            }
            break;
        case PARSE_SIXEL:               KTerm_ProcessSixelChar(term, session, ch); break;
        case PARSE_CHARSET:             KTerm_ProcessCharsetCommand(term, session, ch); break;
        case PARSE_HASH:                KTerm_ProcessHashChar(term, session, ch); break;
        case PARSE_PERCENT:             KTerm_ProcessPercentChar(term, session, ch); break;
        case PARSE_APC:                 KTerm_ProcessAPCChar(term, session, ch); break;
        case PARSE_PM:                  KTerm_ProcessPMChar(term, session, ch); break;
        case PARSE_SOS:                 KTerm_ProcessSOSChar(term, session, ch); break;
        case PARSE_STRING_TERMINATOR:   KTerm_ProcessStringTerminator(term, session, ch); break;
        case PARSE_nF:                  KTerm_ProcessnFChar(term, session, ch); break;
        default:
            session->parse_state = VT_PARSE_NORMAL;
            KTerm_ProcessNormalChar(term, session, ch);
            break;
    }
}

static void KTerm_ApplyAttributeToCell(KTerm* term, EnhancedTermChar* cell, int param, int* i_ptr, bool reverse) {
    if (!term || !cell || !i_ptr) return;

    bool ansi_restricted = (GET_SESSION(term)->conformance.level == VT_LEVEL_ANSI_SYS);

    switch (param) {
        case 0: // Reset
            if (reverse) {
                cell->flags = 0;
            } else {
                cell->flags = 0;
                cell->fg_color.color_mode = 0; cell->fg_color.value.index = 7; // Default FG (White)
                cell->bg_color.color_mode = 0; cell->bg_color.value.index = 0; // Default BG (Black)
                cell->ul_color.color_mode = 2; // Default
                cell->st_color.color_mode = 2; // Default
            }
            break;

        case 1: // Bold
            if (reverse) cell->flags ^= KTERM_ATTR_BOLD;
            else cell->flags |= KTERM_ATTR_BOLD;
            break;
        case 2: // Faint
            if (reverse) cell->flags ^= KTERM_ATTR_FAINT;
            else if (!ansi_restricted) cell->flags |= KTERM_ATTR_FAINT;
            break;
        case 22: // Normal intensity
            if (reverse) cell->flags ^= (KTERM_ATTR_BOLD | KTERM_ATTR_FAINT | KTERM_ATTR_FAINT_BG);
            else cell->flags &= ~(KTERM_ATTR_BOLD | KTERM_ATTR_FAINT | KTERM_ATTR_FAINT_BG);
            break;

        case 3: // Italic
            if (reverse) cell->flags ^= KTERM_ATTR_ITALIC;
            else if (!ansi_restricted) cell->flags |= KTERM_ATTR_ITALIC;
            break;
        case 23:
            if (reverse) cell->flags ^= KTERM_ATTR_ITALIC;
            else if (!ansi_restricted) cell->flags &= ~KTERM_ATTR_ITALIC;
            break;

        case 4: // Underline
            {
                int idx = *i_ptr;
                if (idx < GET_SESSION(term)->param_count && GET_SESSION(term)->escape_separators[idx] == ':') {
                     // Subparams exist. Consume them.
                     if (idx + 1 < GET_SESSION(term)->param_count) {
                         int style = GET_SESSION(term)->escape_params[idx+1];
                         (*i_ptr)++; // Consume

                         if (!reverse) {
                             cell->flags &= ~(KTERM_ATTR_UNDERLINE | KTERM_ATTR_DOUBLE_UNDERLINE | KTERM_ATTR_UL_STYLE_MASK);
                             switch (style) {
                                 case 1: cell->flags |= KTERM_ATTR_UNDERLINE | KTERM_ATTR_UL_STYLE_SINGLE; break;
                                 case 2: cell->flags |= KTERM_ATTR_DOUBLE_UNDERLINE | KTERM_ATTR_UL_STYLE_DOUBLE; break;
                                 case 3: cell->flags |= KTERM_ATTR_UNDERLINE | KTERM_ATTR_UL_STYLE_CURLY; break;
                                 case 4: cell->flags |= KTERM_ATTR_UNDERLINE | KTERM_ATTR_UL_STYLE_DOTTED; break;
                                 case 5: cell->flags |= KTERM_ATTR_UNDERLINE | KTERM_ATTR_UL_STYLE_DASHED; break;
                                 default: cell->flags |= KTERM_ATTR_UNDERLINE | KTERM_ATTR_UL_STYLE_SINGLE; break;
                             }
                         }
                     } else {
                         if (!reverse) cell->flags |= KTERM_ATTR_UNDERLINE;
                     }
                } else {
                    if (reverse) cell->flags ^= KTERM_ATTR_UNDERLINE;
                    else {
                        cell->flags &= ~KTERM_ATTR_UL_STYLE_MASK;
                        cell->flags |= KTERM_ATTR_UNDERLINE | KTERM_ATTR_UL_STYLE_SINGLE;
                    }
                }
            }
            break;
        case 21: // Double Underline
             if (reverse) cell->flags ^= (KTERM_ATTR_DOUBLE_UNDERLINE | KTERM_ATTR_UL_STYLE_DOUBLE);
             else if (!ansi_restricted) cell->flags |= (KTERM_ATTR_DOUBLE_UNDERLINE | KTERM_ATTR_UL_STYLE_DOUBLE);
             break;
        case 24:
             if (reverse) cell->flags ^= (KTERM_ATTR_UNDERLINE | KTERM_ATTR_DOUBLE_UNDERLINE);
             else cell->flags &= ~(KTERM_ATTR_UNDERLINE | KTERM_ATTR_DOUBLE_UNDERLINE | KTERM_ATTR_UL_STYLE_MASK);
             break;

        case 5: // Slow Blink
            if (reverse) cell->flags ^= KTERM_ATTR_BLINK_SLOW;
            else { cell->flags |= KTERM_ATTR_BLINK_SLOW; cell->flags &= ~KTERM_ATTR_BLINK; }
            break;
        case 6: // Rapid Blink
            if (reverse) cell->flags ^= KTERM_ATTR_BLINK;
            else if (!ansi_restricted) { cell->flags |= KTERM_ATTR_BLINK; cell->flags &= ~KTERM_ATTR_BLINK_SLOW; }
            break;
        case 25: // Blink off
             if (reverse) cell->flags ^= (KTERM_ATTR_BLINK | KTERM_ATTR_BLINK_SLOW | KTERM_ATTR_BLINK_BG);
             else cell->flags &= ~(KTERM_ATTR_BLINK | KTERM_ATTR_BLINK_SLOW | KTERM_ATTR_BLINK_BG);
             break;

        case 7: // Reverse
            if (reverse) cell->flags ^= KTERM_ATTR_REVERSE;
            else cell->flags |= KTERM_ATTR_REVERSE;
            break;
        case 27:
            if (reverse) cell->flags ^= KTERM_ATTR_REVERSE;
            else cell->flags &= ~KTERM_ATTR_REVERSE;
            break;

        case 8: // Conceal
            if (reverse) cell->flags ^= KTERM_ATTR_CONCEAL;
            else cell->flags |= KTERM_ATTR_CONCEAL;
            break;
        case 28:
            if (reverse) cell->flags ^= KTERM_ATTR_CONCEAL;
            else cell->flags &= ~KTERM_ATTR_CONCEAL;
            break;

        case 9: // Strike
            if (reverse) cell->flags ^= KTERM_ATTR_STRIKE;
            else if (!ansi_restricted) cell->flags |= KTERM_ATTR_STRIKE;
            break;
        case 29:
            if (reverse) cell->flags ^= KTERM_ATTR_STRIKE;
            else if (!ansi_restricted) cell->flags &= ~KTERM_ATTR_STRIKE;
            break;

        case 53: // Overline
            if (reverse) cell->flags ^= KTERM_ATTR_OVERLINE;
            else if (!ansi_restricted) cell->flags |= KTERM_ATTR_OVERLINE;
            break;
        case 55:
            if (reverse) cell->flags ^= KTERM_ATTR_OVERLINE;
            else if (!ansi_restricted) cell->flags &= ~KTERM_ATTR_OVERLINE;
            break;

        case 51: // Framed
            if (reverse) cell->flags ^= KTERM_ATTR_FRAMED;
            else if (!ansi_restricted) cell->flags |= KTERM_ATTR_FRAMED;
            break;

        case 52: // Encircled
            if (reverse) cell->flags ^= KTERM_ATTR_ENCIRCLED;
            else if (!ansi_restricted) cell->flags |= KTERM_ATTR_ENCIRCLED;
            break;

        case 54: // Not Framed or Encircled
            if (reverse) cell->flags ^= (KTERM_ATTR_FRAMED | KTERM_ATTR_ENCIRCLED);
            else if (!ansi_restricted) cell->flags &= ~(KTERM_ATTR_FRAMED | KTERM_ATTR_ENCIRCLED);
            break;

        case 73: // Superscript
            if (reverse) {
                cell->flags ^= KTERM_ATTR_SUPERSCRIPT;
            } else if (!ansi_restricted) {
                cell->flags |= KTERM_ATTR_SUPERSCRIPT;
                cell->flags &= ~KTERM_ATTR_SUBSCRIPT; // Mutual exclusion
            }
            break;

        case 74: // Subscript
            if (reverse) {
                cell->flags ^= KTERM_ATTR_SUBSCRIPT;
            } else if (!ansi_restricted) {
                cell->flags |= KTERM_ATTR_SUBSCRIPT;
                cell->flags &= ~KTERM_ATTR_SUPERSCRIPT; // Mutual exclusion
            }
            break;

        case 75: // Not Superscript or Subscript
            if (reverse) cell->flags ^= (KTERM_ATTR_SUPERSCRIPT | KTERM_ATTR_SUBSCRIPT);
            else if (!ansi_restricted) cell->flags &= ~(KTERM_ATTR_SUPERSCRIPT | KTERM_ATTR_SUBSCRIPT);
            break;

        // Standard colors
        case 30: case 31: case 32: case 33:
        case 34: case 35: case 36: case 37:
            if (!reverse) {
                cell->fg_color.color_mode = 0;
                cell->fg_color.value.index = param - 30;
            }
            break;

        case 40: case 41: case 42: case 43:
        case 44: case 45: case 46: case 47:
            if (!reverse) {
                cell->bg_color.color_mode = 0;
                cell->bg_color.value.index = param - 40;
            }
            break;

        case 90: case 91: case 92: case 93:
        case 94: case 95: case 96: case 97:
            if (!reverse && !ansi_restricted) {
                cell->fg_color.color_mode = 0;
                cell->fg_color.value.index = param - 90 + 8;
            }
            break;

        case 100: case 101: case 102: case 103:
        case 104: case 105: case 106: case 107:
            if (!reverse && !ansi_restricted) {
                cell->bg_color.color_mode = 0;
                cell->bg_color.value.index = param - 100 + 8;
            }
            break;

        case 38: // Extended FG
             if (!ansi_restricted) {
                 if (reverse) {
                     ExtendedKTermColor dummy;
                     *i_ptr += ProcessExtendedKTermColor(term, &dummy, *i_ptr);
                 } else {
                     *i_ptr += ProcessExtendedKTermColor(term, &cell->fg_color, *i_ptr);
                 }
             }
             break;

        case 48: // Extended BG
             if (!ansi_restricted) {
                 if (reverse) {
                     ExtendedKTermColor dummy;
                     *i_ptr += ProcessExtendedKTermColor(term, &dummy, *i_ptr);
                 } else {
                     *i_ptr += ProcessExtendedKTermColor(term, &cell->bg_color, *i_ptr);
                 }
             }
             break;

        case 58: // UL Color
             if (!ansi_restricted) {
                 if (reverse) {
                     ExtendedKTermColor dummy;
                     *i_ptr += ProcessExtendedKTermColor(term, &dummy, *i_ptr);
                 } else {
                     *i_ptr += ProcessExtendedKTermColor(term, &cell->ul_color, *i_ptr);
                 }
             }
             break;

        case 39: // Default FG
            if (!reverse) {
                cell->fg_color.color_mode = 0;
                cell->fg_color.value.index = COLOR_WHITE;
            }
            break;

        case 49: // Default BG
            if (!reverse) {
                cell->bg_color.color_mode = 0;
                cell->bg_color.value.index = COLOR_BLACK;
            }
            break;

        case 59: // Default UL Color
            if (!reverse && !ansi_restricted) {
                cell->ul_color.color_mode = 2;
            }
            break;
    }
    cell->flags |= KTERM_FLAG_DIRTY;
}

void ExecuteDECCARA(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    if (!(session->conformance.features & KTERM_FEATURE_RECT_OPERATIONS)) {
        KTerm_LogUnsupportedSequence(term, "DECCARA requires rectangular operations support");
        return;
    }
    // CSI Pt ; Pl ; Pb ; Pr ; Ps $ t
    int top = KTerm_GetCSIParam(term, session, 0, 1) - 1;
    int left = KTerm_GetCSIParam(term, session, 1, 1) - 1;
    int bottom = KTerm_GetCSIParam(term, session, 2, term->height) - 1;
    int right = KTerm_GetCSIParam(term, session, 3, term->width) - 1;

    if (top < 0) top = 0;
    if (left < 0) left = 0;
    if (bottom >= term->height) bottom = term->height - 1;
    if (right >= term->width) right = term->width - 1;
    if (top > bottom || left > right) return;

    if (session->param_count <= 4) return; // No attributes

    uint32_t attr_mask = 0;
    uint32_t attr_values = 0;
    bool set_fg = false;
    ExtendedKTermColor fg = {0};
    bool set_bg = false;
    ExtendedKTermColor bg = {0};

    for (int i = 4; i < session->param_count; i++) {
        int param = session->escape_params[i];

        switch (param) {
            case 0:
                attr_mask |= (KTERM_ATTR_BOLD | KTERM_ATTR_UNDERLINE | KTERM_ATTR_BLINK | KTERM_ATTR_REVERSE | KTERM_ATTR_ITALIC | KTERM_ATTR_STRIKE);
                attr_values &= ~(KTERM_ATTR_BOLD | KTERM_ATTR_UNDERLINE | KTERM_ATTR_BLINK | KTERM_ATTR_REVERSE | KTERM_ATTR_ITALIC | KTERM_ATTR_STRIKE);
                set_fg = true; fg.color_mode = 0; fg.value.index = 7;
                set_bg = true; bg.color_mode = 0; bg.value.index = 0;
                break;
            case 1: attr_mask |= KTERM_ATTR_BOLD; attr_values |= KTERM_ATTR_BOLD; break;
            case 3: attr_mask |= KTERM_ATTR_ITALIC; attr_values |= KTERM_ATTR_ITALIC; break;
            case 4: attr_mask |= KTERM_ATTR_UNDERLINE; attr_values |= KTERM_ATTR_UNDERLINE; break;
            case 5: attr_mask |= KTERM_ATTR_BLINK; attr_values |= KTERM_ATTR_BLINK; break;
            case 7: attr_mask |= KTERM_ATTR_REVERSE; attr_values |= KTERM_ATTR_REVERSE; break;
            case 9: attr_mask |= KTERM_ATTR_STRIKE; attr_values |= KTERM_ATTR_STRIKE; break;
            case 22: attr_mask |= KTERM_ATTR_BOLD; attr_values &= ~KTERM_ATTR_BOLD; break;
            case 23: attr_mask |= KTERM_ATTR_ITALIC; attr_values &= ~KTERM_ATTR_ITALIC; break;
            case 24: attr_mask |= KTERM_ATTR_UNDERLINE; attr_values &= ~KTERM_ATTR_UNDERLINE; break;
            case 25: attr_mask |= KTERM_ATTR_BLINK; attr_values &= ~KTERM_ATTR_BLINK; break;
            case 27: attr_mask |= KTERM_ATTR_REVERSE; attr_values &= ~KTERM_ATTR_REVERSE; break;
            case 29: attr_mask |= KTERM_ATTR_STRIKE; attr_values &= ~KTERM_ATTR_STRIKE; break;
            case 30 ... 37:
                set_fg = true; fg.color_mode = 0; fg.value.index = param - 30; break;
            case 40 ... 47:
                set_bg = true; bg.color_mode = 0; bg.value.index = param - 40; break;
            case 90 ... 97:
                set_fg = true; fg.color_mode = 0; fg.value.index = param - 90 + 8; break;
            case 100 ... 107:
                set_bg = true; bg.color_mode = 0; bg.value.index = param - 100 + 8; break;
            case 38:
                i = ProcessExtendedKTermColor(term, &fg, i);
                set_fg = true;
                break;
            case 48:
                i = ProcessExtendedKTermColor(term, &bg, i);
                set_bg = true;
                break;
        }
    }
    KTermRect rect = {left, top, right - left + 1, bottom - top + 1};
    KTerm_QueueSetAttrRect(session, rect, attr_mask, attr_values, 0, set_fg, fg, set_bg, bg);
        return;

}
void ExecuteDECRARA(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    if (!(session->conformance.features & KTERM_FEATURE_RECT_OPERATIONS)) {
        KTerm_LogUnsupportedSequence(term, "DECRARA requires rectangular operations support");
        return;
    }
    // CSI Pt ; Pl ; Pb ; Pr ; Ps $ u
    int top = KTerm_GetCSIParam(term, session, 0, 1) - 1;
    int left = KTerm_GetCSIParam(term, session, 1, 1) - 1;
    int bottom = KTerm_GetCSIParam(term, session, 2, term->height) - 1;
    int right = KTerm_GetCSIParam(term, session, 3, term->width) - 1;

    if (top < 0) top = 0;
    if (left < 0) left = 0;
    if (bottom >= term->height) bottom = term->height - 1;
    if (right >= term->width) right = term->width - 1;
    if (top > bottom || left > right) return;

    if (session->param_count <= 4) return; // No attributes

    
        uint32_t attr_xor_mask = 0;

        for (int i = 4; i < session->param_count; i++) {
            int param = session->escape_params[i];
            switch (param) {
                case 1: attr_xor_mask |= KTERM_ATTR_BOLD; break;
                case 3: attr_xor_mask |= KTERM_ATTR_ITALIC; break;
                case 4: attr_xor_mask |= KTERM_ATTR_UNDERLINE; break;
                case 5: attr_xor_mask |= KTERM_ATTR_BLINK; break;
                case 7: attr_xor_mask |= KTERM_ATTR_REVERSE; break;
                case 9: attr_xor_mask |= KTERM_ATTR_STRIKE; break;
                // Colors are ignored in DECRARA standard behavior
            }
        }
        KTermRect rect = {left, top, right - left + 1, bottom - top + 1};
        ExtendedKTermColor empty_color = {0};
        KTerm_QueueSetAttrRect(session, rect, 0, 0, attr_xor_mask, false, empty_color, false, empty_color);
        return;
}


static unsigned int KTerm_CalculateRectChecksum(KTerm* term, int top, int left, int bottom, int right) {
    unsigned int checksum = 0;
    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            EnhancedTermChar* cell = GetActiveScreenCell(GET_SESSION(term), y, x);
            if (cell) {
                checksum += cell->ch;
            }
        }
    }
    return checksum;
}

void ExecuteDECECR(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Enable Checksum Reporting
    // CSI Pt ; Pc z (Enable/Disable Checksum Reporting)
    // Pt = Request ID (ignored/stored)
    // Pc = 0 (Disable), 1 (Enable)
    int pc = KTerm_GetCSIParam(term, session, 1, 0); // Default to 0 (Disable)

    if (pc == 1) {
        session->dec_modes |= KTERM_MODE_DECECR;
    } else {
        session->dec_modes &= ~KTERM_MODE_DECECR;
    }
}

void ExecuteDECRQCRA(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Request Rectangular Area Checksum
    if (!(session->conformance.features & KTERM_FEATURE_RECT_OPERATIONS)) {
        KTerm_LogUnsupportedSequence(term, "DECRQCRA requires rectangular operations support");
        return;
    }

    if (!(session->dec_modes & KTERM_MODE_DECECR)) {
        return; // Checksum reporting disabled
    }

    // CSI Pid ; Pp ; Pt ; Pl ; Pb ; Pr $ w
    int pid = KTerm_GetCSIParam(term, session, 0, 1);
    // int page = KTerm_GetCSIParam(term, session, 1, 1); // Ignored
    int top = KTerm_GetCSIParam(term, session, 2, 1) - 1;
    int left = KTerm_GetCSIParam(term, session, 3, 1) - 1;
    int bottom = KTerm_GetCSIParam(term, session, 4, term->height) - 1;
    int right = KTerm_GetCSIParam(term, session, 5, term->width) - 1;

    if (top < 0) top = 0;
    if (left < 0) left = 0;
    if (bottom >= term->height) bottom = term->height - 1;
    if (right >= term->width) right = term->width - 1;

    unsigned int checksum = 0;
    if (top <= bottom && left <= right) {
        checksum = KTerm_CalculateRectChecksum(term, top, left, bottom, right);
    }

    // Response: DCS Pid ! ~ Checksum ST
    char response[32];
    snprintf(response, sizeof(response), "\x1BP%d!~%04X\x1B\\", pid, checksum & 0xFFFF);
    KTerm_QueueResponse(term, response);
}

// CSI Pch ; Pt ; Pl ; Pb ; Pr $ x
void ExecuteDECFRA(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Fill Rectangular Area
    if (!(session->conformance.features & KTERM_FEATURE_RECT_OPERATIONS)) {
        KTerm_LogUnsupportedSequence(term, "DECFRA requires rectangular operations support");
        return;
    }

    if (session->param_count != 5) {
        KTerm_LogUnsupportedSequence(term, "Invalid parameters for DECFRA");
        return;
    }

    int char_code = KTerm_GetCSIParam(term, session, 0, ' ');
    int top = KTerm_GetCSIParam(term, session, 1, 1) - 1;
    int left = KTerm_GetCSIParam(term, session, 2, 1) - 1;
    int bottom = KTerm_GetCSIParam(term, session, 3, 1) - 1;
    int right = KTerm_GetCSIParam(term, session, 4, 1) - 1;

    if (top < 0) top = 0;
    if (left < 0) left = 0;
    if (bottom >= term->height) bottom = term->height - 1;
    if (right >= term->width) right = term->width - 1;
    if (top > bottom || left > right) return;

    unsigned int fill_char = (unsigned int)char_code;

    EnhancedTermChar fill_cell = {0};
    fill_cell.ch = fill_char;
    fill_cell.fg_color = session->current_fg;
    fill_cell.bg_color = session->current_bg;
    fill_cell.flags = session->current_attributes;

    KTermRect rect = {left, top, right - left + 1, bottom - top + 1};
    KTerm_QueueFillRect(session, rect, fill_cell);
}

// CSI ? Psl {
void ExecuteDECSLE(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Select Locator Events
    if (!(session->conformance.features & KTERM_FEATURE_LOCATOR)) {
        KTerm_LogUnsupportedSequence(term, "DECSLE requires locator support");
        return;
    }

    // Ensure session-specific locator events are updated
    if (session->param_count == 0) {
        // No parameters, defaults to 0
        session->locator_events.report_on_request_only = true;
        session->locator_events.report_button_down = false;
        session->locator_events.report_button_up = false;
        return;
    }

    for (int i = 0; i < session->param_count; i++) {
        switch (session->escape_params[i]) {
            case 0:
                session->locator_events.report_on_request_only = true;
                session->locator_events.report_button_down = false;
                session->locator_events.report_button_up = false;
                break;
            case 1:
                session->locator_events.report_button_down = true;
                session->locator_events.report_on_request_only = false;
                break;
            case 2:
                session->locator_events.report_button_down = false;
                break;
            case 3:
                session->locator_events.report_button_up = true;
                session->locator_events.report_on_request_only = false;
                break;
            case 4:
                session->locator_events.report_button_up = false;
                break;
            default:
                if (session->options.debug_sequences) {
                    char debug_msg[64];
                    snprintf(debug_msg, sizeof(debug_msg), "Unknown DECSLE parameter: %d", session->escape_params[i]);
                    KTerm_LogUnsupportedSequence(term, debug_msg);
                }
                break;
        }
    }
}

void ExecuteDECSASD(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    // CSI Ps $ }
    // Select Active Status Display
    // 0 = Main Display, 1 = Status Line
    int mode = KTerm_GetCSIParam(term, session, 0, 0);
    if (mode == 0 || mode == 1) {
        session->active_display = mode;
    }
}

void ExecuteDECSSDT(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    // CSI Ps $ ~
    // Select Split Definition Type
    // 0 = No Split, 1 = Horizontal Split
    if (!(session->conformance.features & KTERM_FEATURE_MULTI_SESSION_MODE)) {
        KTerm_LogUnsupportedSequence(term, "DECSSDT requires multi-session support");
        return;
    }

    int mode = KTerm_GetCSIParam(term, session, 0, 0);
    if (mode == 0) {
        KTerm_SetSplitScreen(term, false, 0, 0, 0);
    } else if (mode == 1) {
        // Default split: Center, Session 0 Top, Session 1 Bottom
        // Future: Support parameterized split points
        KTerm_SetSplitScreen(term, true, term->height / 2, 0, 1);
    } else {
        if (session->options.debug_sequences) {
            char msg[64];
            snprintf(msg, sizeof(msg), "DECSSDT mode %d not supported", mode);
            KTerm_LogUnsupportedSequence(term, msg);
        }
    }
}

// CSI Plc |
void ExecuteDECRQLP(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Request Locator Position
    if (!(session->conformance.features & KTERM_FEATURE_LOCATOR)) {
        KTerm_LogUnsupportedSequence(term, "DECRQLP requires locator support");
        return;
    }

    char response[64]; // Increased buffer size for longer response

    if (!session->locator_enabled || session->mouse.cursor_x < 1 || session->mouse.cursor_y < 1) {
        // Locator not enabled or position unknown, respond with "no locator"
        snprintf(response, sizeof(response), "\x1B[0!|");
    } else {
        // Locator enabled and position is known, report position.
        // Format: CSI Pe;Pr;Pc;Pp!|
        // Pe = 1 (request response)
        // Pr = row
        // Pc = column
        // Pp = page (hardcoded to 1)
        int row = session->mouse.cursor_y;
        int col = session->mouse.cursor_x;

        if (term->split_screen_active && term->active_session == term->session_bottom) {
            row -= (term->split_row + 1);
        }

        int page = 1; // Page memory not implemented, so hardcode to 1.
        snprintf(response, sizeof(response), "\x1B[1;%d;%d;%d!|", row, col, page);
    }

    KTerm_QueueResponse(term, response);
}


// CSI Pt ; Pl ; Pb ; Pr $ x
void ExecuteDECERA(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Erase Rectangular Area
    if (!(session->conformance.features & KTERM_FEATURE_RECT_OPERATIONS)) {
        KTerm_LogUnsupportedSequence(term, "DECERA requires rectangular operations support");
        return;
    }
    if (session->param_count != 4) {
        KTerm_LogUnsupportedSequence(term, "Invalid parameters for DECERA");
        return;
    }
    int top = KTerm_GetCSIParam(term, session, 0, 1) - 1;
    int left = KTerm_GetCSIParam(term, session, 1, 1) - 1;
    int bottom = KTerm_GetCSIParam(term, session, 2, 1) - 1;
    int right = KTerm_GetCSIParam(term, session, 3, 1) - 1;

    if (top < 0) top = 0;
    if (left < 0) left = 0;
    if (bottom >= term->height) bottom = term->height - 1;
    if (right >= term->width) right = term->width - 1;
    if (top > bottom || left > right) return;

    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            KTerm_ClearCell(term, GetActiveScreenCell(session, y, x));
        }
        session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }
}


// CSI Ps ; Pt ; Pl ; Pb ; Pr $ {
void ExecuteDECSERA(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Selective Erase Rectangular Area
    if (!(session->conformance.features & KTERM_FEATURE_RECT_OPERATIONS)) {
        KTerm_LogUnsupportedSequence(term, "DECSERA requires rectangular operations support");
        return;
    }
    if (session->param_count < 4 || session->param_count > 5) {
        KTerm_LogUnsupportedSequence(term, "Invalid parameters for DECSERA");
        return;
    }
    int erase_param, top, left, bottom, right;

    if (session->param_count == 5) {
        erase_param = KTerm_GetCSIParam(term, session, 0, 0);
        top = KTerm_GetCSIParam(term, session, 1, 1) - 1;
        left = KTerm_GetCSIParam(term, session, 2, 1) - 1;
        bottom = KTerm_GetCSIParam(term, session, 3, 1) - 1;
        right = KTerm_GetCSIParam(term, session, 4, 1) - 1;
    } else { // param_count == 4
        erase_param = 0; // Default when Ps is omitted
        top = KTerm_GetCSIParam(term, session, 0, 1) - 1;
        left = KTerm_GetCSIParam(term, session, 1, 1) - 1;
        bottom = KTerm_GetCSIParam(term, session, 2, 1) - 1;
        right = KTerm_GetCSIParam(term, session, 3, 1) - 1;
    }

    if (top < 0) top = 0;
    if (left < 0) left = 0;
    if (bottom >= term->height) bottom = term->height - 1;
    if (right >= term->width) right = term->width - 1;
    if (top > bottom || left > right) return;

    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            bool should_erase = false;
            EnhancedTermChar* cell = GetActiveScreenCell(session, y, x);
            switch (erase_param) {
                case 0: if (!(cell->flags & KTERM_ATTR_PROTECTED)) should_erase = true; break;
                case 1: should_erase = true; break;
                case 2: if (cell->flags & KTERM_ATTR_PROTECTED) should_erase = true; break;
            }
            if (should_erase) {
                KTerm_ClearCell(term, cell);
            }
        }
        session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }
}

void KTerm_ProcessOSCChar(KTerm* term, KTermSession* session, unsigned char ch) {
    // Phase 7.2: Harden Escape Buffers (Bounds Check)
    if ((size_t)session->escape_pos < sizeof(session->escape_buffer) - 1) {
        if (ch == '\x1B') {
            session->saved_parse_state = PARSE_OSC;
            session->parse_state = PARSE_STRING_TERMINATOR;
            return;
        }

        session->escape_buffer[session->escape_pos++] = ch;

        if (ch == '\a') {
            session->escape_pos--; // exclude BEL
            KTerm_DispatchSequence(term, session, PARSE_OSC);
            session->parse_state = VT_PARSE_NORMAL;
            session->escape_pos = 0;
        }
    } else {
        KTerm_DispatchSequence(term, session, PARSE_OSC);
        session->parse_state = VT_PARSE_NORMAL;
        session->escape_pos = 0;
        KTerm_LogUnsupportedSequence(term, "OSC sequence too long, truncated");
    }
}

void KTerm_ProcessDCSChar(KTerm* term, KTermSession* session, unsigned char ch) {
    // Phase 7.2: Harden Escape Buffers (Bounds Check)
    if ((size_t)session->escape_pos < sizeof(session->escape_buffer) - 1) {
        if (ch == '\x1B') {
            session->saved_parse_state = PARSE_DCS;
            session->parse_state = PARSE_STRING_TERMINATOR;
            return;
        }

        session->escape_buffer[session->escape_pos++] = ch;

        // Ensure this is not DECRQSS ($q)
        bool is_decrqss = (session->escape_pos >= 2 && session->escape_buffer[session->escape_pos - 2] == '$');

        if (ch == 'q' && (session->conformance.features & KTERM_FEATURE_SIXEL_GRAPHICS) && !is_decrqss) {
            // Sixel Graphics command
            // Determine Target Session
            KTermSession* target_session = session;
            if (term->sixel_target_session >= 0 && term->sixel_target_session < MAX_SESSIONS) {
                target_session = &term->sessions[term->sixel_target_session];
            }

            KTerm_ParseCSIParams(term, session->escape_buffer, target_session->sixel.params, MAX_ESCAPE_PARAMS);
            target_session->sixel.param_count = session->param_count;

            target_session->sixel.pos_x = 0;
            target_session->sixel.pos_y = 0;
            target_session->sixel.max_x = 0;
            target_session->sixel.max_y = 0;
            target_session->sixel.color_index = 0;
            target_session->sixel.repeat_count = 0;

            // Parse P2 for background transparency (0=Device Default, 1=Transparent, 2=Opaque)
            int p2 = (session->param_count >= 2) ? target_session->sixel.params[1] : 0;
            target_session->sixel.transparent_bg = (p2 == 1);

            if (!target_session->sixel.data) {
                target_session->sixel.width = term->width * term->char_width;
                target_session->sixel.height = term->height * term->char_height;
                target_session->sixel.data = KTerm_Calloc(target_session->sixel.width * target_session->sixel.height * 4, 1);
            }

            if (target_session->sixel.data) {
                memset(target_session->sixel.data, 0, target_session->sixel.width * target_session->sixel.height * 4);
            }

            if (!target_session->sixel.strips) {
                target_session->sixel.strip_capacity = 65536;
                target_session->sixel.strips = (GPUSixelStrip*)KTerm_Calloc(target_session->sixel.strip_capacity, sizeof(GPUSixelStrip));
            }
            target_session->sixel.strip_count = 0;

            target_session->sixel.active = true;
            target_session->sixel.scrolling = true; // Default Sixel behavior scrolls
            target_session->sixel.logical_start_row = target_session->screen_head;
            target_session->sixel.x = target_session->cursor.x * term->char_width;
            target_session->sixel.y = target_session->cursor.y * term->char_height;

            session->parse_state = PARSE_SIXEL;
            session->escape_pos = 0;
            return;
        }

        if (ch == 'p' && (session->conformance.features & KTERM_FEATURE_REGIS_GRAPHICS)) {
            // ReGIS (Remote Graphics Instruction Set)
            // Initialize ReGIS state
            session->regis.state = 0; // Expecting command
            session->regis.command = 0;
            session->regis.x = 0;
            session->regis.y = 0;
            session->regis.color = 0xFFFFFFFF; // White
            session->regis.write_mode = 0; // Default to Overlay/Additive
            session->regis.param_count = 0;
            session->regis.has_comma = false;
            session->regis.has_bracket = false;

            session->parse_state = PARSE_REGIS;
            session->escape_pos = 0;
            return;
        }

        if (ch == '\a') { // Non-standard, but some terminals accept BEL for DCS
            session->escape_pos--; // exclude BEL
            KTerm_DispatchSequence(term, session, PARSE_DCS);
            session->parse_state = VT_PARSE_NORMAL;
            session->escape_pos = 0;
        }
    } else { // Buffer overflow
        KTerm_DispatchSequence(term, session, PARSE_DCS);
        session->parse_state = VT_PARSE_NORMAL;
        session->escape_pos = 0;
        KTerm_LogUnsupportedSequence(term, "DCS sequence too long, truncated");
    }
}

// =============================================================================
// ENHANCED FONT SYSTEM WITH UNICODE SUPPORT
// =============================================================================

void KTerm_CreateFontTexture(KTerm* term) {
    if (term->font_texture.generation != 0) {
        KTerm_DestroyTexture(&term->font_texture);
    }

    const int num_chars_base = 256;

    // Allocate persistent CPU buffer if not present
    if (!term->font_atlas_pixels) {
        term->font_atlas_pixels = KTerm_Calloc(term->atlas_width * term->atlas_height * 4, 1);
        if (!term->font_atlas_pixels) return;
        term->next_atlas_index = 256; // Start dynamic allocation after base set
    }
    // Clear buffer for new layout
    memset(term->font_atlas_pixels, 0, term->atlas_width * term->atlas_height * 4);

    unsigned char* pixels = term->font_atlas_pixels;

    int char_w = term->char_width;
    int char_h = term->char_height;
    if (GET_SESSION(term)->soft_font.active) {
        char_w = GET_SESSION(term)->soft_font.char_width;
        char_h = GET_SESSION(term)->soft_font.char_height;
    }
    int dynamic_chars_per_row = term->atlas_width / char_w;

    // Calculate Centering Offsets
    int pad_x = (char_w - term->font_data_width) / 2;
    int pad_y = (char_h - term->font_data_height) / 2;

    // Unpack the font data (Base 256 chars)
    for (int i = 0; i < num_chars_base; i++) {
        int glyph_col = i % dynamic_chars_per_row;
        int glyph_row = i / dynamic_chars_per_row;
        int dest_x_start = glyph_col * char_w;
        int dest_y_start = glyph_row * char_h;

        for (int y = 0; y < char_h; y++) {
            uint16_t row_data = 0;
            bool in_glyph_y = (y >= pad_y && y < (pad_y + term->font_data_height));

            if (in_glyph_y) {
                int src_y = y - pad_y;
                if (GET_SESSION(term)->soft_font.active && GET_SESSION(term)->soft_font.loaded[i]) {
                    row_data = GET_SESSION(term)->soft_font.font_data[i][src_y]; // Soft fonts limited to 8-bit width for now
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

                bool in_glyph_x = (x >= pad_x && x < (pad_x + term->font_data_width));

                if (in_glyph_y && in_glyph_x) {
                    int src_x = x - pad_x;
                    // Shift: For 8-bit, 7-src_x. For W-bit, (W - 1 - src_x).
                    pixel_on = (row_data >> (term->font_data_width - 1 - src_x)) & 1;
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

    // Create GPU Texture
    KTermImage img = {0};
    img.width = term->atlas_width;
    img.height = term->atlas_height;
    img.channels = 4;
    img.data = pixels;

    if (term->font_texture.generation != 0) KTerm_DestroyTexture(&term->font_texture);
    KTerm_CreateTexture(img, false, &term->font_texture);
    // Don't unload image data as it points to persistent buffer
}

void KTerm_InitCompute(KTerm* term) {
    if (term->compute_initialized) return;

    // 1. Create SSBO
    size_t buffer_size = term->width * term->height * sizeof(GPUCell);
    KTerm_CreateBuffer(buffer_size, NULL, KTERM_BUFFER_USAGE_STORAGE_BUFFER | KTERM_BUFFER_USAGE_TRANSFER_DST, &term->terminal_buffer);
    if (term->terminal_buffer.id == 0) {
        KTerm_ReportError(term, KTERM_LOG_FATAL, KTERM_SOURCE_RENDER, "Failed to create terminal GPU buffer");
    }

    // 2. Create Storage Image (Output)
    KTermImage empty_img = {0};
    // Use current dimensions
    int win_width = term->width * term->char_width * DEFAULT_WINDOW_SCALE;
    int win_height = term->height * term->char_height * DEFAULT_WINDOW_SCALE;

    if (KTerm_CreateImage(win_width, win_height, 4, &empty_img) != KTERM_SUCCESS) { // RGBA
        KTerm_ReportError(term, KTERM_LOG_FATAL, KTERM_SOURCE_RENDER, "Failed to create terminal output image in memory");
    }
    // We can init to black if we want, but compute will overwrite.
    KTerm_CreateTextureEx(empty_img, false, KTERM_TEXTURE_USAGE_SAMPLED | KTERM_TEXTURE_USAGE_STORAGE | KTERM_TEXTURE_USAGE_TRANSFER_SRC, &term->output_texture);
    if (term->output_texture.id == 0) {
        KTerm_ReportError(term, KTERM_LOG_FATAL, KTERM_SOURCE_RENDER, "Failed to create terminal output texture");
    }
    KTerm_UnloadImage(empty_img);

    // 3. Create Compute Pipeline
    {
        unsigned char* shader_body = NULL;
        unsigned int bytes_read = 0;
        if (KTerm_LoadFileData(KTERM_TERMINAL_SHADER_PATH, &bytes_read, &shader_body) == KTERM_SUCCESS && shader_body) {
            size_t l1 = strlen(terminal_compute_preamble);
            char* src = (char*)KTerm_Malloc(l1 + bytes_read + 1);
            if (src) {
                strcpy(src, terminal_compute_preamble);
                memcpy(src + l1, shader_body, bytes_read);
                src[l1 + bytes_read] = '\0';
                KTerm_CreateComputePipeline(src, KTERM_COMPUTE_LAYOUT_TERMINAL, &term->compute_pipeline);
                if (term->compute_pipeline.id == 0) {
                    KTerm_ReportError(term, KTERM_LOG_FATAL, KTERM_SOURCE_RENDER, "Failed to compile/create terminal compute pipeline");
                }
                KTerm_Free(src);
            } else {
                KTerm_ReportError(term, KTERM_LOG_FATAL, KTERM_SOURCE_SYSTEM, "Failed to allocate memory for shader source");
            }
            KTerm_Free(shader_body);
        } else {
             KTerm_ReportError(term, KTERM_LOG_FATAL, KTERM_SOURCE_SYSTEM, "Failed to load terminal shader file: %s", KTERM_TERMINAL_SHADER_PATH);
        }
    }

    // Create Dummy Sixel Texture (1x1 transparent)
    KTermImage dummy_img = {0};
    if (KTerm_CreateImage(1, 1, 4, &dummy_img) == KTERM_SUCCESS) {
        memset(dummy_img.data, 0, 4); // Clear to transparent
        KTerm_CreateTextureEx(dummy_img, false, KTERM_TEXTURE_USAGE_SAMPLED, &term->dummy_sixel_texture);
        KTerm_UnloadImage(dummy_img);
    }

    // Create Clear Texture (1x1 opaque black)
    KTermImage clear_img = {0};
    if (KTerm_CreateImage(1, 1, 4, &clear_img) == KTERM_SUCCESS) {
        clear_img.data[0] = 0; clear_img.data[1] = 0; clear_img.data[2] = 0; clear_img.data[3] = 255;
        KTerm_CreateTextureEx(clear_img, false, KTERM_TEXTURE_USAGE_SAMPLED, &term->clear_texture);
        KTerm_UnloadImage(clear_img);
    }

    // term->gpu_staging_buffer = (GPUCell*)KTerm_Calloc(term->width * term->height, sizeof(GPUCell));
    term->row_scratch_buffer = (EnhancedTermChar*)KTerm_Calloc(term->width, sizeof(EnhancedTermChar));

    // 4. Init Vector Engine (Storage Tube Architecture)
    term->vector_capacity = 65536; // Max new lines per frame
    KTerm_CreateBuffer(term->vector_capacity * sizeof(GPUVectorLine), NULL, KTERM_BUFFER_USAGE_STORAGE_BUFFER | KTERM_BUFFER_USAGE_TRANSFER_DST, &term->vector_buffer);
    term->vector_staging_buffer = (GPUVectorLine*)KTerm_Calloc(term->vector_capacity, sizeof(GPUVectorLine));

    // Create Persistent Vector Layer Texture (Storage Tube Surface)
    KTermImage vec_img = {0};
    KTerm_CreateImage(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, 4, &vec_img);
    memset(vec_img.data, 0, DEFAULT_WINDOW_WIDTH * DEFAULT_WINDOW_HEIGHT * 4); // Clear to transparent black
    KTerm_CreateTextureEx(vec_img, false, KTERM_TEXTURE_USAGE_SAMPLED | KTERM_TEXTURE_USAGE_STORAGE | KTERM_TEXTURE_USAGE_TRANSFER_DST, &term->vector_layer_texture);
    KTerm_UnloadImage(vec_img);

    // Create Vector Pipeline
    {
        unsigned char* shader_body = NULL;
        unsigned int bytes_read = 0;
        if (KTerm_LoadFileData(KTERM_VECTOR_SHADER_PATH, &bytes_read, &shader_body) == KTERM_SUCCESS && shader_body) {
            size_t l1 = strlen(vector_compute_preamble);
            char* src = (char*)KTerm_Malloc(l1 + bytes_read + 1);
            if (src) {
                strcpy(src, vector_compute_preamble);
                memcpy(src + l1, shader_body, bytes_read);
                src[l1 + bytes_read] = '\0';
                KTerm_CreateComputePipeline(src, KTERM_COMPUTE_LAYOUT_VECTOR, &term->vector_pipeline);
                KTerm_Free(src);
            }
            KTerm_Free(shader_body);
        } else {
             if (term->sessions[0].options.debug_sequences) KTerm_LogUnsupportedSequence(term, "Failed to load vector shader");
        }
    }

    // 5. Init Sixel Engine
    KTerm_CreateBuffer(65536 * sizeof(GPUSixelStrip), NULL, KTERM_BUFFER_USAGE_STORAGE_BUFFER | KTERM_BUFFER_USAGE_TRANSFER_DST, &term->sixel_buffer);
    KTerm_CreateBuffer(256 * sizeof(uint32_t), NULL, KTERM_BUFFER_USAGE_STORAGE_BUFFER | KTERM_BUFFER_USAGE_TRANSFER_DST, &term->sixel_palette_buffer);
    {
        unsigned char* shader_body = NULL;
        unsigned int bytes_read = 0;
        if (KTerm_LoadFileData(KTERM_SIXEL_SHADER_PATH, &bytes_read, &shader_body) == KTERM_SUCCESS && shader_body) {
            size_t l1 = strlen(sixel_compute_preamble);
            char* src = (char*)KTerm_Malloc(l1 + bytes_read + 1);
            if (src) {
                strcpy(src, sixel_compute_preamble);
                memcpy(src + l1, shader_body, bytes_read);
                src[l1 + bytes_read] = '\0';
                KTerm_CreateComputePipeline(src, KTERM_COMPUTE_LAYOUT_SIXEL, &term->sixel_pipeline);
                KTerm_Free(src);
            }
            KTerm_Free(shader_body);
        } else {
             if (term->sessions[0].options.debug_sequences) KTerm_LogUnsupportedSequence(term, "Failed to load sixel shader");
        }
    }

    // 6. Init Texture Blit Pipeline (Kitty)
    {
        unsigned char* shader_body = NULL;
        unsigned int bytes_read = 0;
        if (KTerm_LoadFileData("shaders/texture_blit.comp", &bytes_read, &shader_body) == KTERM_SUCCESS && shader_body) {
            size_t l1 = strlen(blit_compute_preamble);
            char* src = (char*)KTerm_Malloc(l1 + bytes_read + 1);
            if (src) {
                strcpy(src, blit_compute_preamble);
                memcpy(src + l1, shader_body, bytes_read);
                src[l1 + bytes_read] = '\0';
                // Using TERMINAL layout since it roughly matches (Image at Binding 1 + Bindless)
                KTerm_CreateComputePipeline(src, KTERM_COMPUTE_LAYOUT_TERMINAL, &term->texture_blit_pipeline);
                KTerm_Free(src);
            }
            KTerm_Free(shader_body);
        }
    }

    term->compute_initialized = true;
}

// =============================================================================
// CHARACTER SET TRANSLATION SYSTEM
// =============================================================================

unsigned int TranslateCharacter(KTerm* term, unsigned char ch, CharsetState* state) {
    CharacterSet active_set;

    // 1. Determine Active Set
    if (state->single_shift_2) {
        active_set = state->g2;
        state->single_shift_2 = false;
    } else if (state->single_shift_3) {
        active_set = state->g3;
        state->single_shift_3 = false;
    } else {
        // GL (0x00-0x7F) vs GR (0x80-0xFF)
        if (ch < 0x80) {
            active_set = *state->gl;
        } else {
            active_set = *state->gr;
        }
    }

    // 2. UTF-8 Bypass
    if (active_set == CHARSET_UTF8) {
        return ch;
    }

    // 3. Translation
    if (ch >= 0x80) {
        // High-bit characters
        if (active_set == CHARSET_ISO_LATIN_1 || active_set == CHARSET_DEC_MULTINATIONAL) {
            return ch; // Pass-through for 8-bit sets
        }
        // For 7-bit sets mapped to GR, strip high bit for lookup
        // e.g. DEC Special Graphics in GR (rare, but valid in ISO 2022)
        unsigned char seven_bit = ch & 0x7F;
        if (active_set < CHARSET_COUNT) {
            return term->charset_lut[active_set][seven_bit];
        }
        return ch;
    } else {
        // Low-bit characters (GL)
        if (active_set < CHARSET_COUNT) {
            return term->charset_lut[active_set][ch];
        }
    }

    return ch;
}

// Render a glyph from TTF or fallback
static void RenderGlyphToAtlas(KTerm* term, uint32_t codepoint, uint32_t idx) {
    int col = idx % term->atlas_cols;
    int row = idx / term->atlas_cols;
    int x_start = col * DEFAULT_CHAR_WIDTH;
    int y_start = row * DEFAULT_CHAR_HEIGHT;
    bool rendered = false;

    if (term->ttf.loaded) {
        // TTF Rendering
        int w, h, xoff, yoff;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&term->ttf.info, term->ttf.scale, term->ttf.scale, codepoint, &w, &h, &xoff, &yoff);

        if (bitmap) {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int px = x + (DEFAULT_CHAR_WIDTH - w)/2; // Simple center X
                    int py = y + term->ttf.baseline + yoff; // yoff is negative (distance from baseline up)

                    if (px >= 0 && px < DEFAULT_CHAR_WIDTH && py >= 0 && py < DEFAULT_CHAR_HEIGHT) {
                        int val = bitmap[y * w + x];
                        int px_idx = ((y_start + py) * term->atlas_width + (x_start + px)) * 4;
                        term->font_atlas_pixels[px_idx+0] = 255;
                        term->font_atlas_pixels[px_idx+1] = 255;
                        term->font_atlas_pixels[px_idx+2] = 255;
                        term->font_atlas_pixels[px_idx+3] = val; // Use alpha
                    }
                }
            }
            stbtt_FreeBitmap(bitmap, NULL);
            rendered = true;
        }
    }

    if (!rendered) {
        // Fallback
        if (codepoint == 0xFFFD) {
            // Draw Replacement Character (Diamond with ?)
            for (int y = 0; y < DEFAULT_CHAR_HEIGHT; y++) {
                for (int x = 0; x < DEFAULT_CHAR_WIDTH; x++) {
                    bool on = false;
                    // Diamond shape: abs(x - center_x) + abs(y - center_y) <= size
                    int cx = DEFAULT_CHAR_WIDTH / 2;
                    int cy = DEFAULT_CHAR_HEIGHT / 2;
                    if (abs(x - cx) + abs(y - cy) <= 3) on = true;
                    // Hollow it out
                    if (abs(x - cx) + abs(y - cy) < 2) on = false;
                    // Add question mark dot
                    if (x == cx && y == cy + 1) on = true;
                    // Add question mark curve (simple)
                    if (y == cy - 1 && x == cx) on = true;
                    if (y == cy - 2 && (x == cx || x == cx + 1)) on = true;

                    int px_idx = ((y_start + y) * term->atlas_width + (x_start + x)) * 4;
                    unsigned char val = on ? 255 : 0;
                    term->font_atlas_pixels[px_idx+0] = val;
                    term->font_atlas_pixels[px_idx+1] = val;
                    term->font_atlas_pixels[px_idx+2] = val;
                    term->font_atlas_pixels[px_idx+3] = val;
                }
            }
        } else {
            // Draw Hex Box (Fallback)
            for (int y = 0; y < DEFAULT_CHAR_HEIGHT; y++) {
                for (int x = 0; x < DEFAULT_CHAR_WIDTH; x++) {
                    bool on = false;
                    if (x == 0 || x == DEFAULT_CHAR_WIDTH-1 || y == 0 || y == DEFAULT_CHAR_HEIGHT-1) on = true;
                    if (x == DEFAULT_CHAR_WIDTH/2 && y == DEFAULT_CHAR_HEIGHT/2) on = true; // Dot

                    int px_idx = ((y_start + y) * term->atlas_width + (x_start + x)) * 4;
                    unsigned char val = on ? 255 : 0;
                    term->font_atlas_pixels[px_idx+0] = val;
                    term->font_atlas_pixels[px_idx+1] = val;
                    term->font_atlas_pixels[px_idx+2] = val;
                    term->font_atlas_pixels[px_idx+3] = val;
                }
            }
        }
    }
}

void KTerm_LoadFont(KTerm* term, const char* filepath) {
    unsigned int size;
    unsigned char* buffer = NULL;
    if (KTerm_LoadFileData(filepath, &size, &buffer) != KTERM_SUCCESS || !buffer) {
        KTerm_ReportError(term, KTERM_LOG_ERROR, KTERM_SOURCE_SYSTEM, "Failed to load font file: %s", filepath);
        return;
    }

    if (term->ttf.file_buffer) KTERM_FREE(term->ttf.file_buffer);
    term->ttf.file_buffer = buffer;

    if (!stbtt_InitFont(&term->ttf.info, buffer, 0)) {
        KTerm_ReportError(term, KTERM_LOG_ERROR, KTERM_SOURCE_SYSTEM, "Failed to init TrueType font: %s", filepath);
        return;
    }

    term->ttf.scale = stbtt_ScaleForPixelHeight(&term->ttf.info, (float)DEFAULT_CHAR_HEIGHT * 0.8f); // 80% height to leave room
    stbtt_GetFontVMetrics(&term->ttf.info, &term->ttf.ascent, &term->ttf.descent, &term->ttf.line_gap);

    // Calculate baseline
    int pixel_height = (int)((term->ttf.ascent - term->ttf.descent) * term->ttf.scale);
    int y_adjust = (DEFAULT_CHAR_HEIGHT - pixel_height) / 2;
    term->ttf.baseline = (int)(term->ttf.ascent * term->ttf.scale) + y_adjust;

    term->ttf.loaded = true;
}

// Helper to allocate a glyph index in the dynamic atlas for any Unicode codepoint
uint32_t KTerm_AllocateGlyph(KTerm* term, uint32_t codepoint) {
    // Limit to Unicode range
    if (codepoint >= 0x110000) {
        return '?'; // Return safe fallback
    }

    // Check if already mapped
    if (term->glyph_map && term->glyph_map[codepoint] != 0) {
        return term->glyph_map[codepoint];
    }

    // Safety check if glyph_map wasn't allocated
    if (!term->glyph_map) return '?';

    // Check capacity
    uint32_t capacity = (term->atlas_width / DEFAULT_CHAR_WIDTH) * (term->atlas_height / DEFAULT_CHAR_HEIGHT);
    if (term->next_atlas_index >= capacity) {
        // Atlas full. Use Clock Eviction Algorithm.
        // Iterate cyclically starting from atlas_clock_hand.
        // We look for an entry that wasn't used THIS frame.
        // If all were used this frame, we just pick the one at the hand (FIFO fallback).
        
        uint32_t lru_index = 0;
        uint32_t start_hand = term->atlas_clock_hand;
        if (start_hand < 256) start_hand = 256; // Ensure range

        while (true) {
             // Check if used this frame
             if (term->glyph_last_used[term->atlas_clock_hand] != term->frame_count) {
                 lru_index = term->atlas_clock_hand;
                 break;
             }
             
             // Move hand
             term->atlas_clock_hand++;
             if (term->atlas_clock_hand >= capacity) term->atlas_clock_hand = 256;
             
             // Full circle?
             if (term->atlas_clock_hand == start_hand) {
                 // All used this frame! Force eviction at current hand.
                 lru_index = start_hand;
                 break;
             }
        }
        
        // Move hand forward for next time
        term->atlas_clock_hand = lru_index + 1;
        if (term->atlas_clock_hand >= capacity) term->atlas_clock_hand = 256;

        // Evict
        if (lru_index >= 256) {
            uint32_t old_codepoint = term->atlas_to_codepoint[lru_index];
            if (old_codepoint < 65536) { // Safety check for map size
                term->glyph_map[old_codepoint] = 0; // Clear from map
            } else if (old_codepoint < 0x110000) {
                 // Handle full unicode range if map supports it (it does: 0x110000)
                 term->glyph_map[old_codepoint] = 0;
            }

            // Reuse this index
            term->glyph_map[codepoint] = (uint16_t)lru_index;
            term->atlas_to_codepoint[lru_index] = codepoint;
            term->glyph_last_used[lru_index] = term->frame_count; // Touch

            RenderGlyphToAtlas(term, codepoint, lru_index);
            term->font_atlas_dirty = true;
            return lru_index;
        } else {
            return '?'; // Should not happen if capacity > 256
        }
    }

    uint32_t idx = term->next_atlas_index++;
    term->glyph_map[codepoint] = (uint16_t)idx;
    term->atlas_to_codepoint[idx] = codepoint;
    term->glyph_last_used[idx] = term->frame_count;

    RenderGlyphToAtlas(term, codepoint, idx);

    term->font_atlas_dirty = true;
    return idx;
}

// Helper to map Unicode codepoints to Dynamic Atlas indices
uint32_t MapUnicodeToAtlas(KTerm* term, uint32_t codepoint) {
    // KTerm_AllocateGlyph checks glyph_map, which is now pre-populated
    // with the CP437 base mapping in KTerm_Init.
    // This ensures we find the static glyphs (0-255) for characters like '£' (0xA3 Unicode -> 0x9C CP437).
    return KTerm_AllocateGlyph(term, codepoint);
}

// Legacy wrapper (deprecated in favor of dynamic system, but kept for logic compat)
uint8_t MapUnicodeToCP437(uint32_t codepoint) {
    // This function returns a CP437 index (0-255).
    // It logic is hardcoded.
    if (codepoint < 128) return (uint8_t)codepoint;

    // Direct mappings for common box drawing and symbols present in CP437
    switch (codepoint) {
        case 0xFFFD: return '?'; // Replacement character
        case 0x00C7: return 128; // Ç
        case 0x00FC: return 129; // ü
        case 0x00E9: return 130; // é
        case 0x00E2: return 131; // â
        case 0x00E4: return 132; // ä
        case 0x00E0: return 133; // à
        case 0x00E5: return 134; // å
        case 0x00E7: return 135; // ç
        case 0x00EA: return 136; // ê
        case 0x00EB: return 137; // ë
        case 0x00E8: return 138; // è
        case 0x00EF: return 139; // ï
        case 0x00EE: return 140; // î
        case 0x00EC: return 141; // ì
        case 0x00C4: return 142; // Ä
        case 0x00C5: return 143; // Å
        case 0x00C9: return 144; // É
        case 0x00E6: return 145; // æ
        case 0x00C6: return 146; // Æ
        case 0x00F4: return 147; // ô
        case 0x00F6: return 148; // ö
        case 0x00F2: return 149; // ò
        case 0x00FB: return 150; // û
        case 0x00F9: return 151; // ù
        case 0x00FF: return 152; // ÿ
        case 0x00D6: return 153; // Ö
        case 0x00DC: return 154; // Ü
        case 0x00A2: return 155; // ¢
        case 0x00A3: return 156; // £
        case 0x00A5: return 157; // ¥
        case 0x20A7: return 158; // ₧
        case 0x0192: return 159; // ƒ
        case 0x00E1: return 160; // á
        case 0x00ED: return 161; // í
        case 0x00F3: return 162; // ó
        case 0x00FA: return 163; // ú
        case 0x00F1: return 164; // ñ
        case 0x00D1: return 165; // Ñ
        case 0x00AA: return 166; // ª
        case 0x00BA: return 167; // º
        case 0x00BF: return 168; // ¿
        case 0x2310: return 169; // ⌐
        case 0x00AC: return 170; // ¬
        case 0x00BD: return 171; // ½
        case 0x00BC: return 172; // ¼
        case 0x00A1: return 173; // ¡
        case 0x00AB: return 174; // «
        case 0x00BB: return 175; // »
        case 0x2591: return 176; // ░
        case 0x2592: return 177; // ▒
        case 0x2593: return 178; // ▓
        case 0x2502: return 179; // │
        case 0x2524: return 180; // ┤
        case 0x2561: return 181; // ╡
        case 0x2562: return 182; // ╢
        case 0x2556: return 183; // ╖
        case 0x2555: return 184; // ╕
        case 0x2563: return 185; // ╣
        case 0x2551: return 186; // ║
        case 0x2557: return 187; // ╗
        case 0x255D: return 188; // ╝
        case 0x255C: return 189; // ╜
        case 0x255B: return 190; // ╛
        case 0x2510: return 191; // ┐
        case 0x2514: return 192; // └
        case 0x2534: return 193; // ┴
        case 0x252C: return 194; // ┬
        case 0x251C: return 195; // ├
        case 0x2500: return 196; // ─
        case 0x253C: return 197; // ┼
        case 0x255E: return 198; // ╞
        case 0x255F: return 199; // ╟
        case 0x255A: return 200; // ╚
        case 0x2554: return 201; // ╔
        case 0x2569: return 202; // ╩
        case 0x2566: return 203; // ╦
        case 0x2560: return 204; // ╠
        case 0x2550: return 205; // ═
        case 0x256C: return 206; // ╬
        case 0x2567: return 207; // ╧
        case 0x2568: return 208; // ╨
        case 0x2564: return 209; // ╤
        case 0x2565: return 210; // ╥
        case 0x2559: return 211; // ╙
        case 0x2558: return 212; // ╘
        case 0x2552: return 213; // ╒
        case 0x2553: return 214; // ╓
        case 0x256B: return 215; // ╫
        case 0x256A: return 216; // ╪
        case 0x2518: return 217; // ┘
        case 0x250C: return 218; // ┌
        case 0x2588: return 219; // █
        case 0x2584: return 220; // ▄
        case 0x258C: return 221; // ▌
        case 0x2590: return 222; // ▐
        case 0x2580: return 223; // ▀
        case 0x03B1: return 224; // α
        case 0x00DF: return 225; // ß
        case 0x0393: return 226; // Γ
        case 0x03C0: return 227; // π
        case 0x03A3: return 228; // Σ
        case 0x03C3: return 229; // σ
        case 0x00B5: return 230; // µ
        case 0x03C4: return 231; // τ
        case 0x03A6: return 232; // Φ
        case 0x0398: return 233; // Θ
        case 0x03A9: return 234; // Ω
        case 0x03B4: return 235; // δ
        case 0x221E: return 236; // ∞
        case 0x03C6: return 237; // φ
        case 0x03B5: return 238; // ε
        case 0x2229: return 239; // ∩
        case 0x2261: return 240; // ≡
        case 0x00B1: return 241; // ±
        case 0x2265: return 242; // ≥
        case 0x2264: return 243; // ≤
        case 0x2320: return 244; // ⌠
        case 0x2321: return 245; // ⌡
        case 0x00F7: return 246; // ÷
        case 0x2248: return 247; // ≈
        case 0x00B0: return 248; // °
        case 0x2219: return 249; // ∙
        case 0x00B7: return 250; // ·
        case 0x221A: return 251; // √
        case 0x207F: return 252; // ⁿ
        case 0x00B2: return 253; // ²
        case 0x25A0: return 254; // ■
        case 0x00A0: return 255; // NBSP
        default: return '?';     // Fallback
    }
}

// VT100/220 "Special Graphics" to CP437 Lookup Table.
static const uint8_t vt_special_graphics_lut[32] = {
    0x20, // 0x5F (Blank)       -> Space
    0x04, // 0x60 (Diamond)     -> CP437 Diamond
    0xB1, // 0x61 (Checker)     -> CP437 Medium Shade
    0x09, // 0x62 (HT)          -> CP437 Tab/Circle
    0x0C, // 0x63 (FF)          -> CP437 Female (closest approx)
    0x0D, // 0x64 (CR)          -> CP437 Note
    0x0A, // 0x65 (LF)          -> CP437 Inv Circle
    0xF8, // 0x66 (Degree)      -> CP437 Degree
    0xF1, // 0x67 (Plus-Minus)  -> CP437 Plus-Minus
    0x0A, // 0x68 (NL)          -> (Approx)
    0x0B, // 0x69 (VT)          -> CP437 Male
    0xD9, // 0x6A (┘)           -> CP437 Bottom Right
    0xBF, // 0x6B (┐)           -> CP437 Top Right
    0xDA, // 0x6C (┌)           -> CP437 Top Left
    0xC0, // 0x6D (└)           -> CP437 Bottom Left
    0xC5, // 0x6E (┼)           -> CP437 Cross
    0xC4, // 0x6F (Scan 1)      -> CP437 Horiz Line (Approx)
    0xC4, // 0x70 (Scan 3)      -> CP437 Horiz Line (Approx)
    0xC4, // 0x71 (─ Scan 5)    -> CP437 Horiz Line (Exact)
    0xC4, // 0x72 (Scan 7)      -> CP437 Horiz Line (Approx)
    0xC4, // 0x73 (Scan 9)      -> CP437 Horiz Line (Approx)
    0xC3, // 0x74 (├)           -> CP437 Left T
    0xB4, // 0x75 (┤)           -> CP437 Right T
    0xC1, // 0x76 (┴)           -> CP437 Bottom T
    0xC2, // 0x77 (┬)           -> CP437 Top T
    0xB3, // 0x78 (│)           -> CP437 Vert Bar
    0xF3, // 0x79 (≤)           -> CP437 Less/Equal
    0xF2, // 0x7A (≥)           -> CP437 Greater/Equal
    0xE3, // 0x7B (π)           -> CP437 Pi
    0x9C, // 0x7C (≠)           -> CP437 Pound (closest/conflict) or 0x3D
    0x9C, // 0x7D (£)           -> CP437 Pound
    0xFA  // 0x7E (·)           -> CP437 Dot
};

unsigned int KTerm_TranslateDECSpecial(KTerm* term, unsigned char ch) {
    (void)term;
    if (ch >= 0x5F && ch <= 0x7E) {
        return vt_special_graphics_lut[ch - 0x5F];
    }
    return ch;
}

unsigned int KTerm_TranslateDECMultinational(KTerm* term, unsigned char ch) {
    (void)term;
    // DEC Multinational Character Set (partial implementation)
    if (ch >= 0x80) {
        // High bit characters map to Latin-1 supplement
        return 0x0080 + (ch - 0x80);
    }
    return ch;
}

// =============================================================================
// TAB STOP MANAGEMENT
// =============================================================================

static void KTerm_SetTabStop_Internal(KTermSession* session, int column) {
    if (column >= 0 && column < session->tab_stops.capacity && column < session->cols) {
        if (!session->tab_stops.stops[column]) {
            session->tab_stops.stops[column] = true;
            session->tab_stops.count++;
        }
    }
}

void KTerm_SetTabStop(KTerm* term, int column) {
    KTerm_SetTabStop_Internal(GET_SESSION(term), column);
}

void KTerm_ClearTabStop(KTerm* term, int column) {
    if (column >= 0 && column < GET_SESSION(term)->tab_stops.capacity) {
        if (GET_SESSION(term)->tab_stops.stops[column]) {
            GET_SESSION(term)->tab_stops.stops[column] = false;
            GET_SESSION(term)->tab_stops.count--;
        }
    }
}

void KTerm_ClearAllTabStops(KTerm* term) {
    if (GET_SESSION(term)->tab_stops.stops) {
        memset(GET_SESSION(term)->tab_stops.stops, false, GET_SESSION(term)->tab_stops.capacity * sizeof(bool));
        GET_SESSION(term)->tab_stops.count = 0;
    }
}

static int NextTabStop_Internal(KTermSession* session, int current_column) {
    if (!session->tab_stops.stops) return (current_column + 1 < session->cols) ? current_column + 1 : session->cols - 1;

    for (int i = current_column + 1; i < session->tab_stops.capacity && i < session->cols; i++) {
        if (session->tab_stops.stops[i]) {
            return i;
        }
    }

    // If no explicit tab stop found, move to right margin/edge
    return session->cols - 1;
}

int NextTabStop(KTerm* term, int current_column) {
    return NextTabStop_Internal(GET_SESSION(term), current_column);
}

static int PreviousTabStop_Internal(KTermSession* session, int current_column) {
    if (!session->tab_stops.stops) return (current_column > 0) ? current_column - 1 : 0;

    for (int i = current_column - 1; i >= 0; i--) {
        if (i < session->tab_stops.capacity && session->tab_stops.stops[i]) {
            return i;
        }
    }

    // If no explicit tab stop found, use default spacing
    int prev = ((current_column - 1) / session->tab_stops.default_width) * session->tab_stops.default_width;
    return (prev >= 0) ? prev : 0;
}

int PreviousTabStop(KTerm* term, int current_column) {
    return PreviousTabStop_Internal(GET_SESSION(term), current_column);
}

// =============================================================================
// ENHANCED SCREEN MANIPULATION
// =============================================================================

static void KTerm_ClearCell_Internal(KTermSession* session, EnhancedTermChar* cell) {
    cell->ch = ' ';
    cell->fg_color = session->current_fg;
    cell->bg_color = session->current_bg;
    cell->flags = session->current_attributes | KTERM_FLAG_DIRTY;
}

void KTerm_ClearCell(KTerm* term, EnhancedTermChar* cell) {
    KTerm_ClearCell_Internal(GET_SESSION(term), cell);
}

static bool IsRegionProtected(KTermSession* session, int top, int bottom, int left, int right) {
    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            if (GetActiveScreenCell(session, y, x)->flags & KTERM_ATTR_PROTECTED) return true;
        }
    }
    return false;
}

static void KTerm_ScrollUpRegion_Internal(KTerm* term, KTermSession* session, int top, int bottom, int lines) {
    (void)term;

     if (IsRegionProtected(session, top, bottom, session->left_margin, session->right_margin)) return;
    
    // Check for full screen scroll (Top to Bottom, Full Width)
    // This allows optimization via Ring Buffer pointer arithmetic.
    if (top == 0 && bottom == term->height - 1 &&
        session->left_margin == 0 && session->right_margin == term->width - 1)
    {
        for (int i = 0; i < lines; i++) {
            // Increment head (scrolling down in memory, visually up)
            session->screen_head = (session->screen_head + 1) % session->buffer_height;
            if (session->history_rows_populated < MAX_SCROLLBACK_LINES) {
                session->history_rows_populated++;
            }

            // Adjust view_offset to keep historical view stable if user is looking back
            if (session->view_offset > 0) {
                 session->view_offset++;
                 // Cap at buffer limits
                 int max_offset = session->buffer_height - term->height;
                 if (session->view_offset > max_offset) session->view_offset = max_offset;
            }

            // Clear the new bottom line (logical row 'bottom')
            for (int x = 0; x < term->width; x++) {
                KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, bottom, x));
            }
        }
        // Invalidate all viewport rows because the data under them has shifted
        for (int y = 0; y < term->height; y++) {
            session->row_dirty[y] = KTERM_DIRTY_FRAMES;
        }
        return;
    }

    // Partial Scroll (Fallback to copy) - Strictly NO head manipulation
    for (int i = 0; i < lines; i++) {
        // Move lines up within the region
        for (int y = top; y < bottom; y++) {
            for (int x = session->left_margin; x <= session->right_margin; x++) {
                *GetActiveScreenCell(session, y, x) = *GetActiveScreenCell(session, y + 1, x);
                GetActiveScreenCell(session, y, x)->flags |= KTERM_FLAG_DIRTY;
            }
            session->row_dirty[y] = KTERM_DIRTY_FRAMES;
        }

        // Clear bottom line of the region
        for (int x = session->left_margin; x <= session->right_margin; x++) {
            KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, bottom, x));
        }
        session->row_dirty[bottom] = KTERM_DIRTY_FRAMES;
    }
}

void KTerm_ScrollUpRegion(KTerm* term, int top, int bottom, int lines) {
    KTermSession* session = GET_SESSION(term);
    KTermRect rect;
    rect.x = session->left_margin;
    rect.y = top;
    rect.w = session->right_margin - session->left_margin + 1;
    rect.h = bottom - top + 1;
    KTerm_QueueScrollRegion(session, rect, lines);
}

static void KTerm_ScrollDownRegion_Internal(KTerm* term, KTermSession* session, int top, int bottom, int lines) {
    (void)term;

    if (IsRegionProtected(session, top, bottom, session->left_margin, session->right_margin)) return;
    
    for (int i = 0; i < lines; i++) {
        // Move lines down
        for (int y = bottom; y > top; y--) {
            for (int x = session->left_margin; x <= session->right_margin; x++) {
                *GetActiveScreenCell(session, y, x) = *GetActiveScreenCell(session, y - 1, x);
                GetActiveScreenCell(session, y, x)->flags |= KTERM_FLAG_DIRTY;
            }
            session->row_dirty[y] = KTERM_DIRTY_FRAMES;
        }

        // Clear top line
        for (int x = session->left_margin; x <= session->right_margin; x++) {
            KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, top, x));
        }
        session->row_dirty[top] = KTERM_DIRTY_FRAMES;
    }
}

void KTerm_ScrollDownRegion(KTerm* term, int top, int bottom, int lines) {
    KTermSession* session = GET_SESSION(term);
    KTermRect rect;
    rect.x = session->left_margin;
    rect.y = top;
    rect.w = session->right_margin - session->left_margin + 1;
    rect.h = bottom - top + 1;
    KTerm_QueueScrollRegion(session, rect, -lines);
}

static void KTerm_InsertLinesAt_Internal(KTerm* term, KTermSession* session, int row, int count) {
    (void)term;
    if (row < session->scroll_top || row > session->scroll_bottom) {
        return;
    }

    if (IsRegionProtected(session, row, session->scroll_bottom, session->left_margin, session->right_margin)) return;
    
    // Move existing lines down
    for (int y = session->scroll_bottom; y >= row + count; y--) {
        if (y - count >= row) {
            for (int x = session->left_margin; x <= session->right_margin; x++) {
                *GetActiveScreenCell(session, y, x) = *GetActiveScreenCell(session, y - count, x);
                GetActiveScreenCell(session, y, x)->flags |= KTERM_FLAG_DIRTY;
            }
            session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }
    }

    // Clear inserted lines
    for (int y = row; y < row + count && y <= session->scroll_bottom; y++) {
        for (int x = session->left_margin; x <= session->right_margin; x++) {
            KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, y, x));
        }
        session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }
}

void KTerm_InsertLinesAt(KTerm* term, int row, int count) {
    KTermSession* session = GET_SESSION(term);
    if (row < session->scroll_top || row > session->scroll_bottom) return;

    KTermRect region;
    region.x = session->left_margin;
    region.y = row;
    region.w = session->right_margin - session->left_margin + 1;
    region.h = session->scroll_bottom - row + 1;

    KTermOp op;
    op.type = KTERM_OP_INSERT_LINES;
    op.u.vertical.region = region;
    op.u.vertical.count = count;
    op.u.vertical.respect_protected = true;
    op.u.vertical.downward = true;
    KTerm_QueueOp(&session->op_queue, op);
}

EnhancedTermChar* KTerm_GetCell(KTerm* term, int x, int y) {
    if (!term) return NULL;
    KTermSession* session = GET_SESSION(term);
    return GetScreenCell(session, y, x);
}

void KTerm_SetCellDirect(KTerm* term, int x, int y, EnhancedTermChar c) {
    if (!term) return;
    KTermSession* session = GET_SESSION(term);
    EnhancedTermChar* cell = GetScreenCell(session, y, x);
    if (cell) {
        *cell = c;
        cell->flags |= KTERM_FLAG_DIRTY;
        if (y >= 0 && y < session->rows) session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }
}

void KTerm_MarkRegionDirty(KTerm* term, KTermRect rect) {
    if (!term) return;
    KTermSession* session = GET_SESSION(term);
    
    // Clamp to viewport
    int top = rect.y;
    int bottom = rect.y + rect.h - 1;
    if (top < 0) top = 0;
    if (bottom >= session->rows) bottom = session->rows - 1;

    for (int y = top; y <= bottom; y++) {
        session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }

    // Merge into dirty rect
    if (session->dirty_rect.w == 0) {
        session->dirty_rect = rect;
    } else {
        int x1 = (rect.x < session->dirty_rect.x) ? rect.x : session->dirty_rect.x;
        int y1 = (rect.y < session->dirty_rect.y) ? rect.y : session->dirty_rect.y;
        int x2_a = rect.x + rect.w;
        int x2_b = session->dirty_rect.x + session->dirty_rect.w;
        int x2 = (x2_a > x2_b) ? x2_a : x2_b;
        int y2_a = rect.y + rect.h;
        int y2_b = session->dirty_rect.y + session->dirty_rect.h;
        int y2 = (y2_a > y2_b) ? y2_a : y2_b;
        session->dirty_rect = (KTermRect){x1, y1, x2 - x1, y2 - y1};
    }
}

void KTerm_DeleteLinesAt(KTerm* term, int row, int count) {
    KTermSession* session = GET_SESSION(term);
    if (row < session->scroll_top || row > session->scroll_bottom) return;

    KTermRect region;
    region.x = session->left_margin;
    region.y = row;
    region.w = session->right_margin - session->left_margin + 1;
    region.h = session->scroll_bottom - row + 1;

    KTermOp op;
    op.type = KTERM_OP_DELETE_LINES;
    op.u.vertical.region = region;
    op.u.vertical.count = count;
    op.u.vertical.respect_protected = true;
    op.u.vertical.downward = false;
    KTerm_QueueOp(&session->op_queue, op);
}

static void KTerm_InsertCharactersAt_Internal(KTerm* term, KTermSession* session, int row, int col, int count) {
    (void)term;

    if (IsRegionProtected(session, row, row, col, session->right_margin)) return;
    
    // Shift existing characters right
    for (int x = session->right_margin; x >= col + count; x--) {
        if (x - count >= col) {
            *GetActiveScreenCell(session, row, x) = *GetActiveScreenCell(session, row, x - count);
            GetActiveScreenCell(session, row, x)->flags |= KTERM_FLAG_DIRTY;
        }
    }

    // Clear inserted positions
    for (int x = col; x < col + count && x <= session->right_margin; x++) {
        KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, row, x));
    }
    session->row_dirty[row] = KTERM_DIRTY_FRAMES;
}

void KTerm_InsertCharactersAt(KTerm* term, int row, int col, int count) {
    KTerm_InsertCharactersAt_Internal(term, GET_SESSION(term), row, col, count);
}

static void KTerm_DeleteCharactersAt_Internal(KTerm* term, KTermSession* session, int row, int col, int count) {
    (void)term;

    if (IsRegionProtected(session, row, row, col, session->right_margin)) return;
    
    // Shift remaining characters left
    for (int x = col; x <= session->right_margin - count; x++) {
        *GetActiveScreenCell(session, row, x) = *GetActiveScreenCell(session, row, x + count);
        GetActiveScreenCell(session, row, x)->flags |= KTERM_FLAG_DIRTY;
    }

    // Clear rightmost positions
    for (int x = session->right_margin - count + 1; x <= session->right_margin; x++) {
        if (x >= 0) {
            KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, row, x));
        }
    }
    session->row_dirty[row] = KTERM_DIRTY_FRAMES;
}

void KTerm_DeleteCharactersAt(KTerm* term, int row, int col, int count) {
    KTerm_DeleteCharactersAt_Internal(term, GET_SESSION(term), row, col, count);
}

// =============================================================================
// VT100 INSERT MODE IMPLEMENTATION
// =============================================================================

static void EnableInsertMode(KTerm* term, KTermSession* session, bool enable) {
    if (enable) session->dec_modes |= KTERM_MODE_INSERT; else session->dec_modes &= ~KTERM_MODE_INSERT;
}

// Internal version of InsertCharacterAtCursor taking session
static void KTerm_InsertCharacterAtCursor_Internal(KTerm* term, KTermSession* session, unsigned int ch, int width) {
    int storage_width = (width == 0) ? 1 : width;
    bool is_combining = (width == 0);

    if ((session->dec_modes & KTERM_MODE_INSERT)) {
        // Insert mode: shift existing characters right
        // If line is protected, ignore typing completely (VT520)
        if (IsRegionProtected(session, session->cursor.y, session->cursor.y, session->cursor.x, session->right_margin)) return;
        
        if (storage_width > 0) {
            KTerm_InsertCharactersAt(term, session->cursor.y, session->cursor.x, storage_width);
        }
    } else {
        // Replace Mode: Cannot overwrite protected character
        EnhancedTermChar* target = GetActiveScreenCell(session, session->cursor.y, session->cursor.x);
        if (target->flags & KTERM_ATTR_PROTECTED) return;
        if (storage_width > 1) {
            EnhancedTermChar* target2 = GetActiveScreenCell(session, session->cursor.y, session->cursor.x + 1);
            if (target2 && (target2->flags & KTERM_ATTR_PROTECTED)) return;
        }

        // Handling Combining Characters in Replace Mode:
        if (is_combining) {
             if (IsRegionProtected(session, session->cursor.y, session->cursor.y, session->cursor.x, session->right_margin)) return;
             KTerm_InsertCharactersAt(term, session->cursor.y, session->cursor.x, storage_width);
        }
    }

    // Place character at cursor position (Mandatory Queue)
    KTermOp op;
    op.type = KTERM_OP_SET_CELL;
    op.u.set_cell.x = session->cursor.x;
    op.u.set_cell.y = session->cursor.y;

    // Prepare cell data
    EnhancedTermChar target_cell;
    // Need to preserve existing line attributes from the target cell
    // We can peek at the grid safely (read-only)
    EnhancedTermChar* existing = GetActiveScreenCell(session, session->cursor.y, session->cursor.x);
    uint32_t line_attrs = 0;
    if (existing) {
            line_attrs = existing->flags & (KTERM_ATTR_DOUBLE_WIDTH | KTERM_ATTR_DOUBLE_HEIGHT_TOP | KTERM_ATTR_DOUBLE_HEIGHT_BOT);
    }

    target_cell.ch = ch;
    target_cell.fg_color = session->current_fg;
    target_cell.bg_color = session->current_bg;
    target_cell.ul_color = session->current_ul_color;
    target_cell.st_color = session->current_st_color;
    target_cell.flags = session->current_attributes | line_attrs | KTERM_FLAG_DIRTY;

    if (is_combining) {
        target_cell.flags |= KTERM_FLAG_COMBINING;
    }
    op.u.set_cell.cell = target_cell;

    KTerm_QueueOp(&session->op_queue, op);

    if (width > 1) {
        // Clear second cell
        EnhancedTermChar fill_char = {0};
        fill_char.ch = ' ';
        fill_char.fg_color = session->current_fg;
        fill_char.bg_color = session->current_bg;
        fill_char.flags = 0;
        
        KTermRect r = {session->cursor.x + 1, session->cursor.y, 1, 1};
        KTerm_QueueFillRect(session, r, fill_char);
    }

    // Track last printed character for REP command
    session->last_char = ch;
}

void KTerm_InsertCharacterAtCursor(KTerm* term, unsigned int ch) {
    KTermSession* session = GET_SESSION(term);
    int width = 1;
    if (session->enable_wide_chars && *session->charset.gl == CHARSET_UTF8) {
        width = KTerm_wcwidth(ch);
        if (width < 0) width = 1;
    }
    KTerm_InsertCharacterAtCursor_Internal(term, session, ch, width);
}

// =============================================================================
// COMPREHENSIVE CHARACTER PROCESSING
// =============================================================================

void KTerm_ProcessNormalChar(KTerm* term, KTermSession* session, unsigned char ch) {
    // Handle control characters first
    if (ch < 32) {
        KTerm_ProcessControlChar(term, session, ch);
        return;
    }

    // Translate character through active character set
    unsigned int unicode_ch = TranslateCharacter(term, ch, &session->charset);

    // Handle UTF-8 decoding if enabled
    if (*session->charset.gl == CHARSET_UTF8) {
        if (session->utf8.bytes_remaining == 0) {
            if (ch < 0x80) {
                // 1-byte sequence (ASCII)
                unicode_ch = ch;
            } else if ((ch & 0xE0) == 0xC0) {
                // 2-byte sequence
                if (ch < 0xC2) { // Overlong (C0, C1)
                    unicode_ch = 0xFFFD;
                    session->utf8.bytes_remaining = 0; // Explicit reset
                    // Fall through to display replacement
                } else {
                    session->utf8.codepoint = ch & 0x1F;
                    session->utf8.min_codepoint = 0x80;
                    session->utf8.bytes_remaining = 1;
                    return;
                }
            } else if ((ch & 0xF0) == 0xE0) {
                // 3-byte sequence
                session->utf8.codepoint = ch & 0x0F;
                session->utf8.min_codepoint = 0x800;
                session->utf8.bytes_remaining = 2;
                return;
            } else if ((ch & 0xF8) == 0xF0) {
                // 4-byte sequence
                if (ch > 0xF4) { // Restricted by RFC 3629
                    unicode_ch = 0xFFFD;
                    session->utf8.bytes_remaining = 0; // Explicit reset
                    // Fall through
                } else {
                    session->utf8.codepoint = ch & 0x07;
                    session->utf8.min_codepoint = 0x10000;
                    session->utf8.bytes_remaining = 3;
                    return;
                }
            } else {
                // Invalid start byte
                unicode_ch = 0xFFFD;
                session->utf8.bytes_remaining = 0; // Explicit reset
                // Fall through to display replacement and consume byte
            }
        } else {
            // Continuation byte
            if ((ch & 0xC0) == 0x80) {
                session->utf8.codepoint = (session->utf8.codepoint << 6) | (ch & 0x3F);
                session->utf8.bytes_remaining--;
                if (session->utf8.bytes_remaining > 0) {
                    return;
                }
                // Sequence complete
                uint32_t cp = session->utf8.codepoint;
                bool valid = true;

                // Check for overlong encodings
                if (cp < session->utf8.min_codepoint) valid = false;
                // Check for surrogates (D800-DFFF)
                if (cp >= 0xD800 && cp <= 0xDFFF) valid = false;
                // Check max value
                if (cp > 0x10FFFF) valid = false;

                if (valid) {
                    unicode_ch = cp;
                    // Attempt to map to CP437 for pixel-perfect box drawing, but preserve Unicode if not found
                    uint8_t cp437 = MapUnicodeToCP437(unicode_ch);
                    if (cp437 != '?' || unicode_ch == '?') {
                        unicode_ch = cp437;
                    }
                } else {
                    unicode_ch = 0xFFFD;
                }
                // bytes_remaining is already 0 here
            } else {
                // Invalid continuation byte
                KTerm_InsertCharacterAtCursor_Internal(term, session, 0xFFFD, 1);
                session->cursor.x++;

                // Reset and try to recover
                session->utf8.bytes_remaining = 0;
                session->utf8.codepoint = 0;

                // Recursively process this char as new
                KTerm_ProcessNormalChar(term, session, ch);
                return;
            }
        }
    }

    // Handle character display
    int width = 1;
    if (session->enable_wide_chars && *session->charset.gl == CHARSET_UTF8) {
        width = KTerm_wcwidth(unicode_ch);
        if (width < 0) width = 1; // Fallback for C1 controls or invalid chars in UTF-8 to ensure we print *something* if it passed decoding
    }

    if ((session->dec_modes & KTERM_MODE_DECAWM)) {
        if (session->cursor.x + width - 1 > session->right_margin) {
            // Auto-wrap to next line
            session->cursor.x = session->left_margin;
            session->cursor.y++;

            if (session->cursor.y > session->scroll_bottom) {
                session->cursor.y = session->scroll_bottom;
                KTermRect r = {0, session->scroll_top, session->cols, session->scroll_bottom - session->scroll_top + 1}; KTerm_QueueScrollRegion(session, r, 1);
            }
        }
    } else {
        // No wrap - stay at right margin
        if (session->cursor.x > session->right_margin) {
            session->cursor.x = session->right_margin;
        }
    }

    // Insert character (handles insert mode internally)
    KTerm_InsertCharacterAtCursor_Internal(term, session, unicode_ch, width);

    // Advance cursor
    // If width is 0 (combining), we still advance cursor in the storage grid
    // because we store it in a separate cell.
    int advance = (width == 0) ? 1 : width;
    session->cursor.x += advance;
}

// Update KTerm_ProcessControlChar
void KTerm_ProcessControlChar(KTerm* term, KTermSession* session, unsigned char ch) {
    switch (ch) {
        case 0x05: // ENQ - Enquiry
            if (session->answerback_buffer[0] != '\0') {
                KTerm_QueueResponse(term, session->answerback_buffer);
            }
            break;
        case 0x07: // BEL - Bell
            if (term->bell_callback) {
                term->bell_callback(term);
            } else {
                // Visual bell
                session->visual_bell_timer = 0.2;
            }
            break;
        case 0x08: // BS - Backspace
            if (session->cursor.x > session->left_margin) {
                session->cursor.x--;
            }
            break;
        case 0x09: // HT - Horizontal Tab
            session->cursor.x = NextTabStop(term, session->cursor.x);
            if (session->cursor.x > session->right_margin) {
                session->cursor.x = session->right_margin;
            }
            break;
        case 0x0A: // LF - Line Feed
        case 0x0B: // VT - Vertical Tab
        case 0x0C: // FF - Form Feed
            session->cursor.y++;
            if (session->cursor.y > session->scroll_bottom) {
                session->cursor.y = session->scroll_bottom;
                KTerm_ScrollUpRegion(term, session->scroll_top, session->scroll_bottom, 1);
            }
            if (session->ansi_modes.line_feed_new_line) {
                session->cursor.x = session->left_margin;
            }
            break;
        case 0x0D: // CR - Carriage Return
            session->cursor.x = session->left_margin;
            break;
        case 0x0E: // SO - Shift Out (invoke G1 into GL)
            session->charset.gl = &session->charset.g1;
            break;
        case 0x0F: // SI - Shift In (invoke G0 into GL)
            session->charset.gl = &session->charset.g0;
            break;
        case 0x11: // XON - Resume transmission
            // Flow control - not implemented
            break;
        case 0x13: // XOFF - Stop transmission
            // Flow control - not implemented
            break;
        case 0x18: // CAN - Cancel
        case 0x1A: // SUB - Substitute
            // Cancel current escape sequence
            session->parse_state = VT_PARSE_NORMAL;
            session->escape_pos = 0;
            break;
        case 0x1B: // ESC - Escape
            if ((session->dec_modes & KTERM_MODE_VT52)) {
                session->parse_state = PARSE_VT52;
            } else {
                session->parse_state = VT_PARSE_ESCAPE;
            }
            session->escape_pos = 0;
            break;
        case 0x7F: // DEL - Delete
            // Ignored
            break;
        default:
            if (session->options.debug_sequences) {
                char debug_msg[64];
                snprintf(debug_msg, sizeof(debug_msg), "Unknown control char: 0x%02X", ch);
                KTerm_LogUnsupportedSequence(term, debug_msg);
            }
            break;
    }
}


// =============================================================================
// ENHANCED ESCAPE SEQUENCE PROCESSING
// =============================================================================

void KTerm_ProcessEscapeChar(KTerm* term, KTermSession* session, unsigned char ch) {
    switch (ch) {
        // CSI - Control Sequence Introducer
        case '[':
            session->parse_state = PARSE_CSI;
            session->escape_pos = 0;
            memset(session->escape_params, 0, sizeof(session->escape_params));
            session->param_count = 0;
            break;

        // OSC - Operating System Command
        case ']':
            session->parse_state = PARSE_OSC;
            session->escape_pos = 0;
            break;

        // DCS - Device Control String
        case 'P':
            session->parse_state = PARSE_DCS;
            session->escape_pos = 0;
            break;

        // APC - Application Program Command
        case '_':
            session->parse_state = PARSE_APC;
            session->escape_pos = 0;
            break;

        // PM - Privacy Message
        case '^':
            session->parse_state = PARSE_PM;
            session->escape_pos = 0;
            break;

        // SOS - Start of String
        case 'X':
            session->parse_state = PARSE_SOS;
            session->escape_pos = 0;
            break;

        // Character set selection
        case '(':
            session->parse_state = PARSE_CHARSET;
            session->escape_buffer[0] = '(';
            session->escape_pos = 1;
            break;

        case ')':
            session->parse_state = PARSE_CHARSET;
            session->escape_buffer[0] = ')';
            session->escape_pos = 1;
            break;

        case '*':
            session->parse_state = PARSE_CHARSET;
            session->escape_buffer[0] = '*';
            session->escape_pos = 1;
            break;

        case '+':
            session->parse_state = PARSE_CHARSET;
            session->escape_buffer[0] = '+';
            session->escape_pos = 1;
            break;

        // Locking Shifts (ISO 2022)
        case 'n': // LS2 (GL = G2)
            session->charset.gl = &session->charset.g2;
            session->parse_state = VT_PARSE_NORMAL;
            break;
        case 'o': // LS3 (GL = G3)
            session->charset.gl = &session->charset.g3;
            session->parse_state = VT_PARSE_NORMAL;
            break;
        case '~': // LS1R (GR = G1)
            session->charset.gr = &session->charset.g1;
            session->parse_state = VT_PARSE_NORMAL;
            break;
        case '}': // LS2R (GR = G2)
            session->charset.gr = &session->charset.g2;
            session->parse_state = VT_PARSE_NORMAL;
            break;
        case '|': // LS3R (GR = G3)
            session->charset.gr = &session->charset.g3;
            session->parse_state = VT_PARSE_NORMAL;
            break;

        // Single character commands
        case '7': // DECSC - Save Cursor
            KTerm_ExecuteSaveCursor_Internal(session);
            session->parse_state = VT_PARSE_NORMAL;
            break;

        case '8': // DECRC - Restore Cursor
            KTerm_ExecuteRestoreCursor_Internal(session);
            session->parse_state = VT_PARSE_NORMAL;
            break;

        case '#': // DEC Line Attributes
            session->parse_state = PARSE_HASH;
            break;

        case '%': // Select Character Set
            session->parse_state = PARSE_PERCENT;
            break;

        case '\\': // ST - String Terminator (7-bit)
            // If received in ESC state, it terminates the sequence (NOP)
            session->parse_state = VT_PARSE_NORMAL;
            break;

        case ' ': // nF Escape Sequence (e.g., S7C1T/S8C1T)
            session->parse_state = PARSE_nF;
            break;

        case 'D': // IND - Index
            session->cursor.y++;
            if (session->cursor.y > session->scroll_bottom) {
                session->cursor.y = session->scroll_bottom;
                KTermRect r = {0, session->scroll_top, session->cols, session->scroll_bottom - session->scroll_top + 1}; KTerm_QueueScrollRegion(session, r, 1);
            }
            session->parse_state = VT_PARSE_NORMAL;
            break;

        case 'E': // NEL - Next Line
            session->cursor.x = session->left_margin;
            session->cursor.y++;
            if (session->cursor.y > session->scroll_bottom) {
                session->cursor.y = session->scroll_bottom;
                KTermRect r = {0, session->scroll_top, session->cols, session->scroll_bottom - session->scroll_top + 1}; KTerm_QueueScrollRegion(session, r, 1);
            }
            session->parse_state = VT_PARSE_NORMAL;
            break;

        case 'H': // HTS - Set Tab Stop
            KTerm_SetTabStop_Internal(session, session->cursor.x);
            session->parse_state = VT_PARSE_NORMAL;
            break;

        case 'M': // RI - Reverse Index
            session->cursor.y--;
            if (session->cursor.y < session->scroll_top) {
                session->cursor.y = session->scroll_top;
                KTerm_ScrollDownRegion(term, session->scroll_top, session->scroll_bottom, 1);
            }
            session->parse_state = VT_PARSE_NORMAL;
            break;

        // case 'n': // LS2 - Locking Shift 2 (Handled above as LS2)
        // case 'o': // LS3 - Locking Shift 3 (Handled above as LS3)

        case 'N': // SS2 - Single Shift 2
            session->charset.single_shift_2 = true;
            session->parse_state = VT_PARSE_NORMAL;
            break;

        case 'O': // SS3 - Single Shift 3
            session->charset.single_shift_3 = true;
            session->parse_state = VT_PARSE_NORMAL;
            break;

        case 'Z': // DECID - Identify Terminal
            KTerm_QueueResponse(term, session->device_attributes);
            session->parse_state = VT_PARSE_NORMAL;
            break;

        case 'c': // RIS - Reset to Initial State
            KTerm_ResetGraphics(term, session, GRAPHICS_RESET_ALL);
            KTerm_Init(term);
            break;

        case '=': // DECKPAM - Keypad Application Mode
            session->input.keypad_application_mode = true;
            GET_SESSION(term)->parse_state = VT_PARSE_NORMAL;
            break;

        case '>': // DECKPNM - Keypad Numeric Mode
            session->input.keypad_application_mode = false;
            GET_SESSION(term)->parse_state = VT_PARSE_NORMAL;
            break;

        case '<': // Enter VT52 mode (if enabled)
            if ((GET_SESSION(term)->conformance.features & KTERM_FEATURE_VT52_MODE)) {
                GET_SESSION(term)->parse_state = PARSE_VT52;
            } else {
                GET_SESSION(term)->parse_state = VT_PARSE_NORMAL;
                if (GET_SESSION(term)->options.log_unsupported) {
                    KTerm_LogUnsupportedSequence(term, "VT52 mode not supported");
                }
            }
            break;

        default:
            // Unknown escape sequence
            if (GET_SESSION(term)->options.debug_sequences) {
                char debug_msg[64];
                snprintf(debug_msg, sizeof(debug_msg), "Unknown ESC %c (0x%02X)", ch, ch);
                KTerm_LogUnsupportedSequence(term, debug_msg);
            }
            GET_SESSION(term)->parse_state = VT_PARSE_NORMAL;
            break;
    }
}

// =============================================================================
// Internal helper for writing to a specific session without changing global state
static bool KTerm_WriteCharToSessionInternal(KTerm* term, KTermSession* session, unsigned char ch) {
    (void)term; // Currently unused, but kept for signature consistency

    // Load head relaxed (only this thread writes to it)
    int current_head = atomic_load_explicit(&session->pipeline_head, memory_order_relaxed);
    int next_head = (current_head + 1) % sizeof(session->input_pipeline);

    // Load tail acquire (another thread writes to it)
    int current_tail = atomic_load_explicit(&session->pipeline_tail, memory_order_acquire);

    if (next_head == current_tail) {
        atomic_store_explicit(&session->pipeline_overflow, true, memory_order_relaxed);
        return false;
    }

    session->input_pipeline[current_head] = ch;

    // Store head release (publishes the data write)
    atomic_store_explicit(&session->pipeline_head, next_head, memory_order_release);
    return true;
}

// ENHANCED PIPELINE PROCESSING
// =============================================================================
bool KTerm_WriteChar(KTerm* term, unsigned char ch) {
    // Wrapper around internal function using active session
    // Reads active_session once atomically (in C sense)
    KTermSession* session = &term->sessions[term->active_session];
    return KTerm_WriteCharToSessionInternal(term, session, ch);
}

bool KTerm_WriteString(KTerm* term, const char* str) {
    if (!str) return false;

    while (*str) {
        if (!KTerm_WriteChar(term, *str)) {
            return false;
        }
        str++;
    }
    return true;
}

bool KTerm_WriteFormat(KTerm* term, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    return KTerm_WriteString(term, buffer);
}

void KTerm_ClearEvents(KTerm* term) {
    GET_SESSION(term)->pipeline_head = 0;
    GET_SESSION(term)->pipeline_tail = 0;
    GET_SESSION(term)->pipeline_count = 0;
    GET_SESSION(term)->pipeline_overflow = false;
}

int KTerm_GetPendingEventCount(KTerm* term) {
    KTermSession* session = GET_SESSION(term);
    int head = atomic_load_explicit(&session->pipeline_head, memory_order_relaxed);
    int tail = atomic_load_explicit(&session->pipeline_tail, memory_order_relaxed);
    return (head - tail + KTERM_INPUT_PIPELINE_SIZE) % KTERM_INPUT_PIPELINE_SIZE;
}

bool KTerm_IsEventOverflow(KTerm* term) {
    return atomic_load_explicit(&GET_SESSION(term)->pipeline_overflow, memory_order_relaxed);
}

// =============================================================================
// BASIC IMPLEMENTATIONS FOR MISSING FUNCTIONS
// =============================================================================

void KTerm_SetResponseCallback(KTerm* term, ResponseCallback callback) {
    term->response_callback = callback;
}

void KTerm_SetOutputSink(KTerm* term, KTermOutputSink sink, void* ctx) {
    KTermSession* session = GET_SESSION(term);
    // Flush from ring buffer if switching
    int head = atomic_load_explicit(&session->response_ring.head, memory_order_acquire);
    int tail = atomic_load_explicit(&session->response_ring.tail, memory_order_relaxed);

    if (head != tail) {
        if (sink) {
            // New Sink
            if (tail > head) {
                sink(ctx, &session->response_ring.data[head], tail - head);
            } else {
                sink(ctx, &session->response_ring.data[head], KTERM_OUTPUT_PIPELINE_SIZE - head);
                if (tail > 0) sink(ctx, &session->response_ring.data[0], tail);
            }
        } else if (term->output_sink) {
             // Old Sink (Flush before removing)
            if (tail > head) {
                term->output_sink(term->output_sink_ctx, &session->response_ring.data[head], tail - head);
            } else {
                term->output_sink(term->output_sink_ctx, &session->response_ring.data[head], KTERM_OUTPUT_PIPELINE_SIZE - head);
                if (tail > 0) term->output_sink(term->output_sink_ctx, &session->response_ring.data[0], tail);
            }
        } else if (term->response_callback) {
             // Old Callback (Flush before mode switch if any)
             // Typically we don't switch from callback to sink dynamically but good to be safe
             if (tail > head) {
                term->response_callback(term, &session->response_ring.data[head], tail - head);
            } else {
                term->response_callback(term, &session->response_ring.data[head], KTERM_OUTPUT_PIPELINE_SIZE - head);
                if (tail > 0) term->response_callback(term, &session->response_ring.data[0], tail);
            }
        }
        atomic_store_explicit(&session->response_ring.head, tail, memory_order_release);
    }
    term->output_sink = sink;
    term->output_sink_ctx = ctx;
}

void KTerm_SetPrinterCallback(KTerm* term, PrinterCallback callback) {
    term->printer_callback = callback;
}

void KTerm_SetTitleCallback(KTerm* term, TitleCallback callback) {
    term->title_callback = callback;
}

void KTerm_SetBellCallback(KTerm* term, BellCallback callback) {
    term->bell_callback = callback;
}

void KTerm_SetNotificationCallback(KTerm* term, NotificationCallback callback) {
    term->notification_callback = callback;
    term->notification_callback = callback;
}

#ifdef KTERM_ENABLE_GATEWAY
void KTerm_SetGatewayCallback(KTerm* term, GatewayCallback callback) {
    term->gateway_callback = callback;
}
#endif

void KTerm_SetSessionResizeCallback(KTerm* term, SessionResizeCallback callback) {
    term->session_resize_callback = callback;
}

const char* KTerm_GetWindowTitle(KTerm* term) {
    return GET_SESSION(term)->title.window_title;
}

const char* KTerm_GetIconTitle(KTerm* term) {
    return GET_SESSION(term)->title.icon_title;
}

void KTerm_SetMode(KTerm* term, const char* mode, bool enable) {
    KTermSession* session = GET_SESSION(term);
    if (strcmp(mode, "application_cursor") == 0) {
        if (enable) session->dec_modes |= KTERM_MODE_DECCKM; else session->dec_modes &= ~KTERM_MODE_DECCKM;
    } else if (strcmp(mode, "auto_wrap") == 0) {
        if (enable) session->dec_modes |= KTERM_MODE_DECAWM; else session->dec_modes &= ~KTERM_MODE_DECAWM;
    } else if (strcmp(mode, "origin") == 0) {
        if (enable) session->dec_modes |= KTERM_MODE_DECOM; else session->dec_modes &= ~KTERM_MODE_DECOM;
    } else if (strcmp(mode, "insert") == 0) {
        if (enable) session->dec_modes |= KTERM_MODE_INSERT; else session->dec_modes &= ~KTERM_MODE_INSERT;
    }
}

void KTerm_SetCursorShape(KTerm* term, CursorShape shape) {
    GET_SESSION(term)->cursor.shape = shape;
}

void KTerm_SetCursorKTermColor(KTerm* term, ExtendedKTermColor color) {
    GET_SESSION(term)->cursor.color = color;
}

void KTerm_SetMouseTracking(KTerm* term, MouseTrackingMode mode) {
    GET_SESSION(term)->mouse.mode = mode;
    GET_SESSION(term)->mouse.enabled = (mode != MOUSE_TRACKING_OFF);
}

// Enable or disable mouse features
// Toggles specific mouse functionalities based on feature name
void KTerm_EnableMouseFeature(KTerm* term, const char* feature, bool enable) {
    if (strcmp(feature, "focus") == 0) {
        // Enable/disable focus tracking for mouse reporting (CSI ?1004 h/l)
        GET_SESSION(term)->mouse.focus_tracking = enable;
    } else if (strcmp(feature, "sgr") == 0) {
        // Enable/disable SGR mouse reporting mode (CSI ?1006 h/l)
        GET_SESSION(term)->mouse.sgr_mode = enable;
        // Update mouse mode to SGR if enabled and tracking is active
        if (enable && GET_SESSION(term)->mouse.mode != MOUSE_TRACKING_OFF &&
            GET_SESSION(term)->mouse.mode != MOUSE_TRACKING_URXVT && GET_SESSION(term)->mouse.mode != MOUSE_TRACKING_PIXEL) {
            GET_SESSION(term)->mouse.mode = MOUSE_TRACKING_SGR;
        } else if (!enable && GET_SESSION(term)->mouse.mode == MOUSE_TRACKING_SGR) {
            GET_SESSION(term)->mouse.mode = MOUSE_TRACKING_VT200; // Fallback to VT200
        }
    } else if (strcmp(feature, "cursor") == 0) {
        // Enable/disable custom mouse cursor rendering
        GET_SESSION(term)->mouse.enabled = enable;
        if (!enable) {
            GET_SESSION(term)->mouse.cursor_x = -1; // Hide cursor
            GET_SESSION(term)->mouse.cursor_y = -1;
        }
    } else if (strcmp(feature, "urxvt") == 0) {
        // Enable/disable URXVT mouse reporting mode (CSI ?1015 h/l)
        if (enable) {
            GET_SESSION(term)->mouse.mode = MOUSE_TRACKING_URXVT;
            GET_SESSION(term)->mouse.enabled = true; // Ensure cursor is enabled
        } else if (GET_SESSION(term)->mouse.mode == MOUSE_TRACKING_URXVT) {
            GET_SESSION(term)->mouse.mode = MOUSE_TRACKING_OFF;
        }
    } else if (strcmp(feature, "pixel") == 0) {
        // Enable/disable pixel position mouse reporting mode (CSI ?1016 h/l)
        if (enable) {
            GET_SESSION(term)->mouse.mode = MOUSE_TRACKING_PIXEL;
            GET_SESSION(term)->mouse.enabled = true; // Ensure cursor is enabled
        } else if (GET_SESSION(term)->mouse.mode == MOUSE_TRACKING_PIXEL) {
            GET_SESSION(term)->mouse.mode = MOUSE_TRACKING_OFF;
        }
    }
}

void KTerm_EnableBracketedPaste(KTerm* term, bool enable) {
    GET_SESSION(term)->bracketed_paste.enabled = enable;
}

bool IsBracketedPasteActive(KTerm* term) {
    return GET_SESSION(term)->bracketed_paste.active;
}

void ProcessPasteData(KTerm* term, const char* data, size_t length) {
    (void)length;
    if (GET_SESSION(term)->bracketed_paste.enabled) {
        KTerm_WriteString(term, "\x1B[200~");
        KTerm_WriteString(term, data);
        KTerm_WriteString(term, "\x1B[201~");
    } else {
        KTerm_WriteString(term, data);
    }
}

static int EncodeUTF8(uint32_t codepoint, char* buffer) {
    if (codepoint <= 0x7F) {
        buffer[0] = (char)codepoint;
        return 1;
    } else if (codepoint <= 0x7FF) {
        buffer[0] = (char)(0xC0 | (codepoint >> 6));
        buffer[1] = (char)(0x80 | (codepoint & 0x3F));
        return 2;
    } else if (codepoint <= 0xFFFF) {
        buffer[0] = (char)(0xE0 | (codepoint >> 12));
        buffer[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buffer[2] = (char)(0x80 | (codepoint & 0x3F));
        return 3;
    } else if (codepoint <= 0x10FFFF) {
        buffer[0] = (char)(0xF0 | (codepoint >> 18));
        buffer[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        buffer[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buffer[3] = (char)(0x80 | (codepoint & 0x3F));
        return 4;
    }
    return 0;
}

void KTerm_CopySelectionToClipboard(KTerm* term) {
    if (!GET_SESSION(term)->selection.active) return;

    int start_y = GET_SESSION(term)->selection.start_y;
    int start_x = GET_SESSION(term)->selection.start_x;
    int end_y = GET_SESSION(term)->selection.end_y;
    int end_x = GET_SESSION(term)->selection.end_x;

    uint32_t s_idx = start_y * term->width + start_x;
    uint32_t e_idx = end_y * term->width + end_x;

    if (s_idx > e_idx) { uint32_t t = s_idx; s_idx = e_idx; e_idx = t; }

    size_t char_count = (e_idx - s_idx) + 1 + (term->height * 2);
    char* text_buf = KTerm_Calloc(char_count * 4, 1); // UTF-8 safety
    if (!text_buf) return;
    size_t buf_idx = 0;

    int last_y = -1;
    for (uint32_t i = s_idx; i <= e_idx; i++) {
        int cy = i / term->width;
        int cx = i % term->width;

        if (last_y != -1 && cy != last_y) {
            text_buf[buf_idx++] = '\n';
        }
        last_y = cy;

        EnhancedTermChar* cell = GetScreenCell(GET_SESSION(term), cy, cx);
        if (cell->ch) {
            buf_idx += EncodeUTF8(cell->ch, &text_buf[buf_idx]);
        }
    }
    text_buf[buf_idx] = '\0';
    KTerm_SetClipboardText(text_buf);
    KTerm_Free(text_buf);
}

void KTerm_SetPipelineTargetFPS(KTerm* term, int fps) {
    if (fps > 0) {
        GET_SESSION(term)->VTperformance.target_frame_time = 1.0 / fps;
        GET_SESSION(term)->VTperformance.time_budget = GET_SESSION(term)->VTperformance.target_frame_time * 0.3;
    }
}

void KTerm_SetPipelineTimeBudget(KTerm* term, double pct) {
    if (pct > 0.0 && pct <= 1.0) {
        GET_SESSION(term)->VTperformance.time_budget = GET_SESSION(term)->VTperformance.target_frame_time * pct;
    }
}


void KTerm_ShowDiagnostics(KTerm* term) {
    KTermStatus status = KTerm_GetStatus(term);
    KTerm_WriteFormat(term, "=== Buffer Diagnostics ===\n");
    KTerm_WriteFormat(term, "Pipeline: %zu/%d bytes\n", status.pipeline_usage, (int)sizeof(GET_SESSION(term)->input_pipeline));
    KTerm_WriteFormat(term, "Keyboard: %zu events\n", status.key_usage);
    KTerm_WriteFormat(term, "Overflow: %s\n", status.overflow_detected ? "YES" : "No");
    KTerm_WriteFormat(term, "Avg Process Time: %.6f ms\n", status.avg_process_time * 1000.0);
}

void KTerm_SwapScreenBuffer(KTerm* term) {
    KTermSession* session = GET_SESSION(term);
    // Swap pointers
    EnhancedTermChar* temp_buf = GET_SESSION(term)->screen_buffer;
    GET_SESSION(term)->screen_buffer = GET_SESSION(term)->alt_buffer;
    GET_SESSION(term)->alt_buffer = temp_buf;

    // Swap dimensions/metadata
    // For now, only buffer_height differs (Main has scrollback, Alt usually doesn't).
    // But since we allocate Alt buffer with term->height, we must handle this.
    // However, if we swap, the "active" buffer height must reflect what we are pointing to.

    // We didn't add "alt_buffer_height" to the struct, assuming Alt is always screen size.
    // But if we swap, GET_SESSION(term)->screen_buffer now points to a small buffer.
    // So GET_SESSION(term)->buffer_height must be updated.

    // Problem: We need to store the height of the buffer currently in 'alt_buffer' so we can restore it.
    // Let's assume standard behavior:
    // Main Buffer: Has scrollback (Large).
    // Alt Buffer: No scrollback (Screen Height).

    // Swap heads
    int temp_head = GET_SESSION(term)->screen_head;
    GET_SESSION(term)->screen_head = GET_SESSION(term)->alt_screen_head;
    GET_SESSION(term)->alt_screen_head = temp_head;

    if ((session->dec_modes & KTERM_MODE_ALTSCREEN)) {
        // Switching BACK to Main Screen
        GET_SESSION(term)->buffer_height = term->height + MAX_SCROLLBACK_LINES;
        session->dec_modes &= ~KTERM_MODE_ALTSCREEN;

        // Restore view offset (if we want to restore scroll position, otherwise 0)
        // For now, reset to 0 (bottom) is safest and standard behavior.
        // Or restore saved? Let's restore saved for better UX.
        GET_SESSION(term)->view_offset = GET_SESSION(term)->saved_view_offset;
    } else {
        // Switching TO Alternate Screen
        GET_SESSION(term)->buffer_height = term->height;
        session->dec_modes |= KTERM_MODE_ALTSCREEN;

        // Save current offset and reset view for Alt screen (which has no scrollback)
        GET_SESSION(term)->saved_view_offset = GET_SESSION(term)->view_offset;
        GET_SESSION(term)->view_offset = 0;
    }

    // Force full redraw
    for (int i=0; i<term->height; i++) {
        GET_SESSION(term)->row_dirty[i] = KTERM_DIRTY_FRAMES;
    }
}

static void KTerm_ProcessEventsInternal(KTerm* term, KTermSession* session) {
    // Load tail relaxed (only this thread writes to it)
    int current_tail = atomic_load_explicit(&session->pipeline_tail, memory_order_relaxed);
    // Load head acquire (another thread writes to it)
    int current_head = atomic_load_explicit(&session->pipeline_head, memory_order_acquire);

    if (current_head == current_tail) {
        return;
    }

    // Capture the index of the session OWNING this buffer
    // int processing_session_idx = term->active_session;

    double start_time = KTerm_TimerGetTime();
    int chars_processed = 0;
    int target_chars = session->VTperformance.chars_per_frame;

    int pipeline_usage = (current_head - current_tail + sizeof(session->input_pipeline)) % sizeof(session->input_pipeline);

    if (session->dec_modes & KTERM_MODE_DECXRLM) {
        int usage_percent = (pipeline_usage * 100) / (int)sizeof(session->input_pipeline);
        if (usage_percent > 75 && !session->xoff_sent) {
            KTerm_QueueResponseBytes(term, "\x13", 1); // XOFF
            session->xoff_sent = true;
        } else if (usage_percent < 25 && session->xoff_sent) {
            KTerm_QueueResponseBytes(term, "\x11", 1); // XON
            session->xoff_sent = false;
        }
    }

    if (pipeline_usage > session->VTperformance.burst_threshold) {
        target_chars *= 2;
        session->VTperformance.burst_mode = true;
    } else if (pipeline_usage < target_chars) {
        target_chars = pipeline_usage;
        session->VTperformance.burst_mode = false;
    }

    while (chars_processed < target_chars) {
        // Re-check for empty (we are consuming)
        // Note: current_head is a snapshot. If producer added more, we will process them next frame.
        if (current_tail == current_head) break;

        if (KTerm_TimerGetTime() - start_time > session->VTperformance.time_budget) {
            break;
        }

        unsigned char ch = session->input_pipeline[current_tail];
        int next_tail = (current_tail + 1) % sizeof(session->input_pipeline);

        // Process char.
        // Pass 'session' explicitly to avoid context fragility if active_session changes.
        KTerm_ProcessChar(term, session, ch);

        current_tail = next_tail;
        // Store tail release (publishes free space)
        atomic_store_explicit(&session->pipeline_tail, current_tail, memory_order_release);

        chars_processed++;
    }

    // Update performance metrics
    if (chars_processed > 0) {
        double total_time = KTerm_TimerGetTime() - start_time;
        double time_per_char = total_time / chars_processed;
        session->VTperformance.avg_process_time =
            session->VTperformance.avg_process_time * 0.9 + time_per_char * 0.1;
    }
}

void KTerm_ProcessEvents(KTerm* term) {
    KTerm_ProcessEventsInternal(term, GET_SESSION(term));
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

void KTerm_SetErrorCallback(KTerm* term, KTermErrorCallback callback, void* user_data) {
    if (!term) return;
    term->error_callback = callback;
    term->error_user_data = user_data;
}

void KTerm_ReportError(KTerm* term, KTermErrorLevel level, KTermErrorSource source, const char* format, ...) {
    if (!term) return;

    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (term->error_callback) {
        term->error_callback(term, level, source, buffer, term->error_user_data);
    } else {
        // Fallback: Print to stderr if level is ERROR or FATAL, or if generic debugging is on
        bool debug = false;
        // Basic check to see if session 0 might be initialized
        if (term->sessions[0].screen_buffer) {
             debug = term->sessions[0].status.debugging;
        }

        if (level >= KTERM_LOG_ERROR || debug) {
            fprintf(stderr, "[KTerm] %s\n", buffer);
        }
    }
}

void KTerm_LogUnsupportedSequence(KTerm* term, const char* sequence) {
    // 1. Report via new Error API
    KTerm_ReportError(term, KTERM_LOG_WARNING, KTERM_SOURCE_PARSER, "Unsupported Sequence: %s", sequence);

    KTermSession* session = GET_SESSION(term);
    if (!GET_SESSION(term)->options.log_unsupported) return;

    session->conformance.compliance.unsupported_sequences++;

    // Use strcpy instead of strncpy to avoid truncation warnings
    size_t len = strlen(sequence);
    if (len >= sizeof(session->conformance.compliance.last_unsupported)) {
        len = sizeof(session->conformance.compliance.last_unsupported) - 1;
    }
    memcpy(session->conformance.compliance.last_unsupported, sequence, len);
    session->conformance.compliance.last_unsupported[len] = '\0';

    if (GET_SESSION(term)->options.debug_sequences) {
        char debug_msg[128];
        snprintf(debug_msg, sizeof(debug_msg),
                "Unsupported: %s (total: %d)\n",
                sequence, session->conformance.compliance.unsupported_sequences);

        if (term->response_callback) {
            term->response_callback(term, debug_msg, strlen(debug_msg));
        }
    }
}

// =============================================================================
// PARAMETER PARSING UTILITIES
// =============================================================================

static int KTerm_ParseCSIParams_Internal(KTermSession* session, const char* params, int* out_params, int max_params) {
    session->param_count = 0;
    memset(session->escape_params, 0, sizeof(session->escape_params));
    memset(session->escape_separators, 0, sizeof(session->escape_separators));

    if (!params || !*params) {
        return 0;
    }

    StreamScanner scanner = { .ptr = params, .len = strlen(params), .pos = 0 };
    char prefix = Stream_Peek(&scanner);
    if (prefix == '?' || prefix == '>' || prefix == '<' || prefix == '=') {
        Stream_Consume(&scanner);
    }

    while (scanner.pos < scanner.len && session->param_count < max_params) {
        int value = 0;
        if (Stream_ReadInt(&scanner, &value)) {
            session->escape_params[session->param_count] = (value >= 0) ? value : 0;
        } else {
            // Default 0 if missing or invalid (e.g. empty between semicolons)
            session->escape_params[session->param_count] = 0;
            // Robustness: Skip non-numeric garbage until next separator or EOF
            while (scanner.pos < scanner.len) {
                char p = Stream_Peek(&scanner);
                if (p == ';' || p == ':' || p == '\0') break;
                Stream_Consume(&scanner);
            }
        }
        char sep = Stream_Peek(&scanner);
        if (sep == ';' || sep == ':') {
            session->escape_separators[session->param_count] = sep;
            Stream_Consume(&scanner);
        } else {
            session->escape_separators[session->param_count] = 0;
        }

        session->param_count++;

        if (sep == ';' || sep == ':') {
            // Handle trailing separator implying a default param
            if (scanner.pos >= scanner.len && session->param_count < max_params) {
                 session->escape_params[session->param_count] = 0;
                 session->escape_separators[session->param_count] = 0;
                 session->param_count++;
            }
        } else {
            break;
        }
    }

    if (out_params) {
        for (int i = 0; i < session->param_count; i++) {
            out_params[i] = session->escape_params[i];
        }
    }

    return session->param_count;
}

int KTerm_ParseCSIParams(KTerm* term, const char* params, int* out_params, int max_params) {
    return KTerm_ParseCSIParams_Internal(GET_SESSION(term), params, out_params, max_params);
}

static void ClearCSIParams(KTermSession* session) {
    session->escape_buffer[0] = '\0';
    session->escape_pos = 0;
    session->param_count = 0;
    memset(session->escape_params, 0, sizeof(session->escape_params));
}

void KTerm_ProcessSixelSTChar(KTerm* term, KTermSession* session, unsigned char ch) {
    if (ch == '\\') { // This is ST
        session->parse_state = VT_PARSE_NORMAL;
        // Finalize sixel image size
        session->sixel.width = session->sixel.max_x;
        session->sixel.height = session->sixel.max_y;
        session->sixel.dirty = true;

        // Handle Cursor Placement (Mode 8452 & Standard Sixel)
        int cw = term->char_width;
        int ch_h = term->char_height;
        if (cw <= 0) cw = 8;
        if (ch_h <= 0) ch_h = 10;

        if ((session->dec_modes & KTERM_MODE_SIXEL_CURSOR)) {
             // Mode 8452 Enabled: Place cursor at end of graphic (right side)
             int cols = (session->sixel.width + cw - 1) / cw;
             session->cursor.x = (session->sixel.x / cw) + cols;
             if (session->cursor.x >= term->width) session->cursor.x = term->width - 1;
        } else {
             // Standard Behavior: Carriage Return + Line Feed (below image)
             int rows = (session->sixel.height + ch_h - 1) / ch_h;
             int start_y = session->sixel.y / ch_h;
             int target_y = start_y + rows;

             session->cursor.x = 0; // CR

             // Handle Scrolling
             int bottom = session->scroll_bottom;
             if (target_y > bottom) {
                 int scroll_lines = target_y - bottom;
                 KTerm_ScrollUpRegion_Internal(term, session, session->scroll_top, session->scroll_bottom, scroll_lines);
                 target_y = bottom;
             }
             session->cursor.y = target_y;
             if (session->cursor.y >= term->height) session->cursor.y = term->height - 1;
        }
    } else {
        // ESC was start of new sequence
        // We treat the current char 'ch' as the one following ESC.
        // e.g. ESC P -> ch='P'. KTerm_ProcessEscapeChar(term, 'P') -> PARSE_DCS.
        KTerm_ProcessEscapeChar(term, session, ch);
    }
}

static int KTerm_GetCSIParam_Internal(KTermSession* session, int index, int default_value) {
    if (index >= 0 && index < session->param_count) {
        return (session->escape_params[index] == 0) ? default_value : session->escape_params[index];
    }
    return default_value;
}

int KTerm_GetCSIParam(KTerm* term, KTermSession* session, int index, int default_value) {
    if (!session) session = GET_SESSION(term);
    return KTerm_GetCSIParam_Internal(session, index, default_value);
}


// =============================================================================
// CURSOR MOVEMENT IMPLEMENTATIONS
// =============================================================================

static void ExecuteCUU_Internal(KTermSession* session) { // Cursor Up
    int n = KTerm_GetCSIParam_Internal(session, 0, 1);
    int new_y = session->cursor.y - n;

    if ((session->dec_modes & KTERM_MODE_DECOM)) {
        session->cursor.y = (new_y < session->scroll_top) ? session->scroll_top : new_y;
    } else {
        session->cursor.y = (new_y < 0) ? 0 : new_y;
    }
}
void ExecuteCUU(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); ExecuteCUU_Internal(session); }

static void ExecuteCUD_Internal(KTermSession* session) { // Cursor Down
    int n = KTerm_GetCSIParam_Internal(session, 0, 1);
    int new_y = session->cursor.y + n;

    if ((session->dec_modes & KTERM_MODE_DECOM)) {
        session->cursor.y = (new_y > session->scroll_bottom) ? session->scroll_bottom : new_y;
    } else {
        session->cursor.y = (new_y >= session->rows) ? session->rows - 1 : new_y;
    }
}
void ExecuteCUD(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); ExecuteCUD_Internal(session); }

static void ExecuteCUF_Internal(KTermSession* session) { // Cursor Forward
    int n = KTerm_GetCSIParam_Internal(session, 0, 1);
    session->cursor.x = (session->cursor.x + n >= session->cols) ? session->cols - 1 : session->cursor.x + n;
}
void ExecuteCUF(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); ExecuteCUF_Internal(session); }

static void ExecuteCUB_Internal(KTermSession* session) { // Cursor Back
    int n = KTerm_GetCSIParam_Internal(session, 0, 1);
    session->cursor.x = (session->cursor.x - n < 0) ? 0 : session->cursor.x - n;
}
void ExecuteCUB(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); ExecuteCUB_Internal(session); }

static void ExecuteCNL_Internal(KTermSession* session) { // Cursor Next Line
    int n = KTerm_GetCSIParam_Internal(session, 0, 1);
    session->cursor.y = (session->cursor.y + n >= session->rows) ? session->rows - 1 : session->cursor.y + n;
    session->cursor.x = session->left_margin;
}
void ExecuteCNL(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); ExecuteCNL_Internal(session); }

static void ExecuteCPL_Internal(KTermSession* session) { // Cursor Previous Line
    int n = KTerm_GetCSIParam_Internal(session, 0, 1);
    session->cursor.y = (session->cursor.y - n < 0) ? 0 : session->cursor.y - n;
    session->cursor.x = session->left_margin;
}
void ExecuteCPL(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); ExecuteCPL_Internal(session); }

static void ExecuteCHA_Internal(KTermSession* session) { // Cursor Horizontal Absolute
    int n = KTerm_GetCSIParam_Internal(session, 0, 1) - 1; // Convert to 0-based
    session->cursor.x = (n < 0) ? 0 : (n >= session->cols) ? session->cols - 1 : n;
}
void ExecuteCHA(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); ExecuteCHA_Internal(session); }

static void ExecuteCUP_Internal(KTermSession* session) { // Cursor Position
    int row = KTerm_GetCSIParam_Internal(session, 0, 1) - 1; // Convert to 0-based
    int col = KTerm_GetCSIParam_Internal(session, 1, 1) - 1;

    if ((session->dec_modes & KTERM_MODE_DECOM)) {
        row += session->scroll_top;
        col += session->left_margin;
    }

    session->cursor.y = (row < 0) ? 0 : (row >= session->rows) ? session->rows - 1 : row;
    session->cursor.x = (col < 0) ? 0 : (col >= session->cols) ? session->cols - 1 : col;

    // Clamp to scrolling region if in origin mode
    if ((session->dec_modes & KTERM_MODE_DECOM)) {
        if (session->cursor.y < session->scroll_top) session->cursor.y = session->scroll_top;
        if (session->cursor.y > session->scroll_bottom) session->cursor.y = session->scroll_bottom;
        if (session->cursor.x < session->left_margin) session->cursor.x = session->left_margin;
        if (session->cursor.x > session->right_margin) session->cursor.x = session->right_margin;
    }
}
void ExecuteCUP(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); ExecuteCUP_Internal(session); }

static void ExecuteVPA_Internal(KTermSession* session) { // Vertical Position Absolute
    int n = KTerm_GetCSIParam_Internal(session, 0, 1) - 1; // Convert to 0-based

    if ((session->dec_modes & KTERM_MODE_DECOM)) {
        n += session->scroll_top;
        session->cursor.y = (n < session->scroll_top) ? session->scroll_top :
                           (n > session->scroll_bottom) ? session->scroll_bottom : n;
    } else {
        session->cursor.y = (n < 0) ? 0 : (n >= session->rows) ? session->rows - 1 : n;
    }
}
void ExecuteVPA(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); ExecuteVPA_Internal(session); }

// =============================================================================
// ERASING IMPLEMENTATIONS
// =============================================================================

static void ExecuteED_Internal(KTerm* term, KTermSession* session, bool private_mode) { // Erase in Display
    int n = KTerm_GetCSIParam_Internal(session, 0, 0);

    switch (n) {
        case 0: // Clear from cursor to end of screen
            // Clear current line from cursor
            for (int x = session->cursor.x; x < term->width; x++) {
                EnhancedTermChar* cell = GetActiveScreenCell(session, session->cursor.y, x);
                if (private_mode && (cell->flags & KTERM_ATTR_PROTECTED)) continue;
                KTerm_ClearCell_Internal(session, cell);
            }
            // Clear remaining lines
            for (int y = session->cursor.y + 1; y < term->height; y++) {
                for (int x = 0; x < term->width; x++) {
                    EnhancedTermChar* cell = GetActiveScreenCell(session, y, x);
                    if (private_mode && (cell->flags & KTERM_ATTR_PROTECTED)) continue;
                    KTerm_ClearCell_Internal(session, cell);
                }
            }
            break;

        case 1: // Clear from beginning of screen to cursor
            // Clear lines before cursor
            for (int y = 0; y < GET_SESSION(term)->cursor.y; y++) {
                for (int x = 0; x < term->width; x++) {
                    EnhancedTermChar* cell = GetActiveScreenCell(GET_SESSION(term), y, x);
                    if (private_mode && (cell->flags & KTERM_ATTR_PROTECTED)) continue;
                    KTerm_ClearCell(term, cell);
                }
            }
            // Clear current line up to cursor
            for (int x = 0; x <= GET_SESSION(term)->cursor.x; x++) {
                EnhancedTermChar* cell = GetActiveScreenCell(GET_SESSION(term), GET_SESSION(term)->cursor.y, x);
                if (private_mode && (cell->flags & KTERM_ATTR_PROTECTED)) continue;
                KTerm_ClearCell(term, cell);
            }
            break;

        case 2: // Clear entire screen
            for (int y = 0; y < term->height; y++) {
                for (int x = 0; x < term->width; x++) {
                    EnhancedTermChar* cell = GetActiveScreenCell(session, y, x);
                    if (private_mode && (cell->flags & KTERM_ATTR_PROTECTED)) continue;
                    KTerm_ClearCell(term, cell);
                }
            }
            if (session->conformance.level == VT_LEVEL_ANSI_SYS) {
                session->cursor.x = 0;
                session->cursor.y = 0;
            }
            break;

        case 3: // Clear entire screen and scrollback (xterm extension)
            for (int i = 0; i < session->buffer_height * session->cols; i++) {
                 if (private_mode && (session->screen_buffer[i].flags & KTERM_ATTR_PROTECTED)) continue;
                 KTerm_ClearCell(term, &session->screen_buffer[i]);
            }
            // Mark all rows dirty
            for(int r=0; r<session->rows; r++) session->row_dirty[r] = KTERM_DIRTY_FRAMES;
            break;

        default:
            KTerm_LogUnsupportedSequence(term, "Unknown ED parameter");
            break;
    }
}
void ExecuteED(KTerm* term, bool private_mode) { ExecuteED_Internal(term, GET_SESSION(term), private_mode); }

static void ExecuteEL_Internal(KTerm* term, KTermSession* session, bool private_mode) { // Erase in Line
    int n = KTerm_GetCSIParam_Internal(session, 0, 0);

    switch (n) {
        case 0: // Clear from cursor to end of line
            for (int x = session->cursor.x; x < term->width; x++) {
                EnhancedTermChar* cell = GetActiveScreenCell(session, session->cursor.y, x);
                if (private_mode && (cell->flags & KTERM_ATTR_PROTECTED)) continue;
                KTerm_ClearCell_Internal(session, cell);
            }
            break;

        case 1: // Clear from beginning of line to cursor
            for (int x = 0; x <= session->cursor.x; x++) {
                EnhancedTermChar* cell = GetActiveScreenCell(session, session->cursor.y, x);
                if (private_mode && (cell->flags & KTERM_ATTR_PROTECTED)) continue;
                KTerm_ClearCell_Internal(session, cell);
            }
            break;

        case 2: // Clear entire line
            for (int x = 0; x < term->width; x++) {
                EnhancedTermChar* cell = GetActiveScreenCell(session, session->cursor.y, x);
                if (private_mode && (cell->flags & KTERM_ATTR_PROTECTED)) continue;
                KTerm_ClearCell_Internal(session, cell);
            }
            break;

        default:
            KTerm_LogUnsupportedSequence(term, "Unknown EL parameter");
            break;
    }
}
void ExecuteEL(KTerm* term, bool private_mode) { ExecuteEL_Internal(term, GET_SESSION(term), private_mode); }

static void ExecuteECH_Internal(KTerm* term, KTermSession* session) { // Erase Character
    (void)term;
    int n = KTerm_GetCSIParam_Internal(session, 0, 1);

    for (int i = 0; i < n && session->cursor.x + i < term->width; i++) {
        KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, session->cursor.y, session->cursor.x + i));
    }
}
void ExecuteECH(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); ExecuteECH_Internal(term, session); }

// =============================================================================
// INSERTION AND DELETION IMPLEMENTATIONS
// =============================================================================

static void ExecuteIL_Internal(KTerm* term, KTermSession* session) { // Insert Line
    int n = KTerm_GetCSIParam_Internal(session, 0, 1);
    KTerm_QueueInsertLines(session, n, true);
}
void ExecuteIL(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); ExecuteIL_Internal(term, session); }

static void ExecuteDL_Internal(KTerm* term, KTermSession* session) { // Delete Line
    int n = KTerm_GetCSIParam_Internal(session, 0, 1);
    KTerm_QueueDeleteLines(session, n, true);
}
void ExecuteDL(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); ExecuteDL_Internal(term, session); }

void ExecuteICH(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Insert Character
    int n = KTerm_GetCSIParam(term, session, 0, 1);
    KTerm_InsertCharactersAt(term, session->cursor.y, session->cursor.x, n);
}

void ExecuteDCH(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Delete Character
    int n = KTerm_GetCSIParam(term, session, 0, 1);
    KTerm_DeleteCharactersAt(term, session->cursor.y, session->cursor.x, n);
}

void ExecuteREP(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Repeat Preceding Graphic Character
    int n = KTerm_GetCSIParam(term, session, 0, 1);
    if (n < 1) n = 1;
    if (session->last_char > 0) {
        int width = 1;
        if (session->enable_wide_chars && *session->charset.gl == CHARSET_UTF8) {
             width = KTerm_wcwidth(session->last_char);
             if (width < 0) width = 1;
        }

        for (int i = 0; i < n; i++) {
            if ((session->dec_modes & KTERM_MODE_DECAWM)) {
                if (session->cursor.x + width - 1 > session->right_margin) {
                    session->cursor.x = session->left_margin;
                    session->cursor.y++;
                    if (session->cursor.y > session->scroll_bottom) {
                        session->cursor.y = session->scroll_bottom;
                        KTermRect r = {0, session->scroll_top, session->cols, session->scroll_bottom - session->scroll_top + 1}; KTerm_QueueScrollRegion(session, r, 1);
                    }
                }
            } else {
                if (session->cursor.x > session->right_margin) {
                    session->cursor.x = session->right_margin;
                }
            }
            KTerm_InsertCharacterAtCursor_Internal(term, session, session->last_char, width);
            session->cursor.x += width;
        }
    }
}

// =============================================================================
// SCROLLING IMPLEMENTATIONS
// =============================================================================

void ExecuteSU(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Scroll Up
    int n = KTerm_GetCSIParam(term, session, 0, 1);
    KTermRect r = {0, session->scroll_top, session->cols, session->scroll_bottom - session->scroll_top + 1}; KTerm_QueueScrollRegion(session, r, n);
}

void ExecuteSD(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Scroll Down
    int n = KTerm_GetCSIParam(term, session, 0, 1);
    KTermRect r = {0, session->scroll_top, session->cols, session->scroll_bottom - session->scroll_top + 1}; KTerm_QueueScrollRegion(session, r, -n);
}

// =============================================================================
// ENHANCED SGR (SELECT GRAPHIC RENDITION) IMPLEMENTATION
// =============================================================================

int ProcessExtendedKTermColor(KTerm* term, ExtendedKTermColor* color, int param_index) {
    int consumed = 0;

    if (param_index + 1 < GET_SESSION(term)->param_count) {
        int color_type = GET_SESSION(term)->escape_params[param_index + 1];

        if (color_type == 5 && param_index + 2 < GET_SESSION(term)->param_count) {
            // 256-color mode: ESC[38;5;n or ESC[48;5;n
            int color_index = GET_SESSION(term)->escape_params[param_index + 2];
            if (color_index >= 0 && color_index < 256) {
                color->color_mode = 0;
                color->value.index = color_index;
            }
            consumed = 2;

        } else if (color_type == 2 && param_index + 4 < GET_SESSION(term)->param_count) {
            // True color mode: ESC[38;2;r;g;b or ESC[48;2;r;g;b
            int r = GET_SESSION(term)->escape_params[param_index + 2] & 0xFF;
            int g = GET_SESSION(term)->escape_params[param_index + 3] & 0xFF;
            int b = GET_SESSION(term)->escape_params[param_index + 4] & 0xFF;

            color->color_mode = 1;
            color->value.rgb = (RGB_KTermColor){r, g, b, 255};
            consumed = 4;
        }
    }

    return consumed;
}

void ExecuteXTPUSHSGR(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    KTermSession* s = session;
    if (s->sgr_stack_depth < 10) {
        SavedSGRState* state = &s->sgr_stack[s->sgr_stack_depth++];
        state->fg_color = s->current_fg;
        state->bg_color = s->current_bg;
        state->ul_color = s->current_ul_color;
        state->st_color = s->current_st_color;
        state->attributes = s->current_attributes;
    }
}

void ExecuteXTPOPSGR(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    KTermSession* s = session;
    if (s->sgr_stack_depth > 0) {
        SavedSGRState* state = &s->sgr_stack[--s->sgr_stack_depth];
        s->current_fg = state->fg_color;
        s->current_bg = state->bg_color;
        s->current_ul_color = state->ul_color;
        s->current_st_color = state->st_color;
        s->current_attributes = state->attributes;
    }
}

void KTerm_ResetAllAttributes(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    session->current_fg.color_mode = 0;
    session->current_fg.value.index = COLOR_WHITE;
    session->current_bg.color_mode = 0;
    session->current_bg.value.index = COLOR_BLACK;
    session->current_ul_color.color_mode = 2; // Default
    session->current_st_color.color_mode = 2; // Default
    session->current_attributes = 0;
}

void ExecuteSGR(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    if (session->param_count == 0) {
        // Reset all attributes
        KTerm_ResetAllAttributes(term, session);
        return;
    }

    bool ansi_restricted = (session->conformance.level == VT_LEVEL_ANSI_SYS);

    for (int i = 0; i < session->param_count; i++) {
        int param = session->escape_params[i];

        switch (param) {
            case 0: // Reset all
                KTerm_ResetAllAttributes(term, session);
                break;

            // Intensity
            case 1: session->current_attributes |= KTERM_ATTR_BOLD; break;
            case 2: if (!ansi_restricted) session->current_attributes |= KTERM_ATTR_FAINT; break;
            case 22: session->current_attributes &= ~(KTERM_ATTR_BOLD | KTERM_ATTR_FAINT | KTERM_ATTR_FAINT_BG); break;

            // Style
            case 3: if (!ansi_restricted) session->current_attributes |= KTERM_ATTR_ITALIC; break;
            case 23: if (!ansi_restricted) session->current_attributes &= ~KTERM_ATTR_ITALIC; break;

            case 4:
                // SGR 4: Underline (with optional subparameters for style)
                // 4:0=None, 4:1=Single, 4:2=Double, 4:3=Curly, 4:4=Dotted, 4:5=Dashed
                if (session->escape_separators[i] == ':') {
                    if (i + 1 < session->param_count) {
                        int style = session->escape_params[i+1];
                        i++; // Consume subparam

                        // Clear existing UL bits and style
                        session->current_attributes &= ~(KTERM_ATTR_UNDERLINE | KTERM_ATTR_DOUBLE_UNDERLINE | KTERM_ATTR_UL_STYLE_MASK);

                        switch (style) {
                            case 0: break; // None (already cleared)
                            case 1: session->current_attributes |= KTERM_ATTR_UNDERLINE | KTERM_ATTR_UL_STYLE_SINGLE; break;
                            case 2: session->current_attributes |= KTERM_ATTR_DOUBLE_UNDERLINE | KTERM_ATTR_UL_STYLE_DOUBLE; break;
                            case 3: session->current_attributes |= KTERM_ATTR_UNDERLINE | KTERM_ATTR_UL_STYLE_CURLY; break;
                            case 4: session->current_attributes |= KTERM_ATTR_UNDERLINE | KTERM_ATTR_UL_STYLE_DOTTED; break;
                            case 5: session->current_attributes |= KTERM_ATTR_UNDERLINE | KTERM_ATTR_UL_STYLE_DASHED; break;
                            default: session->current_attributes |= KTERM_ATTR_UNDERLINE | KTERM_ATTR_UL_STYLE_SINGLE; break; // Fallback
                        }
                    } else {
                        // Trailing colon? Assume single.
                        session->current_attributes |= KTERM_ATTR_UNDERLINE | KTERM_ATTR_UL_STYLE_SINGLE;
                    }
                } else {
                    // Standard SGR 4 (Single)
                    session->current_attributes &= ~KTERM_ATTR_UL_STYLE_MASK;
                    session->current_attributes |= KTERM_ATTR_UNDERLINE | KTERM_ATTR_UL_STYLE_SINGLE;
                }
                break;

            case 21: if (!ansi_restricted) session->current_attributes |= KTERM_ATTR_DOUBLE_UNDERLINE | KTERM_ATTR_UL_STYLE_DOUBLE; break;
            case 24: session->current_attributes &= ~(KTERM_ATTR_UNDERLINE | KTERM_ATTR_DOUBLE_UNDERLINE | KTERM_ATTR_UL_STYLE_MASK); break;

            case 5:
                // SGR 5: Slow Blink (Standard)
                session->current_attributes |= KTERM_ATTR_BLINK_SLOW;
                session->current_attributes &= ~KTERM_ATTR_BLINK;
                break;
            case 6:
                // SGR 6: Rapid Blink (Standard)
                if (!ansi_restricted) {
                    session->current_attributes |= KTERM_ATTR_BLINK;
                    session->current_attributes &= ~KTERM_ATTR_BLINK_SLOW;
                }
                break;
            case 25:
                session->current_attributes &= ~(KTERM_ATTR_BLINK | KTERM_ATTR_BLINK_BG | KTERM_ATTR_BLINK_SLOW);
                break;

            case 7: session->current_attributes |= KTERM_ATTR_REVERSE; break;
            case 27: session->current_attributes &= ~KTERM_ATTR_REVERSE; break;

            case 8: session->current_attributes |= KTERM_ATTR_CONCEAL; break;
            case 28: session->current_attributes &= ~KTERM_ATTR_CONCEAL; break;

            case 9: if (!ansi_restricted) session->current_attributes |= KTERM_ATTR_STRIKE; break;
            case 29: if (!ansi_restricted) session->current_attributes &= ~KTERM_ATTR_STRIKE; break;

            case 53: if (!ansi_restricted) session->current_attributes |= KTERM_ATTR_OVERLINE; break;
            case 55: if (!ansi_restricted) session->current_attributes &= ~KTERM_ATTR_OVERLINE; break;

            case 51: if (!ansi_restricted) session->current_attributes |= KTERM_ATTR_FRAMED; break;
            case 52: if (!ansi_restricted) session->current_attributes |= KTERM_ATTR_ENCIRCLED; break;
            case 54: if (!ansi_restricted) session->current_attributes &= ~(KTERM_ATTR_FRAMED | KTERM_ATTR_ENCIRCLED); break;

            case 73:
                if (!ansi_restricted) {
                    session->current_attributes |= KTERM_ATTR_SUPERSCRIPT;
                    session->current_attributes &= ~KTERM_ATTR_SUBSCRIPT;
                }
                break;
            case 74:
                if (!ansi_restricted) {
                    session->current_attributes |= KTERM_ATTR_SUBSCRIPT;
                    session->current_attributes &= ~KTERM_ATTR_SUPERSCRIPT;
                }
                break;
            case 75: if (!ansi_restricted) session->current_attributes &= ~(KTERM_ATTR_SUPERSCRIPT | KTERM_ATTR_SUBSCRIPT); break;

            // Standard colors (30-37, 40-47)
            case 30: case 31: case 32: case 33:
            case 34: case 35: case 36: case 37:
                session->current_fg.color_mode = 0;
                session->current_fg.value.index = param - 30;
                break;

            case 40: case 41: case 42: case 43:
            case 44: case 45: case 46: case 47:
                session->current_bg.color_mode = 0;
                session->current_bg.value.index = param - 40;
                break;

            // Bright colors (90-97, 100-107)
            case 90: case 91: case 92: case 93:
            case 94: case 95: case 96: case 97:
                if (!ansi_restricted) {
                    session->current_fg.color_mode = 0;
                    session->current_fg.value.index = param - 90 + 8;
                }
                break;

            case 100: case 101: case 102: case 103:
            case 104: case 105: case 106: case 107:
                if (!ansi_restricted) {
                    session->current_bg.color_mode = 0;
                    session->current_bg.value.index = param - 100 + 8;
                }
                break;

            case 62: if (!ansi_restricted) session->current_attributes |= KTERM_ATTR_FAINT_BG; break;
            case 66: if (!ansi_restricted) session->current_attributes |= KTERM_ATTR_BLINK_BG; break;

            // Extended colors
            case 38: // Set foreground color
                if (!ansi_restricted) i += ProcessExtendedKTermColor(term, &session->current_fg, i);
                else {
                    // Skip parameters
                    // This is complex because we need to parse sub-parameters.
                    // For now, just skip.
                }
                break;

            case 48: // Set background color
                if (!ansi_restricted) i += ProcessExtendedKTermColor(term, &session->current_bg, i);
                break;

            case 58: // Set underline color
                if (!ansi_restricted) i += ProcessExtendedKTermColor(term, &session->current_ul_color, i);
                break;

            case 59: // Reset underline color
                if (!ansi_restricted) session->current_ul_color.color_mode = 2; // Default
                break;

            // Default colors
            case 39:
                session->current_fg.color_mode = 0;
                session->current_fg.value.index = COLOR_WHITE;
                break;

            case 49:
                session->current_bg.color_mode = 0;
                session->current_bg.value.index = COLOR_BLACK;
                break;

            default:
                if (session->options.debug_sequences) {
                    char debug_msg[64];
                    snprintf(debug_msg, sizeof(debug_msg), "Unknown SGR parameter: %d", param);
                    KTerm_LogUnsupportedSequence(term, debug_msg);
                }
                break;
        }
    }
}

// =============================================================================
// TERMINAL MODE IMPLEMENTATIONS
// =============================================================================

// Helper function to compute screen buffer checksum (for CSI ?63 n)
static uint32_t ComputeScreenChecksum(KTerm* term, int page) {
    (void)page;
    uint32_t checksum = 0;
    // Simple CRC16-like checksum for screen buffer
    for (int y = 0; y < term->height; y++) {
        for (int x = 0; x < term->width; x++) {
            EnhancedTermChar *cell = GetScreenCell(GET_SESSION(term), y, x);
            checksum += cell->ch;
            checksum += (cell->fg_color.color_mode == 0 ? cell->fg_color.value.index : (cell->fg_color.value.rgb.r << 16 | cell->fg_color.value.rgb.g << 8 | cell->fg_color.value.rgb.b));
            checksum += (cell->bg_color.color_mode == 0 ? cell->bg_color.value.index : (cell->bg_color.value.rgb.r << 16 | cell->bg_color.value.rgb.g << 8 | cell->bg_color.value.rgb.b));
            checksum = (checksum >> 16) + (checksum & 0xFFFF);
        }
    }
    return checksum & 0xFFFF;
}

static void SwitchScreenBuffer(KTerm* term, KTermSession* session, bool to_alternate) {
    if (!(GET_SESSION(term)->conformance.features & KTERM_FEATURE_ALTERNATE_SCREEN)) {
        KTerm_LogUnsupportedSequence(term, "Alternate screen not supported");
        return;
    }

    // In new Ring Buffer architecture, we swap buffers rather than copy.
    KTerm_SwapScreenBuffer(term);
    // KTerm_SwapScreenBuffer handles logic if implemented correctly.
    // However, this function `SwitchScreenBuffer` seems to enforce explicit "to_alternate" direction.
    // We should implement it using pointers.

    if (to_alternate && !(session->dec_modes & KTERM_MODE_ALTSCREEN)) {
        KTerm_SwapScreenBuffer(term); // Swaps to alt
    } else if (!to_alternate && (session->dec_modes & KTERM_MODE_ALTSCREEN)) {
        KTerm_SwapScreenBuffer(term); // Swaps back to main
    }
}

// Set terminal modes internally
// Configures DEC private modes (CSI ? Pm h/l) and ANSI modes (CSI Pm h/l)
static void KTerm_SetModeInternal(KTerm* term, KTermSession* session, int mode, bool enable, bool private_mode) {
    if (!session) session = GET_SESSION(term);
    if (private_mode) {
        // DEC Private Modes
        switch (mode) {
            case 1: // DECCKM - Cursor Key Mode
                // Enable/disable application cursor keys
                if (enable) session->dec_modes |= KTERM_MODE_DECCKM; else session->dec_modes &= ~KTERM_MODE_DECCKM;
                break;

            case 2: // DECANM - ANSI Mode (Set = ANSI, Reset = VT52)
                if (enable) {
                    session->dec_modes &= ~KTERM_MODE_VT52; // ANSI
                } else {
                    session->dec_modes |= KTERM_MODE_VT52;  // VT52
                    if ((session->conformance.features & KTERM_FEATURE_VT52_MODE)) {
                        session->parse_state = PARSE_VT52;
                    }
                }
                break;

            case 3: // DECCOLM - Column Mode
                // Set 132-column mode
                // Standard requires clearing screen, resetting margins, and homing cursor.
                if (!(session->dec_modes & KTERM_MODE_ALLOW_80_132)) break; // Mode 40 must be enabled
                // Normalize bitwise check to boolean (0/1) for comparison with 'enable' (bool)
                if (!!(session->dec_modes & KTERM_MODE_DECCOLM) != enable) {
                    if (enable) session->dec_modes |= KTERM_MODE_DECCOLM; else session->dec_modes &= ~KTERM_MODE_DECCOLM;

                    // Resize the session
                    // We can safely call _Internal here because we are in ProcessEventsInternal which holds session->lock
                    int target_cols = enable ? 132 : 80;
                    KTerm_QueueResize(session, target_cols, session->rows, true);

                    if (!(session->dec_modes & KTERM_MODE_DECNCSM)) {
                        // 1. Clear Screen
                        // Use updated dimensions
                        int rows = session->rows;
                        int cols = session->cols;

                        for (int y = 0; y < rows; y++) {
                            for (int x = 0; x < cols; x++) {
                                KTerm_ClearCell(term, GetScreenCell(session, y, x));
                            }
                            session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }

                        // 2. Reset Margins
                        session->scroll_top = 0;
                        session->scroll_bottom = rows - 1;
                        session->left_margin = 0;
                        session->right_margin = cols - 1;

                        // 3. Home Cursor
                        session->cursor.x = 0;
                        session->cursor.y = 0;
                    }
                }
                break;

            case 4: // DECSCLM - Scrolling Mode
                // Enable/disable smooth scrolling
                if (enable) session->dec_modes |= KTERM_MODE_DECSCLM; else session->dec_modes &= ~KTERM_MODE_DECSCLM;
                break;

            case 5: // DECSCNM - Screen Mode
                // Enable/disable reverse video
                if (enable) session->dec_modes |= KTERM_MODE_DECSCNM; else session->dec_modes &= ~KTERM_MODE_DECSCNM;
                break;

            case 6: // DECOM - Origin Mode
                // Enable/disable origin mode, adjust cursor position
                if (enable) session->dec_modes |= KTERM_MODE_DECOM; else session->dec_modes &= ~KTERM_MODE_DECOM;
                if (enable) {
                    session->cursor.x = session->left_margin;
                    session->cursor.y = session->scroll_top;
                } else {
                    session->cursor.x = 0;
                    session->cursor.y = 0;
                }
                break;

            case 7: // DECAWM - Auto Wrap Mode
                // Enable/disable auto wrap
                if (enable) session->dec_modes |= KTERM_MODE_DECAWM; else session->dec_modes &= ~KTERM_MODE_DECAWM;
                break;

            case 8: // DECARM - Auto Repeat Mode
                // Enable/disable auto repeat keys
                if (enable) session->dec_modes |= KTERM_MODE_DECARM; else session->dec_modes &= ~KTERM_MODE_DECARM;
                break;

            case 9: // X10 Mouse Tracking
                // Enable/disable X10 mouse tracking
                KTerm_EnableMouseFeature(term, "cursor", enable);
                session->mouse.mode = enable ? MOUSE_TRACKING_X10 : MOUSE_TRACKING_OFF;
                break;

            case 12: // DECSET 12 - Local Echo / Send/Receive (Legacy KTerm mapping?)
                // NOTE: Standard VT100 uses ANSI 12 for SRM. DEC ?12 is usually Att610 blink.
                // KTerm seems to map ?12 to Local Echo. Keeping for compatibility but users should use ANSI 12.
                if (enable) session->dec_modes |= KTERM_MODE_LOCALECHO; else session->dec_modes &= ~KTERM_MODE_LOCALECHO;
                break;

            case 18: // DECPFF - Print Form Feed
                if (enable) session->dec_modes |= KTERM_MODE_DECPFF; else session->dec_modes &= ~KTERM_MODE_DECPFF;
                break;

            case 19: // DECPEX - Print Extent
                if (enable) session->dec_modes |= KTERM_MODE_DECPEX; else session->dec_modes &= ~KTERM_MODE_DECPEX;
                break;

            case 25: // DECTCEM - Text Cursor Enable Mode
                // Enable/disable text cursor visibility
                if (enable) session->dec_modes |= KTERM_MODE_DECTCEM; else session->dec_modes &= ~KTERM_MODE_DECTCEM;
                session->cursor.visible = enable;
                break;

            case 38: // DECTEK - Tektronix Mode
                if (enable) {
                    session->parse_state = PARSE_TEKTRONIX;
                    session->tektronix.state = 0; // Alpha
                    session->tektronix.x = 0;
                    session->tektronix.y = 0;
                    session->tektronix.pen_down = false;
                    term->vector_count = 0; // Clear screen on entry
                } else {
                    session->parse_state = VT_PARSE_NORMAL;
                }
                break;

            case 40: // Allow 80/132 Column Mode
                if (enable) session->dec_modes |= KTERM_MODE_ALLOW_80_132; else session->dec_modes &= ~KTERM_MODE_ALLOW_80_132;
                break;

            case 41: // DECELR - Locator Enable
                session->locator_enabled = enable;
                break;

            case 45: // DECEDM - Extended Editing Mode (Insert/Replace)
                if (enable) session->dec_modes |= KTERM_MODE_DECEDM; else session->dec_modes &= ~KTERM_MODE_DECEDM;
                break;

            case 47: // Alternate Screen Buffer
            case 1047: // Alternate Screen Buffer (xterm)
                // Switch between main and alternate screen buffers
                if (enable && (session->dec_modes & KTERM_MODE_ALT_CURSOR_SAVE)) KTerm_ExecuteSaveCursor(term, session);
                SwitchScreenBuffer(term, session, enable);
                if (!enable && (session->dec_modes & KTERM_MODE_ALT_CURSOR_SAVE)) KTerm_ExecuteRestoreCursor(term, session);
                break;

            case 64: // DECSCCM - Multi-Session Support
                 if (enable) session->conformance.features |= KTERM_FEATURE_MULTI_SESSION_MODE; else session->conformance.features &= ~KTERM_FEATURE_MULTI_SESSION_MODE;
                 if (!enable && term->active_session != 0) {
                     KTerm_SetActiveSession(term, 0);
                 }
                 break;

            case 67: // DECBKM - Backarrow Key Mode
                if (enable) session->dec_modes |= KTERM_MODE_DECBKM; else session->dec_modes &= ~KTERM_MODE_DECBKM;
                session->input.backarrow_sends_bs = enable;
                break;

            case 68: // DECKBUM - Keyboard Usage Mode
                if (enable) session->dec_modes |= KTERM_MODE_DECKBUM; else session->dec_modes &= ~KTERM_MODE_DECKBUM;
                break;

            case 88: // DECXRLM - Transmit XOFF/XON on Receive Limit
                if (enable) session->dec_modes |= KTERM_MODE_DECXRLM; else session->dec_modes &= ~KTERM_MODE_DECXRLM;
                break;

            case 103: // DECHDPXM - Half Duplex Mode
                if (enable) session->dec_modes |= KTERM_MODE_DECHDPXM; else session->dec_modes &= ~KTERM_MODE_DECHDPXM;
                break;

            case 104: // DECESKM - Secondary Keyboard Language Mode
                if (enable) session->dec_modes |= KTERM_MODE_DECESKM; else session->dec_modes &= ~KTERM_MODE_DECESKM;
                break;

            case 1041: // Alt Cursor Save Mode
                if (enable) session->dec_modes |= KTERM_MODE_ALT_CURSOR_SAVE; else session->dec_modes &= ~KTERM_MODE_ALT_CURSOR_SAVE;
                break;

            case 1048: // Save/Restore Cursor
                // Save or restore cursor state
                if (enable) {
                    KTerm_ExecuteSaveCursor(term, session);
                } else {
                    KTerm_ExecuteRestoreCursor(term, session);
                }
                break;

            case 10: // DECAKM - Alias for DECNKM (VT100)
            case 66: // DECNKM - Numeric Keypad Mode
                // enable=true -> Application Keypad (DECKPAM)
                // enable=false -> Numeric Keypad (DECKPNM)
                session->input.keypad_application_mode = enable;
                break;

            case 69: // DECLRMM - Vertical Split Mode (Left/Right Margins)
                if (enable) session->dec_modes |= KTERM_MODE_DECLRMM; else session->dec_modes &= ~KTERM_MODE_DECLRMM;
                if (!enable) {
                    // Reset left/right margins when disabled
                    session->left_margin = 0;
                    session->right_margin = term->width - 1;
                }
                break;

            case 80: // DECSDM - Sixel Display Mode
                // xterm: enable (h) -> Scrolling DISABLED. disable (l) -> Scrolling ENABLED.
                if (enable) session->dec_modes |= KTERM_MODE_DECSDM; else session->dec_modes &= ~KTERM_MODE_DECSDM;
                // Sync session-specific graphics state if active?
                // The Sixel state is reset on DCS entry anyway.
                break;

            case 95: // DECNCSM - No Clear Screen on Column Change
                if (enable) session->dec_modes |= KTERM_MODE_DECNCSM; else session->dec_modes &= ~KTERM_MODE_DECNCSM;
                break;

            case 1049: // Alternate Screen + Save/Restore Cursor
                // Save/restore cursor and switch screen buffer
                if (enable) {
                    KTerm_ExecuteSaveCursor(term, session);
                    SwitchScreenBuffer(term, session, true);
                    ExecuteED(term, false); // Clear screen
                    session->cursor.x = 0;
                    session->cursor.y = 0;
                } else {
                    SwitchScreenBuffer(term, session, false);
                    KTerm_ExecuteRestoreCursor(term, session);
                }
                break;

            case 8452: // Sixel Cursor Mode (xterm)
                // enable -> Cursor at end of graphic
                // disable -> Cursor at start of next line (standard)
                if (enable) session->dec_modes |= KTERM_MODE_SIXEL_CURSOR; else session->dec_modes &= ~KTERM_MODE_SIXEL_CURSOR;
                break;

            case 1000: // VT200 Mouse Tracking
                session->mouse.enabled = enable;
                if (!enable) { session->mouse.cursor_x = -1; session->mouse.cursor_y = -1; }
                session->mouse.mode = enable ? (session->mouse.sgr_mode ? MOUSE_TRACKING_SGR : MOUSE_TRACKING_VT200) : MOUSE_TRACKING_OFF;
                break;

            case 1001: // VT200 Highlight Mouse Tracking
                session->mouse.enabled = enable;
                if (!enable) { session->mouse.cursor_x = -1; session->mouse.cursor_y = -1; }
                session->mouse.mode = enable ? MOUSE_TRACKING_VT200_HIGHLIGHT : MOUSE_TRACKING_OFF;
                break;

            case 1002: // Button Event Mouse Tracking
                session->mouse.enabled = enable;
                if (!enable) { session->mouse.cursor_x = -1; session->mouse.cursor_y = -1; }
                session->mouse.mode = enable ? MOUSE_TRACKING_BTN_EVENT : MOUSE_TRACKING_OFF;
                break;

            case 1003: // Any Event Mouse Tracking
                session->mouse.enabled = enable;
                if (!enable) { session->mouse.cursor_x = -1; session->mouse.cursor_y = -1; }
                session->mouse.mode = enable ? MOUSE_TRACKING_ANY_EVENT : MOUSE_TRACKING_OFF;
                break;

            case 1004: // Focus In/Out Events
                session->mouse.focus_tracking = enable;
                break;

            case 1005: // UTF-8 Mouse Mode
                // UTF-8 mouse encoding (legacy, SGR preferred)
                break;

            case 1006: // SGR Mouse Mode
                session->mouse.sgr_mode = enable;
                if (enable && session->mouse.mode != MOUSE_TRACKING_OFF &&
                    session->mouse.mode != MOUSE_TRACKING_URXVT && session->mouse.mode != MOUSE_TRACKING_PIXEL) {
                    session->mouse.mode = MOUSE_TRACKING_SGR;
                } else if (!enable && session->mouse.mode == MOUSE_TRACKING_SGR) {
                    session->mouse.mode = MOUSE_TRACKING_VT200;
                }
                break;

            case 1015: // URXVT Mouse Mode
                if (enable) {
                    session->mouse.mode = MOUSE_TRACKING_URXVT;
                    session->mouse.enabled = true;
                } else if (session->mouse.mode == MOUSE_TRACKING_URXVT) {
                    session->mouse.mode = MOUSE_TRACKING_OFF;
                    session->mouse.enabled = false;
                    session->mouse.cursor_x = -1; session->mouse.cursor_y = -1;
                }
                break;

            case 2026: // Synchronized Output
                session->synchronized_update = enable;
                // Note: Disabling triggers update next frame automatically via row_dirty
                break;

            case 1016: // Pixel Position Mouse Mode
                if (enable) {
                    session->mouse.mode = MOUSE_TRACKING_PIXEL;
                    session->mouse.enabled = true;
                } else if (session->mouse.mode == MOUSE_TRACKING_PIXEL) {
                    session->mouse.mode = MOUSE_TRACKING_OFF;
                    session->mouse.enabled = false;
                    session->mouse.cursor_x = -1; session->mouse.cursor_y = -1;
                }
                break;

            case 8246: // BDSM - Bi-Directional Support Mode (Private)
                if (enable) session->dec_modes |= KTERM_MODE_BDSM; else session->dec_modes &= ~KTERM_MODE_BDSM;
                break;

            case 2004: // Bracketed Paste Mode
                // Enable/disable bracketed paste
                session->bracketed_paste.enabled = enable;
                break;

            default:
                // Log unsupported DEC modes
                if (session->options.debug_sequences) {
                    char debug_msg[64];
                    snprintf(debug_msg, sizeof(debug_msg), "Unknown DEC mode: %d", mode);
                    KTerm_LogUnsupportedSequence(term, debug_msg);
                }
                break;
        }
    } else {
        // ANSI Modes
        switch (mode) {
            case 4: // IRM - Insert/Replace Mode
                // Enable/disable insert mode
                if (enable) session->dec_modes |= KTERM_MODE_INSERT; else session->dec_modes &= ~KTERM_MODE_INSERT;
                break;

            case 12: // SRM - Send/Receive Mode
                // h = Local Echo OFF, l = Local Echo ON
                if (enable) session->dec_modes &= ~KTERM_MODE_LOCALECHO; else session->dec_modes |= KTERM_MODE_LOCALECHO;
                break;

            case 20: // LNM - Line Feed/New Line Mode
                // Enable/disable line feed/new line mode
                session->ansi_modes.line_feed_new_line = enable;
                break;

            case 7: // ANSI Mode 7: Line Wrap (standard in ANSI.SYS, private in VT100)
                // Restrict this override to ANSI.SYS level to avoid conflict with standard ISO 6429 VEM (Vertical Editing Mode)
                if (session->conformance.level == VT_LEVEL_ANSI_SYS) {
                    if (enable) session->dec_modes |= KTERM_MODE_DECAWM; else session->dec_modes &= ~KTERM_MODE_DECAWM;
                }
                break;

            default:
                // Log unsupported ANSI modes
                if (session->options.debug_sequences) {
                    char debug_msg[64];
                    snprintf(debug_msg, sizeof(debug_msg), "Unknown ANSI mode: %d", mode);
                    KTerm_LogUnsupportedSequence(term, debug_msg);
                }
                break;
        }
    }
}

// Set terminal modes (CSI Pm h or CSI ? Pm h)
// Enables specified modes, including mouse tracking and focus reporting
static void ExecuteSM(KTerm* term, KTermSession* session, bool private_mode) {
    if (!session) session = GET_SESSION(term);
    // Iterate through parsed parameters from the CSI sequence
    for (int i = 0; i < session->param_count; i++) {
        int mode = session->escape_params[i];
        if (private_mode && session->conformance.level == VT_LEVEL_ANSI_SYS) continue;
        KTerm_SetModeInternal(term, session, mode, true, private_mode);
    }
}

// Reset terminal modes (CSI Pm l or CSI ? Pm l)
// Disables specified modes, including mouse tracking and focus reporting
static void ExecuteRM(KTerm* term, KTermSession* session, bool private_mode) {
    if (!session) session = GET_SESSION(term);
    // Iterate through parsed parameters from the CSI sequence
    for (int i = 0; i < session->param_count; i++) {
        int mode = session->escape_params[i];
        if (private_mode && session->conformance.level == VT_LEVEL_ANSI_SYS) continue;
        KTerm_SetModeInternal(term, session, mode, false, private_mode);
    }
}

// Continue with device attributes and other implementations...

void ExecuteDA(KTerm* term, bool private_mode) {
    char introducer = private_mode ? GET_SESSION(term)->escape_buffer[0] : 0;
    if (introducer == '>') {
        KTerm_QueueResponse(term, GET_SESSION(term)->secondary_attributes);
    } else if (introducer == '=') {
        KTerm_QueueResponse(term, GET_SESSION(term)->tertiary_attributes);
    } else {
        KTerm_QueueResponse(term, GET_SESSION(term)->device_attributes);
    }
}

// Helper function to convert character to printable ASCII
static char GetPrintableChar(unsigned int ch, CharsetState* charset) {
    if (ch < 0x20 || ch > 0x7E) {
        // Check if DEC Special Graphics is active
        if (charset && charset->gl == &charset->g0 && charset->g0 == CHARSET_DEC_SPECIAL) {
            // Map common DEC line-drawing characters to ASCII approximations
            switch (ch) {
                case 0x6A: return '+'; // Corner
                case 0x6C: return '-'; // Horizontal line
                case 0x6D: return '|'; // Vertical line
                default: return ' ';
            }


        }
        return ' '; // Non-printable or unmapped
    }
    return (char)ch;
}


// Helper function to send data to the printer callback
static void SendToPrinter(KTerm* term, const char* data, size_t length) {
    if (term->printer_callback) {
        term->printer_callback(term, data, length);
    } else {
        // Fallback: If no printer callback, maybe log or ignore?
        // Standard behavior might be to do nothing if no printer attached.
        if (GET_SESSION(term)->options.debug_sequences) {
            fprintf(stderr, "MC: Print requested but no printer callback set (len=%zu)\n", length);
        }
    }
}

// Updated ExecuteMC
static void ExecuteMC(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    bool private_mode = (session->escape_buffer[0] == '?');
    int params[MAX_ESCAPE_PARAMS];
    KTerm_ParseCSIParams(term, session->escape_buffer, params, MAX_ESCAPE_PARAMS);
    int pi = (session->param_count > 0) ? session->escape_params[0] : 0;

    if (!session->printer_available) {
        KTerm_LogUnsupportedSequence(term, "MC: No printer available");
        return;
    }

    if (!private_mode) {
        switch (pi) {
            case 0: // Print entire screen or scrolling region (DECPEX)
            {
                int start_y = 0;
                int end_y = term->height;

                // DECPEX (Mode 19):
                // Enable (h) = Print Scrolling Region
                // Disable (l) = Print Full Screen (Default)
                // Note: Previous implementation might have been inverted.
                // Standard says: 19h -> Print Extent = Scrolling Region.
                if ((session->dec_modes & KTERM_MODE_DECPEX)) {
                    start_y = session->scroll_top;
                    end_y = session->scroll_bottom + 1;
                }

                // Calculate buffer size: (cols + newline) * rows + possible FF + null + safety
                size_t buf_size = (term->width + 1) * (end_y - start_y) + 8;
                char* print_buffer = (char*)KTerm_Malloc(buf_size);
                if (!print_buffer) break; // Allocation failed
                size_t pos = 0;

                for (int y = start_y; y < end_y; y++) {
                    for (int x = 0; x < term->width; x++) {
                        EnhancedTermChar* cell = GetScreenCell(session, y, x);
                        if (pos < buf_size - 3) { // Reserve space for FF/Null
                            print_buffer[pos++] = GetPrintableChar(cell->ch, &session->charset);
                        }
                    }
                    if (pos < buf_size - 3) {
                        print_buffer[pos++] = '\n';
                    }
                }

                if ((session->dec_modes & KTERM_MODE_DECPFF)) {
                     print_buffer[pos++] = '\f'; // 0x0C
                }

                print_buffer[pos] = '\0';
                SendToPrinter(term, print_buffer, pos);
                if (session->options.debug_sequences) {
                    KTerm_LogUnsupportedSequence(term, "MC: Print screen completed");
                }
                KTerm_Free(print_buffer);
                break;
            }
            case 1: // Print current line
            {
                char print_buffer[term->width + 3]; // + newline + FF + null
                size_t pos = 0;
                int y = session->cursor.y;
                for (int x = 0; x < term->width; x++) {
                    EnhancedTermChar* cell = GetScreenCell(session, y, x);
                    if (pos < sizeof(print_buffer) - 3) {
                        print_buffer[pos++] = GetPrintableChar(cell->ch, &session->charset);
                    }
                }
                print_buffer[pos++] = '\n';

                if ((session->dec_modes & KTERM_MODE_DECPFF)) {
                     print_buffer[pos++] = '\f'; // 0x0C
                }

                print_buffer[pos] = '\0';
                SendToPrinter(term, print_buffer, pos);
                if (session->options.debug_sequences) {
                    KTerm_LogUnsupportedSequence(term, "MC: Print line completed");
                }
                break;
            }
            case 4: // Disable auto-print
                session->auto_print_enabled = false;
                if (session->options.debug_sequences) {
                    KTerm_LogUnsupportedSequence(term, "MC: Auto-print disabled");
                }
                break;
            case 5: // Enable auto-print
                session->auto_print_enabled = true;
                if (session->options.debug_sequences) {
                    KTerm_LogUnsupportedSequence(term, "MC: Auto-print enabled");
                }
                break;
            default:
                if (session->options.log_unsupported) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "CSI %d i", pi);
                    snprintf(session->conformance.compliance.last_unsupported,
                             sizeof(session->conformance.compliance.last_unsupported),
                             "%s", msg);
                    session->conformance.compliance.unsupported_sequences++;
                }
                break;
        }
    } else {
        switch (pi) {
            case 4: // Disable printer controller mode
                session->printer_controller_enabled = false;
                if (session->options.debug_sequences) {
                    KTerm_LogUnsupportedSequence(term, "MC: Printer controller disabled");
                }
                break;
            case 5: // Enable printer controller mode
                session->printer_controller_enabled = true;
                if (session->options.debug_sequences) {
                    // Log?
                }
                break;
            case 9: // Print Screen (DEC specific private parameter for same action as CSI 0 i)
            {
                size_t buf_size = term->width * term->height + term->height + 1;
                char* print_buffer = (char*)KTerm_Malloc(buf_size);
                if (!print_buffer) break; // Allocation failed
                size_t pos = 0;
                for (int y = 0; y < term->height; y++) {
                    for (int x = 0; x < term->width; x++) {
                        EnhancedTermChar* cell = GetScreenCell(session, y, x);
                        if (pos < buf_size - 2) {
                            print_buffer[pos++] = GetPrintableChar(cell->ch, &session->charset);
                        }
                    }
                    if (pos < buf_size - 2) {
                        print_buffer[pos++] = '\n';
                    }
                }
                print_buffer[pos] = '\0';
                SendToPrinter(term, print_buffer, pos);
                if (session->options.debug_sequences) {
                    KTerm_LogUnsupportedSequence(term, "MC: Print screen (DEC) completed");
                }
                KTerm_Free(print_buffer);
                break;
            }
            default:
                if (session->options.log_unsupported) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "CSI ?%d i", pi);
                    snprintf(session->conformance.compliance.last_unsupported,
                             sizeof(session->conformance.compliance.last_unsupported),
                             "%s", msg);
                    session->conformance.compliance.unsupported_sequences++;
                }
                break;
        }
    }
}
// Internal Write Primitive (Handles Sink vs Legacy Buffer)
static void KTerm_WriteInternal(KTerm* term, KTermSession* session, const char* data, size_t len, bool is_binary) {
    (void)is_binary;
    // Ring Buffering Logic (Always buffer to prevent drops/blocking)
    if (!session) session = GET_SESSION(term);
    if (!session->response_enabled) return;

    int tail = atomic_load_explicit(&session->response_ring.tail, memory_order_relaxed);
    int head = atomic_load_explicit(&session->response_ring.head, memory_order_acquire);

    // Calculate free space
    int free_space;
    if (head <= tail) free_space = KTERM_OUTPUT_PIPELINE_SIZE - 1 - (tail - head);
    else free_space = head - tail - 1;

    if ((int)len > free_space) {
        atomic_store_explicit(&session->response_ring.overflow, true, memory_order_relaxed);
        len = free_space;
    }

    if (len > 0) {
        int chunk1 = KTERM_OUTPUT_PIPELINE_SIZE - tail;
        if (chunk1 > (int)len) chunk1 = (int)len;
        memcpy(&session->response_ring.data[tail], data, chunk1);

        if (chunk1 < (int)len) {
            memcpy(&session->response_ring.data[0], &data[chunk1], len - chunk1);
        }

        tail = (tail + len) % KTERM_OUTPUT_PIPELINE_SIZE;
        atomic_store_explicit(&session->response_ring.tail, tail, memory_order_release);
    }
}

void KTerm_QueueResponse(KTerm* term, const char* response) {
    if (!response) return;
    KTerm_WriteInternal(term, NULL, response, strlen(response), false);
}

void KTerm_QueueResponseBytes(KTerm* term, const char* data, size_t len) {
    if (!data || len == 0) return;
    KTerm_WriteInternal(term, NULL, data, len, true);
}

void KTerm_QueueSessionResponse(KTerm* term, KTermSession* session, const char* response) {
    if (!response) return;
    KTerm_WriteInternal(term, session, response, strlen(response), false);
}

void KTerm_QueueSessionResponseBytes(KTerm* term, KTermSession* session, const char* data, size_t len) {
    if (!data || len == 0) return;
    KTerm_WriteInternal(term, session, data, len, true);
}

// Enhanced ExecuteDSR function with new handlers
static void ExecuteDSR(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    bool private_mode = (session->escape_buffer[0] == '?');
    int params[MAX_ESCAPE_PARAMS];
    KTerm_ParseCSIParams(term, session->escape_buffer, params, MAX_ESCAPE_PARAMS);
    int command = (session->param_count > 0) ? session->escape_params[0] : 0;

    if (!private_mode) {
        switch (command) {
            case 5: KTerm_QueueSessionResponse(term, session, "\x1B[0n"); break;
            case 6: {
                int row = session->cursor.y + 1;
                int col = session->cursor.x + 1;
                if ((session->dec_modes & KTERM_MODE_DECOM)) {
                    row = session->cursor.y - session->scroll_top + 1;
                    col = session->cursor.x - session->left_margin + 1;
                }
                char response[32];
                snprintf(response, sizeof(response), "\x1B[%d;%dR", row, col);
                KTerm_QueueSessionResponse(term, session, response);
                break;
            }
            case 98: { // Error Reporting (Non-private extension)
                 char response[32];
                 snprintf(response, sizeof(response), "\x1B[98;%dn", session->status.error_count);
                 KTerm_QueueSessionResponse(term, session, response);
                 break;
            }
            default:
                if (session->options.log_unsupported) {
                    snprintf(session->conformance.compliance.last_unsupported,
                             sizeof(session->conformance.compliance.last_unsupported),
                             "CSI %dn", command);
                    session->conformance.compliance.unsupported_sequences++;
                }
                break;
        }
    } else {
        switch (command) {
            case 10: { // Graphics Capabilities (Sixel/ReGIS/Tek/Kitty/TrueColor)
                int mask = 0;
                if (session->conformance.features & KTERM_FEATURE_SIXEL_GRAPHICS) mask |= 1;
                if (session->conformance.features & KTERM_FEATURE_REGIS_GRAPHICS) mask |= 2;
                mask |= 4; // Tektronix
                mask |= 8; // Kitty
                if (session->conformance.features & KTERM_FEATURE_TRUE_COLOR) mask |= 16;
                char response[32];
                snprintf(response, sizeof(response), "\x1B[?10;%dn", mask);
                KTerm_QueueSessionResponse(term, session, response);
                break;
            }
            case 15: KTerm_QueueSessionResponse(term, session, session->printer_available ? "\x1B[?10n" : "\x1B[?13n"); break;
            case 20: { // Macro Storage (Free ; Loaded)
                size_t free_bytes = session->macro_space.total - session->macro_space.used;
                char response[64];
                snprintf(response, sizeof(response), "\x1B[?20;%zu;%zun", free_bytes, session->stored_macros.count);
                KTerm_QueueSessionResponse(term, session, response);
                break;
            }
            case 30: { // Session State (ActiveID ; Count ; Region)
                char response[64];
                int id = 0;
                for(int i=0; i<MAX_SESSIONS; i++) if(&term->sessions[i] == session) id = i;
                snprintf(response, sizeof(response), "\x1B[?30;%d;%d;%d;%dn", id + 1, MAX_SESSIONS, session->cols, session->rows);
                KTerm_QueueSessionResponse(term, session, response);
                break;
            }
            case 21: { // DECRS - Report Session Status
                // If multi-session is not supported/enabled, we might choose to ignore or report limited info.
                // VT520 spec says DECRS reports on sessions.
                // If the terminal doesn't support sessions, it shouldn't respond or should respond with just 1.
                if (!(session->conformance.features & KTERM_FEATURE_MULTI_SESSION_MODE)) {
                     if (session->options.debug_sequences) {
                         KTerm_LogUnsupportedSequence(term, "DECRS ignored: Multi-session mode disabled");
                     }
                     break;
                }

                int limit = session->conformance.max_session_count;
                if (limit == 0) limit = 1;
                if (limit > MAX_SESSIONS) limit = MAX_SESSIONS;

                char response[MAX_COMMAND_BUFFER];
                int offset = snprintf(response, sizeof(response), "\x1BP$p");
                for (int i = 0; i < limit; i++) {
                    int seq = i + 1;
                    int status = 1; // Not open
                    if (term->sessions[i].session_open) {
                        status = (i == term->active_session) ? 2 : 3;
                    }
                    int attr = 0;
                    offset += snprintf(response + offset, sizeof(response) - offset, "%d;%d;%d", seq, status, attr);
                    if (i < limit - 1) {
                         offset += snprintf(response + offset, sizeof(response) - offset, "|");
                    }
                }
                snprintf(response + offset, sizeof(response) - offset, "\x1B\\");
                KTerm_QueueSessionResponse(term, session, response);
                break;
            }
            case 25: KTerm_QueueSessionResponse(term, session, session->programmable_keys.udk_locked ? "\x1B[?21n" : "\x1B[?20n"); break;
            case 26: {
                char response[32];
                snprintf(response, sizeof(response), "\x1B[?27;%dn", session->input.keyboard_dialect);
                KTerm_QueueSessionResponse(term, session, response);
                break;
            }
            case 27: // Locator Type (DECREPTPARM)
                KTerm_QueueSessionResponse(term, session, "\x1B[?27;0n"); // No locator
                break;
            case 53: KTerm_QueueSessionResponse(term, session, session->locator_enabled ? "\x1B[?53n" : "\x1B[?50n"); break;
            case 55: KTerm_QueueSessionResponse(term, session, "\x1B[?57;0n"); break;
            case 56: KTerm_QueueSessionResponse(term, session, "\x1B[?56;0n"); break;
            case 62: {
                char response[32];
                snprintf(response, sizeof(response), "\x1B[?62;%zu;%zun",
                         session->macro_space.used, session->macro_space.total);
                KTerm_QueueSessionResponse(term, session, response);
                break;
            }
            case 63: {
                int page = (session->param_count > 1) ? session->escape_params[1] : 1;
                session->checksum.last_checksum = ComputeScreenChecksum(term, page);
                char response[64];
                snprintf(response, sizeof(response), "\x1B[?63;%d;%d;%04Xn",
                         page, session->checksum.algorithm, session->checksum.last_checksum);
                KTerm_QueueSessionResponse(term, session, response);
                break;
            }
            case 75: KTerm_QueueSessionResponse(term, session, "\x1B[?75;0n"); break;
            case 12: { // DECRSN - Report Session Number
                char response[32];
                snprintf(response, sizeof(response), "\x1B[?12;%dn", term->active_session + 1);
                KTerm_QueueSessionResponse(term, session, response);
                break;
            }
            case 98: { // Error Reporting (Private extension)
                 char response[32];
                 snprintf(response, sizeof(response), "\x1B[?98;%dn", session->status.error_count);
                 KTerm_QueueSessionResponse(term, session, response);
                 break;
            }
            default:
                if (session->options.log_unsupported) {
                    snprintf(session->conformance.compliance.last_unsupported,
                             sizeof(session->conformance.compliance.last_unsupported),
                             "CSI ?%dn", command);
                    session->conformance.compliance.unsupported_sequences++;
                }
                break;
        }
    }
}

void ExecuteDECSTBM(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Set Top and Bottom Margins
    int top = KTerm_GetCSIParam(term, session, 0, 1) - 1;    // Convert to 0-based
    int bottom = KTerm_GetCSIParam(term, session, 1, term->height) - 1;

    // Validate parameters
    if (top >= 0 && top < term->height && bottom >= top && bottom < term->height) {
        session->scroll_top = top;
        session->scroll_bottom = bottom;

        // Move cursor to home position
        if ((session->dec_modes & KTERM_MODE_DECOM)) {
            session->cursor.x = session->left_margin;
            session->cursor.y = session->scroll_top;
        } else {
            session->cursor.x = 0;
            session->cursor.y = 0;
        }
    }
}

void ExecuteDECSLRM(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Set Left and Right Margins (VT420)
    if (!(session->conformance.features & KTERM_FEATURE_VT420_MODE)) {
        KTerm_LogUnsupportedSequence(term, "DECSLRM requires VT420 mode");
        return;
    }

    int left = KTerm_GetCSIParam(term, session, 0, 1) - 1;    // Convert to 0-based
    int right = KTerm_GetCSIParam(term, session, 1, term->width) - 1;

    // Validate parameters
    if (left >= 0 && left < term->width && right >= left && right < term->width) {
        session->left_margin = left;
        session->right_margin = right;

        // Move cursor to home position
        if ((session->dec_modes & KTERM_MODE_DECOM)) {
            session->cursor.x = session->left_margin;
            session->cursor.y = session->scroll_top;
        } else {
            session->cursor.x = 0;
            session->cursor.y = 0;
        }
    }
}

// Updated ExecuteDECRQPSR
static void ExecuteDECRQPSR(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    int params[MAX_ESCAPE_PARAMS];
    KTerm_ParseCSIParams(term, session->escape_buffer, params, MAX_ESCAPE_PARAMS);
    int pfn = (session->param_count > 0) ? session->escape_params[0] : 0;

    char response[64];
    switch (pfn) {
        case 1: // Sixel geometry
            snprintf(response, sizeof(response), "DCS 2 $u %d ; %d;%d;%d;%d ST",
                     session->conformance.level, session->sixel.x, session->sixel.y,
                     session->sixel.width, session->sixel.height);
            KTerm_QueueSessionResponse(term, session, response);
            break;
        case 2: // Sixel color palette
            for (int i = 0; i < 256; i++) {
                RGB_KTermColor c = term->color_palette[i];
                snprintf(response, sizeof(response), "DCS 1 $u #%d;%d;%d;%d ST",
                         i, c.r, c.g, c.b);
                KTerm_QueueSessionResponse(term, session, response);
            }
            break;
        case 3: // ReGIS (unsupported)
            if (session->options.log_unsupported) {
                snprintf(session->conformance.compliance.last_unsupported,
                         sizeof(session->conformance.compliance.last_unsupported),
                         "CSI %d $ u (ReGIS unsupported)", pfn);
                session->conformance.compliance.unsupported_sequences++;
            }
            break;
        default:
            if (session->options.log_unsupported) {
                snprintf(session->conformance.compliance.last_unsupported,
                         sizeof(session->conformance.compliance.last_unsupported),
                         "CSI %d $ u", pfn);
                session->conformance.compliance.unsupported_sequences++;
            }
            break;
    }
}

void ExecuteDECLL(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Load LEDs
    int n = KTerm_GetCSIParam(term, session, 0, 0);

    // DECLL - Load LEDs (VT220+ feature)
    // Parameters: 0=all off, 1=LED1 on, 2=LED2 on, 4=LED3 on, 8=LED4 on
    // Modern terminals don't have LEDs, so this is mostly ignored

    if (session->options.debug_sequences) {
        char debug_msg[64];
        snprintf(debug_msg, sizeof(debug_msg), "DECLL: LED state %d", n);
        KTerm_LogUnsupportedSequence(term, debug_msg);
    }

    // Could be used for visual indicators in a GUI implementation
    // For now, just acknowledge the command
}

void ExecuteDECSTR(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Soft KTerm Reset
    // Reset terminal to power-on defaults but keep communication settings

    // Reset display modes
    session->dec_modes |= KTERM_MODE_DECTCEM;
    session->dec_modes |= KTERM_MODE_DECAWM;
    session->dec_modes &= ~KTERM_MODE_DECOM;
    session->dec_modes &= ~KTERM_MODE_INSERT;
    session->dec_modes &= ~KTERM_MODE_DECCKM;

    // Reset character attributes
    KTerm_ResetAllAttributes(term, session);

    // Reset scrolling region
    session->scroll_top = 0;
    session->scroll_bottom = term->height - 1;
    session->left_margin = 0;
    session->right_margin = term->width - 1;

    // Reset character sets
    KTerm_InitCharacterSets(term, session);

    // Reset graphics
    KTerm_ResetGraphics(term, session, GRAPHICS_RESET_ALL);

    // Reset tab stops
    KTerm_InitTabStops(term, session);

    // Home cursor
    session->cursor.x = 0;
    session->cursor.y = 0;

    // Clear saved cursor
    session->saved_cursor_valid = false;

    KTerm_InitKTermColorPalette(term);
    KTerm_InitSixelGraphics(term, session);

    if (session->options.debug_sequences) {
        KTerm_LogUnsupportedSequence(term, "DECSTR: Soft terminal reset");
    }
}

void ExecuteDECSCL(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Select Conformance Level
    int level = KTerm_GetCSIParam(term, session, 0, 61);
    int c1_control = KTerm_GetCSIParam(term, session, 1, 0);
    (void)c1_control;  // Mark as intentionally unused

    // Set conformance level based on parameter
    switch (level) {
        case 61: KTerm_SetLevel(term, session, VT_LEVEL_100); break;
        case 62: KTerm_SetLevel(term, session, VT_LEVEL_220); break;
        case 63: KTerm_SetLevel(term, session, VT_LEVEL_320); break;
        case 64: KTerm_SetLevel(term, session, VT_LEVEL_420); break;
        default:
            if (session->options.debug_sequences) {
                char debug_msg[64];
                snprintf(debug_msg, sizeof(debug_msg), "Unknown conformance level: %d", level);
                KTerm_LogUnsupportedSequence(term, debug_msg);
            }
            break;
    }

    // c1_control: 0=8-bit controls, 1=7-bit controls, 2=8-bit controls
    // Modern implementations typically use 7-bit controls
}

// Enhanced ExecuteDECRQM
void ExecuteDECRQM(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Request Mode
    int mode = KTerm_GetCSIParam(term, session, 0, 0);
    bool private_mode = (session->escape_buffer[0] == '?');

    char response[32];
    int mode_state = 0; // 0=not recognized, 1=set, 2=reset, 3=permanently set, 4=permanently reset

    if (private_mode) {
        // Check DEC private modes
        switch (mode) {
            case 1: // DECCKM (Application Cursor Keys)
                mode_state = (session->dec_modes & KTERM_MODE_DECCKM) ? 1 : 2;
                break;
            case 3: // DECCOLM (132 Column Mode)
                mode_state = (session->dec_modes & KTERM_MODE_DECCOLM) ? 1 : 2;
                break;
            case 4: // DECSCLM (Smooth Scroll)
                mode_state = (session->dec_modes & KTERM_MODE_DECSCLM) ? 1 : 2;
                break;
            case 5: // DECSCNM (Reverse Video)
                mode_state = (session->dec_modes & KTERM_MODE_DECSCNM) ? 1 : 2;
                break;
            case 6: // DECOM (Origin Mode)
                mode_state = (session->dec_modes & KTERM_MODE_DECOM) ? 1 : 2;
                break;
            case 7: // DECAWM (Auto Wrap Mode)
                mode_state = (session->dec_modes & KTERM_MODE_DECAWM) ? 1 : 2;
                break;
            case 8: // DECARM (Auto Repeat Keys)
                mode_state = (session->dec_modes & KTERM_MODE_DECARM) ? 1 : 2;
                break;
            case 9: // X10 Mouse
                mode_state = (session->dec_modes & KTERM_MODE_X10MOUSE) ? 1 : 2;
                break;
            case 10: // Show Toolbar
                mode_state = (session->dec_modes & KTERM_MODE_TOOLBAR) ? 1 : 4; // Permanently reset
                break;
            case 12: // Blinking Cursor
                mode_state = (session->dec_modes & KTERM_MODE_BLINKCURSOR) ? 1 : 2;
                break;
            case 18: // DECPFF (Print Form Feed)
                mode_state = (session->dec_modes & KTERM_MODE_DECPFF) ? 1 : 2;
                break;
            case 19: // Print Extent
                mode_state = (session->dec_modes & KTERM_MODE_DECPEX) ? 1 : 2;
                break;
            case 25: // DECTCEM (Cursor Visible)
                mode_state = (session->dec_modes & KTERM_MODE_DECTCEM) ? 1 : 2;
                break;
            case 38: // Tektronix Mode
                mode_state = (session->parse_state == PARSE_TEKTRONIX) ? 1 : 2;
                break;
            case 47: // Alternate Screen
            case 1047:
            case 1049:
                mode_state = (session->dec_modes & KTERM_MODE_ALTSCREEN) ? 1 : 2;
                break;
            case 1000: // VT200 Mouse
                mode_state = (session->mouse.mode == MOUSE_TRACKING_VT200) ? 1 : 2;
                break;
            case 2004: // Bracketed Paste
                mode_state = session->bracketed_paste.enabled ? 1 : 2;
                break;
            case 61: // DECSCL VT100
                mode_state = (session->conformance.level == VT_LEVEL_100) ? 1 : 2;
                break;
            case 62: // DECSCL VT220
                mode_state = (session->conformance.level == VT_LEVEL_220) ? 1 : 2;
                break;
            case 63: // DECSCL VT320
                mode_state = (session->conformance.level == VT_LEVEL_520) ? 1 : 2;
                break;
            case 64: // DECSCL VT420
                mode_state = (session->conformance.level == VT_LEVEL_420) ? 1 : 2;
                break;
            default:
                mode_state = 0;
                break;
        }
        snprintf(response, sizeof(response), "\x1B[?%d;%d$y", mode, mode_state);
    } else {
        // Check ANSI modes
        switch (mode) {
            case 4: // IRM (Insert/Replace Mode)
                mode_state = session->ansi_modes.insert_replace ? 1 : 2;
                break;
            case 20: // LNM (Line Feed/New Line Mode)
                mode_state = session->ansi_modes.line_feed_new_line ? 1 : 3; // Permanently set
                break;
            default:
                mode_state = 0;
                break;
        }
        snprintf(response, sizeof(response), "\x1B[%d;%d$y", mode, mode_state);
    }

    KTerm_QueueSessionResponse(term, session, response);
}

static void ExecuteDECSCUSR_Internal(KTerm* term, KTermSession* session) { // Set Cursor Style
    int style = (session->param_count > 0) ? session->escape_params[0] : 1;

    switch (style) {
        case 0: case 1: // Default or blinking block
            session->cursor.shape = CURSOR_BLOCK_BLINK;
            session->cursor.blink_enabled = true;
            break;
        case 2: // Steady block
            session->cursor.shape = CURSOR_BLOCK;
            session->cursor.blink_enabled = false;
            break;
        case 3: // Blinking underline
            session->cursor.shape = CURSOR_UNDERLINE_BLINK;
            session->cursor.blink_enabled = true;
            break;
        case 4: // Steady underline
            session->cursor.shape = CURSOR_UNDERLINE;
            session->cursor.blink_enabled = false;
            break;
        case 5: // Blinking bar
            session->cursor.shape = CURSOR_BAR_BLINK;
            session->cursor.blink_enabled = true;
            break;
        case 6: // Steady bar
            session->cursor.shape = CURSOR_BAR;
            session->cursor.blink_enabled = false;
            break;
        default:
            if (session->options.debug_sequences) {
                char debug_msg[64];
                snprintf(debug_msg, sizeof(debug_msg), "Unknown cursor style: %d", style);
                KTerm_LogUnsupportedSequence(term, debug_msg);
            }
            break;
    }
}

void ExecuteDECSCUSR(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Set Cursor Style
    ExecuteDECSCUSR_Internal(term, session);
}

void ExecuteCSI_P(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Various P commands
    // This handles CSI sequences ending in 'p' with different intermediates
    char* params = session->escape_buffer;

    if (strstr(params, "!") != NULL) {
        // DECSTR - Soft KTerm Reset
        ExecuteDECSTR(term, session);
    } else if (strstr(params, "\"") != NULL) {
        // DECSCL - Select Conformance Level
        ExecuteDECSCL(term, session);
    } else if (strstr(params, "$") != NULL) {
        // DECRQM - Request Mode
        ExecuteDECRQM(term, session);
    } else if (strstr(params, " ") != NULL) {
        // Set cursor style (DECSCUSR)
        ExecuteDECSCUSR(term, session);
    } else {
        // No intermediate? Check for ANSI.SYS Key Redefinition (CSI code ; string ; ... p)
        // If we are in ANSI.SYS mode, we should acknowledge this.
        if (session->conformance.level == VT_LEVEL_ANSI_SYS) {
             if (session->options.debug_sequences) {
                KTerm_LogUnsupportedSequence(term, "ANSI.SYS Key Redefinition ignored (security restriction)");
             }
             return;
        }

        // Unknown p command
        if (session->options.debug_sequences) {
            char debug_msg[MAX_COMMAND_BUFFER + 64];
            snprintf(debug_msg, sizeof(debug_msg), "Unknown CSI p command: %s", params);
            KTerm_LogUnsupportedSequence(term, debug_msg);
        }
    }
}

void ExecuteDECSCA(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Select Character Protection Attribute
    // CSI Ps " q
    // Ps = 0, 2 -> Not protected
    // Ps = 1 -> Protected
    int ps = KTerm_GetCSIParam(term, session, 0, 0);
    if (ps == 1) {
        session->current_attributes |= KTERM_ATTR_PROTECTED;
    } else {
        session->current_attributes &= ~KTERM_ATTR_PROTECTED;
    }
}


void ExecuteWindowOps(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Window manipulation (xterm extension)
    int operation = KTerm_GetCSIParam(term, session, 0, 0);

    switch (operation) {
        case 1: // De-iconify window (Restore)
            KTerm_RestoreWindow();
            break;
        case 2: // Iconify window (Minimize)
            KTerm_MinimizeWindow();
            break;
        case 3: // Move window to position (in pixels)
            {
                int x = KTerm_GetCSIParam(term, session, 1, 0);
                int y = KTerm_GetCSIParam(term, session, 2, 0);
                KTerm_SetWindowPosition(x, y);
            }
            break;
        case 4: // Resize window (in pixels)
            {
                int height = KTerm_GetCSIParam(term, session, 1, DEFAULT_WINDOW_HEIGHT);
                int width = KTerm_GetCSIParam(term, session, 2, DEFAULT_WINDOW_WIDTH);
                KTerm_SetWindowSize(width, height);
            }
            break;
        case 5: // Raise window (Bring to front)
            KTerm_SetWindowFocused(); // Closest approximation
            break;
        case 6: // Lower window
            // Not directly supported by Situation/GLFW easily
            if (session->options.debug_sequences) KTerm_LogUnsupportedSequence(term, "Window lower not supported");
            break;
        case 7: // Refresh window
            // Handled automatically by game loop
            break;
        case 8: // Resize text area (in chars)
            {
                int rows = KTerm_GetCSIParam(term, session, 1, term->height);
                int cols = KTerm_GetCSIParam(term, session, 2, term->width);
                int width = cols * DEFAULT_CHAR_WIDTH * DEFAULT_WINDOW_SCALE;
                int height = rows * DEFAULT_CHAR_HEIGHT * DEFAULT_WINDOW_SCALE;
                KTerm_SetWindowSize(width, height);
            }
            break;

        case 9: // Maximize/restore window
            if (KTerm_GetCSIParam(term, session, 1, 0) == 1) KTerm_MaximizeWindow();
            else KTerm_RestoreWindow();
            break;
        case 10: // Full-screen toggle
            if (KTerm_GetCSIParam(term, session, 1, 0) == 1) {
                if (!KTerm_IsWindowFullscreen()) KTerm_ToggleFullscreen();
            } else {
                if (KTerm_IsWindowFullscreen()) KTerm_ToggleFullscreen();
            }
            break;

        case 11: // Report window state
            KTerm_QueueSessionResponse(term, session, "\x1B[1t"); // Always report "not iconified"
            break;

        case 13: // Report window position
        case 14: // Report window size in pixels
        case 18: // Report text area size
            {
                char response[32];
                if (operation == 18) {
                    snprintf(response, sizeof(response), "\x1B[8;%d;%dt", term->height, term->width);
                } else {
                    snprintf(response, sizeof(response), "\x1B[3;%d;%dt", 100, 100); // Dummy values
                }
                KTerm_QueueSessionResponse(term, session, response);
            }
            break;

        case 19: // Report screen size
            {
                char response[32];
                snprintf(response, sizeof(response), "\x1B[9;%d;%dt",
                        KTerm_GetScreenHeight() / DEFAULT_CHAR_HEIGHT, KTerm_GetScreenWidth() / DEFAULT_CHAR_WIDTH);
                KTerm_QueueSessionResponse(term, session, response);
            }
            break;

        case 20: // Report icon label
            {
                char response[MAX_TITLE_LENGTH + 32];
                snprintf(response, sizeof(response), "\x1B]L%s\x1B\\", session->title.icon_title);
                KTerm_QueueSessionResponse(term, session, response);
            }
            break;

        case 21: // Report window title
            {
                char response[MAX_TITLE_LENGTH + 32];
                snprintf(response, sizeof(response), "\x1B]l%s\x1B\\", session->title.window_title);
                KTerm_QueueSessionResponse(term, session, response);
            }
            break;

        default:
            if (session->options.debug_sequences) {
                char debug_msg[64];
                snprintf(debug_msg, sizeof(debug_msg), "Unknown window operation: %d", operation);
                KTerm_LogUnsupportedSequence(term, debug_msg);
            }
            break;
    }
}

static void KTerm_ExecuteSaveCursor_Internal(KTermSession* session) {
    session->saved_cursor.x = session->cursor.x;
    session->saved_cursor.y = session->cursor.y;

    // Modes
    session->saved_cursor.origin_mode = (session->dec_modes & KTERM_MODE_DECOM);
    session->saved_cursor.auto_wrap_mode = (session->dec_modes & KTERM_MODE_DECAWM);

    // Attributes
    session->saved_cursor.fg_color = session->current_fg;
    session->saved_cursor.bg_color = session->current_bg;
    session->saved_cursor.attributes = session->current_attributes;

    // Charset
    session->saved_cursor.charset = session->charset;

    session->saved_cursor_valid = true;
}

void KTerm_ExecuteSaveCursor(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    KTerm_ExecuteSaveCursor_Internal(session);
}

static void KTerm_ExecuteRestoreCursor_Internal(KTermSession* session) {
    if (session->saved_cursor_valid) {
        // Restore Position
        session->cursor.x = session->saved_cursor.x;
        session->cursor.y = session->saved_cursor.y;

        // Restore Modes
        if (session->saved_cursor.origin_mode) session->dec_modes |= KTERM_MODE_DECOM; else session->dec_modes &= ~KTERM_MODE_DECOM;
        if (session->saved_cursor.auto_wrap_mode) session->dec_modes |= KTERM_MODE_DECAWM; else session->dec_modes &= ~KTERM_MODE_DECAWM;

        // Restore Attributes
        session->current_fg = session->saved_cursor.fg_color;
        session->current_bg = session->saved_cursor.bg_color;
        session->current_attributes = session->saved_cursor.attributes;

        // Restore Charset
        session->charset = session->saved_cursor.charset;
    }
}

void KTerm_ExecuteRestoreCursor(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Restore cursor (non-ANSI.SYS)
    // This is the VT terminal version, not ANSI.SYS
    // Restores cursor from per-session saved state
    KTerm_ExecuteRestoreCursor_Internal(session);
}

void ExecuteDECREQTPARM(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Request KTerm Parameters
    int parm = KTerm_GetCSIParam(term, session, 0, 0);

    char response[32];
    // Report terminal parameters
    // Format: CSI sol ; par ; nbits ; xspeed ; rspeed ; clkmul ; flags x
    // Simplified response with standard values
    snprintf(response, sizeof(response), "\x1B[%d;1;1;120;120;1;0x", parm + 2);
    KTerm_QueueSessionResponse(term, session, response);
}

void ExecuteDECTST(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Invoke Confidence Test
    int test = KTerm_GetCSIParam(term, session, 0, 0);

    switch (test) {
        case 1: // Power-up self test
        case 2: // Data loop back test
        case 3: // EIA loop back test
        case 4: // Printer port loop back test
            // These tests would require hardware access
            // For software terminal, just acknowledge
            if (session->options.debug_sequences) {
                char debug_msg[64];
                snprintf(debug_msg, sizeof(debug_msg), "DECTST test %d - not applicable", test);
                KTerm_LogUnsupportedSequence(term, debug_msg);
            }
            break;

        default:
            if (session->options.debug_sequences) {
                char debug_msg[64];
                snprintf(debug_msg, sizeof(debug_msg), "Unknown DECTST test: %d", test);
                KTerm_LogUnsupportedSequence(term, debug_msg);
            }
            break;
    }
}

void ExecuteDECVERP(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Verify Parity
    // DECVERP - Enable/disable parity verification
    // Not applicable to software terminals
    if (session->options.debug_sequences) {
        KTerm_LogUnsupportedSequence(term, "DECVERP - parity verification not applicable");
    }
}

// =============================================================================
// COMPREHENSIVE CSI SEQUENCE PROCESSING
// =============================================================================

// Complete the missing API functions from earlier phases
void ExecuteTBC(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Tab Clear
    int n = KTerm_GetCSIParam(term, session, 0, 0);

    switch (n) {
        case 0: // Clear tab stop at current column
            KTerm_ClearTabStop(term, session->cursor.x);
            break;
        case 3: // Clear all tab stops
            KTerm_ClearAllTabStops(term);
            break;
    }
}

void ExecuteCTC(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term); // Cursor Tabulation Control
    int n = KTerm_GetCSIParam(term, session, 0, 0);

    switch (n) {
        case 0: // Set tab stop at current column
            KTerm_SetTabStop(term, session->cursor.x);
            break;
        case 2: // Clear tab stop at current column
            KTerm_ClearTabStop(term, session->cursor.x);
            break;
        case 5: // Clear all tab stops
            KTerm_ClearAllTabStops(term);
            // DECST8C (CSI ? 5 W) - Also set tabs every 8 columns if Private Mode (which ExecuteCTC implies here)
            // But CTC case 5 standard is just "Clear all".
            // However, KTerm calls ExecuteCTC only for private mode CSI ? ... W.
            // So case 5 here IS DECST8C.
            for (int i = 8; i < term->width; i += 8) {
                KTerm_SetTabStop(term, i);
            }
            break;
    }
}

void ExecuteDECSN(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    int session_id = KTerm_GetCSIParam(term, session, 0, 0);
    // If param is omitted (0 returned by KTerm_GetCSIParam if 0 is default), VT520 DECSN usually defaults to 1.
    if (session_id == 0) session_id = 1;

    // Use max_session_count from features if set, otherwise default to MAX_SESSIONS (or 1 if single session)
    // Actually we should rely on max_session_count. If 0 (uninitialized safety), default to 1.
    int limit = session->conformance.max_session_count;
    if (limit == 0) limit = 1;
    if (limit > MAX_SESSIONS) limit = MAX_SESSIONS;

    if (session_id >= 1 && session_id <= limit) {
        // Respect Multi-Session Mode Lock
        if (!(session->conformance.features & KTERM_FEATURE_MULTI_SESSION_MODE)) {
            // If disabled, ignore request (or log it)
            if (session->options.debug_sequences) {
                char msg[64];
                snprintf(msg, sizeof(msg), "DECSN %d ignored: Multi-session mode disabled", session_id);
                KTerm_LogUnsupportedSequence(term, msg);
            }
            return;
        }

        if (term->sessions[session_id - 1].session_open) {
            KTerm_SetActiveSession(term, session_id - 1);
        } else {
            if (session->options.debug_sequences) {
                char msg[64];
                snprintf(msg, sizeof(msg), "DECSN %d ignored: Session not open", session_id);
                KTerm_LogUnsupportedSequence(term, msg);
            }
        }
    }
}

// New ExecuteCSI_Dollar for multi-byte CSI $ sequences
void ExecuteCSI_Dollar(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    // This function is now the central dispatcher for sequences with a '$' intermediate.
    // It looks for the character *after* the '$'.
    char* dollar_ptr = strchr(session->escape_buffer, '$');
    if (dollar_ptr && *(dollar_ptr + 1) != '\0') {
        char final_char = *(dollar_ptr + 1);
        switch (final_char) {
            case 'v':
                KTerm_ExecuteRectangularOps(term, session);
                break;
            case 'w':
                ExecuteDECRQCRA(term, session);
                break;
            case 'x':
                // DECERA and DECFRA share the same sequence ending ($x),
                // but are distinguished by parameter count.
                if (session->param_count == 4) {
                    ExecuteDECERA(term, session);
                } else if (session->param_count == 5) {
                    ExecuteDECFRA(term, session);
                } else {
                    KTerm_LogUnsupportedSequence(term, "Invalid parameters for DECERA/DECFRA");
                }
                break;
            case '{':
                ExecuteDECSERA(term, session);
                break;
            case 'u':
                ExecuteDECRQPSR(term, session);
                break;
            case 'q':
                ExecuteDECRQM(term, session);
                break;
            default:
                if (session->options.debug_sequences) {
                    char debug_msg[128];
                    snprintf(debug_msg, sizeof(debug_msg), "Unknown CSI $ sequence with final char '%c'", final_char);
                    KTerm_LogUnsupportedSequence(term, debug_msg);
                }
                break;
        }
    } else {
         if (session->options.debug_sequences) {
            char debug_msg[MAX_COMMAND_BUFFER + 64];
            snprintf(debug_msg, sizeof(debug_msg), "Malformed CSI $ sequence in buffer: %s", session->escape_buffer);
            KTerm_LogUnsupportedSequence(term, debug_msg);
        }
    }
}

void KTerm_ProcessCSIChar(KTerm* term, KTermSession* session, unsigned char ch) {
    if (session->parse_state != PARSE_CSI) return;

    bool is_final = (ch >= 0x40 && ch <= 0x7E);
    // DECSKCV (CSI Ps SP =) uses '=' as Final Byte if preceded by Space.
    if (ch == '=' && session->escape_pos >= 1 && session->escape_buffer[session->escape_pos - 1] == ' ') {
        is_final = true;
    }

    if (is_final) {
        // Parse parameters into session->escape_params
        KTerm_ParseCSIParams_Internal(session, session->escape_buffer, NULL, MAX_ESCAPE_PARAMS);

        // Handle DECSCUSR (CSI Ps SP q)
        if (ch == 'q' && session->escape_pos >= 1 && session->escape_buffer[session->escape_pos - 1] == ' ') {
            ExecuteDECSCUSR_Internal(term, session);
        } else {
            // Dispatch to KTerm_ExecuteCSICommand
            // KTerm_ExecuteCSICommand(term, ch);
            // We should use an internal version, but for now calling the public one which uses GET_SESSION.
            // This is UNSAFE if active_session changed.
            // But implementing the full tree refactor is immense.
            // I will assume for now that CSI commands do NOT change session, except explicit session commands?
            // DECSN is a CSI command (CSI Ps $ |).
            // If DECSN changes active_session inside ExecuteCSICommand, that is fine for THAT function.
            // The issue is if ProcessEvents loop continues with OLD session pointer but NEW active_session index.
            // We fixed ProcessEvents to stick to the OLD session pointer.
            // So subsequent calls in ProcessEvents use the correct session.
            // What if ExecuteCSICommand calls something that uses session?
            // It will use the NEW session.
            // If DECSN switches session, we WANT subsequent logic in ExecuteCSICommand (if any) to affect... which session?
            // DECSN is usually final.
            // I'll call KTerm_ExecuteCSICommand(term, ch). It uses GET_SESSION.
            // I will refactor KTerm_ExecuteCSICommand to use session later if I can.
            KTerm_ExecuteCSICommand(term, session, ch);
        }

        // Reset parser state
        session->parse_state = VT_PARSE_NORMAL;
        ClearCSIParams(session);
    } else if (ch >= 0x20 && ch <= 0x3F) {
        // Accumulate intermediate characters (e.g., digits, ';', '?')
        // Phase 7.2: Harden Escape Buffers (Bounds Check)
        if (session->escape_pos < MAX_COMMAND_BUFFER - 1) {
            session->escape_buffer[session->escape_pos++] = ch;
            session->escape_buffer[session->escape_pos] = '\0';
        } else {
            KTerm_LogUnsupportedSequence(term, "CSI escape buffer overflow");
            session->parse_state = VT_PARSE_NORMAL;
            ClearCSIParams(session);
        }
    } else if (ch == '$') {
        // Handle multi-byte CSI sequences (e.g., CSI $ q, CSI $ u)
        if (session->escape_pos < MAX_COMMAND_BUFFER - 1) {
            session->escape_buffer[session->escape_pos++] = ch;
            session->escape_buffer[session->escape_pos] = '\0';
        } else {
            KTerm_LogUnsupportedSequence(term, "CSI escape buffer overflow");
            session->parse_state = VT_PARSE_NORMAL;
            ClearCSIParams(session);
        }
    } else {
        // Invalid character
        if (session->options.debug_sequences) {
            snprintf(session->conformance.compliance.last_unsupported,
                     sizeof(session->conformance.compliance.last_unsupported),
                     "Invalid CSI char: 0x%02X", ch);
            session->conformance.compliance.unsupported_sequences++;
        }
        session->parse_state = VT_PARSE_NORMAL;
        ClearCSIParams(session);
    }
}

// Updated KTerm_ExecuteCSICommand
void KTerm_ExecuteCSICommand(KTerm* term, KTermSession* session, unsigned char command) {
    if (!session) session = GET_SESSION(term);
    bool private_mode = (session->escape_buffer[0] == '?');

    // Handle CSI ... SP q (DECSCUSR with space intermediate)
    if (command == 'q' && strstr(session->escape_buffer, " ")) {
        ExecuteDECSCUSR(term, session);
        return;
    }



    switch (command) {
        case '$': // L_CSI_dollar_multi
            ExecuteCSI_Dollar(term, session);
            // Multi-byte CSI sequences (e.g., CSI $ q, CSI $ u)
            break;
        case '@': // L_CSI_at_ASC
            ExecuteICH(term, session);
            // ICH - Insert Character(s) (CSI Pn @)
            break;
        case 'A': // L_CSI_A_CUU
            ExecuteCUU(term, session);
            // CUU - Cursor Up (CSI Pn A)
            break;
        case 'B': // L_CSI_B_CUD
            ExecuteCUD(term, session);
            // CUD - Cursor Down (CSI Pn B)
            break;
        case 'C': // L_CSI_C_CUF
            ExecuteCUF(term, session);
            // CUF - Cursor Forward (CSI Pn C)
            break;
        case 'D': // L_CSI_D_CUB
            ExecuteCUB(term, session);
            // CUB - Cursor Back (CSI Pn D)
            break;
        case 'E': // L_CSI_E_CNL
            ExecuteCNL(term, session);
            // CNL - Cursor Next Line (CSI Pn E)
            break;
        case 'F': // L_CSI_F_CPL
            ExecuteCPL(term, session);
            // CPL - Cursor Previous Line (CSI Pn F)
            break;
        case 'G': // L_CSI_G_CHA
            ExecuteCHA(term, session);
            // CHA - Cursor Horizontal Absolute (CSI Pn G)
            break;
        case 'H': // L_CSI_H_CUP
            ExecuteCUP(term, session);
            // CUP - Cursor Position (CSI Pn ; Pn H)
            break;
        case 'I': // L_CSI_I_CHT
            { int n=KTerm_GetCSIParam(term, session, 0,1); while(n-->0) session->cursor.x = NextTabStop(term, session->cursor.x); if (session->cursor.x >= term->width) session->cursor.x = term->width -1; }
            // CHT - Cursor Horizontal Tab (CSI Pn I)
            break;
        case 'i': // L_CSI_i_MC
            ExecuteMC(term, session);
            // MC  - Media Copy (CSI Pn i) / DEC Printer Control
            break;
        case 'J': // L_CSI_J_ED
            ExecuteED(term, private_mode);
            // ED  - Erase in Display (CSI Pn J) / DECSED (CSI ? Pn J)
            break;
        case 'K': // L_CSI_K_EL
            ExecuteEL(term, private_mode);
            // EL  - Erase in Line (CSI Pn K) / DECSEL (CSI ? Pn K)
            break;
        case 'L': // L_CSI_L_IL
            ExecuteIL(term, session);
            // IL  - Insert Line(s) (CSI Pn L)
            break;
        case 'M': // L_CSI_M_DL
            ExecuteDL(term, session);
            // DL  - Delete Line(s) (CSI Pn M)
            break;
        case 'P': // L_CSI_P_DCH
            ExecuteDCH(term, session);
            // DCH - Delete Character(s) (CSI Pn P)
            break;
        case 'S': // L_CSI_S_SU
            ExecuteSU(term, session);
            // SU  - Scroll Up (CSI Pn S)
            break;
        case 'T': // L_CSI_T_SD
            ExecuteSD(term, session);
            // SD  - Scroll Down (CSI Pn T)
            break;
        case 'W': // L_CSI_W_CTC_etc
            if(private_mode) ExecuteCTC(term, session); else KTerm_LogUnsupportedSequence(term, "CSI W (non-private)");
            // CTC - Cursor Tab Control (CSI ? Ps W)
            break;
        case 'X': // L_CSI_X_ECH
            ExecuteECH(term, session);
            // ECH - Erase Character(s) (CSI Pn X)
            break;
        case 'Z': // L_CSI_Z_CBT
            { int n=KTerm_GetCSIParam(term, session, 0,1); while(n-->0) session->cursor.x = PreviousTabStop(term, session->cursor.x); }
            // CBT - Cursor Backward Tab (CSI Pn Z)
            break;
        case '`': // L_CSI_tick_HPA
            ExecuteCHA(term, session);
            // HPA - Horizontal Position Absolute (CSI Pn `) (Same as CHA)
            break;
        case 'a': // L_CSI_a_HPR
            { int n=KTerm_GetCSIParam(term, session, 0,1); session->cursor.x+=n; if(session->cursor.x<0)session->cursor.x=0; if(session->cursor.x>=term->width)session->cursor.x=term->width-1;}
            // HPR - Horizontal Position Relative (CSI Pn a)
            break;
        case 'b': // L_CSI_b_REP
            ExecuteREP(term, session);
            // REP - Repeat Preceding Graphic Character (CSI Pn b)
            break;
        case 'c': // L_CSI_c_DA
            ExecuteDA(term, private_mode);
            // DA  - Device Attributes (CSI Ps c or CSI ? Ps c)
            break;
        case 'd': // L_CSI_d_VPA
            ExecuteVPA(term, session);
            // VPA - Vertical Line Position Absolute (CSI Pn d)
            break;
        case 'e': // L_CSI_e_VPR
            { int n=KTerm_GetCSIParam(term, session, 0,1); session->cursor.y+=n; if(session->cursor.y<0)session->cursor.y=0; if(session->cursor.y>=term->height)session->cursor.y=term->height-1;}
            // VPR - Vertical Position Relative (CSI Pn e)
            break;
        case 'f': // L_CSI_f_HVP
            ExecuteCUP(term, session);
            // HVP - Horizontal and Vertical Position (CSI Pn ; Pn f) (Same as CUP)
            break;
        case 'g': // L_CSI_g_TBC
            ExecuteTBC(term, session);
            // TBC - Tabulation Clear (CSI Ps g)
            break;
        case 'h': // L_CSI_h_SM
            ExecuteSM(term, session, private_mode);
            // SM  - Set Mode (CSI ? Pm h or CSI Pm h)
            break;
        case 'j': // L_CSI_j_HPB
            ExecuteCUB(term, session);
            // HPB - Horizontal Position Backward (like CUB) (CSI Pn j)
            break;
        case 'k': // L_CSI_k_VPB
            ExecuteCUU(term, session);
            // VPB - Vertical Position Backward (like CUU) (CSI Pn k)
            break;
        case 'l': // L_CSI_l_RM
            ExecuteRM(term, session, private_mode);
            // RM  - Reset Mode (CSI ? Pm l or CSI Pm l)
            break;
        case 'm': // L_CSI_m_SGR
            ExecuteSGR(term, session);
            // SGR - Select Graphic Rendition (CSI Pm m)
            break;
        case 'n': // L_CSI_n_DSR
            ExecuteDSR(term, session);
            // DSR - Device Status Report (CSI Ps n or CSI ? Ps n)
            break;
        case 'o': // L_CSI_o_VT420
            if(session->options.debug_sequences) KTerm_LogUnsupportedSequence(term, "VT420 'o'");
            // DECDMAC, etc. (CSI Pt;Pb;Pl;Pr;Pp;Pattr o)
            break;
        case 'p': // L_CSI_p_DECSOFT_etc
            ExecuteCSI_P(term, session);
            // Various 'p' suffixed: DECSTR, DECSCL, DECRQM, DECUDK (CSI ! p, CSI " p, etc.)
            break;
        case 'q': // L_CSI_q_DECLL_DECSCUSR
            if(strstr(session->escape_buffer, "\"")) ExecuteDECSCA(term, session); else if(private_mode) ExecuteDECLL(term, session); else ExecuteDECSCUSR(term, session);
            // DECSCA / DECLL / DECSCUSR
            break;
        case 'r': // L_CSI_r_DECSTBM
            if(strstr(session->escape_buffer, "$")) ExecuteDECCARA(term, session);
            else if(strchr(session->escape_buffer, ' ')) ExecuteDECARR(term, session);
            else if(!private_mode) ExecuteDECSTBM(term, session); else KTerm_LogUnsupportedSequence(term, "CSI ? r invalid");
            // DECSTBM - Set Top/Bottom Margins (CSI Pt ; Pb r)
            break;
        case 's': // L_CSI_s_SAVRES_CUR
            // If DECLRMM is enabled (CSI ? 69 h), CSI Pl ; Pr s sets margins (DECSLRM).
            // Otherwise, it saves the cursor (SCOSC/ANSISYSSC).
            if ((session->dec_modes & KTERM_MODE_DECLRMM)) {
                if((session->conformance.features & KTERM_FEATURE_VT420_MODE)) ExecuteDECSLRM(term, session);
                else KTerm_LogUnsupportedSequence(term, "DECSLRM requires VT420");
            } else {
                KTerm_ExecuteSaveCursor(term, session);
            }
            break;
        case 't': // L_CSI_t_WINMAN
            if(strstr(session->escape_buffer, "$")) ExecuteDECRARA(term, session);
            else ExecuteWindowOps(term, session);
            // Window Manipulation (xterm) / DECSLPP (Set lines per page) (CSI Ps t) / DECRARA
            break;
        case 'u': // L_CSI_u_RES_CUR
            // Kitty Keyboard Protocol (CSI > u, CSI < u, CSI = u, CSI ? u)
            if (session->escape_buffer[0] == '>') {
                int flags = KTerm_GetCSIParam(term, session, 0, 0);
                if (session->input.kitty_keyboard_stack_depth < 16) {
                    session->input.kitty_keyboard_stack[session->input.kitty_keyboard_stack_depth++] = session->input.kitty_keyboard_flags;
                }
                session->input.kitty_keyboard_flags = flags;
            } else if (session->escape_buffer[0] == '<') {
                if (session->input.kitty_keyboard_stack_depth > 0) {
                    session->input.kitty_keyboard_flags = session->input.kitty_keyboard_stack[--session->input.kitty_keyboard_stack_depth];
                } else {
                    session->input.kitty_keyboard_flags = 0;
                }
            } else if (session->escape_buffer[0] == '=') {
                int flags = KTerm_GetCSIParam(term, session, 0, 0);
                int mode = KTerm_GetCSIParam(term, session, 1, 1);
                switch(mode) {
                    case 1: session->input.kitty_keyboard_flags = flags; break;
                    case 2: session->input.kitty_keyboard_flags |= flags; break;
                    case 3: session->input.kitty_keyboard_flags &= ~flags; break;
                }
            } else if(strstr(session->escape_buffer, "$")) {
                if (private_mode) ExecuteDECRQTSR(term, session);
                else ExecuteDECRARA(term, session);
            }
            else if(private_mode) {
                if (session->param_count == 0) {
                     char resp[64];
                     snprintf(resp, sizeof(resp), "\x1B[?%du", session->input.kitty_keyboard_flags);
                     KTerm_QueueSessionResponse(term, session, resp);
                } else {
                     ExecuteDECRQPKU(term, session);
                }
            }
            else KTerm_ExecuteRestoreCursor(term, session);
            // Restore Cursor (ANSI.SYS) (CSI u) / DECRARA / DECRQPKU / DECRQTSR / Kitty
            break;
        case 'v': // L_CSI_v_RECTCOPY
            if(strstr(session->escape_buffer, "$")) KTerm_ExecuteRectangularOps(term, session); else KTerm_LogUnsupportedSequence(term, "CSI v non-private invalid");
            // DECCRA
            break;
        case 'w': // L_CSI_w_RECTCHKSUM
            if(private_mode) KTerm_ExecuteRectangularOps2(term, session); else KTerm_LogUnsupportedSequence(term, "CSI w non-private invalid");
            // Note: DECRQCRA moved to 'y' with * intermediate as per standard.
            break;
        case 'x': // L_CSI_x_DECREQTPARM
            if(strstr(session->escape_buffer, "$")) ExecuteDECFRA(term, session); else ExecuteDECREQTPARM(term, session);
            // DECFRA / DECREQTPARM
            break;
        case 'y': // L_CSI_y_DECTST
            // DECRQCRA is CSI ... * y (Checksum Rectangular Area)
            if(strstr(session->escape_buffer, "*")) ExecuteDECRQCRA(term, session); else ExecuteDECTST(term, session);
            // DECTST / DECRQCRA
            break;
        case 'z': // L_CSI_z_DECVERP
            if(strstr(session->escape_buffer, "$")) ExecuteDECERA(term, session);
            else if(private_mode) ExecuteDECVERP(term, session);
            else ExecuteDECECR(term, session); // CSI Pt ; Pc z (Enable Checksum Reporting)
            break;
        case '}': // L_CSI_RSBrace_VT420
            if (strstr(session->escape_buffer, "#")) { ExecuteXTPOPSGR(term, session); }
            else if (strstr(session->escape_buffer, "$")) { ExecuteDECSASD(term, session); }
            else { KTerm_LogUnsupportedSequence(term, "CSI } invalid"); }
            break;
        case '~': // L_CSI_Tilde_VT420
            if (strstr(session->escape_buffer, "!")) { ExecuteDECSN(term, session); } else if (strstr(session->escape_buffer, "$")) { ExecuteDECSSDT(term, session); } else { KTerm_LogUnsupportedSequence(term, "CSI ~ invalid"); }
            break;

        case '=': // L_CSI_Equal_DECSKCV
            if (strchr(session->escape_buffer, ' ')) ExecuteDECSKCV(term, session);
            else KTerm_LogUnsupportedSequence(term, "CSI = invalid");
            break;

        case '{': // L_CSI_LSBrace_DECSLE
            if (strstr(session->escape_buffer, "#")) { ExecuteXTPUSHSGR(term, session); }
            else if(strstr(session->escape_buffer, "$")) ExecuteDECSERA(term, session);
            else if(strstr(session->escape_buffer, "*")) ExecuteDECSLPP(term, session);
            else ExecuteDECSLE(term, session);
            // DECSERA / DECSLE / XTPUSHSGR / DECSLPP
            break;
        case '|': // L_CSI_Pipe_DECRQLP
            if (strchr(session->escape_buffer, '$')) {
                ExecuteDECSCPP(term, session);
            } else if (strchr(session->escape_buffer, '*')) {
                ExecuteDECSNLS(term, session);
            } else {
                ExecuteDECRQLP(term, session);
            }
            // DECRQLP - Request Locator Position (CSI Plc |)
            // DECSCPP - Select 80/132 Columns (CSI Pn $ |)
            // DECSNLS - Set Number of Lines per Screen (CSI Ps * |)
            break;
        default:
            if (session->options.debug_sequences) {
                char debug_msg[128];
                snprintf(debug_msg, sizeof(debug_msg),
                         "Unknown CSI %s%c (0x%02X)", private_mode ? "?" : "", command, command);
                KTerm_LogUnsupportedSequence(term, debug_msg);
            }
            session->conformance.compliance.unsupported_sequences++;
            break;
    }
}

// =============================================================================
// OSC (OPERATING SYSTEM COMMAND) PROCESSING
// =============================================================================


void KTerm_SetWindowTitle(KTerm* term, const char* title) {
    strncpy(GET_SESSION(term)->title.window_title, title, MAX_TITLE_LENGTH - 1);
    GET_SESSION(term)->title.window_title[MAX_TITLE_LENGTH - 1] = '\0';
    GET_SESSION(term)->title.title_changed = true;

    if (term->title_callback) {
        term->title_callback(term, GET_SESSION(term)->title.window_title, false);
    }

    // Also set KTerm window title
    KTerm_SetWindowTitlePlatform(GET_SESSION(term)->title.window_title);
}

void KTerm_SetIconTitle(KTerm* term, const char* title) {
    strncpy(GET_SESSION(term)->title.icon_title, title, MAX_TITLE_LENGTH - 1);
    GET_SESSION(term)->title.icon_title[MAX_TITLE_LENGTH - 1] = '\0';
    GET_SESSION(term)->title.icon_changed = true;

    if (term->title_callback) {
        term->title_callback(term, GET_SESSION(term)->title.icon_title, true);
    }
}

void ResetForegroundKTermColor(KTerm* term) {
    GET_SESSION(term)->current_fg.color_mode = 0;
    GET_SESSION(term)->current_fg.value.index = COLOR_WHITE;
}

void ResetBackgroundKTermColor(KTerm* term) {
    GET_SESSION(term)->current_bg.color_mode = 0;
    GET_SESSION(term)->current_bg.value.index = COLOR_BLACK;
}

void ResetCursorKTermColor(KTerm* term) {
    GET_SESSION(term)->cursor.color.color_mode = 0;
    GET_SESSION(term)->cursor.color.value.index = COLOR_WHITE;
}

void ProcessKTermColorCommand(KTerm* term, KTermSession* session, const char* data) {
    // Format: color_index;rgb:rr/gg/bb or color_index;? (Supports multiple pairs)
    StreamScanner scanner = { .ptr = data, .len = strlen(data), .pos = 0 };

    while (scanner.pos < scanner.len) {
        int color_index;
        if (!Stream_ReadInt(&scanner, &color_index)) break;
        if (!Stream_Expect(&scanner, ';')) break;

        if (Stream_Peek(&scanner) == '?') {
            Stream_Consume(&scanner); // Consume '?'
            // Query color
            char response[128];
            if (color_index >= 0 && color_index < 256) {
                RGB_KTermColor c = term->color_palette[color_index];
                snprintf(response, sizeof(response), "\x1B]4;%d;rgb:%02x/%02x/%02x\x1B\\",
                        color_index, c.r, c.g, c.b);
                KTerm_QueueSessionResponse(term, session, response);
            }
        } else if (Stream_MatchToken(&scanner, "rgb")) {
            // Set color: rgb:rr/gg/bb
            if (!Stream_Expect(&scanner, ':')) break;
            unsigned int r, g, b;
            if (Stream_ReadHex(&scanner, &r) && Stream_Expect(&scanner, '/') &&
                Stream_ReadHex(&scanner, &g) && Stream_Expect(&scanner, '/') &&
                Stream_ReadHex(&scanner, &b)) {
                if (color_index >= 0 && color_index < 256) {
                    term->color_palette[color_index] = (RGB_KTermColor){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
                }
            }
        } else {
            // Unknown spec format, skip to next semicolon or end
            while (scanner.pos < scanner.len && Stream_Peek(&scanner) != ';') {
                Stream_Consume(&scanner);
            }
        }

        // Consume optional semicolon for next pair
        if (Stream_Peek(&scanner) == ';') {
            Stream_Consume(&scanner);
        }
    }
}

// Additional helper functions for OSC commands
void ResetKTermColorPalette(KTerm* term, const char* data) {
    if (!data || strlen(data) == 0) {
        // Reset entire palette
        KTerm_InitKTermColorPalette(term);
    } else {
        // Reset specific colors (semicolon-separated list)
        KTermLexer lexer;
        KTerm_LexerInit(&lexer, data);
        KTermToken token = KTerm_LexerNext(&lexer);

        while (token.type != KT_TOK_EOF) {
            if (token.type == KT_TOK_NUMBER) {
                int color_index = token.value.i;
                if (color_index >= 0 && color_index < 16) {
                    // Reset to default ANSI color
                    term->color_palette[color_index] = (RGB_KTermColor){
                        ansi_colors[color_index].r,
                        ansi_colors[color_index].g,
                        ansi_colors[color_index].b,
                        255
                    };
                }
            }
            token = KTerm_LexerNext(&lexer);
        }
    }
}

void ProcessForegroundKTermColorCommand(KTerm* term, KTermSession* session, const char* data) {
    StreamScanner scanner = { .ptr = data, .len = strlen(data), .pos = 0 };

    if (Stream_Peek(&scanner) == '?') {
        // Query foreground color
        char response[64];
        ExtendedKTermColor fg = GET_SESSION(term)->current_fg;
        if (fg.color_mode == 0 && fg.value.index < 16) {
            RGB_KTermColor c = term->color_palette[fg.value.index];
            snprintf(response, sizeof(response), "\x1B]10;rgb:%02x/%02x/%02x\x1B\\", c.r, c.g, c.b);
        } else if (fg.color_mode == 1) {
            snprintf(response, sizeof(response), "\x1B]10;rgb:%02x/%02x/%02x\x1B\\",
                    fg.value.rgb.r, fg.value.rgb.g, fg.value.rgb.b);
        }
        KTerm_QueueSessionResponse(term, session, response);
    } else if (Stream_MatchToken(&scanner, "rgb")) {
         if (!Stream_Expect(&scanner, ':')) return;
         unsigned int r, g, b;
         if (Stream_ReadHex(&scanner, &r) && Stream_Expect(&scanner, '/') &&
             Stream_ReadHex(&scanner, &g) && Stream_Expect(&scanner, '/') &&
             Stream_ReadHex(&scanner, &b)) {
             GET_SESSION(term)->current_fg.color_mode = 1;
             GET_SESSION(term)->current_fg.value.rgb = (RGB_KTermColor){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
         }
    }
}

void ProcessBackgroundKTermColorCommand(KTerm* term, KTermSession* session, const char* data) {
    StreamScanner scanner = { .ptr = data, .len = strlen(data), .pos = 0 };

    if (Stream_Peek(&scanner) == '?') {
        // Query background color
        char response[64];
        ExtendedKTermColor bg = GET_SESSION(term)->current_bg;
        if (bg.color_mode == 0 && bg.value.index < 16) {
            RGB_KTermColor c = term->color_palette[bg.value.index];
            snprintf(response, sizeof(response), "\x1B]11;rgb:%02x/%02x/%02x\x1B\\", c.r, c.g, c.b);
        } else if (bg.color_mode == 1) {
            snprintf(response, sizeof(response), "\x1B]11;rgb:%02x/%02x/%02x\x1B\\",
                    bg.value.rgb.r, bg.value.rgb.g, bg.value.rgb.b);
        }
        KTerm_QueueSessionResponse(term, session, response);
    } else if (Stream_MatchToken(&scanner, "rgb")) {
         if (!Stream_Expect(&scanner, ':')) return;
         unsigned int r, g, b;
         if (Stream_ReadHex(&scanner, &r) && Stream_Expect(&scanner, '/') &&
             Stream_ReadHex(&scanner, &g) && Stream_Expect(&scanner, '/') &&
             Stream_ReadHex(&scanner, &b)) {
             GET_SESSION(term)->current_bg.color_mode = 1;
             GET_SESSION(term)->current_bg.value.rgb = (RGB_KTermColor){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
         }
    }
}

void ProcessCursorKTermColorCommand(KTerm* term, KTermSession* session, const char* data) {
    StreamScanner scanner = { .ptr = data, .len = strlen(data), .pos = 0 };

    if (Stream_Peek(&scanner) == '?') {
        // Query cursor color
        char response[64];
        ExtendedKTermColor cursor_color = GET_SESSION(term)->cursor.color;
        if (cursor_color.color_mode == 0 && cursor_color.value.index < 16) {
            RGB_KTermColor c = term->color_palette[cursor_color.value.index];
            snprintf(response, sizeof(response), "\x1B]12;rgb:%02x/%02x/%02x\x1B\\", c.r, c.g, c.b);
        } else if (cursor_color.color_mode == 1) {
            snprintf(response, sizeof(response), "\x1B]12;rgb:%02x/%02x/%02x\x1B\\",
                    cursor_color.value.rgb.r, cursor_color.value.rgb.g, cursor_color.value.rgb.b);
        }
        KTerm_QueueSessionResponse(term, session, response);
    } else if (Stream_MatchToken(&scanner, "rgb")) {
         if (!Stream_Expect(&scanner, ':')) return;
         unsigned int r, g, b;
         if (Stream_ReadHex(&scanner, &r) && Stream_Expect(&scanner, '/') &&
             Stream_ReadHex(&scanner, &g) && Stream_Expect(&scanner, '/') &&
             Stream_ReadHex(&scanner, &b)) {
             GET_SESSION(term)->cursor.color.color_mode = 1;
             GET_SESSION(term)->cursor.color.value.rgb = (RGB_KTermColor){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
         }
    }
}

void ProcessFontCommand(KTerm* term, const char* data) {
    if (data[0] == '?') {
        // Query not supported yet
        return;
    }
    // Set font
    KTerm_LoadFont(term, data);
}

// Base64 decoding helper
static int Base64Val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static int DecodeBase64(const char* input, unsigned char* output, size_t out_max) {
    size_t in_len = strlen(input);
    size_t out_len = 0;
    unsigned int val = 0;
    int valb = -8;
    for (size_t i = 0; i < in_len; i++) {
        int c = Base64Val(input[i]);
        if (c == -1) continue; // Skip whitespace/invalid
        val = (val << 6) | c;
        valb += 6;
        if (valb >= 0) {
            if (out_len < out_max) {
                output[out_len++] = (unsigned char)((val >> valb) & 0xFF);
            }
            valb -= 8;
        }
    }
    if (out_len < out_max) output[out_len] = 0;
    return (int)out_len;
}

static void EncodeBase64(const unsigned char* input, size_t input_len, char* output, size_t out_max) {
    const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_pos = 0;
    unsigned int val = 0;
    int valb = -6;
    for (size_t i = 0; i < input_len; i++) {
        val = (val << 8) | input[i];
        valb += 8;
        while (valb >= 0) {
            if (out_pos < out_max) {
                output[out_pos++] = base64_chars[(val >> valb) & 0x3F];
            }
            valb -= 6;
        }
    }
    if (valb > -6) {
        if (out_pos < out_max) {
            output[out_pos++] = base64_chars[((val << 8) >> (valb + 8)) & 0x3F];
        }
    }
    while (out_pos % 4) {
        if (out_pos < out_max) output[out_pos++] = '=';
    }
    if (out_pos < out_max) output[out_pos] = 0;
}

void ProcessClipboardCommand(KTerm* term, KTermSession* session, const char* data) {
    // Clipboard operations: c;base64data or c;?
    // data format is: Pc;Pd
    // Pc = clipboard selection (c, p, s, 0-7)
    // Pd = data (base64) or ?

    StreamScanner scanner = { .ptr = data, .len = strlen(data), .pos = 0 };

    size_t pc_start = scanner.pos;
    while (scanner.pos < scanner.len && Stream_Peek(&scanner) != ';') Stream_Consume(&scanner);

    if (!Stream_Expect(&scanner, ';')) return;

    char clipboard_selector = data[pc_start];

    if (Stream_Peek(&scanner) == '?') {
        // Query clipboard
        const char* clipboard_text = NULL;
        if (KTerm_GetClipboardText(&clipboard_text) == KTERM_SUCCESS && clipboard_text) {
            size_t text_len = strlen(clipboard_text);
            size_t encoded_len = 4 * ((text_len + 2) / 3) + 1;
            char* encoded_data = KTerm_Malloc(encoded_len);
            if (encoded_data) {
                EncodeBase64((const unsigned char*)clipboard_text, text_len, encoded_data, encoded_len);
                char response_header[16];
                snprintf(response_header, sizeof(response_header), "\x1B]52;%c;", clipboard_selector);
                KTerm_QueueSessionResponse(term, session, response_header);
                KTerm_QueueSessionResponse(term, session, encoded_data);
                KTerm_QueueSessionResponse(term, session, "\x1B\\");
                KTerm_Free(encoded_data);
            }
            KTerm_FreeString((char*)clipboard_text);
        } else {
            // Empty clipboard response
            char response[16];
            snprintf(response, sizeof(response), "\x1B]52;%c;\x1B\\", clipboard_selector);
            KTerm_QueueSessionResponse(term, session, response);
        }
    } else {
        // Set clipboard data (base64 encoded)
        if (clipboard_selector == 'c' || clipboard_selector == '0') {
            const char* pd_str = scanner.ptr + scanner.pos;
            size_t decoded_size = scanner.len - scanner.pos; // Upper bound
            unsigned char* decoded_data = KTerm_Malloc(decoded_size + 1);
            if (decoded_data) {
                DecodeBase64(pd_str, decoded_data, decoded_size + 1);
                KTerm_SetClipboardText((const char*)decoded_data);
                KTerm_Free(decoded_data);
            }
        }
    }
}

void KTerm_ExecuteOSCCommand(KTerm* term, KTermSession* session) {
    StreamScanner scanner = { .ptr = session->escape_buffer, .len = strlen(session->escape_buffer), .pos = 0 };

    int command = 0;
    if (!Stream_ReadInt(&scanner, &command)) {
        KTerm_LogUnsupportedSequence(term, "Malformed OSC sequence (missing command)");
        return;
    }

    if (!Stream_Expect(&scanner, ';')) {
        // Some XTerm sequences might omit semicolon if payload empty?
        // Usually OSC Ps ; Pt ST.
        // If we hit ST or end without semicolon, it's malformed or just Ps.
        // Assuming strict for now as per previous logic.
        KTerm_LogUnsupportedSequence(term, "Malformed OSC sequence (missing semicolon)");
        return;
    }

    const char* data = scanner.ptr + scanner.pos;

    switch (command) {
        case 0: // Set window and icon title
        case 2: // Set window title
            KTerm_SetWindowTitle(term, data);
            break;

        case 1: // Set icon title
            KTerm_SetIconTitle(term, data);
            break;

        case 9: // Notification
            if (term->notification_callback) {
                term->notification_callback(term, data);
            }
            break;

        case 4: // Set/query color palette
            ProcessKTermColorCommand(term, session, data);
            break;

        case 10: // Query/set foreground color
            ProcessForegroundKTermColorCommand(term, session, data);
            break;

        case 11: // Query/set background color
            ProcessBackgroundKTermColorCommand(term, session, data);
            break;

        case 12: // Query/set cursor color
            ProcessCursorKTermColorCommand(term, session, data);
            break;

        case 50: // Set font
            ProcessFontCommand(term, data);
            break;

        case 52: // Clipboard operations
            ProcessClipboardCommand(term, session, data);
            break;

        case 104: // Reset color palette
            ResetKTermColorPalette(term, data);
            break;

        case 110: // Reset foreground color
            ResetForegroundKTermColor(term);
            break;

        case 111: // Reset background color
            ResetBackgroundKTermColor(term);
            break;

        case 112: // Reset cursor color
            ResetCursorKTermColor(term);
            break;

        default:
            if (GET_SESSION(term)->options.debug_sequences) {
                char debug_msg[128];
                snprintf(debug_msg, sizeof(debug_msg), "Unknown OSC command: %d", command);
                KTerm_LogUnsupportedSequence(term, debug_msg);
            }
            break;
    }
}

// =============================================================================
// DCS (DEVICE CONTROL STRING) PROCESSING
// =============================================================================

void ProcessTermcapRequest(KTerm* term, KTermSession* session, const char* request) {
    (void)session;
    // XTGETTCAP - Get terminal capability
    StreamScanner scanner = { .ptr = request, .len = strlen(request), .pos = 0 };
    char response[256];

    if (Stream_MatchToken(&scanner, "Co")) {
        // Number of colors
        snprintf(response, sizeof(response), "\x1BP1+r436f=323536\x1B\\"); // "256" in hex
    } else if (Stream_MatchToken(&scanner, "lines")) {
        // Number of lines
        char hex_lines[16];
        snprintf(hex_lines, sizeof(hex_lines), "%X", term->height);
        snprintf(response, sizeof(response), "\x1BP1+r6c696e6573=%s\x1B\\", hex_lines);
    } else if (Stream_MatchToken(&scanner, "cols")) {
        // Number of columns
        char hex_cols[16];
        snprintf(hex_cols, sizeof(hex_cols), "%X", term->width);
        snprintf(response, sizeof(response), "\x1BP1+r636f6c73=%s\x1B\\", hex_cols);
    } else {
        // Unknown capability
        snprintf(response, sizeof(response), "\x1BP0+r%s\x1B\\", request);
    }

    KTerm_QueueSessionResponse(term, session, response);
}

// Helper function to convert a single hex character to its integer value
static int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1; // Invalid hex char
}

void DefineUserKey(KTerm* term, KTermSession* session, int key_code, const char* sequence, size_t sequence_len) {
    (void)term;
    // Expand programmable keys array if needed
    if (session->programmable_keys.count >= session->programmable_keys.capacity) {
        size_t new_capacity = session->programmable_keys.capacity == 0 ? 16 :
                             session->programmable_keys.capacity * 2;

        ProgrammableKey* new_keys = KTerm_Realloc(session->programmable_keys.keys,
                                           new_capacity * sizeof(ProgrammableKey));
        if (!new_keys) return;

        session->programmable_keys.keys = new_keys;
        session->programmable_keys.capacity = new_capacity;
    }

    // Find existing key or add new one
    ProgrammableKey* key = NULL;
    for (size_t i = 0; i < session->programmable_keys.count; i++) {
        if (session->programmable_keys.keys[i].key_code == key_code) {
            key = &session->programmable_keys.keys[i];
            if (key->sequence) KTerm_Free(key->sequence); // Free old sequence
            break;
        }
    }

    if (!key) {
        key = &session->programmable_keys.keys[session->programmable_keys.count++];
        key->key_code = key_code;
    }

    // Store new sequence
    key->sequence_length = sequence_len;
    key->sequence = KTerm_Malloc(key->sequence_length);
    if (key->sequence) {
        memcpy(key->sequence, sequence, key->sequence_length);
    }
    key->active = true;
}

void ProcessUserDefinedKeys(KTerm* term, KTermSession* session, const char* data) {
    // Parse user defined key format: key/string;key/string;...
    // The string is a sequence of hexadecimal pairs.
    if (!(session->conformance.features & KTERM_FEATURE_USER_DEFINED_KEYS)) {
        KTerm_LogUnsupportedSequence(term, "User defined keys require VT320 mode");
        return;
    }

    StreamScanner scanner = { .ptr = data, .len = strlen(data), .pos = 0 };

    while (scanner.pos < scanner.len) {
        int key_code = 0;
        if (!Stream_ReadInt(&scanner, &key_code)) break; // Or skip to next semicolon?

        if (!Stream_Expect(&scanner, '/')) {
             // Skip until semicolon or end
             while(scanner.pos < scanner.len && Stream_Peek(&scanner) != ';') Stream_Consume(&scanner);
        } else {
             // Parse Hex String
             // Use a dynamic buffer or temp buffer? 
             // We can just iterate and decode directly using Stream_ReadHex (which reads int, but we need 2 digits).
             // Actually, the previous implementation scanned first. Let's do robust scanning.
             
             // We'll use a temporary buffer. 
             // Since we don't know the length, we can't malloc easily without scanning.
             // But we can decode in place if we don't need the source anymore (we do, const char).
             
             // Two-pass approach: Scan length, then decode.
             size_t start_pos = scanner.pos;
             while(scanner.pos < scanner.len && isxdigit((unsigned char)Stream_Peek(&scanner))) {
                 Stream_Consume(&scanner);
             }
             size_t hex_len = scanner.pos - start_pos;
             
             if (hex_len % 2 != 0) {
                  KTerm_LogUnsupportedSequence(term, "Invalid hex string in DECUDK (odd length)");
             } else if (hex_len > 0) {
                  size_t decoded_len = hex_len / 2;
                  char* decoded_sequence = KTerm_Malloc(decoded_len);
                  if (decoded_sequence) {
                       for (size_t i = 0; i < decoded_len; i++) {
                            int high = hex_char_to_int(scanner.ptr[start_pos + i * 2]);
                            int low = hex_char_to_int(scanner.ptr[start_pos + i * 2 + 1]);
                            decoded_sequence[i] = (char)((high << 4) | low);
                       }
                       DefineUserKey(term, session, key_code, decoded_sequence, decoded_len);
                       KTerm_Free(decoded_sequence);
                  }
             }
        }

        // Consume delimiter if present
        if (Stream_Peek(&scanner) == ';') Stream_Consume(&scanner);
    }
}

void ClearUserDefinedKeys(KTerm* term, KTermSession* session) {
    (void)term;
    for (size_t i = 0; i < session->programmable_keys.count; i++) {
        KTerm_Free(session->programmable_keys.keys[i].sequence);
    }
    session->programmable_keys.count = 0;
}

void ProcessSoftFontDownload(KTerm* term, KTermSession* session, const char* data) {
    // Phase 7.2: Verify Safe Parsing (StreamScanner)
    if (!(session->conformance.features & KTERM_FEATURE_SOFT_FONTS)) {
        KTerm_LogUnsupportedSequence(term, "Soft fonts not supported");
        return;
    }

    StreamScanner scanner = { .ptr = data, .len = strlen(data), .pos = 0 };
    int params[8] = {0};
    int param_idx = 0;

    // Parse parameters
    while (param_idx < 8 && scanner.pos < scanner.len) {
        if (Stream_Peek(&scanner) == '{') break;

        // Try reading integer
        int val = 0;
        if (isdigit((unsigned char)Stream_Peek(&scanner)) || Stream_Peek(&scanner) == '-') {
             Stream_ReadInt(&scanner, &val);
             params[param_idx++] = val;
        } else {
             // Default to 0 if missing
             params[param_idx++] = 0;
        }

        if (Stream_Peek(&scanner) == ';') {
            Stream_Consume(&scanner);
        } else if (Stream_Peek(&scanner) != '{') {
            // Unexpected char? Stop or skip?
            // If we are not at separator or end, consume garbage?
            // Safer to just consume until next separator or end.
            while(scanner.pos < scanner.len && Stream_Peek(&scanner) != ';' && Stream_Peek(&scanner) != '{') {
                Stream_Consume(&scanner);
            }
            if (Stream_Peek(&scanner) == ';') Stream_Consume(&scanner);
        }
    }

    // Move to data block
    while (scanner.pos < scanner.len && Stream_Peek(&scanner) != '{') {
        Stream_Consume(&scanner);
    }
    if (!Stream_Expect(&scanner, '{')) {
        return; // No data block
    }

    // Parse Dscs (Designation String)
    // Consists of 0-2 intermediate chars (0x20-0x2F) and one final char (0x30-0x7E)
    // Example: " @", "A", "!@"
    char dscs[4] = {0};
    int dscs_len = 0;
    while (scanner.pos < scanner.len && dscs_len < 3) {
        char ch = Stream_Peek(&scanner);
        if (ch >= 0x20 && ch <= 0x2F) { // Intermediate
            dscs[dscs_len++] = Stream_Consume(&scanner);
        } else if (ch >= 0x30 && ch <= 0x7E) { // Final
             dscs[dscs_len++] = Stream_Consume(&scanner);
             break; // End of Dscs
        } else {
             break; // Unexpected
        }
    }
    if (dscs_len > 0) {
        strncpy(session->soft_font.name, dscs, 3);
        session->soft_font.name[3] = '\0';
    }

    // Update dimensions if provided
    // Pcmw (Matrix Width) is at index 3 (4th parameter)
    if (param_idx >= 4) {
        int w = params[3];
        if (w > 0 && w <= 32) session->soft_font.char_width = w;
    }
    // Pcmh (Height) is at index 6 (7th parameter)
    if (param_idx >= 7) {
        int h = params[6];
        if (h > 0 && h <= 32) session->soft_font.char_height = h;
    }

    // Parse sixel-encoded font data
    int current_char = (param_idx >= 2) ? params[1] : 0;
    int sixel_row_base = 0;
    int current_col = 0;

    if (current_char < 256) {
        memset(session->soft_font.font_data[current_char], 0, 32);
    }

    while (scanner.pos < scanner.len) {
        unsigned char ch = Stream_Consume(&scanner);

        if (ch == '/' || ch == ';') {
            if (current_char < 256) {
                session->soft_font.loaded[current_char] = true;
            }
            current_char++;
            if (current_char >= 256) break;

            memset(session->soft_font.font_data[current_char], 0, 32);
            sixel_row_base = 0;
            current_col = 0;

        } else if (ch == '-') {
            sixel_row_base += 6;
            current_col = 0;

        } else if (ch >= 63 && ch <= 126) {
            int sixel_val = ch - 63;
            if (current_char < 256 && current_col < 8) {
                for (int b = 0; b < 6; b++) {
                    int pixel_y = sixel_row_base + b;
                    if (pixel_y < 32) {
                        if ((sixel_val >> b) & 1) {
                            session->soft_font.font_data[current_char][pixel_y] |= (1 << (7 - current_col));
                        }
                    }
                }
                current_col++;
            }
        }
    }
    // Ensure last character is marked loaded if we processed data
    if (current_char < 256) {
         session->soft_font.loaded[current_char] = true;
    }
    session->soft_font.dirty = true;
    session->soft_font.active = true;
    KTerm_CalculateFontMetrics(session->soft_font.font_data, 256, session->soft_font.char_width, session->soft_font.char_height, 32, false, session->soft_font.metrics);
}

void ProcessStatusRequest(KTerm* term, KTermSession* session, const char* request) {
    // DECRQSS - Request Status String
    char response[2048];

    if (strcmp(request, "m") == 0) {
        // Request SGR status - Reconstruct SGR string
        char sgr[256];
        int len = 0;

        len += snprintf(sgr + len, sizeof(sgr) - len, "0"); // Reset first

        if (session->current_attributes & KTERM_ATTR_BOLD) len += snprintf(sgr + len, sizeof(sgr) - len, ";1");
        if (session->current_attributes & KTERM_ATTR_FAINT) len += snprintf(sgr + len, sizeof(sgr) - len, ";2");
        if (session->current_attributes & KTERM_ATTR_ITALIC) len += snprintf(sgr + len, sizeof(sgr) - len, ";3");
        if (session->current_attributes & KTERM_ATTR_UNDERLINE) len += snprintf(sgr + len, sizeof(sgr) - len, ";4");
        if (session->current_attributes & KTERM_ATTR_BLINK) len += snprintf(sgr + len, sizeof(sgr) - len, ";5");
        if (session->current_attributes & KTERM_ATTR_REVERSE) len += snprintf(sgr + len, sizeof(sgr) - len, ";7");
        if (session->current_attributes & KTERM_ATTR_CONCEAL) len += snprintf(sgr + len, sizeof(sgr) - len, ";8");
        if (session->current_attributes & KTERM_ATTR_STRIKE) len += snprintf(sgr + len, sizeof(sgr) - len, ";9");
        if (session->current_attributes & KTERM_ATTR_DOUBLE_UNDERLINE) len += snprintf(sgr + len, sizeof(sgr) - len, ";21");
        if (session->current_attributes & KTERM_ATTR_OVERLINE) len += snprintf(sgr + len, sizeof(sgr) - len, ";53");

        // Foreground
        if (session->current_fg.color_mode == 0) {
            int idx = session->current_fg.value.index;
            if (idx != COLOR_WHITE) {
                if (idx < 8) len += snprintf(sgr + len, sizeof(sgr) - len, ";%d", 30 + idx);
                else if (idx < 16) len += snprintf(sgr + len, sizeof(sgr) - len, ";%d", 90 + (idx - 8));
                else len += snprintf(sgr + len, sizeof(sgr) - len, ";38;5;%d", idx);
            }
        } else {
             len += snprintf(sgr + len, sizeof(sgr) - len, ";38;2;%d;%d;%d",
                 session->current_fg.value.rgb.r,
                 session->current_fg.value.rgb.g,
                 session->current_fg.value.rgb.b);
        }

        // Background
        if (session->current_bg.color_mode == 0) {
            int idx = session->current_bg.value.index;
            if (idx != COLOR_BLACK) {
                if (idx < 8) len += snprintf(sgr + len, sizeof(sgr) - len, ";%d", 40 + idx);
                else if (idx < 16) len += snprintf(sgr + len, sizeof(sgr) - len, ";%d", 100 + (idx - 8));
                else len += snprintf(sgr + len, sizeof(sgr) - len, ";48;5;%d", idx);
            }
        } else {
             len += snprintf(sgr + len, sizeof(sgr) - len, ";48;2;%d;%d;%d",
                 session->current_bg.value.rgb.r,
                 session->current_bg.value.rgb.g,
                 session->current_bg.value.rgb.b);
        }

        snprintf(response, sizeof(response), "\x1BP1$r%sm\x1B\\", sgr);
        KTerm_QueueSessionResponse(term, session, response);
    } else if (strcmp(request, "r") == 0) {
        // Request scrolling region
        snprintf(response, sizeof(response), "\x1BP1$r%d;%dr\x1B\\",
                GET_SESSION(term)->scroll_top + 1, GET_SESSION(term)->scroll_bottom + 1);
        KTerm_QueueSessionResponse(term, session, response);
    } else if (strcmp(request, "s") == 0) {
        // Request Left/Right Margins (DECSLRM)
        snprintf(response, sizeof(response), "\x1BP1$r%d;%ds\x1B\\",
                session->left_margin + 1, session->right_margin + 1);
        KTerm_QueueSessionResponse(term, session, response);
    } else if (strcmp(request, "t") == 0) {
        // Request Lines Per Page (DECSLPP)
        snprintf(response, sizeof(response), "\x1BP1$r%dt\x1B\\", session->rows);
        KTerm_QueueSessionResponse(term, session, response);
    } else if (strcmp(request, "|") == 0) {
        // Request Columns Per Page (DECSCPP)
        snprintf(response, sizeof(response), "\x1BP1$r%d|\x1B\\", session->cols);
        KTerm_QueueSessionResponse(term, session, response);
    } else if (strcmp(request, "\"p") == 0) {
        // Request Conformance Level (DECSCL)
        // Response: DCS 1 $ r Pl ; Pc " p ST
        // Pl: 61(VT100)..65(VT500)
        // Pc: 0(8-bit), 1(7-bit) - We default to 1 (7-bit controls) for compatibility unless strict 8-bit mode
        int pl = 61;
        if (session->conformance.level >= VT_LEVEL_510) pl = 65;
        else if (session->conformance.level >= VT_LEVEL_420) pl = 64;
        else if (session->conformance.level >= VT_LEVEL_320) pl = 63;
        else if (session->conformance.level >= VT_LEVEL_220) pl = 62;

        int pc = session->input.use_8bit_controls ? 0 : 1;

        snprintf(response, sizeof(response), "\x1BP1$r%d;%d\"p\x1B\\", pl, pc);
        KTerm_QueueSessionResponse(term, session, response);
    } else if (strcmp(request, " q") == 0) {
        // Request Cursor Style (DECSCUSR)
        // Response: DCS 1 $ r Pn SP q ST
        int style = 1; // Default Blinking Block
        switch(session->cursor.shape) {
            case CURSOR_BLOCK: style = session->cursor.blink_enabled ? 1 : 2; break;
            case CURSOR_UNDERLINE: style = session->cursor.blink_enabled ? 3 : 4; break;
            case CURSOR_BAR: style = session->cursor.blink_enabled ? 5 : 6; break;
            default: style = 1; break;
        }
        snprintf(response, sizeof(response), "\x1BP1$r%d q\x1B\\", style);
        KTerm_QueueSessionResponse(term, session, response);
    } else if (strcmp(request, "*x") == 0) {
        // Request Attribute Change Extent (DECSACE)
        // Response: DCS 1 $ r 2 * x ST (2 = Rectangular)
        snprintf(response, sizeof(response), "\x1BP1$r2*x\x1B\\");
        KTerm_QueueSessionResponse(term, session, response);
    } else if (strcmp(request, "*|") == 0) {
        // Request Number of Lines per Screen (DECSNLS)
        // Response: DCS 1 $ r Pn * | ST
        snprintf(response, sizeof(response), "\x1BP1$r%d*|\x1B\\", session->rows);
        KTerm_QueueSessionResponse(term, session, response);
    } else if (strcmp(request, "q") == 0 || strcmp(request, "state") == 0) {
        // State Snapshot
        int cx = session->cursor.x + 1;
        int cy = session->cursor.y + 1;
        int st = session->scroll_top + 1;
        int sb = session->scroll_bottom + 1;
        unsigned int dec_m = session->dec_modes;
        int ansi_m = session->ansi_modes.insert_replace ? 1 : 0;
        int fg = session->current_fg.value.index;
        int bg = session->current_bg.value.index;
        unsigned int attr = session->current_attributes;

        snprintf(response, sizeof(response),
            "\x1BP1$rCURSOR:%d,%d|SCROLL:%d,%d|MODES:%u,%d|ATTR:%d,%d,%u\x1B\\",
            cx, cy, st, sb, dec_m, ansi_m, fg, bg, attr);
        KTerm_QueueSessionResponse(term, session, response);
    } else {
        // Unknown request
        snprintf(response, sizeof(response), "\x1BP0$r%s\x1B\\", request);
        KTerm_QueueSessionResponse(term, session, response);
    }
}

// New KTerm_ExecuteDCSAnswerback for DCS 0 ; 0 $ t <message> ST
void KTerm_ExecuteDCSAnswerback(KTerm* term, KTermSession* session) {
    char* message_start = strstr(session->escape_buffer, "$t");
    if (message_start) {
        message_start += 2; // Skip "$t"
        char* message_end = strstr(message_start, "\x1B\\"); // Find ST
        if (message_end) {
            size_t length = message_end - message_start;
            if (length >= MAX_COMMAND_BUFFER) {
                length = MAX_COMMAND_BUFFER - 1; // Prevent overflow
            }
            strncpy(session->answerback_buffer, message_start, length);
            session->answerback_buffer[length] = '\0';
        } else if (session->options.debug_sequences) {
            KTerm_LogUnsupportedSequence(term, "Incomplete DCS $ t sequence");
        }
    } else if (session->options.debug_sequences) {
        KTerm_LogUnsupportedSequence(term, "Invalid DCS $ t sequence");
    }
}

typedef struct {
    const char* name;
    int cell_width;
    int cell_height;
    int data_width;
    int data_height;
    const void* data;
    bool is_16bit;
} KTermFontDef;

static const KTermFontDef available_fonts[] = {
    {"VT220", 8, 10, 8, 10, dec_vt220_cp437_8x10, false},
    {"IBM", 10, 10, 8, 8, ibm_font_8x8, false}, // 10x10 Cell, 8x8 Data (Centered)
    {"VGA", 8, 8, 8, 8, vga_perfect_8x8_font, false},
    {"ULTIMATE", 8, 16, 8, 16, ultimate_oldschool_pc_font_8x16, false},
    {"CP437_16", 8, 16, 8, 16, cp437_font__8x16, false},
    {"NEC", 8, 16, 8, 16, nec_apc3_font_8x16, false},
    {"TOSHIBA", 8, 16, 8, 16, toshiba_sat_8x16, false},
    {"TRIDENT", 8, 16, 8, 16, trident_8x16, false},
    {"COMPAQ", 8, 16, 8, 16, compaq_portable3_8x16, false},
    {"OLYMPIAD", 8, 16, 8, 16, olympiad_font_8x16, false},
    {"MC6847", 8, 8, 8, 8, MC6847_font_8x8, false},
    {"NEOGEO", 8, 8, 8, 8, neogeo_bios_8x8, false},
    {"ATASCII", 8, 8, 8, 8, atascii_font_8x8, false},
    {"PETSCII", 8, 8, 8, 8, petscii_unshifted_font_8x8, false},
    {"PETSCII_SHIFT", 8, 8, 8, 8, petscii_shifted_font_8x8, false},
    {"TOPAZ", 8, 8, 8, 8, topaz_font_8x8, false},
    {"PREPPIE", 8, 8, 8, 8, preppie_font_8x8, false},
    {"VCR", 12, 14, 12, 14, vcr_osd_font_12x14, true},
    {NULL, 0, 0, 0, 0, NULL, false}
};

static int KTerm_Strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

void KTerm_SetFont(KTerm* term, const char* name) {
    for (int i = 0; available_fonts[i].name != NULL; i++) {
        if (KTerm_Strcasecmp(available_fonts[i].name, name) == 0) {
            term->char_width = available_fonts[i].cell_width;
            term->char_height = available_fonts[i].cell_height;
            term->font_data_width = available_fonts[i].data_width;
            term->font_data_height = available_fonts[i].data_height;
            term->current_font_data = available_fonts[i].data;
            term->current_font_is_16bit = available_fonts[i].is_16bit;

            KTerm_CalculateFontMetrics(term->current_font_data, 256, term->font_data_width, term->font_data_height, 0, term->current_font_is_16bit, term->font_metrics);

            // Recreate Font Texture
            KTerm_CreateFontTexture(term);

            // Trigger Resize to update window dimensions/buffers
            KTerm_Resize(term, term->width, term->height);
            return;
        }
    }
}

#ifdef KTERM_ENABLE_GATEWAY
static void KTerm_ParseGatewayCommand(KTerm* term, KTermSession* session, const char* data, size_t len) {
    char buffer[2048];
    if (len >= sizeof(buffer)) len = sizeof(buffer) - 1;
    memcpy(buffer, data, len);
    buffer[len] = '\0';

    char* class_id = buffer;
    char* id = strchr(class_id, ';');
    if (!id) return; // Invalid
    *id = '\0';
    id++;

    char* command = strchr(id, ';');
    if (!command) return; // Invalid
    *command = '\0';
    command++;

    char* params = strchr(command, ';');
    if (params) {
        *params = '\0';
        params++;
    } else {
        params = "";
    }

    KTerm_GatewayProcess(term, session, class_id, id, command, params);
}
#endif

void ProcessMacroDefinition(KTerm* term, KTermSession* session, const char* data) {
    StreamScanner scanner = { .ptr = data, .len = strlen(data), .pos = 0 };
    int pid = 0;
    int pst = 0;
    int penc = 0;

    // Parse params: Pid ; Pst ; Penc
    Stream_ReadInt(&scanner, &pid);
    if (Stream_Expect(&scanner, ';')) Stream_ReadInt(&scanner, &pst);
    if (Stream_Expect(&scanner, ';')) Stream_ReadInt(&scanner, &penc);

    // Scan for !z separator
    while (scanner.pos < scanner.len) {
        if (Stream_Peek(&scanner) == '!' && scanner.pos + 1 < scanner.len && scanner.ptr[scanner.pos+1] == 'z') {
            Stream_Consume(&scanner);
            Stream_Consume(&scanner);
            break;
        }
        Stream_Consume(&scanner);
    }
    
    // Data starts here
    const char* data_start = scanner.ptr + scanner.pos;
    size_t data_len = scanner.len - scanner.pos;

    StoredMacro* macro = NULL;
    for (size_t i = 0; i < session->stored_macros.count; i++) {
        if (session->stored_macros.macros[i].id == pid) { macro = &session->stored_macros.macros[i]; break; }
    }

    if (!macro) {
        if (session->stored_macros.count >= session->stored_macros.capacity) {
            size_t new_cap = (session->stored_macros.capacity == 0) ? 16 : session->stored_macros.capacity * 2;
            StoredMacro* new_arr = KTerm_Realloc(session->stored_macros.macros, new_cap * sizeof(StoredMacro));
            if (!new_arr) {
                KTerm_QueueSessionResponse(term, session, "\x1BP1$sERR\x1B\\"); // Alloc fail
                return;
            }
            session->stored_macros.macros = new_arr;
            session->stored_macros.capacity = new_cap;
        }
        macro = &session->stored_macros.macros[session->stored_macros.count++];
        macro->id = pid;
        macro->content = NULL;
    }

    if (macro->content) {
        if (session->stored_macros.total_memory_used >= macro->length)
            session->stored_macros.total_memory_used -= macro->length;
        KTerm_Free(macro->content);
    }

    // Check Limits (e.g. 4KB default per session if not configured)
    size_t limit = session->macro_space.total > 0 ? session->macro_space.total : 4096;
    size_t new_len = (penc == 1) ? data_len / 2 : data_len;

    if (session->stored_macros.total_memory_used + new_len > limit) {
        // Error: Full
        KTerm_QueueSessionResponse(term, session, "\x1BP1$sERR\x1B\\");
        macro->content = NULL;
        macro->length = 0;
        return;
    }

    if (penc == 1) {
        size_t decoded_len = data_len / 2;
        macro->content = KTerm_Malloc(decoded_len + 1);
        if (macro->content) {
            for (size_t i = 0; i < decoded_len; i++) {
                int h = hex_char_to_int(data_start[i * 2]);
                int l = hex_char_to_int(data_start[i * 2 + 1]);
                macro->content[i] = (char)((h << 4) | l);
            }
            macro->content[decoded_len] = '\0';
            macro->length = decoded_len;
        }
    } else {
        macro->content = strdup(data_start);
        macro->length = data_len;
    }

    if (macro->content) {
        session->stored_macros.total_memory_used += macro->length;
        macro->encoding = penc; // Store encoding type
        KTerm_QueueSessionResponse(term, session, "\x1BP1$sOK\x1B\\");
    } else {
        // Error: Alloc fail
        KTerm_QueueSessionResponse(term, session, "\x1BP1$sERR\x1B\\");
    }
    (void)pst;
}

void ExecuteInvokeMacro(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    int pid = KTerm_GetCSIParam(term, session, 0, 0);
    for (size_t i = 0; i < session->stored_macros.count; i++) {
        if (session->stored_macros.macros[i].id == pid && session->stored_macros.macros[i].content) {
            KTerm_WriteString(term, session->stored_macros.macros[i].content);
            return;
        }
    }
}

void ExecuteDECSRFR(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    // DECSRFR - Select Refresh Rate (CSI Ps " t)
    // Ignored in emulation
    (void)term;
}

void KTerm_ExecuteDCSCommand(KTerm* term, KTermSession* session) {
    StreamScanner scanner = { .ptr = session->escape_buffer, .len = strlen(session->escape_buffer), .pos = 0 };

    // GATE check
    if (Stream_MatchToken(&scanner, "GATE")) {
#ifdef KTERM_ENABLE_GATEWAY
        if (Stream_Peek(&scanner) == ';') Stream_Consume(&scanner);
        KTerm_ParseGatewayCommand(term, session, scanner.ptr + scanner.pos, scanner.len - scanner.pos);
#endif
        return;
    }
    scanner.pos = 0;

    // XTGETTCAP (+q)
    if (Stream_Expect(&scanner, '+') && Stream_Expect(&scanner, 'q')) {
        ProcessTermcapRequest(term, session, scanner.ptr + scanner.pos);
        return;
    }
    scanner.pos = 0;

    // DECRQSS ($q)
    if (Stream_Expect(&scanner, '$') && Stream_Expect(&scanner, 'q')) {
        ProcessStatusRequest(term, session, scanner.ptr + scanner.pos);
        return;
    }
    scanner.pos = 0;

    // Generic Parameter Scanner for DECDLD, DECUDK, DECDMAC
    // Scan past parameters to find the Intermediate/Final bytes
    // Params: 0-9 ;
    while (scanner.pos < scanner.len) {
        char ch = Stream_Peek(&scanner);
        if (isdigit((unsigned char)ch) || ch == ';') {
            Stream_Consume(&scanner);
        } else {
            break;
        }
    }
    
    // Now look at what follows params
    char next = Stream_Peek(&scanner);
    
    if (next == '{') {
        // DECDLD (Soft Font)
        ProcessSoftFontDownload(term, session, session->escape_buffer);
        return;
    }
    
    if (next == '|') {
        // DECUDK (User Defined Keys)
        // Or DECDLD variant "2;1|"
        // Let's re-parse from start to check params
        StreamScanner p_scan = { .ptr = session->escape_buffer, .len = strlen(session->escape_buffer), .pos = 0 };
        int p1 = 0, p2 = 0;
        bool has_p1 = Stream_ReadInt(&p_scan, &p1);
        Stream_Expect(&p_scan, ';');
        bool has_p2 = Stream_ReadInt(&p_scan, &p2);
        
        if (has_p1 && p1 == 2 && has_p2 && p2 == 1 && Stream_Expect(&p_scan, '|')) {
             ProcessSoftFontDownload(term, session, session->escape_buffer);
             return;
        }
        
        // Otherwise DECUDK
        // Check clear flag (p1)
        if (has_p1 && p1 == 0) ClearUserDefinedKeys(term, session);
        
        // Pass the FULL string to helper as it re-parses
        ProcessUserDefinedKeys(term, session, session->escape_buffer);
        return;
    }
    
    if (next == '!' && scanner.pos + 1 < scanner.len && scanner.ptr[scanner.pos+1] == 'z') {
        // DECDMAC (Macro)
        ProcessMacroDefinition(term, session, session->escape_buffer);
        return;
    }

    if (session->options.debug_sequences) {
        KTerm_LogUnsupportedSequence(term, "Unknown DCS command");
    }
}

// =============================================================================
// VT52 COMPATIBILITY MODE
// =============================================================================

void KTerm_ProcessHashChar(KTerm* term, KTermSession* session, unsigned char ch) {
    // DEC Line Attributes (ESC # Pn)

    // These commands apply to the *entire line* containing the active position.
    // In a real hardware terminal, this changes the scan-out logic.
    // Here, we set flags on all characters in the current row.
    // Note: Using term->width assumes fixed-width allocation, which matches
    // the current implementation of 'screen' in session->h. If dynamic resizing is
    // added, this should iterate up to session->width or similar.

    switch (ch) {
        case '3': // DECDHL - Double-height line, top half
            for (int x = 0; x < term->width; x++) {
                EnhancedTermChar* cell = GetActiveScreenCell(session, session->cursor.y, x);
                cell->flags &= ~KTERM_ATTR_DOUBLE_HEIGHT_BOT;
                cell->flags |= (KTERM_ATTR_DOUBLE_HEIGHT_TOP | KTERM_ATTR_DOUBLE_WIDTH | KTERM_FLAG_DIRTY);
            }
            session->row_dirty[session->cursor.y] = KTERM_DIRTY_FRAMES;
            break;

        case '4': // DECDHL - Double-height line, bottom half
            for (int x = 0; x < term->width; x++) {
                EnhancedTermChar* cell = GetActiveScreenCell(session, session->cursor.y, x);
                cell->flags &= ~KTERM_ATTR_DOUBLE_HEIGHT_TOP;
                cell->flags |= (KTERM_ATTR_DOUBLE_HEIGHT_BOT | KTERM_ATTR_DOUBLE_WIDTH | KTERM_FLAG_DIRTY);
            }
            session->row_dirty[session->cursor.y] = KTERM_DIRTY_FRAMES;
            break;

        case '5': // DECSWL - Single-width single-height line
            for (int x = 0; x < term->width; x++) {
                EnhancedTermChar* cell = GetActiveScreenCell(session, session->cursor.y, x);
                cell->flags &= ~(KTERM_ATTR_DOUBLE_HEIGHT_TOP | KTERM_ATTR_DOUBLE_HEIGHT_BOT | KTERM_ATTR_DOUBLE_WIDTH);
                cell->flags |= KTERM_FLAG_DIRTY;
            }
            session->row_dirty[session->cursor.y] = KTERM_DIRTY_FRAMES;
            break;

        case '6': // DECDWL - Double-width single-height line
            for (int x = 0; x < term->width; x++) {
                EnhancedTermChar* cell = GetActiveScreenCell(session, session->cursor.y, x);
                cell->flags &= ~(KTERM_ATTR_DOUBLE_HEIGHT_TOP | KTERM_ATTR_DOUBLE_HEIGHT_BOT);
                cell->flags |= (KTERM_ATTR_DOUBLE_WIDTH | KTERM_FLAG_DIRTY);
            }
            session->row_dirty[session->cursor.y] = KTERM_DIRTY_FRAMES;
            break;

        case '8': // DECALN - Screen Alignment Pattern
            // Fill screen with 'E'
            for (int y = 0; y < term->height; y++) {
                for (int x = 0; x < term->width; x++) {
                    EnhancedTermChar* cell = GetActiveScreenCell(session, y, x);
                    cell->ch = 'E';
                    cell->fg_color = session->current_fg;
                    cell->bg_color = session->current_bg;
                    // Reset attributes
                    cell->flags = KTERM_FLAG_DIRTY;
                }
            }
            session->cursor.x = 0;
            session->cursor.y = 0;
            break;

        default:
            if (session->options.debug_sequences) {
                char debug_msg[64];
                snprintf(debug_msg, sizeof(debug_msg), "Unknown ESC # %c", ch);
                KTerm_LogUnsupportedSequence(term, debug_msg);
            }
            break;
    }

    session->parse_state = VT_PARSE_NORMAL;
}

void KTerm_ProcessnFChar(KTerm* term, KTermSession* session, unsigned char ch) {
    // nF Escape Sequences (ESC SP ...)
    switch (ch) {
        case 'F': // S7C1T - 7-bit controls (ESC + Fe)
            session->input.use_8bit_controls = false;
            break;
        case 'G': // S8C1T - 8-bit controls (C1)
            session->input.use_8bit_controls = true;
            break;
        default:
            // Intermediate bytes (0x20-0x2F) continue the sequence
            if (ch >= 0x20 && ch <= 0x2F) {
                return; // Stay in PARSE_nF
            }
            // Other final bytes are ignored or unsupported
            if (session->options.debug_sequences) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Unknown nF sequence: ESC SP %c", ch);
                KTerm_LogUnsupportedSequence(term, msg);
            }
            break;
    }
    session->parse_state = VT_PARSE_NORMAL;
}

void KTerm_ProcessPercentChar(KTerm* term, KTermSession* session, unsigned char ch) {
    // ISO 2022 Select Character Set (ESC % P)

    switch (ch) {
        case '@': // Select default (ISO 8859-1)
            session->charset.g0 = CHARSET_ISO_LATIN_1;
            session->charset.gl = &session->charset.g0;
            // Technically this selects the 'return to default' for ISO 2022.
            break;

        case 'G': // Select UTF-8 (ISO 2022 standard for UTF-8 level 1/2/3)
            session->charset.g0 = CHARSET_UTF8;
            session->charset.gl = &session->charset.g0;
            break;

        default:
             if (GET_SESSION(term)->options.debug_sequences) {
                char debug_msg[64];
                snprintf(debug_msg, sizeof(debug_msg), "Unknown ESC %% %c", ch);
                KTerm_LogUnsupportedSequence(term, debug_msg);
            }
            break;
    }

    GET_SESSION(term)->parse_state = VT_PARSE_NORMAL;
}

static void ReGIS_DrawLine(KTerm* term, KTermSession* session, int x0, int y0, int x1, int y1) {
    if (term->vector_count < term->vector_capacity) {
        GPUVectorLine* line = &term->vector_staging_buffer[term->vector_count];

        // Aspect Ratio Correction
        float logical_w = (float)(session->regis.screen_max_x - session->regis.screen_min_x + 1);
        float logical_h = (float)(session->regis.screen_max_y - session->regis.screen_min_y + 1);
        if (logical_w <= 0) logical_w = REGIS_WIDTH;
        if (logical_h <= 0) logical_h = REGIS_HEIGHT;

        float screen_w = (float)(term->width * DEFAULT_CHAR_WIDTH);
        float screen_h = (float)(term->height * DEFAULT_CHAR_HEIGHT);
        float scale_x = screen_w / logical_w;
        float scale_y = screen_h / logical_h;
        float scale_factor = (scale_x < scale_y) ? scale_x : scale_y;

        float target_w = logical_w * scale_factor;
        float target_h = logical_h * scale_factor;
        float x_margin = (screen_w - target_w) / 2.0f;
        float y_margin = (screen_h - target_h) / 2.0f;

        float u0_px = x_margin + ((float)(x0 - session->regis.screen_min_x) * scale_factor);
        float v0_px = y_margin + ((float)(y0 - session->regis.screen_min_y) * scale_factor);
        float u1_px = x_margin + ((float)(x1 - session->regis.screen_min_x) * scale_factor);
        float v1_px = y_margin + ((float)(y1 - session->regis.screen_min_y) * scale_factor);

        // Y is inverted in ReGIS (0=Top) vs OpenGL UV (0=Bottom usually)
        line->x0 = u0_px / screen_w;
        line->y0 = 1.0f - (v0_px / screen_h);
        line->x1 = u1_px / screen_w;
        line->y1 = 1.0f - (v1_px / screen_h);

        line->color = session->regis.color;
        line->intensity = 1.0f;
        line->mode = session->regis.write_mode;
        term->vector_count++;
    }
}

static int ReGIS_CompareInt(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

static void ReGIS_FillPolygon(KTerm* term, KTermSession* session) {
    if (session->regis.point_count < 3) {
        session->regis.point_count = 0;
        return;
    }

    // Scanline Fill Algorithm
    int min_y = session->regis.screen_max_y, max_y = session->regis.screen_min_y;
    for(int i=0; i<session->regis.point_count; i++) {
        if (session->regis.point_buffer[i].y < min_y) min_y = session->regis.point_buffer[i].y;
        if (session->regis.point_buffer[i].y > max_y) max_y = session->regis.point_buffer[i].y;
    }
    if (min_y < session->regis.screen_min_y) min_y = session->regis.screen_min_y;
    if (max_y > session->regis.screen_max_y) max_y = session->regis.screen_max_y;

    int nodes[64];
    for (int y = min_y; y <= max_y; y++) {
        int node_count = 0;
        int j = session->regis.point_count - 1;
        for (int i = 0; i < session->regis.point_count; i++) {
            int y1 = session->regis.point_buffer[i].y;
            int y2 = session->regis.point_buffer[j].y;
            int x1 = session->regis.point_buffer[i].x;
            int x2 = session->regis.point_buffer[j].x;

            if ((y1 < y && y2 >= y) || (y2 < y && y1 >= y)) {
                if (node_count < 64) {
                    nodes[node_count++] = x1 + (int)((float)(y - y1) / (float)(y2 - y1) * (float)(x2 - x1));
                }
            }
            j = i;
        }

        qsort(nodes, node_count, sizeof(int), ReGIS_CompareInt);

        for (int i = 0; i < node_count; i += 2) {
            if (i + 1 < node_count) {
                int x_start = nodes[i] < session->regis.screen_min_x ? session->regis.screen_min_x : nodes[i];
                int x_end = nodes[i+1] > session->regis.screen_max_x ? session->regis.screen_max_x : nodes[i+1];
                if (x_start > session->regis.screen_max_x) break;
                if (x_end < session->regis.screen_min_x) continue;
                if (x_start < x_end) {
                     // Draw horizontal line span
                     ReGIS_DrawLine(term, session, x_start, y, x_end, y);
                }
            }
        }
    }
    session->regis.point_count = 0;
}

// Cubic B-Spline interpolation
static void ReGIS_EvalBSpline(KTerm* term, int p0x, int p0y, int p1x, int p1y, int p2x, int p2y, int p3x, int p3y, float t, int* out_x, int* out_y) {
    (void)term;
    float t2 = t * t;
    float t3 = t2 * t;
    float b0 = (-t3 + 3*t2 - 3*t + 1) / 6.0f;
    float b1 = (3*t3 - 6*t2 + 4) / 6.0f;
    float b2 = (-3*t3 + 3*t2 + 3*t + 1) / 6.0f;
    float b3 = t3 / 6.0f;

    *out_x = (int)(b0*p0x + b1*p1x + b2*p2x + b3*p3x);
    *out_y = (int)(b0*p0y + b1*p1y + b2*p2y + b3*p3y);
}

static void ExecuteReGISCommand(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    if (session->regis.command == 0) return;
    if (!session->regis.data_pending && session->regis.command != 'S' && session->regis.command != 'W' && session->regis.command != 'F' && session->regis.command != 'R') return;

    int max_idx = session->regis.param_count;

    // --- P: Position ---
    if (session->regis.command == 'P') {
        for (int i = 0; i <= max_idx; i += 2) {
            int val_x = session->regis.params[i];
            bool rel_x = session->regis.params_relative[i];
            int val_y = (i + 1 <= max_idx) ? session->regis.params[i+1] : session->regis.y;

            if (i+1 > max_idx) val_y = session->regis.y; // Fallback if Y completely missing in pair

            bool rel_y = (i + 1 <= max_idx) ? session->regis.params_relative[i+1] : false;

            int target_x = rel_x ? (session->regis.x + val_x) : val_x;
            int target_y = rel_y ? (session->regis.y + val_y) : val_y;

            // Clamp
            if (target_x < session->regis.screen_min_x) target_x = session->regis.screen_min_x;
            if (target_x > session->regis.screen_max_x) target_x = session->regis.screen_max_x;

            if (target_y < session->regis.screen_min_y) target_y = session->regis.screen_min_y;
            if (target_y > session->regis.screen_max_y) target_y = session->regis.screen_max_y;

            session->regis.x = target_x;
            session->regis.y = target_y;

            session->regis.point_count = 0;
        }
    }
    // --- V: Vector (Line) ---
    else if (session->regis.command == 'V') {
        for (int i = 0; i <= max_idx; i += 2) {
            int val_x = session->regis.params[i];
            bool rel_x = session->regis.params_relative[i];
            int val_y = (i + 1 <= max_idx) ? session->regis.params[i+1] : session->regis.y;
            if (i+1 > max_idx && !rel_x) val_y = session->regis.y;

            bool rel_y = (i + 1 <= max_idx) ? session->regis.params_relative[i+1] : false;

            int target_x = rel_x ? (session->regis.x + val_x) : val_x;
            int target_y = rel_y ? (session->regis.y + val_y) : val_y;


            if (target_x < session->regis.screen_min_x) target_x = session->regis.screen_min_x;
            if (target_x > session->regis.screen_max_x) target_x = session->regis.screen_max_x;
            if (target_y < session->regis.screen_min_y) target_y = session->regis.screen_min_y;
            if (target_y > session->regis.screen_max_y) target_y = session->regis.screen_max_y;

            ReGIS_DrawLine(term, session, session->regis.x, session->regis.y, target_x, target_y);

            session->regis.x = target_x;
            session->regis.y = target_y;
        }
        session->regis.point_count = 0;
    }
    // --- F: Polygon Fill ---
    else if (session->regis.command == 'F') {
        // Collect points but don't draw immediately
        for (int i = 0; i <= max_idx; i += 2) {
            int val_x = session->regis.params[i];
            bool rel_x = session->regis.params_relative[i];
            int val_y = (i + 1 <= max_idx) ? session->regis.params[i+1] : session->regis.y;
            bool rel_y = (i + 1 <= max_idx) ? session->regis.params_relative[i+1] : false;

            int px = rel_x ? (session->regis.x + val_x) : val_x;

            int py = rel_y ? (session->regis.y + val_y) : val_y;

            if (px < session->regis.screen_min_x) px = session->regis.screen_min_x;
            if (px > session->regis.screen_max_x) px = session->regis.screen_max_x;
            if (py < session->regis.screen_min_y) py = session->regis.screen_min_y;
            if (py > session->regis.screen_max_y) py = session->regis.screen_max_y;

            if (session->regis.point_count < 64) {
                 if (session->regis.point_count == 0) {
                     // First point is usually current cursor if implied?
                     // Standard F command might imply current position as start.
                     // We'll add current pos if buffer empty?
                     // ReGIS usually: F(V(P[x,y]...))
                     // Our F implementation just collects points passed to it.
                     // If point_count is 0, we should probably add current cursor?
                     session->regis.point_buffer[0].x = session->regis.x;
                     session->regis.point_buffer[0].y = session->regis.y;
                     session->regis.point_count++;
                 }
                 session->regis.point_buffer[session->regis.point_count].x = px;
                 session->regis.point_buffer[session->regis.point_count].y = py;
                 session->regis.point_count++;
            }
            session->regis.x = px;
            session->regis.y = py;
        }
    }
    // --- C: Circle / Curve ---
    else if (session->regis.command == 'C') {
        if (session->regis.option_command == 'B') {
            // --- B-Spline ---
            for (int i = 0; i <= max_idx; i += 2) {
                int val_x = session->regis.params[i];
                bool rel_x = session->regis.params_relative[i];
                int val_y = (i + 1 <= max_idx) ? session->regis.params[i+1] : session->regis.y;
                bool rel_y = (i + 1 <= max_idx) ? session->regis.params_relative[i+1] : false;

                int px = rel_x ? (session->regis.x + val_x) : val_x;
                int py = rel_y ? (session->regis.y + val_y) : val_y;

                if (session->regis.point_count < 64) {
                    if (session->regis.point_count == 0) {
                        session->regis.point_buffer[0].x = session->regis.x;
                        session->regis.point_buffer[0].y = session->regis.y;
                        session->regis.point_count++;
                    }
                    session->regis.point_buffer[session->regis.point_count].x = px;
                    session->regis.point_buffer[session->regis.point_count].y = py;
                    session->regis.point_count++;
                }
                session->regis.x = px;
                session->regis.y = py;
            }

            // Safety check to prevent infinite loops if points aren't consumed correctly
            if (session->regis.point_count >= 4) {
                for (int i = 0; i <= session->regis.point_count - 4; i++) {
                    int p0x = session->regis.point_buffer[i].x;   int p0y = session->regis.point_buffer[i].y;
                    int p1x = session->regis.point_buffer[i+1].x; int p1y = session->regis.point_buffer[i+1].y;
                    int p2x = session->regis.point_buffer[i+2].x; int p2y = session->regis.point_buffer[i+2].y;
                    int p3x = session->regis.point_buffer[i+3].x; int p3y = session->regis.point_buffer[i+3].y;

                    int seg_steps = 10;
                    int last_x = -1, last_y = -1;

                    for (int s=0; s<=seg_steps; s++) {
                        float t = (float)s / (float)seg_steps;
                        int tx, ty;
                        ReGIS_EvalBSpline(term, p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y, t, &tx, &ty);
                        if (last_x != -1) {
                            ReGIS_DrawLine(term, session, last_x, last_y, tx, ty);
                        }
                        last_x = tx;
                        last_y = ty;
                    }
                }
                int keep = 3;
                if (session->regis.point_count > keep) {
                    for(int k=0; k<keep; k++) {
                        session->regis.point_buffer[k] = session->regis.point_buffer[session->regis.point_count - keep + k];
                    }
                    session->regis.point_count = keep;
                }
            }
        }
        else if (session->regis.option_command == 'A') {
            // --- Arc ---
            if (max_idx >= 0) {
                int cx_val = session->regis.params[0];
                bool cx_rel = session->regis.params_relative[0];
                int cy_val = (1 <= max_idx) ? session->regis.params[1] : session->regis.y;
                bool cy_rel = (1 <= max_idx) ? session->regis.params_relative[1] : false;

                int cx = cx_rel ? (session->regis.x + cx_val) : cx_val;
                int cy = cy_rel ? (session->regis.y + cy_val) : cy_val;

                int sx = session->regis.x;
                int sy = session->regis.y;

                float dx = (float)(sx - cx);
                float dy = (float)(sy - cy);
                float radius = sqrtf(dx*dx + dy*dy);
                float start_angle = atan2f(dy, dx);

                float degrees = 0;
                if (max_idx >= 2) {
                    degrees = (float)session->regis.params[2];
                }

                int segments = (int)(fabsf(degrees) / 5.0f);
                if (segments < 4) segments = 4;
                float rad_step = (degrees * 3.14159f / 180.0f) / segments;

                float current_angle = start_angle;
                int last_x = sx;
                int last_y = sy;

                for (int i=0; i<segments; i++) {
                    current_angle += rad_step;
                    int nx = cx + (int)(cosf(current_angle) * radius);
                    int ny = cy + (int)(sinf(current_angle) * radius);
                    ReGIS_DrawLine(term, session, last_x, last_y, nx, ny);
                    last_x = nx;
                    last_y = ny;
                }

                session->regis.x = last_x;
                session->regis.y = last_y;
            }
        }
        else {
            // --- Standard Circle ---
            for (int i = 0; i <= max_idx; i += 2) {
                 int val1 = session->regis.params[i];
                 bool rel1 = session->regis.params_relative[i];

                 int radius = 0;
                 if (i + 1 > max_idx) {
                     radius = val1;
                 } else {
                     int val2 = session->regis.params[i+1];
                     bool rel2 = session->regis.params_relative[i+1];
                     int px = rel1 ? (session->regis.x + val1) : val1;
                     int py = rel2 ? (session->regis.y + val2) : val2;
                     float dx = (float)(px - session->regis.x);
                     float dy = (float)(py - session->regis.y);
                     radius = (int)sqrtf(dx*dx + dy*dy);
                 }

                 int cx = session->regis.x;
                 int cy = session->regis.y;
                 int segments = 32;
                 float angle_step = 6.283185f / segments;

                 for (int j = 0; j < segments; j++) {
                    if (term->vector_count >= term->vector_capacity) break;
                    float a1 = j * angle_step;
                    float a2 = (j + 1) * angle_step;

                    int x1 = cx + (int)(cosf(a1) * radius);
                    int y1 = cy + (int)(sinf(a1) * radius);
                    int x2 = cx + (int)(cosf(a2) * radius);
                    int y2 = cy + (int)(sinf(a2) * radius);

                    ReGIS_DrawLine(term, session, x1, y1, x2, y2);
                 }
            }
        }
    }
    // --- S: Screen Control ---
    else if (session->regis.command == 'S') {
        if (session->regis.option_command == 'E') {
             // Screen Erase (and optional addressing per prompt behavior)
             if (session->regis.param_count >= 3) {
                 // S(E[x1,y1][x2,y2]) - Set addressing extent and erase
                 session->regis.screen_min_x = session->regis.params[0];
                 session->regis.screen_min_y = session->regis.params[1];
                 session->regis.screen_max_x = session->regis.params[2];
                 session->regis.screen_max_y = session->regis.params[3];
             }
             term->vector_count = 0;
             term->vector_clear_request = true;
        } else if (session->regis.option_command == 'A') {
             // Screen Addressing S(A[x1,y1][x2,y2])
             if (session->regis.param_count >= 3) {
                 session->regis.screen_min_x = session->regis.params[0];
                 session->regis.screen_min_y = session->regis.params[1];
                 session->regis.screen_max_x = session->regis.params[2];
                 session->regis.screen_max_y = session->regis.params[3];
             }
        }
    }
    // --- W: Write Control ---
    else if (session->regis.command == 'W') {
        // Handle explicit KTermColor Index selection W(I...)
        if (session->regis.option_command == 'I') {
             int color_idx = session->regis.params[0];
             if (color_idx >= 0 && color_idx < 16) {
                 RGB_KTermColor c = term->color_palette[color_idx];
                 session->regis.color = (uint32_t)c.r | ((uint32_t)c.g << 8) | ((uint32_t)c.b << 16) | 0xFF000000;
             }
        }
        // Handle Writing Modes
        else if (session->regis.option_command == 'R') {
             session->regis.write_mode = 1; // Replace
        } else if (session->regis.option_command == 'E') {
             session->regis.write_mode = 2; // Erase
        } else if (session->regis.option_command == 'V') {
             session->regis.write_mode = 0; // Overlay (Additive)
        } else if (session->regis.option_command == 'C') {
             // W(C) is ambiguous: could be Complement or KTermColor
             // If we have parameters (e.g. W(C1)), treat as KTermColor (Legacy behavior).
             // If no parameters (e.g. W(C)), treat as Complement (XOR).

             if (session->regis.param_count > 0) {
                 // Likely KTermColor Index W(C1)
                 int color_idx = session->regis.params[0];
                 if (color_idx >= 0 && color_idx < 16) {
                     RGB_KTermColor c = term->color_palette[color_idx];
                     session->regis.color = (uint32_t)c.r | ((uint32_t)c.g << 8) | ((uint32_t)c.b << 16) | 0xFF000000;
                 }
             } else {
                 session->regis.write_mode = 3; // Complement (XOR)
             }
        }
    }
    // --- T: Text Attributes ---
    else if (session->regis.command == 'T') {
        if (session->regis.option_command == 'S') {
             // Size
             session->regis.text_size = (float)session->regis.params[0];
             if (session->regis.text_size <= 0) session->regis.text_size = 1;
        }
        if (session->regis.option_command == 'D') {
             // Direction (degrees)
             session->regis.text_angle = (float)session->regis.params[0] * 3.14159f / 180.0f;
        }
    }
    // --- L: Load Alphabet ---
    else if (session->regis.command == 'L') {
        // L command logic is primarily handled in ProcessReGISChar during string/hex parsing.
        // This block handles parameterized options like S (Size) if provided.
        if (session->regis.option_command == 'S') {
             // Character Cell Size (e.g. L(S1) or L(S[8,16]))
             int w = 8;
             int h = 16;
             if (session->regis.param_count >= 0) { // param_count is max index
                 // Check if it's an index or explicit size
                 if (session->regis.params[0] == 1) { w=8; h=16; }
                 else if (session->regis.params[0] == 0) { w=8; h=16; } // Default
                 else {
                     // Assume width
                     w = session->regis.params[0];
                     if (session->regis.param_count >= 1) h = session->regis.params[1];
                 }
             }
             session->soft_font.char_width = w;
             session->soft_font.char_height = h;
        } else if (session->regis.option_command == 'A') {
             // Alphabet selection L(A1)
             if (session->regis.param_count >= 0) {
                 int alpha = session->regis.params[0];
                 // We only really support loading into "soft font" slot (conceptually A1)
                 // A0 is typically the hardware ROM font.
                 if (alpha != 1) {
                     if (session->options.debug_sequences) {
                         char msg[64];
                         snprintf(msg, sizeof(msg), "ReGIS Load: Alphabet A%d not supported (only A1)", alpha);
                         KTerm_LogUnsupportedSequence(term, msg);
                     }
                 }
             }
        }
    }
    // --- R: Report ---
    else if (session->regis.command == 'R') {
         if (session->regis.option_command == 'P') {
             char buf[64];
             snprintf(buf, sizeof(buf), "\x1BP%d,%d\x1B\\", session->regis.x, session->regis.y);
             KTerm_QueueSessionResponse(term, session, buf);
         }
    }

    session->regis.data_pending = false;
}

static void ProcessReGISChar(KTerm* term, KTermSession* session, unsigned char ch) {
    if (ch == 0x1B) { // ESC \ (ST)
        if (session->regis.command == 'F') ReGIS_FillPolygon(term, session); // Flush pending fill
        if (session->regis.state == 1 || session->regis.state == 3) {
            ExecuteReGISCommand(term, session);
        }
        session->parse_state = VT_PARSE_ESCAPE;
        return;
    }

    if (session->regis.recording_macro) {
        if (ch == ';' && session->regis.macro_len > 0 && session->regis.macro_buffer[session->regis.macro_len-1] == '@') {
             // End of macro definition (@;)
             session->regis.macro_buffer[session->regis.macro_len-1] = '\0'; // Remove @
             session->regis.recording_macro = false;
             // Store macro in slot
             if (session->regis.macro_index >= 0 && session->regis.macro_index < 26) {
                 if (session->regis.macros[session->regis.macro_index]) KTerm_Free(session->regis.macros[session->regis.macro_index]);
                 session->regis.macros[session->regis.macro_index] = strdup(session->regis.macro_buffer);
             }
             if (session->regis.macro_buffer) { KTerm_Free(session->regis.macro_buffer); session->regis.macro_buffer = NULL; }
             return;
        }
        // Append
        if (!session->regis.macro_buffer) {
             session->regis.macro_cap = 1024;
             session->regis.macro_buffer = KTerm_Malloc(session->regis.macro_cap);
             session->regis.macro_len = 0;
        }

        // Enforce Macro Space Limit
        size_t limit = session->macro_space.total > 0 ? session->macro_space.total : 4096;
        if (session->regis.macro_len >= limit) {
             if (session->options.debug_sequences) {
                 KTerm_LogUnsupportedSequence(term, "ReGIS Macro storage limit exceeded");
             }
             // Stop recording, discard macro? Or just truncate?
             // Standard behavior: typically stops recording or overwrites?
             // Safest: Stop appending.
             return;
        }

        if (session->regis.macro_len >= session->regis.macro_cap - 1) {
             size_t new_cap = session->regis.macro_cap * 2;
             if (new_cap > limit + 1) new_cap = limit + 1; // +1 for null terminator

             if (new_cap > session->regis.macro_cap) {
                 session->regis.macro_cap = new_cap;
                 session->regis.macro_buffer = KTerm_Realloc(session->regis.macro_buffer, session->regis.macro_cap);
             }
        }

        // Final check against capacity to be safe
        if (session->regis.macro_len < session->regis.macro_cap - 1) {
            session->regis.macro_buffer[session->regis.macro_len++] = ch;
            session->regis.macro_buffer[session->regis.macro_len] = '\0';
        }
        return;
    }

    if (session->regis.state == 3) { // Parsing Text String
        if (ch == session->regis.string_terminator) {
            session->regis.text_buffer[session->regis.text_pos] = '\0';

            if (session->regis.command == 'L') {
                // Load Alphabet Logic
                if (session->regis.option_command == 'A') {
                    // Set Alphabet Name
                    strncpy(session->regis.load.name, session->regis.text_buffer, 15);
                    session->regis.load.name[15] = '\0';
                    session->regis.option_command = 0; // Reset
                } else {
                    // Define Character
                    if (session->regis.text_pos > 0) {
                        session->regis.load.current_char = (unsigned char)session->regis.text_buffer[0];
                        session->regis.load.pattern_byte_idx = 0;
                        session->regis.load.hex_nibble = -1;
                        // Clear existing pattern for this char
                        memset(session->soft_font.font_data[session->regis.load.current_char], 0, 32);
                        session->soft_font.loaded[session->regis.load.current_char] = true;
                        session->soft_font.active = true;
                    }
                }
            } else {
                // Text Drawing with attributes
                float scale = (session->regis.text_size > 0) ? session->regis.text_size : 1.0f;
                // Base scale 1 = 8x16? ReGIS default size 1 is roughly 9x16 grid?
                // Existing code used scale 2.0f. Let's base it on that.
                scale *= 2.0f;

                float cos_a = cosf(session->regis.text_angle);
                float sin_a = sinf(session->regis.text_angle);

                int start_x = session->regis.x;
                int start_y = session->regis.y;

                const unsigned char* font_base = vga_perfect_8x8_font;
                bool use_soft_font = session->soft_font.active;

                for(int i=0; session->regis.text_buffer[i] != '\0'; i++) {
                    unsigned char c = (unsigned char)session->regis.text_buffer[i];

                    // Use dynamic height if soft font is active
                    int max_rows = use_soft_font ? session->soft_font.char_height : 16;
                    if (max_rows > 32) max_rows = 32;

                    for(int r=0; r<max_rows; r++) { // Iterate up to max_rows
                        unsigned char row = 0;
                        int height_limit = 8; // Default to 8 for VGA font

                        if (use_soft_font && session->soft_font.loaded[c]) {
                            row = session->soft_font.font_data[c][r];
                            height_limit = session->soft_font.char_height;
                        } else {
                            if (r < 8) row = font_base[c * 8 + r];
                            else row = 0;
                        }

                        if (r >= height_limit) continue;

                        for(int c_bit=0; c_bit<8; c_bit++) {
                            if ((row >> (7-c_bit)) & 1) {
                                int len = 1;
                                while(c_bit+len < 8 && ((row >> (7-(c_bit+len))) & 1)) len++;

                                // Local coordinates relative to char origin
                                float lx0 = (float)(c_bit * scale);
                                float ly0 = (float)(r * scale * (height_limit == 8 ? 1.5f : 0.75f)); // Adjust aspect for 16px
                                float lx1 = (float)((c_bit + len) * scale);
                                float ly1 = ly0;

                                // Rotate and translate
                                // Character offset
                                float char_offset = i * 9 * scale;

                                float rx0 = lx0 + char_offset;
                                float rx1 = lx1 + char_offset;

                                float fx0 = start_x + (rx0 * cos_a - ly0 * sin_a);
                                float fy0 = start_y + (rx0 * sin_a + ly0 * cos_a);
                                float fx1 = start_x + (rx1 * cos_a - ly1 * sin_a);
                                float fy1 = start_y + (rx1 * sin_a + ly1 * cos_a);

                                if (term->vector_count < term->vector_capacity) {
                                    GPUVectorLine* line = &term->vector_staging_buffer[term->vector_count];
                                    line->x0 = fx0 / ((float)REGIS_WIDTH);
                                    line->y0 = 1.0f - (fy0 / ((float)REGIS_HEIGHT));
                                    line->x1 = fx1 / ((float)REGIS_WIDTH);
                                    line->y1 = 1.0f - (fy1 / ((float)REGIS_HEIGHT));
                                    line->color = session->regis.color;
                                    line->intensity = 1.0f;
                                    line->mode = session->regis.write_mode;
                                    term->vector_count++;
                                 }
                                c_bit += len - 1;
                            }
                        }
                    }
                }
                // Update cursor position to end of string
                float total_width = session->regis.text_pos * 9 * scale;
                session->regis.x = start_x + (int)(total_width * cos_a);
                session->regis.y = start_y + (int)(total_width * sin_a);
            }

            session->regis.state = 1;
            session->regis.text_pos = 0;
        } else {
            if (session->regis.text_pos < 255) {
                session->regis.text_buffer[session->regis.text_pos++] = ch;
            }
        }
        return;
    }

    if (ch <= 0x20 || ch == 0x7F) return;

    if (session->regis.state == 0) { // Expecting Command
        if (ch == '@') {
            // Macro
            session->regis.command = '@';
            session->regis.state = 1;
            return;
        }
        if (isalpha(ch)) {
            session->regis.command = toupper(ch);
            session->regis.state = 1;
            session->regis.param_count = 0;
            session->regis.has_bracket = false;
            session->regis.has_paren = false;
            session->regis.point_count = 0; // Reset curve points on new command
            for(int i=0; i<16; i++) {
                session->regis.params[i] = 0;
                session->regis.params_relative[i] = false;
            }
        }
    } else if (session->regis.state == 1) { // Expecting Values/Options
        if (session->regis.command == '@') {
             if (ch == ':') {
                 // Definition
                 session->regis.option_command = ':'; // Flag next char as macro name
                 return;
             }
             if (session->regis.option_command == ':') {
                 // Macro Name
                 if (isalpha(ch)) {
                     session->regis.macro_index = toupper(ch) - 'A';
                     session->regis.recording_macro = true;
                     session->regis.macro_len = 0;
                     session->regis.option_command = 0;
                 }
                 return;
             }
             // Execute Macro
             if (isalpha(ch)) {
                 int idx = toupper(ch) - 'A';
                 if (idx >= 0 && idx < 26 && session->regis.macros[idx]) {
                     if (session->regis.recursion_depth < 16) {
                         session->regis.recursion_depth++;
                         // Push macro content to parser
                         const char* m = session->regis.macros[idx];
                         // Reset state to 0 for macro context?
                         // Macros usually contain full commands.
                         int saved_state = session->regis.state;
                         session->regis.state = 0;
                         for (int k=0; m[k]; k++) ProcessReGISChar(term, session, m[k]);
                         session->regis.state = saved_state;
                         session->regis.recursion_depth--;
                     } else {
                         if (session->options.debug_sequences) {
                             KTerm_LogUnsupportedSequence(term, "ReGIS Macro recursion depth exceeded");
                         }
                     }
                 }
                 session->regis.command = 0;
                 session->regis.state = 0;
             }
             return;
        }

        if (ch == '\'' || ch == '"') {
             if (session->regis.command == 'T' || session->regis.command == 'L') {
                 session->regis.state = 3;
                 session->regis.string_terminator = ch;
                 session->regis.text_pos = 0;
                 return;
             }
        }
        if (ch == '[') {
            session->regis.has_bracket = true;
            session->regis.has_comma = false;
            session->regis.parsing_val = false;
        } else if (ch == ']') {
            if (session->regis.parsing_val) {
                 session->regis.params[session->regis.param_count] = session->regis.current_sign * session->regis.current_val;
                 session->regis.params_relative[session->regis.param_count] = session->regis.val_is_relative;
            }
            session->regis.parsing_val = false;
            session->regis.has_bracket = false;

            // Special case for S command: Accumulate parameters for [x1,y1][x2,y2]
            if (session->regis.command != 'S') {
                ExecuteReGISCommand(term, session);
                session->regis.param_count = 0;
                for(int i=0; i<16; i++) {
                    session->regis.params[i] = 0;
                    session->regis.params_relative[i] = false;
                }
            } else {
                // Prepare for next parameter set
                if (session->regis.param_count < 15) {
                    session->regis.param_count++;
                    session->regis.params[session->regis.param_count] = 0;
                    session->regis.params_relative[session->regis.param_count] = false;
                }
            }
        } else if (ch == '(') {
            session->regis.has_paren = true;
            session->regis.parsing_val = false;
        } else if (ch == ')') {
            if (session->regis.parsing_val) {
                 session->regis.params[session->regis.param_count] = session->regis.current_sign * session->regis.current_val;
                 session->regis.params_relative[session->regis.param_count] = session->regis.val_is_relative;
            }
            session->regis.has_paren = false;
            session->regis.parsing_val = false;
            ExecuteReGISCommand(term, session);
            // Don't reset point count here, as options might modify curve mode
            // But we reset param count for next block
            session->regis.param_count = 0;
            for(int i=0; i<16; i++) {
                session->regis.params[i] = 0;
                session->regis.params_relative[i] = false;
            }
        } else if (session->regis.command == 'L' && isxdigit(ch)) {
            // Hex parsing for Load Alphabet
            // Assuming ReGIS "hex string" format (pairs of hex digits)
            int val = 0;
            if (ch >= '0' && ch <= '9') val = ch - '0';
            else if (ch >= 'A' && ch <= 'F') val = ch - 'A' + 10;
            else if (ch >= 'a' && ch <= 'f') val = ch - 'a' + 10;

            if (session->regis.load.hex_nibble == -1) {
                session->regis.load.hex_nibble = val;
            } else {
                int byte = (session->regis.load.hex_nibble << 4) | val;
                session->regis.load.hex_nibble = -1;

                if (session->regis.load.pattern_byte_idx < 32) {
                    GET_SESSION(term)->soft_font.font_data[session->regis.load.current_char][session->regis.load.pattern_byte_idx++] = byte;
                }
            }
            // Defer texture update to KTerm_Draw
            GET_SESSION(term)->soft_font.dirty = true;

        } else if (isdigit(ch) || ch == '-' || ch == '+') {
            if (!session->regis.parsing_val) {
                session->regis.parsing_val = true;
                session->regis.current_val = 0;
                session->regis.current_sign = 1;
                session->regis.val_is_relative = false;
            }
            if (ch == '-') {
                session->regis.current_sign = -1;
                session->regis.val_is_relative = true;
            } else if (ch == '+') {
                session->regis.current_sign = 1;
                session->regis.val_is_relative = true;
            } else if (isdigit(ch)) {
                // Protect against integer overflow
                if (session->regis.current_val < 100000000) { // Reasonable limit for coords/params
                    session->regis.current_val = session->regis.current_val * 10 + (ch - '0');
                }
            }
            session->regis.params[session->regis.param_count] = session->regis.current_sign * session->regis.current_val;
            session->regis.params_relative[session->regis.param_count] = session->regis.val_is_relative;
            session->regis.data_pending = true;
        } else if (ch == ',') {
            if (session->regis.parsing_val) {
                session->regis.params[session->regis.param_count] = session->regis.current_sign * session->regis.current_val;
                session->regis.params_relative[session->regis.param_count] = session->regis.val_is_relative;
                session->regis.parsing_val = false;
            }
            if (session->regis.param_count < 15) {
                session->regis.param_count++;
                session->regis.params[session->regis.param_count] = 0;
                session->regis.params_relative[session->regis.param_count] = false;
            }
            session->regis.has_comma = true;
        } else if (isalpha(ch)) {
            if (session->regis.has_paren) {
                 session->regis.option_command = toupper(ch);
                 session->regis.param_count = 0;
                 session->regis.parsing_val = false;
            } else {
                if (session->regis.command == 'F') ReGIS_FillPolygon(term, session); // Flush fill on new command
                ExecuteReGISCommand(term, session);
                session->regis.command = toupper(ch);
                session->regis.state = 1;
                session->regis.param_count = 0;
                session->regis.parsing_val = false;
                session->regis.point_count = 0; // Reset on new command
                for(int i=0; i<16; i++) {
                    session->regis.params[i] = 0;
                    session->regis.params_relative[i] = false;
                }
            }
        }
    }
}

static void ProcessTektronixChar(KTerm* term, KTermSession* session, unsigned char ch) {
    // 1. Escape Sequence Escape
    if (ch == 0x1B) {
        if ((session->dec_modes & KTERM_MODE_VT52)) {
            session->parse_state = PARSE_VT52;
        } else {
            session->parse_state = VT_PARSE_ESCAPE;
        }
        return;
    }

    // 2. Control Codes
    if (ch == 0x1D) { // GS - Graph Mode
        session->tektronix.state = 1; // Graph
        session->tektronix.pen_down = false; // First coord is Dark (Move)
        session->tektronix.extra_byte = -1;
        return;
    }
    if (ch == 0x1F) { // US - Alpha Mode (Text)
        session->tektronix.state = 0; // Alpha
        return;
    }
    if (ch == 0x0C) { // FF - Clear Screen
        term->vector_count = 0;
        session->tektronix.pen_down = false;
        session->tektronix.extra_byte = -1;
        return;
    }
    if (ch < 0x20) {
        if (session->tektronix.state == 0) KTerm_ProcessControlChar(term, session, ch);
        return;
    }

    // 3. Alpha Mode Handling
    if (session->tektronix.state == 0) {
        KTerm_ProcessNormalChar(term, session, ch);
        return;
    }

    // 4. Graph Mode Coordinate Parsing (12-bit Support)
    // Categories:
    // HiY:   0x20 - 0x3F (001xxxxx) - Sets High Y (5 bits), Closes Y
    // Extra: 0x60 - 0x7F (011xxxxx) - Intermediate LSBs (optional)
    // LoY:   0x60 - 0x7F (011xxxxx) - Sets Low Y (5 bits)
    // HiX:   0x20 - 0x3F (001xxxxx) - Sets High X (5 bits)
    // LoX:   0x40 - 0x5F (010xxxxx) - Sets Low X (5 bits) AND DRAWS

    int val = ch & 0x1F;

    if (ch >= 0x20 && ch <= 0x3F) {
        // HiY or HiX
        if (session->tektronix.sub_state == 1) { // Previous was LoY/Extra
            // Interpret as HiX
            // HiX bits (7-11)
            session->tektronix.holding_x = (session->tektronix.holding_x & 0x07F) | (val << 7);
            session->tektronix.sub_state = 2; // Seen HiX
            session->tektronix.extra_byte = -1; // Reset extra
        } else {
            // Interpret as HiY
            // HiY bits (7-11)
            session->tektronix.holding_y = (session->tektronix.holding_y & 0x07F) | (val << 7);
            session->tektronix.sub_state = 0; // Seen HiY
            session->tektronix.extra_byte = -1; // Reset extra
        }
    } else if (ch >= 0x60 && ch <= 0x7F) {
        // LoY or Extra Byte
        if (session->tektronix.extra_byte != -1) {
            // We already have a buffered byte -> It was Extra.
            int eb = session->tektronix.extra_byte;
            // Decode Extra Byte (Bits 1-5 of eb correspond to LSBs)
            // Tek 4014 Extra Byte Format:
            // Bits 4-3: Y LSBs (Bits 0-1 of Y)
            // Bits 2-1: X LSBs (Bits 0-1 of X)
            int x_lsb = eb & 0x03;
            int y_lsb = (eb >> 2) & 0x03;

            // Apply LSBs (Bits 0-1)
            session->tektronix.holding_x = (session->tektronix.holding_x & ~0x03) | x_lsb;
            session->tektronix.holding_y = (session->tektronix.holding_y & ~0x03) | y_lsb;

            // Current 'val' is the real LoY (Bits 2-6)
            session->tektronix.holding_y = (session->tektronix.holding_y & ~0x07C) | (val << 2);

            session->tektronix.extra_byte = -1; // Consumed
            session->tektronix.sub_state = 1; // LoY processed
        } else {
            // Potential LoY or Extra.
            session->tektronix.extra_byte = val; // Store raw value

            // Apply as LoY (Standard 10-bit behavior compat)
            session->tektronix.holding_y = (session->tektronix.holding_y & ~0x07C) | (val << 2);
            session->tektronix.sub_state = 1; // Flag: Next could be HiX
        }
    } else if (ch >= 0x40 && ch <= 0x5F) {
        // LoX - Trigger
        session->tektronix.holding_x = (session->tektronix.holding_x & ~0x07C) | (val << 2);

        // Reset extra byte (sequence ended)
        session->tektronix.extra_byte = -1;

        // DRAW
        if (session->tektronix.pen_down) {
            if (term->vector_count < term->vector_capacity) {
                GPUVectorLine* line = &term->vector_staging_buffer[term->vector_count];

                // 12-bit Coordinate Normalization (0-4095 -> 0.0-1.0)
                float norm_x1 = (float)session->tektronix.x / 4096.0f;
                float norm_y1 = (float)session->tektronix.y / 4096.0f;
                float norm_x2 = (float)session->tektronix.holding_x / 4096.0f;
                float norm_y2 = (float)session->tektronix.holding_y / 4096.0f;

                // Flip Y (Tektronix 0,0 is bottom-left)
                norm_y1 = 1.0f - norm_y1;
                norm_y2 = 1.0f - norm_y2;

                line->x0 = norm_x1;
                line->y0 = norm_y1;
                line->x1 = norm_x2;
                line->y1 = norm_y2;
                line->color = 0xFF00FF00; // Bright Green
                line->intensity = 1.0f;
                line->mode = 0; // Additive

                term->vector_count++;
            }
        }

        // Update Position
        session->tektronix.x = session->tektronix.holding_x;
        session->tektronix.y = session->tektronix.holding_y;
        session->tektronix.pen_down = true;
        session->tektronix.sub_state = 0;
    }
}

static void KTerm_PrepareKittyUpload(KTerm* term, KTermSession* session) {
    KittyGraphics* kitty = &session->kitty;

    // Check if we are continuing an upload (Chunked)
    if (kitty->active_upload && kitty->continuing) {
         return;
    }

    if (kitty->cmd.action == 't' || kitty->cmd.action == 'T' || kitty->cmd.action == 'f') {
        // Check Limits
        if (term->config.max_kitty_image_pixels > 0 && kitty->cmd.width > 0 && kitty->cmd.height > 0) {
            if ((long long)kitty->cmd.width * kitty->cmd.height > term->config.max_kitty_image_pixels) {
                if (session->options.debug_sequences) KTerm_LogUnsupportedSequence(term, "Kitty: Image exceeds max_kitty_image_pixels");
                return;
            }
        }

        KittyImageBuffer* img = NULL;

        // Ensure images array exists
        if (!kitty->images) {
            kitty->image_capacity = 64;
            kitty->images = KTerm_Calloc(kitty->image_capacity, sizeof(KittyImageBuffer));
        }

        // Find existing image
        for (int i = 0; i < kitty->image_count; i++) {
            if (kitty->images[i].id == kitty->cmd.id) {
                img = &kitty->images[i];
                break;
            }
        }

        if (kitty->cmd.action == 'f') {
            if (!img) return; // Error: Image not found for frame load
        } else {
            // 't' or 'T' - Create new or replace
            if (img) {
                // Free existing frames
                if (img->frames) {
                    for (int f = 0; f < img->frame_count; f++) {
                        if (img->frames[f].data) {
                            if (kitty->current_memory_usage >= img->frames[f].capacity)
                                kitty->current_memory_usage -= img->frames[f].capacity;
                            KTerm_Free(img->frames[f].data);
                        }
                        if (img->frames[f].texture.id != 0) KTerm_DestroyTexture(&img->frames[f].texture);
                    }
                    KTerm_Free(img->frames);
                }
                // Reset image
                img->frames = NULL;
                img->frame_count = 0;
                img->frame_capacity = 0;
            } else {
                // New Image
                if (kitty->image_count >= kitty->image_capacity) {
                    // Resize images array
                    int new_cap = (kitty->image_capacity == 0) ? 16 : kitty->image_capacity * 2;
                    KittyImageBuffer* new_images = KTerm_Realloc(kitty->images, new_cap * sizeof(KittyImageBuffer));
                    if (!new_images) return; // OOM
                    kitty->images = new_images;
                    kitty->image_capacity = new_cap;
                }
                img = &kitty->images[kitty->image_count++];
                memset(img, 0, sizeof(KittyImageBuffer));
                img->id = kitty->cmd.id;
            }

            // Set properties for t/T
            img->visible = (kitty->cmd.action != 't');

            // Defaults
            int char_w = DEFAULT_CHAR_WIDTH;
            int char_h = DEFAULT_CHAR_HEIGHT;
            if (session->soft_font.active) {
                char_w = session->soft_font.char_width;
                char_h = session->soft_font.char_height;
            }

            if (kitty->cmd.has_x) img->x = kitty->cmd.x;
            else img->x = session->cursor.x * char_w;

            if (kitty->cmd.has_y) img->y = kitty->cmd.y;
            else img->y = session->cursor.y * char_h;

            img->start_row = session->screen_head;
            img->z_index = kitty->cmd.z_index;
            img->complete = false;
        }

        // Add new frame
        if (img->frame_count >= img->frame_capacity) {
            int new_cap = (img->frame_capacity == 0) ? 4 : img->frame_capacity * 2;
            KittyFrame* new_frames = KTerm_Realloc(img->frames, new_cap * sizeof(KittyFrame));
            if (!new_frames) return;
            img->frames = new_frames;
            img->frame_capacity = new_cap;
        }

        KittyFrame* frame = &img->frames[img->frame_count++];
        memset(frame, 0, sizeof(KittyFrame));
        frame->width = kitty->cmd.width;
        frame->height = kitty->cmd.height;
        // z for 'f' is delay
        if (kitty->cmd.action == 'f') {
             frame->delay_ms = kitty->cmd.z_index;
             if (frame->delay_ms < 0) frame->delay_ms = 0; // Or global? Assume local for now
        }

        size_t initial_cap = 4096;
        if (kitty->current_memory_usage + initial_cap <= KTERM_KITTY_MEMORY_LIMIT) {
            frame->capacity = initial_cap;
            frame->data = KTerm_Malloc(frame->capacity);
            frame->size = 0;
            if (frame->data) {
                kitty->current_memory_usage += initial_cap;
                kitty->active_upload = img;
            } else {
                img->frame_count--;
                kitty->active_upload = NULL;
            }
        } else {
             if (session->options.debug_sequences) KTerm_LogUnsupportedSequence(term, "Kitty: Memory limit exceeded");
             kitty->active_upload = NULL;
        }
    }
}

static void KTerm_ParseKittyPair(KTermSession* session) {
    KittyGraphics* kitty = &session->kitty;
    char* key = kitty->key_buffer;
    char* val = kitty->val_buffer;
    int v = atoi(val);

    if (strcmp(key, "a") == 0) kitty->cmd.action = val[0];
    else if (strcmp(key, "d") == 0) kitty->cmd.delete_action = val[0];
    else if (strcmp(key, "f") == 0) kitty->cmd.format = v;
    else if (strcmp(key, "s") == 0) kitty->cmd.width = v;
    else if (strcmp(key, "v") == 0) kitty->cmd.height = v;
    else if (strcmp(key, "i") == 0) kitty->cmd.id = (uint32_t)v;
    else if (strcmp(key, "p") == 0) kitty->cmd.placement_id = (uint32_t)v;
    else if (strcmp(key, "x") == 0) { kitty->cmd.x = v; kitty->cmd.has_x = true; }
    else if (strcmp(key, "y") == 0) { kitty->cmd.y = v; kitty->cmd.has_y = true; }
    else if (strcmp(key, "z") == 0) kitty->cmd.z_index = v;
    else if (strcmp(key, "t") == 0) kitty->cmd.transmission_type = val[0];
    else if (strcmp(key, "m") == 0) kitty->cmd.medium = v;
    else if (strcmp(key, "q") == 0) kitty->cmd.quiet = (v != 0);
}

void KTerm_ProcessKittyChar(KTerm* term, KTermSession* session, unsigned char ch) {
    KittyGraphics* kitty = &session->kitty;

    // Handle ESC to terminate sequence (ST)
    if (ch == '\x1B') {
        // Flush pending value if in VALUE state
        if (kitty->state == 1) {
             kitty->val_buffer[kitty->val_len] = '\0';
             KTerm_ParseKittyPair(session);
        }
        session->saved_parse_state = PARSE_KITTY;
        session->parse_state = PARSE_STRING_TERMINATOR;
        return;
    }

    if (kitty->state == 0) { // KEY
        if (ch == '=') {
            kitty->key_buffer[kitty->key_len] = '\0';
            kitty->state = 1; // VALUE
            kitty->val_len = 0;
        } else if (ch == ',' || ch == ';') {
            kitty->key_len = 0; // Reset
            if (ch == ';') {
                kitty->state = 2; // PAYLOAD
                KTerm_PrepareKittyUpload(term, session);
            }
        } else {
            if (kitty->key_len < 31) kitty->key_buffer[kitty->key_len++] = ch;
        }
    } else if (kitty->state == 1) { // VALUE
        if (ch == ',' || ch == ';') {
            kitty->val_buffer[kitty->val_len] = '\0';
            KTerm_ParseKittyPair(session);
            kitty->state = 0; // Back to KEY
            kitty->key_len = 0;

            if (ch == ';') {
                kitty->state = 2; // PAYLOAD
                KTerm_PrepareKittyUpload(term, session);
            }
        } else {
            if (kitty->val_len < 127) kitty->val_buffer[kitty->val_len++] = ch;
        }
    } else if (kitty->state == 2) { // PAYLOAD (Base64)
        int val = -1;
        if (ch >= 'A' && ch <= 'Z') val = ch - 'A';
        else if (ch >= 'a' && ch <= 'z') val = ch - 'a' + 26;
        else if (ch >= '0' && ch <= '9') val = ch - '0' + 52;
        else if (ch == '+') val = 62;
        else if (ch == '/') val = 63;

        if (val != -1) {
            kitty->b64_accumulator = (kitty->b64_accumulator << 6) | val;
            kitty->b64_bits += 6;

            if (kitty->b64_bits >= 8) {
                kitty->b64_bits -= 8;
                unsigned char byte = (kitty->b64_accumulator >> kitty->b64_bits) & 0xFF;

                if (kitty->active_upload && kitty->active_upload->frame_count > 0) {
                    KittyFrame* frame = &kitty->active_upload->frames[kitty->active_upload->frame_count - 1];
                    if (frame->data) {
                        if (frame->size >= frame->capacity) {
                            size_t new_cap = frame->capacity * 2;
                            if (kitty->current_memory_usage + (new_cap - frame->capacity) <= KTERM_KITTY_MEMORY_LIMIT) {
                                unsigned char* new_data = KTerm_Realloc(frame->data, new_cap);
                                if (new_data) {
                                    kitty->current_memory_usage += (new_cap - frame->capacity);
                                    frame->data = new_data;
                                    frame->capacity = new_cap;
                                } else {
                                    // Realloc failed, stop uploading
                                    kitty->active_upload = NULL;
                                }
                            } else {
                                 // Limit exceeded
                                 if (session->options.debug_sequences) KTerm_LogUnsupportedSequence(term, "Kitty: Memory limit exceeded during upload");
                                 kitty->active_upload = NULL;
                            }
                        }

                        if (kitty->active_upload && frame->size < frame->capacity) {
                            frame->data[frame->size++] = byte;
                        }
                    }
                }
            }
        }
    }
}

void KTerm_ExecuteKittyCommand(KTerm* term, KTermSession* session) {
    KittyGraphics* kitty = &session->kitty;

    // Chunked Transmission logic
    kitty->continuing = (kitty->cmd.medium == 1);
    if (kitty->active_upload) {
        kitty->active_upload->complete = !kitty->continuing;
    }

    if (kitty->cmd.action == 't' || kitty->cmd.action == 'T' || kitty->cmd.action == 'p') {
        // Placement Logic (Update x, y, z for ID)
        if (kitty->images) {
            for(int i=0; i<kitty->image_count; i++) {
                if (kitty->images[i].id == kitty->cmd.id) {
                    // Found image, update placement if provided
                    if (kitty->cmd.has_x) kitty->images[i].x = kitty->cmd.x;
                    if (kitty->cmd.has_y) kitty->images[i].y = kitty->cmd.y;
                    if (kitty->cmd.z_index != 0) kitty->images[i].z_index = kitty->cmd.z_index;

                    // a=T or a=p makes it visible. a=t might be just upload.
                    if (kitty->cmd.action == 'T' || kitty->cmd.action == 'p') {
                        kitty->images[i].visible = true;
                        // On placement, re-anchor to current screen head
                        kitty->images[i].start_row = session->screen_head;
                    }
                    break;
                }
            }
        }
    }

    if (kitty->cmd.action == 't' || kitty->cmd.action == 'T') {
        if (session->options.debug_sequences) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Kitty Image Transmitted: ID=%u", kitty->cmd.id);
            KTerm_LogUnsupportedSequence(term, msg);
        }
    } else if (kitty->cmd.action == 'd') {
        if (kitty->cmd.delete_action == 'a') {
             if (kitty->images) {
                for (int i = 0; i < kitty->image_count; i++) {
                    if (kitty->images[i].frames) {
                        for (int f = 0; f < kitty->images[i].frame_count; f++) {
                            if (kitty->images[i].frames[f].data) KTerm_Free(kitty->images[i].frames[f].data);
                            if (kitty->images[i].frames[f].texture.id != 0) KTerm_DestroyTexture(&kitty->images[i].frames[f].texture);
                        }
                        KTerm_Free(kitty->images[i].frames);
                    }
                }
                KTerm_Free(kitty->images);
                kitty->images = NULL;
                kitty->image_count = 0;
                kitty->image_capacity = 0;
                kitty->active_upload = NULL;
                kitty->current_memory_usage = 0; // Reset usage
            }
            if (session->options.debug_sequences) KTerm_LogUnsupportedSequence(term, "Kitty: Deleted All Images");
        } else if (kitty->cmd.delete_action == 'i') {
            for (int i = 0; i < kitty->image_count; i++) {
                if (kitty->images[i].id == kitty->cmd.id) {
                    if (kitty->images[i].frames) {
                        for (int f = 0; f < kitty->images[i].frame_count; f++) {
                            if (kitty->images[i].frames[f].data) {
                                if (kitty->current_memory_usage >= kitty->images[i].frames[f].capacity) {
                                    kitty->current_memory_usage -= kitty->images[i].frames[f].capacity;
                                } else {
                                    kitty->current_memory_usage = 0; // Should not happen
                                }
                                KTerm_Free(kitty->images[i].frames[f].data);
                            }
                            if (kitty->images[i].frames[f].texture.id != 0) KTerm_DestroyTexture(&kitty->images[i].frames[f].texture);
                        }
                        KTerm_Free(kitty->images[i].frames);
                    }
                    memmove(&kitty->images[i], &kitty->images[i+1], (kitty->image_count - i - 1) * sizeof(KittyImageBuffer));
                    kitty->image_count--;
                    break;
                }
            }
             if (session->options.debug_sequences) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Kitty: Deleted Image ID=%u", kitty->cmd.id);
                KTerm_LogUnsupportedSequence(term, msg);
            }
        }
    } else if (kitty->cmd.action == 'q') {
        if (session->options.debug_sequences) KTerm_LogUnsupportedSequence(term, "Kitty: Query received");
    }
}

void KTerm_ProcessVT52Char(KTerm* term, KTermSession* session, unsigned char ch) {
    static bool expect_param = false;
    static char vt52_command = 0;

    if (!expect_param) {
        switch (ch) {
            case 'A': // Cursor up
                if (session->cursor.y > 0) session->cursor.y--;
                session->parse_state = VT_PARSE_NORMAL;
                break;

            case 'B': // Cursor down
                if (session->cursor.y < term->height - 1) session->cursor.y++;
                session->parse_state = VT_PARSE_NORMAL;
                break;

            case 'C': // Cursor right
                if (session->cursor.x < term->width - 1) session->cursor.x++;
                session->parse_state = VT_PARSE_NORMAL;
                break;

            case 'D': // Cursor left
                if (session->cursor.x > 0) session->cursor.x--;
                session->parse_state = VT_PARSE_NORMAL;
                break;

            case 'H': // Home cursor
                session->cursor.x = 0;
                session->cursor.y = 0;
                session->parse_state = VT_PARSE_NORMAL;
                break;

            case 'I': // Reverse line feed
                session->cursor.y--;
                if (session->cursor.y < 0) {
                    session->cursor.y = 0;
                    KTerm_ScrollDownRegion_Internal(term, session, 0, term->height - 1, 1);
                }
                session->parse_state = VT_PARSE_NORMAL;
                break;

            case 'J': // Clear to end of screen
                // Clear from cursor to end of line
                for (int x = session->cursor.x; x < term->width; x++) {
                    KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, session->cursor.y, x));
                }
                // Clear remaining lines
                for (int y = session->cursor.y + 1; y < term->height; y++) {
                    for (int x = 0; x < term->width; x++) {
                        KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, y, x));
                    }
                }
                session->parse_state = VT_PARSE_NORMAL;
                break;

            case 'K': // Clear to end of line
                for (int x = session->cursor.x; x < term->width; x++) {
                    KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, session->cursor.y, x));
                }
                session->parse_state = VT_PARSE_NORMAL;
                break;

            case 'Y': // Direct cursor address
                vt52_command = 'Y';
                expect_param = true;
                session->escape_pos = 0;
                break;

            case 'Z': // Identify
                KTerm_QueueSessionResponse(term, session, "\x1B/Z");
                session->parse_state = VT_PARSE_NORMAL;
                break;

            case '=': // Enter alternate keypad mode
                session->input.keypad_application_mode = true;
                session->parse_state = VT_PARSE_NORMAL;
                break;

            case '>': // Exit alternate keypad mode
                session->input.keypad_application_mode = false;
                session->parse_state = VT_PARSE_NORMAL;
                break;

            case '<': // Enter ANSI mode
                session->parse_state = VT_PARSE_NORMAL;
                session->dec_modes &= ~KTERM_MODE_VT52;
                break;

            case 'F': // Enter graphics mode
                session->charset.gl = &session->charset.g1; // Use G1 (DEC special)
                session->parse_state = VT_PARSE_NORMAL;
                break;

            case 'G': // Exit graphics mode
                session->charset.gl = &session->charset.g0; // Use G0 (ASCII)
                session->parse_state = VT_PARSE_NORMAL;
                break;

            default:
                // Unknown VT52 command
                session->parse_state = VT_PARSE_NORMAL;
                if (session->options.debug_sequences) {
                    char debug_msg[64];
                    snprintf(debug_msg, sizeof(debug_msg), "Unknown VT52 command: %c", ch);
                    KTerm_LogUnsupportedSequence(term, debug_msg);
                }
                break;
        }
    } else {
        // Handle parameterized commands
        if (vt52_command == 'Y') {
            if (session->escape_pos == 0) {
                // First parameter: row
                session->escape_buffer[0] = ch;
                session->escape_pos = 1;
            } else {
                // Second parameter: column
                int row = session->escape_buffer[0] - 32; // VT52 uses offset of 32
                int col = ch - 32;

                // Clamp to valid range
                session->cursor.y = (row < 0) ? 0 : (row >= term->height) ? term->height - 1 : row;
                session->cursor.x = (col < 0) ? 0 : (col >= term->width) ? term->width - 1 : col;

                expect_param = false;
                session->parse_state = VT_PARSE_NORMAL;
            }
        }
    }
}
// =============================================================================
// SIXEL GRAPHICS SUPPORT (Basic Implementation)
// =============================================================================

static float KTerm_HueToRGB(float p, float q, float t) {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f/6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f/2.0f) return q;
    if (t < 2.0f/3.0f) return p + (q - p) * (2.0f/3.0f - t) * 6.0f;
    return p;
}

static void KTerm_HLS2RGB(int h, int l, int s, unsigned char* r, unsigned char* g, unsigned char* b) {
    // H: 0-360, L: 0-100, S: 0-100
    float H = (float)h;
    float L = (float)l / 100.0f;
    float S = (float)s / 100.0f;

    if (S == 0.0f) {
        *r = *g = *b = (unsigned char)(L * 255.0f);
    } else {
        float q = (L < 0.5f) ? (L * (1.0f + S)) : (L + S - L * S);
        float p = 2.0f * L - q;

        float fr = KTerm_HueToRGB(p, q, H / 360.0f + 1.0f/3.0f);
        float fg = KTerm_HueToRGB(p, q, H / 360.0f);
        float fb = KTerm_HueToRGB(p, q, H / 360.0f - 1.0f/3.0f);

        *r = (unsigned char)(fr * 255.0f);
        *g = (unsigned char)(fg * 255.0f);
        *b = (unsigned char)(fb * 255.0f);
    }
}

void KTerm_ProcessSixelChar(KTerm* term, KTermSession* session, unsigned char ch) {
    (void)term;
    if (!session) return;

    // Determine Target Session
    KTermSession* target_session = session;
    if (term->sixel_target_session >= 0 && term->sixel_target_session < MAX_SESSIONS) {
        target_session = &term->sessions[term->sixel_target_session];
    }

    // 1. Check for digits across all states that consume them
    if (isdigit(ch)) {
        if (target_session->sixel.parse_state == SIXEL_STATE_REPEAT) {
            target_session->sixel.repeat_count = target_session->sixel.repeat_count * 10 + (ch - '0');
            return;
        } else if (target_session->sixel.parse_state == SIXEL_STATE_COLOR ||
                   target_session->sixel.parse_state == SIXEL_STATE_RASTER) {
            int idx = target_session->sixel.param_buffer_idx;
            if (idx < 8) {
                target_session->sixel.param_buffer[idx] = target_session->sixel.param_buffer[idx] * 10 + (ch - '0');
            }
            return;
        }
    }

    // 2. Handle Separator ';'
    if (ch == ';') {
        if (target_session->sixel.parse_state == SIXEL_STATE_COLOR ||
            target_session->sixel.parse_state == SIXEL_STATE_RASTER) {
            if (target_session->sixel.param_buffer_idx < 7) {
                target_session->sixel.param_buffer_idx++;
                target_session->sixel.param_buffer[target_session->sixel.param_buffer_idx] = 0;
            }
            return;
        }
    }

    // 3. Command Processing
    // If we are in a parameter state but receive a command char, finalize the previous command implicitly.
    if (target_session->sixel.parse_state == SIXEL_STATE_COLOR) {
        if (ch != '#' && !isdigit(ch) && ch != ';') {
            // Finalize KTermColor Command
            // # Pc ; Pu ; Px ; Py ; Pz (Define) OR # Pc (Select)
            if (target_session->sixel.param_buffer_idx >= 4) {
                // KTermColor Definition
                // Param 0: Index (Pc)
                // Param 1: Type (Pu) - 1=HLS, 2=RGB
                // Param 2,3,4: Components
                int idx = target_session->sixel.param_buffer[0];
                int type = target_session->sixel.param_buffer[1];
                int c1 = target_session->sixel.param_buffer[2];
                int c2 = target_session->sixel.param_buffer[3];
                int c3 = target_session->sixel.param_buffer[4];

                if (idx >= 0 && idx < 256) {
                    if (type == 2) { // RGB (0-100)
                        target_session->sixel.palette[idx] = (RGB_KTermColor){
                            (unsigned char)((c1 * 255) / 100),
                            (unsigned char)((c2 * 255) / 100),
                            (unsigned char)((c3 * 255) / 100),
                            255
                        };
                    } else if (type == 1) { // HLS (0-360, 0-100, 0-100)
                        unsigned char r, g, b;
                        KTerm_HLS2RGB(c1, c2, c3, &r, &g, &b);
                        target_session->sixel.palette[idx] = (RGB_KTermColor){ r, g, b, 255 };
                    }
                    target_session->sixel.color_index = idx; // Auto-select? Usually yes.
                }
            } else {
                // KTermColor Selection # Pc
                int idx = target_session->sixel.param_buffer[0];
                if (idx >= 0 && idx < 256) {
                    target_session->sixel.color_index = idx;
                }
            }
            target_session->sixel.parse_state = SIXEL_STATE_NORMAL;
        }
    } else if (target_session->sixel.parse_state == SIXEL_STATE_RASTER) {
        // Finalize Raster Attributes " Pan ; Pad ; Ph ; Pv
        // Just reset state for now, we don't resize based on this yet.
        target_session->sixel.parse_state = SIXEL_STATE_NORMAL;
    }

    switch (ch) {
        case '"': // Raster attributes
            target_session->sixel.parse_state = SIXEL_STATE_RASTER;
            target_session->sixel.param_buffer_idx = 0;
            memset(target_session->sixel.param_buffer, 0, sizeof(target_session->sixel.param_buffer));
            break;
        case '#': // KTermColor introducer
            target_session->sixel.parse_state = SIXEL_STATE_COLOR;
            target_session->sixel.param_buffer_idx = 0;
            memset(target_session->sixel.param_buffer, 0, sizeof(target_session->sixel.param_buffer));
            break;
        case '!': // Repeat introducer
            target_session->sixel.parse_state = SIXEL_STATE_REPEAT;
            target_session->sixel.repeat_count = 0;
            break;
        case '$': // Carriage return
            target_session->sixel.pos_x = 0;
            target_session->sixel.parse_state = SIXEL_STATE_NORMAL;
            break;
        case '-': // New line
            target_session->sixel.pos_x = 0;
            target_session->sixel.pos_y += 6;
            target_session->sixel.parse_state = SIXEL_STATE_NORMAL;
            break;
        case '\x1B': // Escape character - signals the start of the String Terminator (ST)
             // State change happens on SOURCE session, as it drives the parser loop
             session->parse_state = PARSE_SIXEL_ST;
             return;
        default:
            if (ch >= '?' && ch <= '~') {
                int sixel_val = ch - '?';
                int repeat = 1;

                if (target_session->sixel.parse_state == SIXEL_STATE_REPEAT) {
                    if (target_session->sixel.repeat_count > 0) repeat = target_session->sixel.repeat_count;
                    target_session->sixel.parse_state = SIXEL_STATE_NORMAL;
                    target_session->sixel.repeat_count = 0;
                }

                int actual_repeat = repeat;
                if (term->config.max_sixel_width > 0) {
                    int remaining = term->config.max_sixel_width - target_session->sixel.pos_x;
                    if (remaining < 0) remaining = 0;
                    if (actual_repeat > remaining) actual_repeat = remaining;
                }

                for (int r = 0; r < actual_repeat; r++) {
                    // Check Height Limit
                    if (term->config.max_sixel_height > 0 && target_session->sixel.pos_y >= term->config.max_sixel_height) {
                        break;
                    }

                    // Buffer Growth Logic
                    if (target_session->sixel.strip_count >= target_session->sixel.strip_capacity) {
                        size_t new_cap = target_session->sixel.strip_capacity * 2;
                        if (new_cap == 0) new_cap = 65536; // Fallback
                        GPUSixelStrip* new_strips = KTerm_Realloc(target_session->sixel.strips, new_cap * sizeof(GPUSixelStrip));
                        if (new_strips) {
                            target_session->sixel.strips = new_strips;
                            target_session->sixel.strip_capacity = new_cap;
                        } else {
                            break; // OOM, stop drawing
                        }
                    }

                    if (target_session->sixel.strip_count < target_session->sixel.strip_capacity) {
                        GPUSixelStrip* strip = &target_session->sixel.strips[target_session->sixel.strip_count++];
                        strip->x = target_session->sixel.pos_x + r;
                        strip->y = target_session->sixel.pos_y; // Top of the 6-pixel column
                        strip->pattern = sixel_val; // 6 bits
                        strip->color_index = target_session->sixel.color_index;
                    }
                }
                target_session->sixel.pos_x += actual_repeat;
                if (target_session->sixel.pos_x > target_session->sixel.max_x) {
                    target_session->sixel.max_x = target_session->sixel.pos_x;
                }
                if (target_session->sixel.pos_y + 6 > target_session->sixel.max_y) {
                    target_session->sixel.max_y = target_session->sixel.pos_y + 6;
                }
            }
            break;
    }
}

void KTerm_InitSixelGraphics(KTerm* term, KTermSession* session) {
    if (!session) session = GET_SESSION(term);
    session->sixel.active = false;
    if (session->sixel.data) {
        KTerm_Free(session->sixel.data);
    }
    session->sixel.data = NULL;
    session->sixel.width = 0;
    session->sixel.height = 0;
    session->sixel.x = 0;
    session->sixel.y = 0;

    if (session->sixel.strips) {
        KTerm_Free(session->sixel.strips);
    }
    session->sixel.strips = NULL;
    session->sixel.strip_count = 0;
    session->sixel.strip_capacity = 0;

    // Initialize standard palette (using global terminal palette as default)
    for (int i = 0; i < 256; i++) {
        session->sixel.palette[i] = term->color_palette[i];
    }
    session->sixel.parse_state = SIXEL_STATE_NORMAL;
    session->sixel.param_buffer_idx = 0;
    memset(session->sixel.param_buffer, 0, sizeof(session->sixel.param_buffer));

    // Initialize scrolling state from DEC Mode 80
    // DECSDM (80): true=No Scroll, false=Scroll
    session->sixel.scrolling = !(GET_SESSION(term)->dec_modes & KTERM_MODE_DECSDM);
}

void KTerm_ProcessSixelData(KTerm* term, KTermSession* session, const char* data, size_t length) {
    // Basic sixel processing - this is a complex format
    // This implementation provides framework for sixel support

    if (!(session->conformance.features & KTERM_FEATURE_SIXEL_GRAPHICS)) {
        KTerm_LogUnsupportedSequence(term, "Sixel graphics require support enabled");
        return;
    }

    // Allocate sixel staging buffer
    if (!session->sixel.strips) {
        session->sixel.strip_capacity = 65536;
        session->sixel.strips = (GPUSixelStrip*)KTerm_Calloc(session->sixel.strip_capacity, sizeof(GPUSixelStrip));
    }
    session->sixel.strip_count = 0; // Reset for new image? Or append? Standard DCS q usually starts new.

    session->sixel.active = true;
    session->sixel.x = session->cursor.x * term->char_width;
    session->sixel.y = session->cursor.y * term->char_height;

    // Initialize internal sixel state for parsing
    session->sixel.pos_x = 0;
    session->sixel.pos_y = 0;
    session->sixel.max_x = 0;
    session->sixel.max_y = 0;
    session->sixel.color_index = 0;
    session->sixel.repeat_count = 1;

    // Process the sixel data stream
    for (size_t i = 0; i < length; i++) {
        KTerm_ProcessSixelChar(term, session, data[i]);
    }

    session->sixel.dirty = true; // Mark for upload
}

void KTerm_DrawSixelGraphics(KTerm* term) {
    KTermSession* session = GET_SESSION(term);
    if (!(GET_SESSION(term)->conformance.features & KTERM_FEATURE_SIXEL_GRAPHICS) || !session->sixel.active) return;
    // Just mark dirty, real work happens in KTerm_Draw
    session->sixel.dirty = true;
}

// =============================================================================
// RECTANGULAR OPERATIONS (VT420)
// =============================================================================

void KTerm_ExecuteRectangularOps(KTerm* term, KTermSession* session) {
    // CSI Pts ; Pls ; Pbs ; Prs ; Pps ; Ptd ; Pld ; Ppd $ v
    if (!session) session = GET_SESSION(term);
    if (!(session->conformance.features & KTERM_FEATURE_RECT_OPERATIONS)) {
        KTerm_LogUnsupportedSequence(term, "Rectangular operations require support enabled");
        return;
    }

    // Default top/left is 1
    int top = KTerm_GetCSIParam(term, session, 0, 1);
    int left = KTerm_GetCSIParam(term, session, 1, 1);
    // Default bottom/right is 0 (special sentinel for "Page Limit")
    int bottom = KTerm_GetCSIParam(term, session, 2, 0);
    int right = KTerm_GetCSIParam(term, session, 3, 0);
    // Ignore Pps (Param 4)
    int dest_top = KTerm_GetCSIParam(term, session, 5, 1);
    int dest_left = KTerm_GetCSIParam(term, session, 6, 1);
    // Ignore Ppd (Param 7)

    int offset_top = 0;
    int offset_left = 0;
    int limit_bottom = session->rows;
    int limit_right = session->cols;

    // Handle DECOM (Origin Mode)
    if (session->dec_modes & KTERM_MODE_DECOM) {
        offset_top = session->scroll_top;
        offset_left = session->left_margin;
        limit_bottom = session->scroll_bottom + 1;
        limit_right = session->right_margin + 1;
    }

    // Apply defaults for Bottom/Right if 0
    if (bottom == 0) bottom = limit_bottom - offset_top;
    if (right == 0) right = limit_right - offset_left;

    // Convert to 0-based absolute coordinates
    int abs_top = (top - 1) + offset_top;
    int abs_left = (left - 1) + offset_left;
    int abs_bottom = (bottom - 1) + offset_top;
    int abs_right = (right - 1) + offset_left;

    int abs_dest_top = (dest_top - 1) + offset_top;
    int abs_dest_left = (dest_left - 1) + offset_left;

    // Clip to terminal boundaries
    if (abs_top < 0) abs_top = 0;
    if (abs_left < 0) abs_left = 0;
    if (abs_bottom >= term->height) abs_bottom = term->height - 1;
    if (abs_right >= term->width) abs_right = term->width - 1;

    // Validate
    if (abs_top > abs_bottom || abs_left > abs_right) return;

    VTRectangle rect = {abs_top, abs_left, abs_bottom, abs_right, true};
    KTerm_CopyRectangle(term, rect, abs_dest_left, abs_dest_top);
}

void KTerm_ExecuteRectangularOps2(KTerm* term, KTermSession* session) {
    // CSI Pt ; Pl ; Pb ; Pr $ w - Request checksum of rectangular area
    if (!session) session = GET_SESSION(term);
    if (!(session->conformance.features & KTERM_FEATURE_RECT_OPERATIONS)) {
        KTerm_LogUnsupportedSequence(term, "Rectangular operations require support enabled");
        return;
    }

    // Calculate checksum and respond
    // CSI Pid ; Pp ; Pt ; Pl ; Pb ; Pr $ w
    int pid = KTerm_GetCSIParam(term, session, 0, 1);
    // int page = KTerm_GetCSIParam(term, session, 1, 1); // Ignored
    int top = KTerm_GetCSIParam(term, session, 2, 1) - 1;
    int left = KTerm_GetCSIParam(term, session, 3, 1) - 1;
    int bottom = KTerm_GetCSIParam(term, session, 4, term->height) - 1;
    int right = KTerm_GetCSIParam(term, session, 5, term->width) - 1;

    // Validate
    if (top < 0) top = 0;
    if (left < 0) left = 0;
    if (bottom >= term->height) bottom = term->height - 1;
    if (right >= term->width) right = term->width - 1;

    unsigned int checksum = 0;
    if (top <= bottom && left <= right) {
        checksum = KTerm_CalculateRectChecksum(term, top, left, bottom, right);
    }

    char response[32];
    snprintf(response, sizeof(response), "\x1BP%d!~%04X\x1B\\", pid, checksum & 0xFFFF);
    KTerm_QueueSessionResponse(term, session, response);
}

void KTerm_CopyRectangle(KTerm* term, VTRectangle src, int dest_x, int dest_y) {
    int width = src.right - src.left + 1;
    int height = src.bottom - src.top + 1;

    KTermRect src_rect = {src.left, src.top, width, height};
    KTerm_QueueCopyRect(GET_SESSION(term), src_rect, dest_x, dest_y);
}


// =============================================================================
// COMPLETE TERMINAL TESTING FRAMEWORK
// =============================================================================

// Test helper functions
void KTerm_TestCursorMovement(KTerm* term) {
    KTerm_WriteString(term, "\x1B[2J\x1B[H"); // Clear screen, home cursor
    KTerm_WriteString(term, "VT Cursor Movement Test\n");
    KTerm_WriteString(term, "Testing basic cursor operations...\n\n");

    // Test cursor positioning
    KTerm_WriteString(term, "\x1B[5;10HPosition test");
    KTerm_WriteString(term, "\x1B[10;1H");

    // Test cursor movement
    KTerm_WriteString(term, "Moving: ");
    KTerm_WriteString(term, "\x1B[5CRIGHT ");
    KTerm_WriteString(term, "\x1B[3DBACK ");
    KTerm_WriteString(term, "\x1B[2AUP ");
    KTerm_WriteString(term, "\x1B[1BDOWN\n");

    // Test save/restore
    KTerm_WriteString(term, "\x1B[s"); // Save cursor
    KTerm_WriteString(term, "\x1B[15;20HTemp position");
    KTerm_WriteString(term, "\x1B[u"); // Restore cursor
    KTerm_WriteString(term, "Back to saved position\n");

    KTerm_WriteString(term, "\nCursor test complete.\n");
}

void KTerm_TestKTermColors(KTerm* term) {
    KTerm_WriteString(term, "\x1B[2J\x1B[H"); // Clear screen
    KTerm_WriteString(term, "VT Color Test\n\n");

    // Test basic 16 colors
    KTerm_WriteString(term, "Basic 16 colors:\n");
    for (int i = 0; i < 8; i++) {
        KTerm_WriteFormat(term, "\x1B[%dm Color %d \x1B[0m", 30 + i, i);
        KTerm_WriteFormat(term, "\x1B[%dm Bright %d \x1B[0m\n", 90 + i, i + 8);
    }

    // Test 256 colors (sample)
    KTerm_WriteString(term, "\n256-color sample:\n");
    for (int i = 16; i < 32; i++) {
        KTerm_WriteFormat(term, "\x1B[38;5;%dm███\x1B[0m", i);
    }
    KTerm_WriteString(term, "\n");

    // Test true color
    KTerm_WriteString(term, "\nTrue color gradient:\n");
    for (int i = 0; i < 24; i++) {
        int r = (i * 255) / 23;
        KTerm_WriteFormat(term, "\x1B[38;2;%d;0;0m█\x1B[0m", r);
    }
    KTerm_WriteString(term, "\n\nColor test complete.\n");
}

void KTerm_TestCharacterSets(KTerm* term) {
    KTerm_WriteString(term, "\x1B[2J\x1B[H"); // Clear screen
    KTerm_WriteString(term, "VT Character Set Test\n\n");

    // Test DEC Special Graphics
    KTerm_WriteString(term, "DEC Special Graphics:\n");
    KTerm_WriteString(term, "\x1B(0"); // Select DEC special
    KTerm_WriteString(term, "lqqqqqqqqqqqqqqqqqqqqqqqqqqqqqk\n");
    KTerm_WriteString(term, "x                             x\n");
    KTerm_WriteString(term, "x    DEC Line Drawing Test    x\n");
    KTerm_WriteString(term, "x                             x\n");
    KTerm_WriteString(term, "mqqqqqqqqqqwqqqqqqqqqqqqqqqqqj\n");
    KTerm_WriteString(term, "             x\n");
    KTerm_WriteString(term, "             x\n");
    KTerm_WriteString(term, "             v\n");
    KTerm_WriteString(term, "\x1B(B"); // Back to ASCII

    KTerm_WriteString(term, "\nASCII mode restored.\n");
    KTerm_WriteString(term, "Character set test complete.\n");
}

void KTerm_TestMouseTracking(KTerm* term) {
    KTerm_WriteString(term, "\x1B[2J\x1B[H"); // Clear screen
    KTerm_WriteString(term, "VT Mouse Tracking Test\n\n");

    KTerm_WriteString(term, "Enabling mouse tracking...\n");
    KTerm_WriteString(term, "\x1B[?1000h"); // Enable mouse tracking

    KTerm_WriteString(term, "Click anywhere to test mouse reporting.\n");
    KTerm_WriteString(term, "Mouse coordinates will be reported.\n");
    KTerm_WriteString(term, "Press ESC to disable mouse tracking.\n\n");

    // Mouse tracking will be handled by the input system
    // Results will appear as the user interacts
}

void KTerm_TestModes(KTerm* term) {
    KTerm_WriteString(term, "\x1B[2J\x1B[H"); // Clear screen
    KTerm_WriteString(term, "VT KTerm Modes Test\n\n");

    // Test insert mode
    KTerm_WriteString(term, "Testing insert mode:\n");
    KTerm_WriteString(term, "Original: ABCDEF\n");
    KTerm_WriteString(term, "ABCDEF\x1B[4D\x1B[4h***\x1B[4l");
    KTerm_WriteString(term, "\nAfter insert: AB***CDEF\n\n");

    // Test alternate screen
    KTerm_WriteString(term, "Testing alternate screen buffer...\n");
    KTerm_WriteString(term, "Switching to alternate screen in 2 seconds...\n");
    // Would need timing mechanism for full demo

    KTerm_WriteString(term, "\nMode test complete.\n");
}

void KTerm_RunAllTests(KTerm* term) {
    KTerm_WriteString(term, "\x1B[2J\x1B[H"); // Clear screen
    KTerm_WriteString(term, "Running Complete VT Test Suite\n");
    KTerm_WriteString(term, "==============================\n\n");

    KTerm_TestCursorMovement(term);
    KTerm_WriteString(term, "\nPress any key to continue...\n");
    // Would wait for input in full implementation

    KTerm_TestKTermColors(term);
    KTerm_WriteString(term, "\nPress any key to continue...\n");

    KTerm_TestCharacterSets(term);
    KTerm_WriteString(term, "\nPress any key to continue...\n");

    KTerm_TestModes(term);

    KTerm_WriteString(term, "\n\nAll tests completed!\n");
    KTerm_ShowInfo(term);
}

void KTerm_RunTest(KTerm* term, const char* test_name) {
    if (strcmp(test_name, "cursor") == 0) {
        KTerm_TestCursorMovement(term);
    } else if (strcmp(test_name, "colors") == 0) {
        KTerm_TestKTermColors(term);
    } else if (strcmp(test_name, "charset") == 0) {
        KTerm_TestCharacterSets(term);
    } else if (strcmp(test_name, "mouse") == 0) {
        KTerm_TestMouseTracking(term);
    } else if (strcmp(test_name, "modes") == 0) {
        KTerm_TestModes(term);
    } else if (strcmp(test_name, "all") == 0) {
        KTerm_RunAllTests(term);
    } else {
        KTerm_WriteFormat(term, "Unknown test: %s\n", test_name);
        KTerm_WriteString(term, "Available tests: cursor, colors, charset, mouse, modes, all\n");
    }
}

void KTerm_ShowInfo(KTerm* term) {
    KTermSession* session = GET_SESSION(term);
    KTerm_WriteString(term, "\n");
    KTerm_WriteString(term, "KTerm Information\n");
    KTerm_WriteString(term, "===================\n");
    KTerm_WriteFormat(term, "KTerm Type: %s\n", GET_SESSION(term)->title.terminal_name);
    KTerm_WriteFormat(term, "VT Level: %d\n", GET_SESSION(term)->conformance.level);
    KTerm_WriteFormat(term, "Primary DA: %s\n", GET_SESSION(term)->device_attributes);
    KTerm_WriteFormat(term, "Secondary DA: %s\n", GET_SESSION(term)->secondary_attributes);

    KTerm_WriteString(term, "\nSupported Features:\n");
    KTerm_WriteFormat(term, "- VT52 Mode: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_VT52_MODE) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- VT100 Mode: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_VT100_MODE) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- VT220 Mode: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_VT220_MODE) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- VT320 Mode: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_VT320_MODE) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- VT420 Mode: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_VT420_MODE) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- VT520 Mode: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_VT520_MODE) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- xterm Mode: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_XTERM_MODE) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- Sixel Graphics: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_SIXEL_GRAPHICS) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- ReGIS Graphics: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_REGIS_GRAPHICS) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- Rectangular Ops: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_RECT_OPERATIONS) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- Soft Fonts: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_SOFT_FONTS) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- NRCS: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_NATIONAL_CHARSETS) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- User Defined Keys: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_USER_DEFINED_KEYS) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- Mouse Tracking: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_MOUSE_TRACKING) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- True Color: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_TRUE_COLOR) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- Locator: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_LOCATOR) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- Multi-Session: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_MULTI_SESSION_MODE) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- Selective Erase: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_SELECTIVE_ERASE) ? "Yes" : "No");
    KTerm_WriteFormat(term, "- Left/Right Margin: %s\n", (GET_SESSION(term)->conformance.features & KTERM_FEATURE_LEFT_RIGHT_MARGIN) ? "Yes" : "No");

    KTerm_WriteString(term, "\nCurrent Settings:\n");
    KTerm_WriteFormat(term, "- Cursor Keys: %s\n", (session->dec_modes & KTERM_MODE_DECCKM) ? "Application" : "Normal");
    KTerm_WriteFormat(term, "- Keypad: %s\n", session->input.keypad_application_mode ? "Application" : "Numeric");
    KTerm_WriteFormat(term, "- Auto Wrap: %s\n", (session->dec_modes & KTERM_MODE_DECAWM) ? "On" : "Off");
    KTerm_WriteFormat(term, "- Origin Mode: %s\n", (session->dec_modes & KTERM_MODE_DECOM) ? "On" : "Off");
    KTerm_WriteFormat(term, "- Insert Mode: %s\n", (session->dec_modes & KTERM_MODE_INSERT) ? "On" : "Off");

    KTerm_WriteFormat(term, "\nScrolling Region: %d-%d\n",
                       GET_SESSION(term)->scroll_top + 1, GET_SESSION(term)->scroll_bottom + 1);
    KTerm_WriteFormat(term, "Margins: %d-%d\n",
                       GET_SESSION(term)->left_margin + 1, GET_SESSION(term)->right_margin + 1);

    KTerm_WriteString(term, "\nStatistics:\n");
    KTermStatus status = KTerm_GetStatus(term);
    KTerm_WriteFormat(term, "- Pipeline Usage: %zu/%d\n", status.pipeline_usage, (int)sizeof(GET_SESSION(term)->input_pipeline));
    KTerm_WriteFormat(term, "- Key Buffer: %zu\n", status.key_usage);
    KTerm_WriteFormat(term, "- Unsupported Sequences: %d\n", session->conformance.compliance.unsupported_sequences);

    if (session->conformance.compliance.last_unsupported[0]) {
        KTerm_WriteFormat(term, "- Last Unsupported: %s\n", session->conformance.compliance.last_unsupported);
    }
}

// =============================================================================
// FINAL API IMPLEMENTATIONS (Scripting, Core Loop, Lifecycle, etc.)
// This section includes high-level scripting utilities, functions for managing
// the terminal's core update and drawing loop, VT level configuration,
// processed keyboard event retrieval, debugging, and lifecycle management.
// These APIs are key to integrating the terminal into a host application
// and controlling its primary behaviors and features.
// =============================================================================

// --- Scripting API Functions ---
// These functions provide convenient wrappers for the hosting application to
// directly output text and simple commands to the terminal screen. They are
// primarily for local display purposes by the application embedding the terminal,
// simplifying tasks like showing status messages or basic UI elements.

/**
 * @brief Outputs a single character to the terminal's input pipeline.
 * Part of the scripting API for easy terminal manipulation by the hosting application.
 * This is a convenience wrapper around KTerm_WriteChar(term).
 * @param ch The character to output.
 */
void KTerm_Script_PutChar(KTerm* term, unsigned char ch) {
    KTerm_WriteChar(term, ch);
}

/**
 * @brief Prints a null-terminated string to the terminal's input pipeline.
 * Part of the scripting API. Convenience wrapper around KTerm_WriteString(term).
 * Useful for displaying messages from the hosting application on the term->
 * @param text The string to print.
 */
void KTerm_Script_Print(KTerm* term, const char* text) {
    KTerm_WriteString(term, text);
}

/**
 * @brief Prints a formatted string to the terminal's input pipeline.
 * Part of the scripting API. Convenience wrapper around KTerm_WriteFormat(term).
 * Allows for dynamic string construction for display by the hosting application.
 * @param format The printf-style format string.
 * @param ... Additional arguments for the format string.
 */
void KTerm_Script_Printf(KTerm* term, const char* format, ...) {
    char buffer[1024]; // Note: For very long formatted strings, consider dynamic allocation or a larger buffer.
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    KTerm_WriteString(term, buffer);
}

/**
 * @brief Clears the terminal screen and homes the cursor.
 * Part of the scripting API. This sends the standard escape sequences:
 * "ESC[2J" (Erase Display: entire screen) and "ESC[H" (Cursor Home: top-left).
 */
void KTerm_Script_Cls(KTerm* term) {
    KTerm_WriteString(term, "\x1B[2J\x1B[H");
}

/**
 * @brief Sets basic ANSI foreground and background colors for subsequent text output via the Scripting API.
 * Part of the scripting API.
 * This sends SGR (Select Graphic Rendition) escape sequences.
 * @param fg Foreground color index (0-7 for standard, 8-15 for bright).
 * @param bg Background color index (0-7 for standard, 8-15 for bright).
 */
void KTerm_Script_SetKTermColor(KTerm* term, int fg, int bg) {
    // Ensure fg/bg are within basic ANSI range 0-7 for 30-37/40-47
    // or 8-15 for 90-97/100-107 (bright versions)
    char color_seq[32];
    if (fg >= 0 && fg <= 15 && bg >=0 && bg <= 15) {
        snprintf(color_seq, sizeof(color_seq), "\x1B[%d;%dm",
            (fg < 8 ? 30+fg : 90+(fg-8)), // Map 0-7 to 30-37, 8-15 to 90-97
            (bg < 8 ? 40+bg : 100+(bg-8)) ); // Map 0-7 to 40-47, 8-15 to 100-107
    } else { // Fallback or invalid input
        snprintf(color_seq, sizeof(color_seq), "\x1B[0m"); // Reset to default colors
    }
    KTerm_WriteString(term, color_seq);
}

// --- VT Compliance Level Management ---

/**
 * @brief Sets the terminal's VT compatibility level (e.g., VT100, VT220, XTERM).
 * This is a cornerstone for controlling the terminal's behavior. Changing the level:
 *  - Modifies which escape sequences the terminal recognizes and processes.
 *  - Alters the strings returned for Device Attribute (DA) requests (e.g., CSI c).
 *  - Enables or disables specific features associated with that level, such as:
 *    - Sixel graphics (typically VT240/VT3xx+ or XTERM).
 *    - Advanced mouse tracking modes (VT200+ or XTERM).
 *    - National Replacement Character Sets (NRCS), DEC Special Graphics.
 *    - Rectangular area operations (VT420+).
 *    - User-Defined Keys (DECUDK, VT320+).
 *    - Soft Fonts (DECDLD, VT220+).
 *  - Updates internal feature flags in `GET_SESSION(term)->conformance.features`.
 * The library aims for compatibility with VT52, VT100, VT220, VT320, VT420, and xterm standards.
 *
 * @param level The desired VTLevel (e.g., VT_LEVEL_100, VT_LEVEL_XTERM).
 * @see VTLevel enum for available levels.
 * @see GET_SESSION(term)->h header documentation for a full list of KEY FEATURES and their typical VT level requirements.
 */
// Statically define the feature sets for each VT level for easy lookup.
typedef struct {
    VTLevel level;
    VTFeatures features;
    int max_session_count;
} VTLevelFeatureMapping;

static const VTLevelFeatureMapping vt_level_mappings[] = {
    { VT_LEVEL_52, KTERM_FEATURE_VT52_MODE, 1 },
    { VT_LEVEL_100, KTERM_FEATURE_VT100_MODE | KTERM_FEATURE_NATIONAL_CHARSETS, 1 },
    { VT_LEVEL_102, KTERM_FEATURE_VT100_MODE | KTERM_FEATURE_VT102_MODE | KTERM_FEATURE_NATIONAL_CHARSETS, 1 },
    { VT_LEVEL_132, KTERM_FEATURE_VT100_MODE | KTERM_FEATURE_VT102_MODE | KTERM_FEATURE_VT132_MODE | KTERM_FEATURE_NATIONAL_CHARSETS, 1 },
    { VT_LEVEL_220, KTERM_FEATURE_VT100_MODE | KTERM_FEATURE_VT102_MODE | KTERM_FEATURE_VT220_MODE | KTERM_FEATURE_NATIONAL_CHARSETS | KTERM_FEATURE_SOFT_FONTS | KTERM_FEATURE_USER_DEFINED_KEYS, 1 },
    { VT_LEVEL_320, KTERM_FEATURE_VT100_MODE | KTERM_FEATURE_VT102_MODE | KTERM_FEATURE_VT220_MODE | KTERM_FEATURE_VT320_MODE | KTERM_FEATURE_NATIONAL_CHARSETS | KTERM_FEATURE_SOFT_FONTS | KTERM_FEATURE_USER_DEFINED_KEYS, 1 },
    { VT_LEVEL_340, KTERM_FEATURE_VT100_MODE | KTERM_FEATURE_VT102_MODE | KTERM_FEATURE_VT220_MODE | KTERM_FEATURE_VT320_MODE | KTERM_FEATURE_VT340_MODE | KTERM_FEATURE_NATIONAL_CHARSETS | KTERM_FEATURE_SOFT_FONTS | KTERM_FEATURE_USER_DEFINED_KEYS | KTERM_FEATURE_SIXEL_GRAPHICS | KTERM_FEATURE_REGIS_GRAPHICS | KTERM_FEATURE_MULTI_SESSION_MODE | KTERM_FEATURE_LOCATOR, 2 },
    { VT_LEVEL_420, KTERM_FEATURE_VT100_MODE | KTERM_FEATURE_VT102_MODE | KTERM_FEATURE_VT220_MODE | KTERM_FEATURE_VT320_MODE | KTERM_FEATURE_VT340_MODE | KTERM_FEATURE_VT420_MODE | KTERM_FEATURE_NATIONAL_CHARSETS | KTERM_FEATURE_SOFT_FONTS | KTERM_FEATURE_USER_DEFINED_KEYS | KTERM_FEATURE_RECT_OPERATIONS | KTERM_FEATURE_SELECTIVE_ERASE | KTERM_FEATURE_MULTI_SESSION_MODE | KTERM_FEATURE_LOCATOR | KTERM_FEATURE_LEFT_RIGHT_MARGIN, 2 },
    { VT_LEVEL_510, KTERM_FEATURE_VT100_MODE | KTERM_FEATURE_VT102_MODE | KTERM_FEATURE_VT220_MODE | KTERM_FEATURE_VT320_MODE | KTERM_FEATURE_VT340_MODE | KTERM_FEATURE_VT420_MODE | KTERM_FEATURE_VT510_MODE | KTERM_FEATURE_NATIONAL_CHARSETS | KTERM_FEATURE_SOFT_FONTS | KTERM_FEATURE_USER_DEFINED_KEYS | KTERM_FEATURE_RECT_OPERATIONS | KTERM_FEATURE_SELECTIVE_ERASE | KTERM_FEATURE_LOCATOR | KTERM_FEATURE_LEFT_RIGHT_MARGIN, 2 },
    { VT_LEVEL_520, KTERM_FEATURE_VT100_MODE | KTERM_FEATURE_VT102_MODE | KTERM_FEATURE_VT220_MODE | KTERM_FEATURE_VT320_MODE | KTERM_FEATURE_VT340_MODE | KTERM_FEATURE_VT420_MODE | KTERM_FEATURE_VT510_MODE | KTERM_FEATURE_VT520_MODE | KTERM_FEATURE_NATIONAL_CHARSETS | KTERM_FEATURE_SOFT_FONTS | KTERM_FEATURE_USER_DEFINED_KEYS | KTERM_FEATURE_RECT_OPERATIONS | KTERM_FEATURE_SELECTIVE_ERASE | KTERM_FEATURE_LOCATOR | KTERM_FEATURE_MULTI_SESSION_MODE | KTERM_FEATURE_LEFT_RIGHT_MARGIN, 4 },
    { VT_LEVEL_525, KTERM_FEATURE_VT100_MODE | KTERM_FEATURE_VT102_MODE | KTERM_FEATURE_VT220_MODE | KTERM_FEATURE_VT320_MODE | KTERM_FEATURE_VT340_MODE | KTERM_FEATURE_VT420_MODE | KTERM_FEATURE_VT510_MODE | KTERM_FEATURE_VT520_MODE | KTERM_FEATURE_VT525_MODE | KTERM_FEATURE_NATIONAL_CHARSETS | KTERM_FEATURE_SOFT_FONTS | KTERM_FEATURE_USER_DEFINED_KEYS | KTERM_FEATURE_SIXEL_GRAPHICS | KTERM_FEATURE_REGIS_GRAPHICS | KTERM_FEATURE_RECT_OPERATIONS | KTERM_FEATURE_SELECTIVE_ERASE | KTERM_FEATURE_LOCATOR | KTERM_FEATURE_TRUE_COLOR | KTERM_FEATURE_MULTI_SESSION_MODE | KTERM_FEATURE_LEFT_RIGHT_MARGIN, 4 },
    { VT_LEVEL_XTERM, KTERM_FEATURE_VT100_MODE | KTERM_FEATURE_VT102_MODE | KTERM_FEATURE_VT220_MODE | KTERM_FEATURE_VT320_MODE | KTERM_FEATURE_VT340_MODE | KTERM_FEATURE_VT420_MODE | KTERM_FEATURE_VT520_MODE | KTERM_FEATURE_XTERM_MODE |
        KTERM_FEATURE_NATIONAL_CHARSETS | KTERM_FEATURE_SOFT_FONTS | KTERM_FEATURE_USER_DEFINED_KEYS | KTERM_FEATURE_SIXEL_GRAPHICS | KTERM_FEATURE_REGIS_GRAPHICS |
        KTERM_FEATURE_RECT_OPERATIONS | KTERM_FEATURE_SELECTIVE_ERASE | KTERM_FEATURE_LOCATOR | KTERM_FEATURE_TRUE_COLOR |
        KTERM_FEATURE_MOUSE_TRACKING | KTERM_FEATURE_ALTERNATE_SCREEN | KTERM_FEATURE_WINDOW_MANIPULATION | KTERM_FEATURE_LEFT_RIGHT_MARGIN, 1
    },
    { VT_LEVEL_K95, KTERM_FEATURE_K95_MODE, 1 },
    { VT_LEVEL_TT, KTERM_FEATURE_TT_MODE, 1 },
    { VT_LEVEL_PUTTY, KTERM_FEATURE_PUTTY_MODE, 1 },
    { VT_LEVEL_ANSI_SYS, KTERM_FEATURE_VT100_MODE, 1 },
};

void KTerm_SetLevel(KTerm* term, KTermSession* session, VTLevel level) {
    if (!session) session = GET_SESSION(term);
    bool level_found = false;
    for (size_t i = 0; i < sizeof(vt_level_mappings) / sizeof(vt_level_mappings[0]); i++) {
        if (vt_level_mappings[i].level == level) {
            session->conformance.features = vt_level_mappings[i].features;
            session->conformance.max_session_count = vt_level_mappings[i].max_session_count;
            level_found = true;
            break;
        }
    }

    if (!level_found) {
        // Default to a safe baseline if unknown
        session->conformance.features = vt_level_mappings[0].features;
        session->conformance.max_session_count = vt_level_mappings[0].max_session_count;
    }

    session->conformance.level = level;

    // Update Answerback string based on level
    if (level == VT_LEVEL_ANSI_SYS) {
        snprintf(session->answerback_buffer, KTERM_OUTPUT_PIPELINE_SIZE, "ANSI.SYS");
        // Force IBM Font
        KTerm_SetFont(term, "IBM");
        // Enforce authentic CGA palette (using the standard definitions)
        for (int i = 0; i < 16; i++) {
            term->color_palette[i] = (RGB_KTermColor){ cga_colors[i].r, cga_colors[i].g, cga_colors[i].b, 255 };
        }
    } else if (level == VT_LEVEL_XTERM) {
        snprintf(session->answerback_buffer, KTERM_OUTPUT_PIPELINE_SIZE, "kterm xterm");
    } else if (level >= VT_LEVEL_525) {
        snprintf(session->answerback_buffer, KTERM_OUTPUT_PIPELINE_SIZE, "kterm VT525");
    } else if (level >= VT_LEVEL_520) {
        snprintf(session->answerback_buffer, KTERM_OUTPUT_PIPELINE_SIZE, "kterm VT520");
    } else if (level >= VT_LEVEL_420) {
        snprintf(session->answerback_buffer, KTERM_OUTPUT_PIPELINE_SIZE, "kterm VT420");
    } else if (level >= VT_LEVEL_340) {
        snprintf(session->answerback_buffer, KTERM_OUTPUT_PIPELINE_SIZE, "kterm VT340");
    } else if (level >= VT_LEVEL_320) {
        snprintf(session->answerback_buffer, KTERM_OUTPUT_PIPELINE_SIZE, "kterm VT320");
    } else if (level >= VT_LEVEL_220) {
        snprintf(session->answerback_buffer, KTERM_OUTPUT_PIPELINE_SIZE, "kterm VT220");
    } else if (level >= VT_LEVEL_102) {
        snprintf(session->answerback_buffer, KTERM_OUTPUT_PIPELINE_SIZE, "kterm VT102");
    } else if (level >= VT_LEVEL_100) {
        snprintf(session->answerback_buffer, KTERM_OUTPUT_PIPELINE_SIZE, "kterm VT100");
    } else {
        snprintf(session->answerback_buffer, KTERM_OUTPUT_PIPELINE_SIZE, "kterm VT52");
    }

    // Update Device Attribute strings based on the level.
    if (level == VT_LEVEL_ANSI_SYS) {
        // ANSI.SYS does not typically respond to DA
        session->device_attributes[0] = '\0';
        session->secondary_attributes[0] = '\0';
        session->tertiary_attributes[0] = '\0';
    } else if (level == VT_LEVEL_XTERM) {
        strcpy(session->device_attributes, "\x1B[?41;1;2;6;7;8;9;15;18;21;22c");
        strcpy(session->secondary_attributes, "\x1B[>41;400;0c");
        strcpy(session->tertiary_attributes, "\x1B[>0;1;0c");
    } else if (level >= VT_LEVEL_525) {
        strcpy(session->device_attributes, "\x1B[?65;1;2;6;7;8;9;15;18;21;22;28;29c");
        strcpy(session->secondary_attributes, "\x1B[>52;10;0c");
        strcpy(session->tertiary_attributes, "\x1B[>0;1;0c");
    } else if (level >= VT_LEVEL_520) {
        strcpy(session->device_attributes, "\x1B[?65;1;2;6;7;8;9;15;18;21;22;28;29c");
        strcpy(session->secondary_attributes, "\x1B[>52;10;0c");
        strcpy(session->tertiary_attributes, "\x1B[>0;1;0c");
    } else if (level >= VT_LEVEL_420) {
        strcpy(session->device_attributes, "\x1B[?64;1;2;6;7;8;9;15;18;21;22;28;29c");
        strcpy(session->secondary_attributes, "\x1B[>41;10;0c");
        strcpy(session->tertiary_attributes, "\x1B[>0;1;0c");
    } else if (level >= VT_LEVEL_340 || level >= VT_LEVEL_320) {
        strcpy(session->device_attributes, "\x1B[?63;1;2;6;7;8;9;15;18;21c");
        strcpy(session->secondary_attributes, "\x1B[>24;10;0c");
        strcpy(session->tertiary_attributes, "");
    } else if (level >= VT_LEVEL_220) {
        strcpy(session->device_attributes, "\x1B[?62;1;2;6;7;8;9;15c");
        strcpy(session->secondary_attributes, "\x1B[>1;10;0c");
        strcpy(session->tertiary_attributes, "");
    } else if (level >= VT_LEVEL_102) {
        strcpy(session->device_attributes, "\x1B[?6c");
        strcpy(session->secondary_attributes, "\x1B[>0;95;0c");
        strcpy(session->tertiary_attributes, "");
    } else if (level >= VT_LEVEL_100) {
        strcpy(session->device_attributes, "\x1B[?1;2c");
        strcpy(session->secondary_attributes, "\x1B[>0;95;0c");
        strcpy(session->tertiary_attributes, "");
    } else { // VT52
        strcpy(session->device_attributes, "\x1B/Z");
        session->secondary_attributes[0] = '\0';
        session->tertiary_attributes[0] = '\0';
    }
}

VTLevel KTerm_GetLevel(KTerm* term) {
    return GET_SESSION(term)->conformance.level;
}

// --- Keyboard Input ---

/**
 * @brief Retrieves a fully processed keyboard event from the terminal's internal buffer.
 * The application hosting the terminal should call this function repeatedly (e.g., in its
 * main loop after `KTerm_UpdateKeyboard(term)`) to obtain keyboard input.
 *
 * The `VTKeyboard` system, updated by `KTerm_UpdateKeyboard(term)`, translates raw Platform key
 * presses into appropriate VT sequences or characters. This processing considers:
 *  - Modifier keys (Shift, Ctrl, Alt/Meta).
 *  - KTerm modes such as:
 *    - Application Cursor Keys (DECCKM): e.g., Up Arrow sends `ESC O A` instead of `ESC [ A`.
 *    - Application Keypad Mode (DECKPAM/DECKPNM): Numeric keypad keys send special sequences.
 *  - User-Defined Keys (DECUDK), if programmed.
 *
 * The `event->sequence` field of the returned `VTKeyEvent` struct contains the byte
 * sequence that should be transmitted to the connected PTY, host application, or
 * further processed locally.
 *
 * @param event Pointer to a `VTKeyEvent` structure that will be filled with the event data.
 * @return `true` if a key event was retrieved from the buffer, `false` if the buffer is empty.
 * @see KTerm_UpdateKeyboard(term) which captures Platform input and populates the event buffer.
 * @see VTKeyEvent struct for details on the event data fields.
 * @note The terminal platform provides robust keyboard translation, ensuring that applications
 *       running within the terminal receive the correct input sequences based on active modes.
 */

// --- Debugging ---

/**
 * @brief Enables or disables the terminal's debug mode for diagnostics.
 * When enabled, the terminal provides more verbose logging, which can be invaluable
 * for development and troubleshooting. This typically includes:
 *  - Logging of unrecognized or unsupported escape sequences.
 *  - Notifications about partially implemented features being invoked.
 *  - Potentially, details on internal state changes or parsing decisions.
 *
 * Debug output is usually directed via the `ResponseCallback`. If no callback is set,
 * the behavior might default to standard console output or be suppressed, depending
 * on the library's internal configuration.
 *
 * Enabling debug mode usually activates related flags like:
 *  - `GET_SESSION(term)->options.log_unsupported`: Specifically for unsupported sequences.
 *  - `GET_SESSION(term)->options.conformance_checking`: For stricter checks against VT standards.
 *  - `GET_SESSION(term)->status.debugging`: A general flag indicating debug activity.
 *
 * This capability allows developers to understand the terminal's interaction with
 * host applications and identify issues in escape sequence processing.
 *
 * @param enable `true` to enable debug mode, `false` to disable.
 */
void KTerm_EnableDebug(KTerm* term, bool enable) {
    GET_SESSION(term)->options.debug_sequences = enable;
    GET_SESSION(term)->options.log_unsupported = enable;
    GET_SESSION(term)->options.conformance_checking = enable;
    GET_SESSION(term)->status.debugging = enable; // General debugging flag for other parts of the library
}

// --- Core KTerm Loop Functions ---

/**
 * @brief Updates the terminal's internal state and processes incoming data.
 *
 * Called once per frame in the main loop, this function drives the terminal emulation by:
 * - **Processing Input**: Consumes characters from `GET_SESSION(term)->input_pipeline` via `KTerm_ProcessEvents(term)`, parsing VT52/xterm sequences with `KTerm_ProcessChar(term)`.
 * - **Handling Input Devices**: Updates keyboard (`KTerm_UpdateKeyboard(term)`) and mouse (`KTerm_UpdateMouse(term)`) states.
 * - **Auto-Printing**: Queues lines for printing when `GET_SESSION(term)->auto_print_enabled` and a newline occurs.
 * - **Managing Timers**: Updates cursor blink, text blink, and visual bell timers for visual effects.
 * - **Flushing Responses**: Sends queued responses (e.g., DSR, DA, focus events) via `term->response_callback`.
 * - **Rendering**: Draws the terminal display with `KTerm_Draw(term)`, including the custom mouse cursor.
 *
 * Performance is tuned via `GET_SESSION(term)->VTperformance` (e.g., `chars_per_frame`, `time_budget`) to balance responsiveness and throughput.
 *
 * @see KTerm_ProcessEvents(term) for input processing details.
 * @see KTerm_UpdateKeyboard(term) for keyboard handling.
 * @see KTerm_UpdateMouse(term) for mouse and focus event handling.
 * @see KTerm_Draw(term) for rendering details.
 * @see KTerm_QueueResponse(term) for response queuing.
 */
void KTerm_Update(KTerm* term) {
    term->pending_session_switch = -1; // Reset pending switch
    int saved_session = term->active_session;

    // Process all sessions
    for (int i = 0; i < MAX_SESSIONS; i++) {
        KTermSession* session = &term->sessions[i];

        KTERM_MUTEX_LOCK(session->lock); // Lock Session (Phase 3)

        // Process input from the pipeline
        KTerm_ProcessEventsInternal(term, session);

        // Flush queued operations to the grid
        KTerm_FlushOps(term, session);

        KTERM_MUTEX_UNLOCK(session->lock); // Unlock Session (Phase 3)

        // Update timers and bells for this session
        if (session->cursor.blink_enabled && (session->dec_modes & KTERM_MODE_DECTCEM)) {
            session->cursor.blink_state = KTerm_TimerGetOscillator(KTERM_OSC_SLOT_FAST_BLINK);
        } else {
            session->cursor.blink_state = true;
        }
        // Update Blink States: Bit 0 = Fast, Bit 1 = Slow, Bit 2 = Background
        bool fast_blink = KTerm_TimerGetOscillator(session->fast_blink_rate);
        bool slow_blink = KTerm_TimerGetOscillator(session->slow_blink_rate);
        bool bg_blink = KTerm_TimerGetOscillator(session->bg_blink_rate);
        session->text_blink_state = (fast_blink ? 1 : 0) | (slow_blink ? 2 : 0) | (bg_blink ? 4 : 0);

        if (session->visual_bell_timer > 0) {
            session->visual_bell_timer -= KTerm_GetFrameTime();
            if (session->visual_bell_timer < 0) session->visual_bell_timer = 0;
        }

        // Animation Logic
        if (session->kitty.images) {
            float dt = KTerm_GetFrameTime();
            for (int k=0; k<session->kitty.image_count; k++) {
                KittyImageBuffer* img = &session->kitty.images[k];
                if (img->frame_count > 1 && img->visible && img->complete) {
                    img->frame_timer += (dt * 1000.0);
                    int delay = img->frames[img->current_frame].delay_ms;
                    if (delay <= 0) delay = 40; // Default min delay

                    while (img->frame_timer >= delay) {
                        img->frame_timer -= delay;
                        img->current_frame = (img->current_frame + 1) % img->frame_count;
                        delay = img->frames[img->current_frame].delay_ms;
                        if (delay <= 0) delay = 40;
                    }
                }
            }
        }

        // Flush responses
        int head = atomic_load_explicit(&session->response_ring.head, memory_order_relaxed);
        int tail = atomic_load_explicit(&session->response_ring.tail, memory_order_acquire);

        if (head != tail) {
            if (term->output_sink) {
                // Determine contiguous chunk
                if (tail > head) {
                    term->output_sink(term->output_sink_ctx, &session->response_ring.data[head], tail - head);
                    head = tail;
                } else {
                    // Wrapped
                    term->output_sink(term->output_sink_ctx, &session->response_ring.data[head], KTERM_OUTPUT_PIPELINE_SIZE - head);
                    if (tail > 0) {
                        term->output_sink(term->output_sink_ctx, &session->response_ring.data[0], tail);
                    }
                    head = tail;
                }
                atomic_store_explicit(&session->response_ring.head, head, memory_order_release);
            } else if (term->response_callback) {
                // Determine contiguous chunk
                if (tail > head) {
                    term->response_callback(term, &session->response_ring.data[head], tail - head);
                    head = tail;
                } else {
                    // Wrapped
                    term->response_callback(term, &session->response_ring.data[head], KTERM_OUTPUT_PIPELINE_SIZE - head);
                    if (tail > 0) {
                        term->response_callback(term, &session->response_ring.data[0], tail);
                    }
                    head = tail;
                }
                atomic_store_explicit(&session->response_ring.head, head, memory_order_release);
            }
        }
    }

    // Restore active session for input handling, unless a switch occurred
    if (term->pending_session_switch != -1) {
        term->active_session = term->pending_session_switch;
    } else {
        term->active_session = saved_session;
    }

    // Process Input Buffer
    KTermSession* session = GET_SESSION(term);
    if (session->input.auto_process) {
        int current_tail = atomic_load_explicit(&session->input.buffer_tail, memory_order_relaxed);
        int current_head = atomic_load_explicit(&session->input.buffer_head, memory_order_acquire);

        while (current_tail != current_head) {
            KTermEvent* event = &session->input.buffer[current_tail];

            // 1. Send Sequence to Host
            if (event->sequence[0] != '\0') {
                KTerm_QueueSessionResponse(term, session, event->sequence);

                // 2. Local Echo
                if ((session->dec_modes & KTERM_MODE_LOCALECHO) || (session->dec_modes & KTERM_MODE_DECHDPXM)) {
                    KTerm_WriteString(term, event->sequence);
                }

                // 3. Visual Bell Trigger
                if (event->sequence[0] == 0x07) {
                    session->visual_bell_timer = 0.2f;
                }
            }

            current_tail = (current_tail + 1) % KEY_EVENT_BUFFER_SIZE;
            atomic_store_explicit(&session->input.buffer_tail, current_tail, memory_order_release);

            // Refresh head snapshot
            current_head = atomic_load_explicit(&session->input.buffer_head, memory_order_acquire);
        }
    }

    // Auto-print (Active session only for now, or loop?)
    // Let's assume auto-print works for active session interaction.
    if (GET_SESSION(term)->printer_available && GET_SESSION(term)->auto_print_enabled) {
        if (GET_SESSION(term)->cursor.y > GET_SESSION(term)->last_cursor_y && GET_SESSION(term)->last_cursor_y >= 0) {
            // Queue the previous line for printing
            char print_buffer[term->width + 2];
            size_t pos = 0;
            for (int x = 0; x < term->width && pos < term->width; x++) {
                EnhancedTermChar* cell = GetScreenCell(GET_SESSION(term), GET_SESSION(term)->last_cursor_y, x);
                print_buffer[pos++] = GetPrintableChar(cell->ch, &GET_SESSION(term)->charset);
            }
            if (pos < term->width + 1) {
                print_buffer[pos++] = '\n';
                print_buffer[pos] = '\0';
                KTerm_QueueSessionResponse(term, session, print_buffer);
            }
        }
        GET_SESSION(term)->last_cursor_y = GET_SESSION(term)->cursor.y;
    }

    KTermCompositor_Prepare(&term->compositor, term);

    // KTerm_Draw(term); // Decoupled: Rendering must be called explicitly (on render thread or main thread)
}


/**
 * @brief Renders the current visual state of the terminal to the Platform window.
 * This function must be called once per frame, within KTerm_BeginFrame()`
 * and `KTerm_EndFrame()` block. It translates the terminal's internal model into a
 * graphical representation.
 *
 * Key rendering capabilities and processes:
 *  -   **Character Grid**: Iterates through the active screen buffer (primary or alternate),
 *      drawing each `EnhancedTermChar`.
 *  -   **KTermColor Resolution**: Handles various color modes for foreground and background:
 *      - Standard 16 ANSI colors.
 *      - 256-color indexed palette.
 *      - 24-bit True KTermColor (RGB).
 *  -   **Text Attributes**: Applies a rich set of visual attributes:
 *      - Bold (typically by using brighter ANSI colors or a bold font variant).
 *      - Faint (dimmed text).
 *      - Italic (if supported by font or simulated).
 *      - Underline (single and double).
 *      - Blink (text alternates visibility based on `GET_SESSION(term)->text_blink_state`).
 *      - Reverse video (swaps foreground and background colors, per cell or screen-wide).
 *      - Strikethrough.
 *      - Overline.
 *      - Concealed text (rendered as spaces or not at all).
 *  -   **Glyph Rendering**: Uses a pre-rendered bitmap font (`font_texture`), typically
 *      CP437-based for broad character support within that range. Unicode characters
 *      outside this basic set might be mapped to '?' or require a more advanced font system.
 *  -   **Cursor**: Draws the cursor according to its current `EnhancedCursor` properties:
 *      - Shape: `CURSOR_BLOCK`, `CURSOR_UNDERLINE`, `CURSOR_BAR`.
 *      - Blink: Synchronized with `GET_SESSION(term)->cursor.blink_state`.
 *      - Visibility: Honors `GET_SESSION(term)->cursor.visible`.
 *      - KTermColor: Uses `GET_SESSION(term)->cursor.color`.
 *  -   **Sixel Graphics**: If `session->sixel.active` is true, Sixel graphics data is
 *      rendered, typically overlaid on the text grid.
 *  -   **Visual Bell**: If `GET_SESSION(term)->visual_bell_timer` is active, a visual flash effect
 *      may be rendered.
 *
 * The terminal provides a faithful visual emulation, leveraging the Platform for efficient
 * 2D rendering.
 *
 * @see EnhancedTermChar for the structure defining each character cell's properties.
 * @see EnhancedCursor for cursor attributes.
 * @see SixelGraphics for Sixel display state.
 * @see KTerm_Init(term) where `font_texture` is created.
 */
// =============================================================================
// BIDI (BIDIRECTIONAL) TEXT SUPPORT
// =============================================================================

// BiDi Types
#define BIDI_L  0 // Left-to-Right
#define BIDI_R  1 // Right-to-Left
#define BIDI_N  2 // Neutral (Numbers, Punctuation) - Simplified

// Helper: Check if character is RTL (Hebrew/Arabic ranges)
static bool IsRTL(uint32_t ch) {
    // Hebrew: 0590-05FF
    if (ch >= 0x0590 && ch <= 0x05FF) return true;
    // Arabic: 0600-06FF
    if (ch >= 0x0600 && ch <= 0x06FF) return true;
    // Arabic Supplement: 0750-077F
    if (ch >= 0x0750 && ch <= 0x077F) return true;
    // Arabic Extended-A: 08A0-08FF
    if (ch >= 0x08A0 && ch <= 0x08FF) return true;
    // FB50-FDFF: Arabic Presentation Forms-A
    if (ch >= 0xFB50 && ch <= 0xFDFF) return true;
    // FE70-FEFF: Arabic Presentation Forms-B
    if (ch >= 0xFE70 && ch <= 0xFEFF) return true;

    return false;
}

// Helper: Get Simplified BiDi Type
static int GetBiDiType(uint32_t ch) {
    if (IsRTL(ch)) return BIDI_R;

    // Digits (0-9) are treated as LTR to prevent reversal (e.g. 123 -> 321)
    if (ch >= '0' && ch <= '9') return BIDI_L;

    // Weak/Neutral characters (Simplified)
    // Spaces, Punctuation
    if (ch < 0x41) return BIDI_N; // Space, Punctuation
    if ((ch >= 0x5B && ch <= 0x60) || (ch >= 0x7B && ch <= 0x7E)) return BIDI_N; // brackets, etc.

    // Default to Left-to-Right (Latin, Cyrillic, etc.)
    return BIDI_L;
}

// Helper: Mirror characters (Parentheses, Brackets)
static uint32_t GetMirroredChar(uint32_t ch) {
    switch (ch) {
        case '(': return ')';
        case ')': return '(';
        case '[': return ']';
        case ']': return '[';
        case '{': return '}';
        case '}': return '{';
        case '<': return '>';
        case '>': return '<';
        default: return ch;
    }
}

// Helper: Reverse a run of characters in place
static void ReverseRun(EnhancedTermChar* row, int start, int end) {
    while (start < end) {
        EnhancedTermChar temp = row[start];
        row[start] = row[end];
        row[end] = temp;

        // Apply mirroring when reversing
        row[start].ch = GetMirroredChar(row[start].ch);
        row[end].ch = GetMirroredChar(row[end].ch);

        start++;
        end--;
    }
    // Handle middle char if any
    if (start == end) {
        row[start].ch = GetMirroredChar(row[start].ch);
    }
}

// Main Reordering Algorithm (Visual Reordering)
// Note: This internal implementation is used because fribidi is unavailable.
static void BiDiReorderRow(KTermSession* session, EnhancedTermChar* row, int width) {
    // Only reorder if BDSM is enabled
    if (!(session->dec_modes & KTERM_MODE_BDSM)) return;

    // Allocate types array (dynamic if large, preventing stack overflow)
    int stack_types[512];
    int* types = stack_types;
    if (width > 512) {
        types = (int*)KTerm_Malloc(width * sizeof(int));
        if (!types) return; // Allocation failed
    }
    int effective_width = width;

    // 1. Determine base direction (Assume LTR for now)
    // First pass: Classify
    for (int i = 0; i < effective_width; i++) {
        types[i] = GetBiDiType(row[i].ch);
    }

    // Second pass: Resolve Neutrals
    int last_strong = BIDI_L; // Base direction
    for (int i = 0; i < effective_width; i++) {
        if (types[i] != BIDI_N) {
            last_strong = types[i];
        } else {
            // Look ahead for next strong
            int next_strong = BIDI_L; // Default to base if end of line
            for (int j = i + 1; j < effective_width; j++) {
                if (types[j] != BIDI_N) {
                    next_strong = types[j];
                    break;
                }
            }

            // Resolve neutral
            if (last_strong == next_strong) {
                types[i] = last_strong;
            } else {
                types[i] = BIDI_L; // Base direction (L)
            }
        }
    }

    // Third pass: Identify and Reverse R runs
    int run_start = -1;
    for (int i = 0; i < effective_width; i++) {
        if (types[i] == BIDI_R) {
            if (run_start == -1) run_start = i;
        } else {
            if (run_start != -1) {
                ReverseRun(row, run_start, i - 1);
                run_start = -1;
            }
        }
    }
    if (run_start != -1) {
        ReverseRun(row, run_start, effective_width - 1);
    }

    if (types != stack_types) {
        KTerm_Free(types);
    }
}



// New API functions


void KTerm_Draw(KTerm* term) {
    if (!term->compute_initialized) return;
    KTermCompositor_Render(&term->compositor, term);
}


// --- Lifecycle Management ---

static void KTerm_CleanupSession(KTermSession* session) {
    if (session->screen_buffer) {
        KTerm_Free(session->screen_buffer);
        session->screen_buffer = NULL;
    }
    if (session->alt_buffer) {
        KTerm_Free(session->alt_buffer);
        session->alt_buffer = NULL;
    }

    if (session->tab_stops.stops) {
        KTerm_Free(session->tab_stops.stops);
        session->tab_stops.stops = NULL;
    }

    if (session->row_dirty) {
        KTerm_Free(session->row_dirty);
        session->row_dirty = NULL;
    }

    // Free Kitty Graphics resources per session
    if (session->kitty.images) {
        for (int k = 0; k < session->kitty.image_count; k++) {
            if (session->kitty.images[k].frames) {
                for (int f = 0; f < session->kitty.images[k].frame_count; f++) {
                    if (session->kitty.images[k].frames[f].data) {
                        KTerm_Free(session->kitty.images[k].frames[f].data);
                    }
                    if (session->kitty.images[k].frames[f].texture.id != 0) {
                        KTerm_DestroyTexture(&session->kitty.images[k].frames[f].texture);
                    }
                }
                KTerm_Free(session->kitty.images[k].frames);
            }
        }
        KTerm_Free(session->kitty.images);
        session->kitty.images = NULL;
    }
    session->kitty.current_memory_usage = 0;
    session->kitty.active_upload = NULL;

    // Free memory for programmable key sequences
    for (size_t k = 0; k < session->programmable_keys.count; k++) {
        if (session->programmable_keys.keys[k].sequence) {
            KTerm_Free(session->programmable_keys.keys[k].sequence);
            session->programmable_keys.keys[k].sequence = NULL;
        }
    }
    if (session->programmable_keys.keys) {
        KTerm_Free(session->programmable_keys.keys);
        session->programmable_keys.keys = NULL;
    }
    session->programmable_keys.count = 0;
    session->programmable_keys.capacity = 0;

    // Free stored macros
    if (session->stored_macros.macros) {
        for (size_t m = 0; m < session->stored_macros.count; m++) {
            if (session->stored_macros.macros[m].content) {
                KTerm_Free(session->stored_macros.macros[m].content);
            }
        }
        KTerm_Free(session->stored_macros.macros);
        session->stored_macros.macros = NULL;
    }
    session->stored_macros.count = 0;
    session->stored_macros.capacity = 0;

    // Free ReGIS Macros
    for (int i = 0; i < 26; i++) {
        if (session->regis.macros[i]) {
            KTerm_Free(session->regis.macros[i]);
            session->regis.macros[i] = NULL;
        }
    }
    if (session->regis.macro_buffer) {
        KTerm_Free(session->regis.macro_buffer);
        session->regis.macro_buffer = NULL;
    }

    // Free Sixel graphics data buffer
    if (session->sixel.data) {
        KTerm_Free(session->sixel.data);
        session->sixel.data = NULL;
    }

    // Free bracketed paste buffer
    if (session->bracketed_paste.buffer) {
        KTerm_Free(session->bracketed_paste.buffer);
        session->bracketed_paste.buffer = NULL;
    }
}

/**
 * @brief Cleans up all resources allocated by the terminal library.
 * This function must be called when the application is shutting down, typically
 * after the main loop has exited and before closing the Platform window (if Platform
 * is managed by the application).
 *
 * Its responsibilities include deallocating:
 *  - The main font texture (`font_texture`) loaded into GPU memory.
 *  - Memory used for storing sequences of programmable keys (`GET_SESSION(term)->programmable_keys`).
 *  - The Sixel graphics data buffer (`session->sixel.data`) if it was allocated.
 *  - The buffer for bracketed paste data (`GET_SESSION(term)->bracketed_paste.buffer`) if used.
 *
 * It also ensures the input pipeline is cleared. Proper cleanup prevents memory leaks
 * and releases GPU resources.
 */
void KTerm_Cleanup(KTerm* term) {
    // Free LRU Cache
    if (term->glyph_map) { KTerm_Free(term->glyph_map); term->glyph_map = NULL; }
    if (term->glyph_last_used) { KTerm_Free(term->glyph_last_used); term->glyph_last_used = NULL; }
    if (term->atlas_to_codepoint) { KTerm_Free(term->atlas_to_codepoint); term->atlas_to_codepoint = NULL; }
    if (term->font_atlas_pixels) { KTerm_Free(term->font_atlas_pixels); term->font_atlas_pixels = NULL; }

    if (term->font_texture.generation != 0) KTerm_DestroyTexture(&term->font_texture);
    if (term->output_texture.generation != 0) KTerm_DestroyTexture(&term->output_texture);
    if (term->sixel_texture.generation != 0) KTerm_DestroyTexture(&term->sixel_texture);
    if (term->dummy_sixel_texture.generation != 0) KTerm_DestroyTexture(&term->dummy_sixel_texture);
    if (term->clear_texture.generation != 0) KTerm_DestroyTexture(&term->clear_texture);
    if (term->terminal_buffer.id != 0) KTerm_DestroyBuffer(&term->terminal_buffer);
    if (term->shader_config_buffer.id != 0) KTerm_DestroyBuffer(&term->shader_config_buffer);
    if (term->compute_pipeline.id != 0) KTerm_DestroyPipeline(&term->compute_pipeline);
    if (term->texture_blit_pipeline.id != 0) KTerm_DestroyPipeline(&term->texture_blit_pipeline);

    // if (term->gpu_staging_buffer) {
    //     KTerm_Free(term->gpu_staging_buffer);
    //     term->gpu_staging_buffer = NULL;
    // }

    KTermCompositor_Cleanup(&term->compositor);

    // Free session buffers
    for (int i = 0; i < MAX_SESSIONS; i++) {
        KTerm_CleanupSession(&term->sessions[i]);
    }

    // Free Vector Engine resources
    if (term->vector_buffer.id != 0) KTerm_DestroyBuffer(&term->vector_buffer);
    if (term->vector_pipeline.id != 0) KTerm_DestroyPipeline(&term->vector_pipeline);
    if (term->vector_staging_buffer) {
        KTerm_Free(term->vector_staging_buffer);
        term->vector_staging_buffer = NULL;
    }

    if (term->row_scratch_buffer) {
        KTerm_Free(term->row_scratch_buffer);
        term->row_scratch_buffer = NULL;
    }

    KTerm_ClearEvents(term); // Ensure input pipeline is empty and reset

    // Destroy Locks (Phase 3)
    for (int i = 0; i < MAX_SESSIONS; i++) {
        KTERM_MUTEX_DESTROY(term->sessions[i].lock);
    }
    KTERM_MUTEX_DESTROY(term->lock);

    if (term->layout) {
        KTermLayout_Destroy(term->layout);
        term->layout = NULL;
    }
}

bool KTerm_InitDisplay(KTerm* term) {
    (void)term;
    // Create a virtual display for the terminal
    int vd_id;
    if (KTerm_CreateVirtualDisplay((KTermVector2){{(float)DEFAULT_WINDOW_WIDTH, (float)DEFAULT_WINDOW_HEIGHT}}, 1.0, 0, KTERM_SCALING_INTEGER, KTERM_BLEND_ALPHA, &vd_id) != KTERM_SUCCESS) {
        return false;
    }

    // Set the virtual display as renderable
    // SituationSetVirtualDisplayActive(1, true); // Not part of current API

    return true;
}

/*// Response callback to handle all terminal output (keyboard, mouse, focus events, DSR)
static void HandleKTermResponse(const char* response, int length) {
    // Print response for debugging
    printf("KTerm response: ");
    for (int i = 0; i < length; i++) {
        printf("0x%02X ", (unsigned char)response[i]);
    }
    printf("\n");

    // Echo input back to terminal (simulates PTY)
    KTerm_WriteString(term, response);
}
int main(void) {
    // Initialize Platform window
    KTermInitInfo init_info = { .window_width = DEFAULT_WINDOW_WIDTH, .window_height = DEFAULT_WINDOW_HEIGHT, .window_title = "Terminal", .initial_active_window_flags = KTERM_WINDOW_STATE_RESIZABLE };
    KTerm_Platform_Init(0, NULL, &init_info);
    KTerm_SetTargetFPS(60);

    // Initialize terminal state
    KTerm_Init(term);

    // Set response callback
    KTerm_SetResponseCallback(term, HandleKTermResponse);

    // Configure initial settings
    KTerm_EnableDebug(term, true); // Enable diagnostics
    KTerm_SetPipelineTargetFPS(term, 60); // Match pipeline to FPS

    // Send initial text
    KTerm_WriteString(term, "Welcome to K-Term!\n");
    KTerm_WriteString(term, "\x1B[32mGreen text\x1B[0m | \x1B[1mBold text\x1B[0m\n");
    KTerm_WriteString(term, "Type to interact. Try mouse modes:\n");
    KTerm_WriteString(term, "- \x1B[?1000h: VT200\n");
    KTerm_WriteString(term, "- \x1B[?1006h: SGR\n");
    KTerm_WriteString(term, "- \x1B[?1015h: URXVT\n");
    KTerm_WriteString(term, "- \x1B[?1016h: Pixel\n");
    KTerm_WriteString(term, "Focus window for \x1B[?1004h events.\n");

    // Enable mouse features
    KTerm_WriteString(term, "\x1B[?1004h"); // Focus In/Out
    KTerm_WriteString(term, "\x1B[?1000h"); // VT200
    KTerm_WriteString(term, "\x1B[?1006h"); // SGR

    while (!WindowShouldClose()) {
        // Update and render terminal (all input reported via HandleKTermResponse)
        KTerm_Update(term);

        // Render frame
        KTerm_BeginFrame();
            DrawFPS(10, 10); // Optional GUI element
        KTerm_EndFrame();
    }

    // Cleanup resources
    KTerm_Cleanup(term);
    KTerm_Platform_Shutdown();

    return 0;
}
*/


// =============================================================================
// OP QUEUE & FLUSHING
// =============================================================================

void KTerm_InitOpQueue(KTermOpQueue* queue) {
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
}

bool KTerm_IsOpQueueFull(KTermOpQueue* queue) {
    return queue->count >= KTERM_OP_QUEUE_SIZE;
}

bool KTerm_QueueOp(KTermOpQueue* queue, KTermOp op) {
    if (KTerm_IsOpQueueFull(queue)) return false;
    queue->ops[queue->tail] = op;
    queue->tail = (queue->tail + 1) % KTERM_OP_QUEUE_SIZE;
    queue->count++;
    return true;
}

void KTerm_QueueFillRect(KTermSession* session, KTermRect rect, EnhancedTermChar fill_char) {
    KTermOp op;
    op.type = KTERM_OP_FILL_RECT;
    op.u.fill.rect = rect;
    op.u.fill.fill_char = fill_char;
    KTerm_QueueOp(&session->op_queue, op);
}

void KTerm_QueueCopyRect(KTermSession* session, KTermRect src, int dst_x, int dst_y) {
    KTermOp op;
    op.type = KTERM_OP_COPY_RECT;
    op.u.copy.src = src;
    op.u.copy.dst_x = dst_x;
    op.u.copy.dst_y = dst_y;
    KTerm_QueueOp(&session->op_queue, op);
}

void KTerm_QueueSetAttrRect(KTermSession* session, KTermRect rect, uint32_t attr_mask, uint32_t attr_values, uint32_t attr_xor_mask, bool set_fg, ExtendedKTermColor fg, bool set_bg, ExtendedKTermColor bg) {
    KTermOp op;
    op.type = KTERM_OP_SET_ATTR_RECT;
    op.u.set_attr.rect = rect;
    op.u.set_attr.attr_mask = attr_mask;
    op.u.set_attr.attr_values = attr_values;
    op.u.set_attr.attr_xor_mask = attr_xor_mask;
    op.u.set_attr.set_fg = set_fg;
    op.u.set_attr.fg = fg;
    op.u.set_attr.set_bg = set_bg;
    op.u.set_attr.bg = bg;
    KTerm_QueueOp(&session->op_queue, op);
}

void KTerm_QueueInsertLines(KTermSession* session, int count, bool respect_protected) {
    // Calculate region affected by Insert Line at cursor
    // From cursor row to bottom of scrolling region
    int row = session->cursor.y;
    int scroll_bottom = session->scroll_bottom;

    if (row < session->scroll_top || row > scroll_bottom) return;

    KTermRect region;
    region.x = session->left_margin;
    region.y = row;
    region.w = session->right_margin - session->left_margin + 1;
    region.h = scroll_bottom - row + 1;

    KTermOp op;
    op.type = KTERM_OP_INSERT_LINES;
    op.u.vertical.region = region;
    op.u.vertical.count = count;
    op.u.vertical.respect_protected = respect_protected;
    op.u.vertical.downward = true; // Insert pushes down

    KTerm_QueueOp(&session->op_queue, op);
}

void KTerm_QueueDeleteLines(KTermSession* session, int count, bool respect_protected) {
    // Calculate region affected by Delete Line at cursor
    int row = session->cursor.y;
    int scroll_bottom = session->scroll_bottom;

    if (row < session->scroll_top || row > scroll_bottom) return;

    KTermRect region;
    region.x = session->left_margin;
    region.y = row;
    region.w = session->right_margin - session->left_margin + 1;
    region.h = scroll_bottom - row + 1;

    KTermOp op;
    op.type = KTERM_OP_DELETE_LINES;
    op.u.vertical.region = region;
    op.u.vertical.count = count;
    op.u.vertical.respect_protected = respect_protected;
    op.u.vertical.downward = false; // Delete pulls up

    KTerm_QueueOp(&session->op_queue, op);
}

void KTerm_QueueScrollRegion(KTermSession* session, KTermRect rect, int dy) {
    KTermOp op;
    op.type = KTERM_OP_SCROLL_REGION;
    op.u.scroll.rect = rect;
    op.u.scroll.dy = dy;
    KTerm_QueueOp(&session->op_queue, op);
}

void KTerm_QueueResize(KTermSession* session, int cols, int rows, bool reflow) {
    KTermOp op;
    op.type = KTERM_OP_RESIZE_GRID;
    op.u.resize.new_width = cols;
    op.u.resize.new_height = rows;
    op.u.resize.reflow_scrollback = reflow;
    KTerm_QueueOp(&session->op_queue, op);
}

static void KTerm_ApplyResizeOp(KTerm* term, KTermSession* session, KTermOp* op) {
    int cols = op->u.resize.new_width;
    int rows = op->u.resize.new_height;
    
    // Only resize if dimensions changed
    if (session->cols == cols && session->rows == rows) {
        return;
    }

    int old_cols = session->cols;
    int old_rows = session->rows;

    // Calculate new dimensions
    int new_buffer_height = rows + MAX_SCROLLBACK_LINES;

    // --- Screen Buffer Resize & Content Preservation (Viewport) ---
    EnhancedTermChar* new_screen_buffer = (EnhancedTermChar*)KTerm_Calloc(new_buffer_height * cols, sizeof(EnhancedTermChar));
    if (!new_screen_buffer) return;

    // Allocate new aux buffers before committing changes to avoid partial failure
    uint8_t* new_row_dirty = (uint8_t*)KTerm_Calloc(rows, sizeof(uint8_t));
    if (!new_row_dirty) {
        KTerm_Free(new_screen_buffer);
        return;
    }

    EnhancedTermChar* new_alt_buffer = (EnhancedTermChar*)KTerm_Calloc(rows * cols, sizeof(EnhancedTermChar));
    if (!new_alt_buffer) {
        KTerm_Free(new_screen_buffer);
        KTerm_Free(new_row_dirty);
        return;
    }

    // Default char for initialization
    EnhancedTermChar default_char = {
        .ch = ' ',
        .fg_color = {.color_mode = 0, .value.index = COLOR_WHITE},
        .bg_color = {.color_mode = 0, .value.index = COLOR_BLACK},
        .flags = KTERM_FLAG_DIRTY
    };

    // Initialize new buffer
    for (int k = 0; k < new_buffer_height * cols; k++) new_screen_buffer[k] = default_char;

    // Copy visible viewport AND history from old buffer
    int copy_rows = (old_rows < rows) ? old_rows : rows;
    int copy_cols = (old_cols < cols) ? old_cols : cols;

    // Optimize: Only copy populated history
    int start_y = -session->history_rows_populated;
    if (start_y < -MAX_SCROLLBACK_LINES) start_y = -MAX_SCROLLBACK_LINES;

    // Iterate from oldest history line to bottom of visible viewport
    for (int y = start_y; y < copy_rows; y++) {
        // Get pointer to source row in ring buffer (relative to active head)
        EnhancedTermChar* src_row_ptr = GetActiveScreenRow(session, y);

        // Calculate Destination Row Index in new linear buffer (Head=0)
        // Viewport (y>=0) -> [0, copy_rows-1]
        // History (y<0)   -> [new_buffer_height + y]
        int dst_row_idx;
        if (y >= 0) dst_row_idx = y;
        else dst_row_idx = new_buffer_height + y;

        // Safety check
        if (dst_row_idx >= 0 && dst_row_idx < new_buffer_height) {
            EnhancedTermChar* dst_row_ptr = &new_screen_buffer[dst_row_idx * cols];

            // Copy row content (up to min width)
            for (int x = 0; x < copy_cols; x++) {
                dst_row_ptr[x] = src_row_ptr[x];
                dst_row_ptr[x].flags |= KTERM_FLAG_DIRTY; // Force redraw
            }
        }
    }

    // --- Remap Kitty Image start_row ---
    if (session->kitty.images) {
        int old_head = session->screen_head;
        int old_height = session->buffer_height;
        int history = session->history_rows_populated;

        for (int k = 0; k < session->kitty.image_count; k++) {
            KittyImageBuffer* img = &session->kitty.images[k];

            // Calculate logical offset from the old top-left (screen_head)
            // Range: 0 to old_height-1
            // 0 = Top of visible screen, old_height-1 = Bottom of history (one line above top)
            int offset = (img->start_row - old_head + old_height) % old_height;

            int logical_y = 0;

            if (offset < old_rows) {
                // Visible region (0 to old_rows-1)
                logical_y = offset;
            } else if (offset >= old_height - history) {
                // History region (negative Y mapped to end of buffer logic)
                logical_y = offset - old_height;
            } else {
                // Image is in the "discarded" history gap. Mark invisible.
                img->visible = false;
                continue;
            }

            // Safety check for deep history aliasing
            if (logical_y < 0 && -logical_y >= new_buffer_height) {
                img->visible = false;
                continue;
            }

            // Calculate NEW absolute index in the NEW buffer (Head=0)
            int new_start_row;
            if (logical_y >= 0) {
                new_start_row = logical_y;
            } else {
                new_start_row = new_buffer_height + logical_y;
            }

            // Clamp
            if (new_start_row < 0) new_start_row += new_buffer_height;
            new_start_row %= new_buffer_height;

            img->start_row = new_start_row;
        }
    }

    // Commit changes
    if (session->screen_buffer) KTerm_Free(session->screen_buffer);
    session->screen_buffer = new_screen_buffer;

    if (session->row_dirty) KTerm_Free(session->row_dirty);
    session->row_dirty = new_row_dirty;
    for (int r = 0; r < rows; r++) session->row_dirty[r] = KTERM_DIRTY_FRAMES;

    if (session->alt_buffer) KTerm_Free(session->alt_buffer);
    session->alt_buffer = new_alt_buffer;
    for (int k = 0; k < rows * cols; k++) session->alt_buffer[k] = default_char;
    session->alt_screen_head = 0;

    session->cols = cols;
    session->rows = rows;
    session->buffer_height = new_buffer_height;

    // Reset ring buffer state
    session->screen_head = 0;
    session->view_offset = 0;
    session->saved_view_offset = 0;

    // Clamp cursor
    if (session->cursor.x >= cols) session->cursor.x = cols - 1;
    if (session->cursor.y >= rows) session->cursor.y = rows - 1;

    // Reset margins
    session->left_margin = 0;
    session->right_margin = cols - 1;
    session->scroll_top = 0;
    session->scroll_bottom = rows - 1;

    // --- Tab Stops Resize ---
    if (cols > session->tab_stops.capacity) {
        int old_capacity = session->tab_stops.capacity;
        int new_capacity = cols;
        bool* new_stops = (bool*)KTerm_Realloc(session->tab_stops.stops, new_capacity * sizeof(bool));
        if (new_stops) {
            session->tab_stops.stops = new_stops;
            session->tab_stops.capacity = new_capacity;

            // Initialize new space with default tab stops
            for (int i = old_capacity; i < new_capacity; i++) {
                if ((i % session->tab_stops.default_width) == 0 && i != 0) {
                     session->tab_stops.stops[i] = true;
                     session->tab_stops.count++;
                } else {
                     session->tab_stops.stops[i] = false;
                }
            }
        }
    }

    // Force full dirty
    session->dirty_rect = (KTermRect){0, 0, cols, rows};

    // Calculate index using pointer arithmetic
    ptrdiff_t idx = session - term->sessions;
    if (idx >= 0 && idx < MAX_SESSIONS) {
        if (term->session_resize_callback) {
            term->session_resize_callback(term, (int)idx, cols, rows);
        }
    }
}

static void KTerm_ApplyFillRectOp(KTermSession* session, KTermOp* op) {
    int top = op->u.fill.rect.y;
    int left = op->u.fill.rect.x;
    int width = op->u.fill.rect.w;
    int height = op->u.fill.rect.h;
    int bottom = top + height - 1;
    int right = left + width - 1;

    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            EnhancedTermChar* cell = GetActiveScreenCell(session, y, x);
            if (cell) {
                if (cell->flags & KTERM_ATTR_PROTECTED) continue;
                *cell = op->u.fill.fill_char;
                cell->flags |= KTERM_FLAG_DIRTY;
            }
        }
        if (y >= 0 && y < session->rows) session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }

    if (session->dirty_rect.w == 0) {
        session->dirty_rect = op->u.fill.rect;
    } else {
        int x1 = (left < session->dirty_rect.x) ? left : session->dirty_rect.x;
        int y1 = (top < session->dirty_rect.y) ? top : session->dirty_rect.y;
        int x2 = (right + 1 > session->dirty_rect.x + session->dirty_rect.w) ? right + 1 : session->dirty_rect.x + session->dirty_rect.w;
        int y2 = (bottom + 1 > session->dirty_rect.y + session->dirty_rect.h) ? bottom + 1 : session->dirty_rect.y + session->dirty_rect.h;
        session->dirty_rect = (KTermRect){x1, y1, x2 - x1, y2 - y1};
    }
}

static void KTerm_ApplyCopyRectOp(KTermSession* session, KTermOp* op) {
    KTermRect src = op->u.copy.src;
    int dest_x = op->u.copy.dst_x;
    int dest_y = op->u.copy.dst_y;
    int width = src.w;
    int height = src.h;

    // Allocate temp buffer
    EnhancedTermChar* temp = KTerm_Malloc(width * height * sizeof(EnhancedTermChar));
    if (!temp) return;

    // Copy src to temp
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (src.y + y < session->buffer_height && src.x + x < session->cols) {
                temp[y * width + x] = *GetActiveScreenCell(session, src.y + y, src.x + x);
            }
        }
    }

    // Copy temp to dest
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int dy = dest_y + y;
            int dx = dest_x + x;
            if (dy >= 0 && dy < session->rows && dx >= 0 && dx < session->cols) {
                EnhancedTermChar* cell = GetActiveScreenCell(session, dy, dx);
                if (cell->flags & KTERM_ATTR_PROTECTED) continue;

                *cell = temp[y * width + x];
                cell->flags |= KTERM_FLAG_DIRTY;
            }
        }
        if (dest_y + y >= 0 && dest_y + y < session->rows) {
            session->row_dirty[dest_y + y] = KTERM_DIRTY_FRAMES;
        }
    }
    KTerm_Free(temp);
    // Update dirty rect (Destination)
    KTermRect dst_rect = {dest_x, dest_y, width, height};
    if (session->dirty_rect.w == 0) {
        session->dirty_rect = dst_rect;
    } else {
        int x1 = (dest_x < session->dirty_rect.x) ? dest_x : session->dirty_rect.x;
        int y1 = (dest_y < session->dirty_rect.y) ? dest_y : session->dirty_rect.y;
        int x2 = (dest_x + width > session->dirty_rect.x + session->dirty_rect.w) ? dest_x + width : session->dirty_rect.x + session->dirty_rect.w;
        int y2 = (dest_y + height > session->dirty_rect.y + session->dirty_rect.h) ? dest_y + height : session->dirty_rect.y + session->dirty_rect.h;
        session->dirty_rect = (KTermRect){x1, y1, x2 - x1, y2 - y1};
    }
}

static void KTerm_ApplySetAttrRectOp(KTermSession* session, KTermOp* op) {
    int top = op->u.set_attr.rect.y;
    int left = op->u.set_attr.rect.x;
    int width = op->u.set_attr.rect.w;
    int height = op->u.set_attr.rect.h;
    int bottom = top + height - 1;
    int right = left + width - 1;

    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            EnhancedTermChar* cell = GetActiveScreenCell(session, y, x);
            if (cell) {
                if (cell->flags & KTERM_ATTR_PROTECTED) continue;

                if (op->u.set_attr.set_fg) cell->fg_color = op->u.set_attr.fg;
                if (op->u.set_attr.set_bg) cell->bg_color = op->u.set_attr.bg;

                // Modify flags
                uint32_t new_flags = cell->flags;
                if (op->u.set_attr.attr_mask) {
                    new_flags = (new_flags & ~op->u.set_attr.attr_mask) | (op->u.set_attr.attr_values & op->u.set_attr.attr_mask);
                }
                if (op->u.set_attr.attr_xor_mask) {
                    new_flags ^= op->u.set_attr.attr_xor_mask;
                }
                cell->flags = new_flags | KTERM_FLAG_DIRTY;
            }
        }
        if (y >= 0 && y < session->rows) session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }

    // Update dirty rect
    if (session->dirty_rect.w == 0) {
        session->dirty_rect = op->u.set_attr.rect;
    } else {
        int x1 = (left < session->dirty_rect.x) ? left : session->dirty_rect.x;
        int y1 = (top < session->dirty_rect.y) ? top : session->dirty_rect.y;
        int x2 = (right + 1 > session->dirty_rect.x + session->dirty_rect.w) ? right + 1 : session->dirty_rect.x + session->dirty_rect.w;
        int y2 = (bottom + 1 > session->dirty_rect.y + session->dirty_rect.h) ? bottom + 1 : session->dirty_rect.y + session->dirty_rect.h;
        session->dirty_rect = (KTermRect){x1, y1, x2 - x1, y2 - y1};
    }
}

static void KTerm_ApplyVerticalOp(KTermSession *session, KTermOp *op) {
    KTermRect r = op->u.vertical.region;
    int lines = op->u.vertical.count;
    if (lines <= 0) return;

    if (op->u.vertical.respect_protected) {
        if (IsRegionProtected(session, r.y, r.y + r.h - 1, r.x, r.x + r.w - 1)) {
            return;
        }
    }

    int start_row = r.y;
    int end_row = r.y + r.h; // Exclusive (start_row + height)

    // Safety clamp
    if (end_row > session->rows) end_row = session->rows;

    if (op->u.vertical.downward) { // INSERT: Shift Down
        // Shift lines down from bottom up
        for (int y = end_row - 1; y >= start_row + lines; y--) {
             for (int x = r.x; x < r.x + r.w; x++) {
                 // Direct cell copy
                 *GetActiveScreenCell(session, y, x) = *GetActiveScreenCell(session, y - lines, x);
                 GetActiveScreenCell(session, y, x)->flags |= KTERM_FLAG_DIRTY;
             }
             if (y < session->rows) session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }

        // Clear inserted lines at top
        for (int y = start_row; y < start_row + lines && y < end_row; y++) {
             for (int x = r.x; x < r.x + r.w; x++) {
                 KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, y, x));
             }
             if (y < session->rows) session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }

    } else { // DELETE: Shift Up
        // Shift lines up from top down
        for (int y = start_row; y <= end_row - lines - 1; y++) {
            for (int x = r.x; x < r.x + r.w; x++) {
                 *GetActiveScreenCell(session, y, x) = *GetActiveScreenCell(session, y + lines, x);
                 GetActiveScreenCell(session, y, x)->flags |= KTERM_FLAG_DIRTY;
            }
             if (y < session->rows) session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }

        // Clear bottom lines
        for (int y = end_row - lines; y < end_row; y++) {
             for (int x = r.x; x < r.x + r.w; x++) {
                 KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, y, x));
             }
             if (y < session->rows) session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }
    }

    // Update Dirty Rect
    // Affected area is the whole region r
    if (session->dirty_rect.w == 0) {
        session->dirty_rect = r;
    } else {
        int x1 = (r.x < session->dirty_rect.x) ? r.x : session->dirty_rect.x;
        int y1 = (r.y < session->dirty_rect.y) ? r.y : session->dirty_rect.y;
        int x2 = (r.x + r.w > session->dirty_rect.x + session->dirty_rect.w) ? r.x + r.w : session->dirty_rect.x + session->dirty_rect.w;
        int y2 = (r.y + r.h > session->dirty_rect.y + session->dirty_rect.h) ? r.y + r.h : session->dirty_rect.y + session->dirty_rect.h;
        session->dirty_rect = (KTermRect){x1, y1, x2 - x1, y2 - y1};
    }
}

static void KTerm_ApplyScrollOp(KTermSession* session, KTermOp* op) {
    int top = op->u.scroll.rect.y;
    int h = op->u.scroll.rect.h;
    int bottom = top + h - 1;
    int x_start = op->u.scroll.rect.x;
    int w = op->u.scroll.rect.w;
    int x_end = x_start + w - 1;

    int lines = abs(op->u.scroll.dy);
    int dy = op->u.scroll.dy;

    if (IsRegionProtected(session, top, bottom, x_start, x_end)) return;

    if (dy > 0) { // Scroll Up
        if (top == 0 && bottom == session->rows - 1 &&
            x_start == 0 && x_end == session->cols - 1) {

            for (int i = 0; i < lines; i++) {
                int new_row_idx = (session->screen_head + session->rows) % session->buffer_height;
                EnhancedTermChar* row_ptr = &session->screen_buffer[new_row_idx * session->cols];
                for (int c = 0; c < session->cols; c++) {
                    KTerm_ClearCell_Internal(session, &row_ptr[c]);
                }
                session->screen_head = (session->screen_head + 1) % session->buffer_height;
                if (session->view_offset > 0) session->view_offset++;
            }
        } else {
            for (int i = 0; i < lines; i++) {
                for (int y = top; y < bottom; y++) {
                    for (int x = x_start; x <= x_end; x++) {
                        *GetActiveScreenCell(session, y, x) = *GetActiveScreenCell(session, y + 1, x);
                        GetActiveScreenCell(session, y, x)->flags |= KTERM_FLAG_DIRTY;
                    }
                     session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }
                for (int x = x_start; x <= x_end; x++) {
                    KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, bottom, x));
                    GetActiveScreenCell(session, bottom, x)->flags |= KTERM_FLAG_DIRTY;
                }
                session->row_dirty[bottom] = KTERM_DIRTY_FRAMES;
            }
        }
    } else { // Scroll Down
         for (int i = 0; i < lines; i++) {
            for (int y = bottom; y > top; y--) {
                for (int x = x_start; x <= x_end; x++) {
                    *GetActiveScreenCell(session, y, x) = *GetActiveScreenCell(session, y - 1, x);
                     GetActiveScreenCell(session, y, x)->flags |= KTERM_FLAG_DIRTY;
                }
                session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }
            for (int x = x_start; x <= x_end; x++) {
                KTerm_ClearCell_Internal(session, GetActiveScreenCell(session, top, x));
                GetActiveScreenCell(session, top, x)->flags |= KTERM_FLAG_DIRTY;
            }
            session->row_dirty[top] = KTERM_DIRTY_FRAMES;
        }
    }

    if (session->dirty_rect.w == 0) {
        session->dirty_rect = (KTermRect){x_start, top, w, h};
    } else {
        int x1 = (x_start < session->dirty_rect.x) ? x_start : session->dirty_rect.x;
        int y1 = (top < session->dirty_rect.y) ? top : session->dirty_rect.y;
        int x2_a = x_start + w;
        int x2_b = session->dirty_rect.x + session->dirty_rect.w;
        int x2 = (x2_a > x2_b) ? x2_a : x2_b;
        int y2_a = top + h;
        int y2_b = session->dirty_rect.y + session->dirty_rect.h;
        int y2 = (y2_a > y2_b) ? y2_a : y2_b;

        session->dirty_rect = (KTermRect){x1, y1, x2 - x1, y2 - y1};
    }
}

void KTerm_FlushOps(KTerm* term, KTermSession* session) {
    KTermOpQueue* queue = &session->op_queue;
    int ops_processed = 0;
    while (queue->count > 0) {
        if (term->config.max_ops_per_flush > 0 && ops_processed >= term->config.max_ops_per_flush) break;
        ops_processed++;

        KTermOp* op = &queue->ops[queue->head];

        switch(op->type) {
            case KTERM_OP_SET_CELL:
                {
                    EnhancedTermChar* cell = GetActiveScreenCell(session, op->u.set_cell.y, op->u.set_cell.x);
                    if (cell) {
                        *cell = op->u.set_cell.cell;
                        if (op->u.set_cell.y < session->rows) session->row_dirty[op->u.set_cell.y] = KTERM_DIRTY_FRAMES;

                        int x = op->u.set_cell.x;
                        int y = op->u.set_cell.y;
                        if (session->dirty_rect.w == 0) {
                            session->dirty_rect = (KTermRect){x, y, 1, 1};
                        } else {
                            int min_x = (x < session->dirty_rect.x) ? x : session->dirty_rect.x;
                            int min_y = (y < session->dirty_rect.y) ? y : session->dirty_rect.y;
                            int max_x = (x + 1 > session->dirty_rect.x + session->dirty_rect.w) ? x + 1 : session->dirty_rect.x + session->dirty_rect.w;
                            int max_y = (y + 1 > session->dirty_rect.y + session->dirty_rect.h) ? y + 1 : session->dirty_rect.y + session->dirty_rect.h;
                            session->dirty_rect = (KTermRect){min_x, min_y, max_x - min_x, max_y - min_y};
                        }
                    }
                }
                break;
            case KTERM_OP_SCROLL_REGION:
                KTerm_ApplyScrollOp(session, op);
                break;
            case KTERM_OP_FILL_RECT:
                KTerm_ApplyFillRectOp(session, op);
                break;
            case KTERM_OP_COPY_RECT:
                KTerm_ApplyCopyRectOp(session, op);
                break;
            case KTERM_OP_SET_ATTR_RECT:
                KTerm_ApplySetAttrRectOp(session, op);
                break;
            case KTERM_OP_INSERT_LINES:
            case KTERM_OP_DELETE_LINES:
                KTerm_ApplyVerticalOp(session, op);
                break;
            case KTERM_OP_RESIZE_GRID:
                KTerm_ApplyResizeOp(term, session, op);
                break;
            default:
                break;
        }

        queue->head = (queue->head + 1) % KTERM_OP_QUEUE_SIZE;
        queue->count--;
    }
}

static void KTerm_ResetSessionDefaults(KTerm* term, KTermSession* session) {
    session->selection.active = false;
    session->selection.dragging = false;
    session->selection.start_x = -1;
    session->selection.start_y = -1;
    session->selection.end_x = -1;
    session->selection.end_y = -1;

    // Initialize mouse state
    session->mouse.enabled = true;
    session->mouse.mode = MOUSE_TRACKING_OFF;
    session->mouse.buttons[0] = session->mouse.buttons[1] = session->mouse.buttons[2] = false;
    session->mouse.last_x = session->mouse.last_y = 0;
    session->mouse.last_pixel_x = session->mouse.last_pixel_y = 0;
    session->mouse.focused = false;
    session->mouse.focus_tracking = false;
    session->mouse.sgr_mode = false;
    session->mouse.cursor_x = -1;
    session->mouse.cursor_y = -1;
    session->input.auto_process = true;

    session->cursor.visible = true;
    session->cursor.blink_enabled = true;
    session->cursor.blink_state = true;
    session->cursor.blink_timer = 0.0f;
    session->cursor.x = session->cursor.y = 0;
    session->cursor.color.color_mode = 0;
    session->cursor.color.value.index = 7; // White
    session->cursor.shape = CURSOR_BLOCK;

    session->text_blink_state = 1; // Default ON
    session->text_blink_timer = 0.0f;
    session->fast_blink_rate = KTERM_OSC_SLOT_FAST_BLINK;
    session->slow_blink_rate = KTERM_OSC_SLOT_SLOW_BLINK;
    session->bg_blink_rate = KTERM_OSC_SLOT_SLOW_BLINK;
    session->auto_repeat_rate = 30; // Default slow (~2Hz) or Standard
    session->auto_repeat_delay = 500; // Default 500ms
    session->enable_wide_chars = false; // Default: Fixed width mode (all chars width 1)
    session->visual_bell_timer = 0.0f;
    session->response_length = 0;
    session->response_enabled = true;
    session->parse_state = VT_PARSE_NORMAL;
    session->left_margin = 0;
    session->right_margin = term->width - 1;
    session->scroll_top = 0;
    session->scroll_bottom = term->height - 1;

    session->dec_modes &= ~KTERM_MODE_DECCKM;
    session->dec_modes &= ~KTERM_MODE_DECOM;
    session->dec_modes |= KTERM_MODE_DECAWM;
    session->dec_modes |= KTERM_MODE_DECTCEM;
    session->dec_modes &= ~KTERM_MODE_ALTSCREEN;
    session->dec_modes &= ~KTERM_MODE_INSERT;
    session->dec_modes &= ~KTERM_MODE_LNM;
    session->dec_modes &= ~KTERM_MODE_DECCOLM;
    session->dec_modes &= ~KTERM_MODE_LOCALECHO;
    session->dec_modes &= ~KTERM_MODE_VT52;
    session->dec_modes |= KTERM_MODE_DECBKM; // Default to BS (match input.backarrow_sends_bs)
    session->dec_modes &= ~KTERM_MODE_DECSDM; // Default: Scrolling Enabled
    session->dec_modes &= ~KTERM_MODE_DECEDM;
    session->dec_modes &= ~KTERM_MODE_SIXEL_CURSOR;
    session->dec_modes |= KTERM_MODE_DECECR; // Default: Enabled
    session->dec_modes &= ~KTERM_MODE_DECPFF;
    session->dec_modes &= ~KTERM_MODE_DECPEX;
    session->dec_modes &= ~KTERM_MODE_ALLOW_80_132;
    session->dec_modes &= ~KTERM_MODE_ALT_CURSOR_SAVE;

    session->ansi_modes.insert_replace = false;
    session->ansi_modes.line_feed_new_line = true;

    session->soft_font.active = false;
    session->soft_font.dirty = false;
    session->soft_font.char_width = 8;
    session->soft_font.char_height = 16;

    // Grid defaults
    session->grid_enabled = false;
    session->grid_color = (RGB_KTermColor){255, 255, 255, 255}; // Default White
    session->conceal_char_code = 0;

    // Reset attributes manually as KTerm_ResetAllAttributes depends on (*GET_SESSION(term))
    session->current_fg.color_mode = 0; session->current_fg.value.index = COLOR_WHITE;
    session->current_bg.color_mode = 0; session->current_bg.value.index = COLOR_BLACK;
    session->current_ul_color.color_mode = 2; // Default/Inherit
    session->current_st_color.color_mode = 2; // Default/Inherit
    session->current_attributes = 0;

    session->bracketed_paste.enabled = false;
    session->bracketed_paste.active = false;
    session->bracketed_paste.buffer = NULL;

    session->programmable_keys.keys = NULL;
    session->programmable_keys.count = 0;
    session->programmable_keys.capacity = 0;

    ptrdiff_t index = session - term->sessions;
    snprintf(session->title.terminal_name, sizeof(session->title.terminal_name), "Session %d", (int)index + 1);
    snprintf(session->title.window_title, sizeof(session->title.window_title), "KTerm Session %d", (int)index + 1);
    snprintf(session->title.icon_title, sizeof(session->title.icon_title), "Term %d", (int)index + 1);

    session->input_pipeline_length = 0; // Fix: was missing, implicitly 0
    session->pipeline_head = 0;
    session->pipeline_tail = 0;
    session->pipeline_count = 0;
    session->pipeline_overflow = false;
    session->xoff_sent = false;

    session->VTperformance.chars_per_frame = 200;
    session->VTperformance.target_frame_time = 1.0 / 60.0;
    session->VTperformance.time_budget = session->VTperformance.target_frame_time * 0.5;
    session->VTperformance.avg_process_time = 0.000001;
    session->VTperformance.burst_mode = false;
    session->VTperformance.burst_threshold = 8192; // Approx half of 16384
    session->VTperformance.adaptive_processing = true;

    session->parse_state = VT_PARSE_NORMAL;
    session->escape_pos = 0;
    session->param_count = 0;

    session->options.conformance_checking = true;
    session->options.vttest_mode = false;
    session->options.debug_sequences = false;
    session->options.log_unsupported = true;

    session->synchronized_update = false;

    session->session_open = true;
    session->echo_enabled = true;
    session->input_enabled = true;
    session->password_mode = false;
    session->raw_mode = false;
    session->paused = false;

    session->printer_available = false;
    session->auto_print_enabled = false;
    session->printer_controller_enabled = false;
    session->locator_events.report_button_down = false;
    session->locator_events.report_button_up = false;
    session->locator_events.report_on_request_only = true;
    session->locator_enabled = false;
    session->programmable_keys.udk_locked = false;

    session->macro_space.used = 0;
    session->macro_space.total = 4096;

    session->printer_buf_len = 0;
    memset(session->printer_buffer, 0, sizeof(session->printer_buffer));

    // Initial answerback (will be updated by SetLevel)
    session->answerback_buffer[0] = '\0';
}

bool KTerm_InitSession(KTerm* term, int index) {
    KTermSession* session = &term->sessions[index];

    session->last_cursor_y = -1;

    // Initialize dimensions if not already set (defaults)
    if (session->cols == 0) session->cols = (term->width > 0) ? term->width : DEFAULT_TERM_WIDTH;
    if (session->rows == 0) session->rows = (term->height > 0) ? term->height : DEFAULT_TERM_HEIGHT;

    // Initialize Op Queue
    KTerm_InitOpQueue(&session->op_queue);
    session->dirty_rect = (KTermRect){0, 0, 0, 0};

    // Initialize Graphics Subsystems (Safely resets if already initialized)
    KTerm_InitSixelGraphics(term, session);
    KTerm_InitReGIS(term, session);
    KTerm_InitTektronix(term, session);
    KTerm_InitKitty(term, session);

    // Initialize session defaults
    EnhancedTermChar default_char = {
        .ch = ' ',
        .fg_color = {.color_mode = 0, .value.index = COLOR_WHITE},
        .bg_color = {.color_mode = 0, .value.index = COLOR_BLACK},
        .flags = KTERM_FLAG_DIRTY
    };

    // Initialize Ring Buffer
    // Primary buffer includes scrollback
    session->buffer_height = session->rows + MAX_SCROLLBACK_LINES;
    session->screen_head = 0;
    session->alt_screen_head = 0;
    session->view_offset = 0;
    session->saved_view_offset = 0;

    if (session->screen_buffer) KTerm_Free(session->screen_buffer);
    session->screen_buffer = (EnhancedTermChar*)KTerm_Calloc(session->buffer_height * session->cols, sizeof(EnhancedTermChar));
    if (!session->screen_buffer) {
        KTerm_ReportError(term, KTERM_LOG_FATAL, KTERM_SOURCE_SYSTEM, "Failed to allocate screen buffer for session %d", index);
        return false;
    }

    if (session->alt_buffer) KTerm_Free(session->alt_buffer);
    // Alt buffer is typically fixed size (no scrollback)
    session->alt_buffer = (EnhancedTermChar*)KTerm_Calloc(session->rows * session->cols, sizeof(EnhancedTermChar));
    if (!session->alt_buffer) {
        KTerm_ReportError(term, KTERM_LOG_FATAL, KTERM_SOURCE_SYSTEM, "Failed to allocate alt buffer for session %d", index);
        KTerm_Free(session->screen_buffer);
        session->screen_buffer = NULL;
        return false;
    }

    // Fill with default char
    for (int i = 0; i < session->buffer_height * session->cols; i++) {
        session->screen_buffer[i] = default_char;
    }

    // Alt buffer is smaller
    for (int i = 0; i < session->rows * session->cols; i++) {
        session->alt_buffer[i] = default_char;
    }

    // Initialize dirty rows for viewport
    if (session->row_dirty) KTerm_Free(session->row_dirty);
    session->row_dirty = (uint8_t*)KTerm_Calloc(session->rows, sizeof(uint8_t));
    if (!session->row_dirty) {
        KTerm_ReportError(term, KTERM_LOG_FATAL, KTERM_SOURCE_SYSTEM, "Failed to allocate dirty row flags for session %d", index);
        KTerm_Free(session->screen_buffer);
        session->screen_buffer = NULL;
        KTerm_Free(session->alt_buffer);
        session->alt_buffer = NULL;
        return false;
    }
    for (int y = 0; y < session->rows; y++) {
        session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }

    KTerm_ResetSessionDefaults(term, session);

    // Init charsets, tabs, keyboard
    // We can reuse the helper functions if they operate on (*GET_SESSION(term)) and we switch context

    return true;
}

void KTerm_SetResponseEnabled(KTerm* term, int session_index, bool enable) {
    if (session_index >= 0 && session_index < MAX_SESSIONS) {
        term->sessions[session_index].response_enabled = enable;
    }
}

void KTerm_SetActiveSession(KTerm* term, int index) {
    if (index >= 0 && index < MAX_SESSIONS) {
        // Only switch if actually changing
        if (term->active_session != index) {
            term->active_session = index;
            term->pending_session_switch = index;

            // Force redraw of the newly active session
            KTermSession* new_session = &term->sessions[index];
            for(int y = 0; y < term->height; y++) {
                if (y < new_session->rows) {
                    new_session->row_dirty[y] = KTERM_DIRTY_FRAMES;
    }
            }

            // Flag font atlas as dirty to ensure correct font is loaded for this session
            term->font_atlas_dirty = true;

            // Update window title immediately
            if (term->title_callback) {
                term->title_callback(term, new_session->title.window_title, false);
            }
            KTerm_SetWindowTitlePlatform(new_session->title.window_title);
        }
    }
}


void KTerm_SetSplitScreen(KTerm* term, bool active, int row, int top_idx, int bot_idx) {
    term->split_screen_active = active;
    if (active) {
        term->split_row = row;
        if (top_idx >= 0 && top_idx < MAX_SESSIONS) term->session_top = top_idx;
        if (bot_idx >= 0 && bot_idx < MAX_SESSIONS) term->session_bottom = bot_idx;

        // Invalidate both sessions to force redraw
        for(int y=0; y<term->height; y++) {
            term->sessions[term->session_top].row_dirty[y] = KTERM_DIRTY_FRAMES;
            term->sessions[term->session_bottom].row_dirty[y] = KTERM_DIRTY_FRAMES;
        }
    } else {
        // Invalidate active session
         for(int y=0; y<term->height; y++) {
            term->sessions[term->active_session].row_dirty[y] = KTERM_DIRTY_FRAMES;
        }
    }
}


void KTerm_WriteCharToSession(KTerm* term, int session_index, unsigned char ch) {
    if (session_index >= 0 && session_index < MAX_SESSIONS) {
        KTerm_WriteCharToSessionInternal(term, &term->sessions[session_index], ch);
    }
}

// Helper to resize a specific session
// Phase 3: The caller MUST hold session->lock if calling this function?
// Actually, KTerm_Resize (public API) calls RecalculateLayout which calls this.
// KTerm_Resize does not take a lock.
// KTerm_RecalculateLayout calls this.
// So KTerm_ResizeSession should take the lock.
// BUT, if called from Update -> ... -> Resize (e.g. DECCOLM), Update already holds the lock?
// Let's check call paths.
// 1. KTerm_Resize (Public) -> KTerm_RecalculateLayout -> KTerm_ResizeSession. (No lock held).
// 2. KTerm_Update (Public) -> KTerm_ProcessEventsInternal -> (DECCOLM?) -> ???
// If KTerm_Update processes DECCOLM, does it call Resize?
// Currently DECCOLM toggles a flag. Does it trigger resize immediately?
// Search for DECCOLM logic.
// If DECCOLM only sets a flag and KTerm_Update handles it later, or if it modifies internal state directly?
// The prompt instructions for Phase 3 were: "Lock the Session Mutex during KTerm_Update (logic update) and KTerm_ResizeSession."
// This implies they are separate entry points.
// If KTerm_ResizeSession is only called from KTerm_Resize (Public API), then it SHOULD lock.
// If it is called internally from logic that already locks, we have a problem.
// Let's assume KTerm_ResizeSession is primarily for the public API or external layout events.
// The recursive mutex would solve this, but we are using standard mutexes.
// Let's implement locking in KTerm_ResizeSession as requested, but keep the unlock.

static void KTerm_ResizeSession_Internal(KTerm* term, KTermSession* session, int cols, int rows) {

    // Lock must be held by caller

    // Only resize if dimensions changed
    if (session->cols == cols && session->rows == rows) {
        return;
    }

    int old_cols = session->cols;
    int old_rows = session->rows;

    // Calculate new dimensions
    int new_buffer_height = rows + MAX_SCROLLBACK_LINES;

    // --- Screen Buffer Resize & Content Preservation (Viewport) ---
    EnhancedTermChar* new_screen_buffer = (EnhancedTermChar*)KTerm_Calloc(new_buffer_height * cols, sizeof(EnhancedTermChar));
    if (!new_screen_buffer) return;

    // Allocate new aux buffers before committing changes to avoid partial failure
    uint8_t* new_row_dirty = (uint8_t*)KTerm_Calloc(rows, sizeof(uint8_t));
    if (!new_row_dirty) {
        KTerm_Free(new_screen_buffer);
        return;
    }

    EnhancedTermChar* new_alt_buffer = (EnhancedTermChar*)KTerm_Calloc(rows * cols, sizeof(EnhancedTermChar));
    if (!new_alt_buffer) {
        KTerm_Free(new_screen_buffer);
        KTerm_Free(new_row_dirty);
        return;
    }

    // Default char for initialization
    EnhancedTermChar default_char = {
        .ch = ' ',
        .fg_color = {.color_mode = 0, .value.index = COLOR_WHITE},
        .bg_color = {.color_mode = 0, .value.index = COLOR_BLACK},
        .flags = KTERM_FLAG_DIRTY
    };

    // Initialize new buffer
    for (int k = 0; k < new_buffer_height * cols; k++) new_screen_buffer[k] = default_char;

    // Copy visible viewport AND history from old buffer
    int copy_rows = (old_rows < rows) ? old_rows : rows;
    int copy_cols = (old_cols < cols) ? old_cols : cols;

    // Optimize: Only copy populated history
    int start_y = -session->history_rows_populated;
    if (start_y < -MAX_SCROLLBACK_LINES) start_y = -MAX_SCROLLBACK_LINES;

    // Iterate from oldest history line to bottom of visible viewport
    for (int y = start_y; y < copy_rows; y++) {
        // Get pointer to source row in ring buffer (relative to active head)
        // GetActiveScreenRow handles wrapping and negative indices correctly
        EnhancedTermChar* src_row_ptr = GetActiveScreenRow(session, y);

        // Calculate Destination Row Index in new linear buffer (Head=0)
        // Viewport (y>=0) -> [0, copy_rows-1]
        // History (y<0)   -> [new_buffer_height + y]
        int dst_row_idx;
        if (y >= 0) dst_row_idx = y;
        else dst_row_idx = new_buffer_height + y;

        // Safety check
        if (dst_row_idx >= 0 && dst_row_idx < new_buffer_height) {
            EnhancedTermChar* dst_row_ptr = &new_screen_buffer[dst_row_idx * cols];

            // Copy row content (up to min width)
            for (int x = 0; x < copy_cols; x++) {
                dst_row_ptr[x] = src_row_ptr[x];
                dst_row_ptr[x].flags |= KTERM_FLAG_DIRTY; // Force redraw
            }
        }
    }

    // Commit changes
    if (session->screen_buffer) KTerm_Free(session->screen_buffer);
    session->screen_buffer = new_screen_buffer;

    if (session->row_dirty) KTerm_Free(session->row_dirty);
    session->row_dirty = new_row_dirty;
    for (int r = 0; r < rows; r++) session->row_dirty[r] = KTERM_DIRTY_FRAMES;

    if (session->alt_buffer) KTerm_Free(session->alt_buffer);
    session->alt_buffer = new_alt_buffer;
    for (int k = 0; k < rows * cols; k++) session->alt_buffer[k] = default_char;
    session->alt_screen_head = 0;

    session->cols = cols;
    session->rows = rows;
    session->buffer_height = new_buffer_height;

    // Reset ring buffer state
    session->screen_head = 0;
    session->view_offset = 0;
    session->saved_view_offset = 0;

    // Clamp cursor
    if (session->cursor.x >= cols) session->cursor.x = cols - 1;
    if (session->cursor.y >= rows) session->cursor.y = rows - 1;

    // Reset margins
    session->left_margin = 0;
    session->right_margin = cols - 1;
    session->scroll_top = 0;
    session->scroll_bottom = rows - 1;

    // --- Tab Stops Resize ---
    if (cols > session->tab_stops.capacity) {
        int old_capacity = session->tab_stops.capacity;
        int new_capacity = cols;
        bool* new_stops = (bool*)KTerm_Realloc(session->tab_stops.stops, new_capacity * sizeof(bool));
        if (new_stops) {
            session->tab_stops.stops = new_stops;
            session->tab_stops.capacity = new_capacity;

            // Initialize new space with default tab stops
            for (int i = old_capacity; i < new_capacity; i++) {
                if ((i % session->tab_stops.default_width) == 0 && i != 0) {
                     session->tab_stops.stops[i] = true;
                     session->tab_stops.count++;
                } else {
                     session->tab_stops.stops[i] = false;
                }
            }
        }
}
}
static void KTerm_ResizeSession(KTerm* term, int session_index, int cols, int rows) {
    if (session_index < 0 || session_index >= MAX_SESSIONS) return;
    KTermSession* session = &term->sessions[session_index];

    KTerm_QueueResize(session, cols, rows, true);

    if (term->session_resize_callback) term->session_resize_callback(term, session_index, cols, rows);
}

KTermPane* KTerm_SplitPane(KTerm* term, KTermPane* target_pane, KTermPaneType split_type, float ratio) {
    if (!term->layout) return NULL;

    // Find a free session for the new pane
    int new_session_idx = -1;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!term->sessions[i].session_open) {
             new_session_idx = i;
             break;
        }
    }

    if (new_session_idx == -1) return NULL;

    // Initialize the new session
    if (!KTerm_InitSession(term, new_session_idx)) {
        return NULL;
    }
    term->sessions[new_session_idx].session_open = true;

    return KTermLayout_Split(term->layout, target_pane, split_type, ratio, new_session_idx, KTerm_LayoutResizeCallback, term);
}

void KTerm_ClosePane(KTerm* term, KTermPane* pane) {
    if (!term->layout || !pane) return;

    int session_idx = pane->session_index;

    KTermLayout_Close(term->layout, pane, KTerm_LayoutResizeCallback, term);

    // Close session
    if (session_idx >= 0 && session_idx < MAX_SESSIONS) {
        term->sessions[session_idx].session_open = false;
    }

    // Update active session if needed
    if (term->layout->focused && term->layout->focused->session_index >= 0) {
        KTerm_SetActiveSession(term, term->layout->focused->session_index);
    }
}

// Resizes the terminal grid and window texture
// Note: This operation destroys and recreates GPU resources, so it might be slow.
void KTerm_Resize(KTerm* term, int cols, int rows) {
    if (cols < 1 || rows < 1) return;

    // 1. Synchronization: Lock global state to prevent race with KTerm_Update
    KTERM_MUTEX_LOCK(term->lock);

    // 2. Performance: Throttle resize events to avoid allocation thrashing
    // Limit to ~30ms (approx 30 FPS) to coalesce rapid OS window drag events
    double now = KTerm_TimerGetTime();
    if ((now - term->last_resize_time) < 0.033 &&
        (cols != term->width || rows != term->height)) {
        // Allow immediate execution if dimensions haven't changed (force redraw),
        // otherwise skip if too frequent.
        KTERM_MUTEX_UNLOCK(term->lock);
        return;
    }
    term->last_resize_time = now;

    bool global_dim_changed = (cols != term->width || rows != term->height);

    // 1. Update Global Dimensions
    term->width = cols;
    term->height = rows;

    // 2. Resize Layout Tree (Recalculate dimensions for all panes)
    if (term->layout) {
        KTermLayout_Resize(term->layout, cols, rows, KTerm_LayoutResizeCallback, term);
    } else {
        // Fallback for initialization or if tree is missing (should verify)
        // Resize all active sessions to full size (legacy behavior)
        for(int i=0; i<MAX_SESSIONS; i++) {
             KTerm_ResizeSession(term, i, cols, rows);
        }
    }

    // 3. Recreate GPU Resources
    if (term->compute_initialized && global_dim_changed) {
        KTERM_MUTEX_LOCK(term->compositor.render_lock);

        if (term->terminal_buffer.id != 0) KTerm_DestroyBuffer(&term->terminal_buffer);
        if (term->output_texture.generation != 0) KTerm_DestroyTexture(&term->output_texture);
        // Don't free gpu_staging_buffer here, we will realloc it below

        size_t buffer_size = cols * rows * sizeof(GPUCell);
        KTerm_CreateBuffer(buffer_size, NULL, KTERM_BUFFER_USAGE_STORAGE_BUFFER | KTERM_BUFFER_USAGE_TRANSFER_DST, &term->terminal_buffer);
        if (term->terminal_buffer.id == 0) {
             KTerm_ReportError(term, KTERM_LOG_ERROR, KTERM_SOURCE_RENDER, "Failed to create terminal GPU buffer in Resize");
        }

        int win_width = cols * term->char_width * DEFAULT_WINDOW_SCALE;
        int win_height = rows * term->char_height * DEFAULT_WINDOW_SCALE;
        KTermImage empty_img = {0};
        KTerm_CreateImage(win_width, win_height, 4, &empty_img);
        KTerm_CreateTextureEx(empty_img, false, KTERM_TEXTURE_USAGE_SAMPLED | KTERM_TEXTURE_USAGE_STORAGE | KTERM_TEXTURE_USAGE_TRANSFER_SRC, &term->output_texture);
        KTerm_UnloadImage(empty_img);

        // Optimization: Use realloc to reuse memory if possible, avoiding frequent free/alloc
        // size_t new_size = cols * rows * sizeof(GPUCell);
        // ... (legacy gpu_staging_buffer removed)

        KTermCompositor_Resize(&term->compositor, cols, rows);

        // Resize scratch buffer
        void* new_scratch = KTerm_Realloc(term->row_scratch_buffer, cols * sizeof(EnhancedTermChar));
        if (new_scratch) {
            term->row_scratch_buffer = (EnhancedTermChar*)new_scratch;
        } else {
            if (term->row_scratch_buffer) KTerm_Free(term->row_scratch_buffer);
            term->row_scratch_buffer = NULL;
        }

        if (term->vector_layer_texture.generation != 0) KTerm_DestroyTexture(&term->vector_layer_texture);
        // Note: Vector layer content is lost on resize. Preserving it would require a texture copy,
        // but the current Situation adapter does not expose a suitable compute layout or blit function for this.
        KTermImage vec_img = {0};
        KTerm_CreateImage(win_width, win_height, 4, &vec_img);
        memset(vec_img.data, 0, win_width * win_height * 4);
        KTerm_CreateTextureEx(vec_img, false, KTERM_TEXTURE_USAGE_SAMPLED | KTERM_TEXTURE_USAGE_STORAGE | KTERM_TEXTURE_USAGE_TRANSFER_DST, &term->vector_layer_texture);
        KTerm_UnloadImage(vec_img);

        KTERM_MUTEX_UNLOCK(term->compositor.render_lock);
    }

    // Update Split Row if needed
    if (term->split_screen_active) {
        if (term->split_row >= rows) term->split_row = rows / 2;
    } else {
        term->split_row = rows / 2;
    }

    KTERM_MUTEX_UNLOCK(term->lock);
}

KTermStatus KTerm_GetStatus(KTerm* term) {
    KTermSession* session = GET_SESSION(term);
    KTermStatus status = {0};
    int head = atomic_load_explicit(&GET_SESSION(term)->pipeline_head, memory_order_relaxed);
    int tail = atomic_load_explicit(&GET_SESSION(term)->pipeline_tail, memory_order_relaxed);
    status.pipeline_usage = (head - tail + sizeof(GET_SESSION(term)->input_pipeline)) % sizeof(GET_SESSION(term)->input_pipeline);

    int kb_head = atomic_load_explicit(&session->input.buffer_head, memory_order_relaxed);
    int kb_tail = atomic_load_explicit(&session->input.buffer_tail, memory_order_relaxed);
    status.key_usage = (kb_head - kb_tail + KEY_EVENT_BUFFER_SIZE) % KEY_EVENT_BUFFER_SIZE;

    status.overflow_detected = atomic_load_explicit(&GET_SESSION(term)->pipeline_overflow, memory_order_relaxed);
    status.avg_process_time = GET_SESSION(term)->VTperformance.avg_process_time;
    return status;
}

void KTerm_SetKeyboardMode(KTerm* term, const char* mode, bool enable) {
    KTermSession* session = GET_SESSION(term);
    if (strcmp(mode, "application_cursor") == 0) {
        if (enable) session->dec_modes |= KTERM_MODE_DECCKM; else session->dec_modes &= ~KTERM_MODE_DECCKM;
    } else if (strcmp(mode, "keypad_application") == 0) {
        session->input.keypad_application_mode = enable;
    } else if (strcmp(mode, "keypad_numeric") == 0) {
        session->input.keypad_application_mode = !enable;
    }
}

void KTerm_SetFocus(KTerm* term, bool focused) {
    KTermSession* session = GET_SESSION(term);

    // Only report if state actually changes
    if (session->mouse.focused == focused) return;
    session->mouse.focused = focused;

    if (session->mouse.focus_tracking) {
        const char* seq = focused ? "\x1B[I" : "\x1B[O";
        KTerm_QueueSessionResponse(term, session, seq);
    }
}

void KTerm_DefineFunctionKey(KTerm* term, int key_num, const char* sequence) {
    KTermSession* session = GET_SESSION(term);
    if (key_num >= 1 && key_num <= 24) {
        strncpy(session->input.function_keys[key_num - 1], sequence, 31);
        session->input.function_keys[key_num - 1][31] = '\0';
    }
}

static void KTerm_TranslateKey(KTermSession* session, KTermEvent* event) {
    if (!session || !event) return;

    // Kitty Keyboard Protocol Handling
    if (session->input.kitty_keyboard_flags != 0) {
        int mods = 1;
        if (event->shift) mods += 1;
        if (event->alt) mods += 2;
        if (event->ctrl) mods += 4;
        if (event->meta) mods += 8;

        int code = 0;
        bool is_func = false;

        // Map KTermKey to Kitty Code
        if (event->key_code >= KTERM_KEY_A && event->key_code <= KTERM_KEY_Z) {
            code = event->key_code + (event->shift ? 0 : 32); // 'A'=65, 'a'=97
        } else if (event->key_code >= KTERM_KEY_0 && event->key_code <= KTERM_KEY_9) {
            code = event->key_code;
        } else {
            switch(event->key_code) {
                case KTERM_KEY_ESCAPE: code = 27; break;
                case KTERM_KEY_ENTER: code = 13; break;
                case KTERM_KEY_TAB: code = 9; break;
                case KTERM_KEY_BACKSPACE: code = 127; break;
                case KTERM_KEY_INSERT: code = 57349; is_func = true; break;
                case KTERM_KEY_DELETE: code = 57350; is_func = true; break;
                case KTERM_KEY_LEFT: code = 57351; is_func = true; break;
                case KTERM_KEY_RIGHT: code = 57352; is_func = true; break;
                case KTERM_KEY_UP: code = 57353; is_func = true; break;
                case KTERM_KEY_DOWN: code = 57354; is_func = true; break;
                case KTERM_KEY_PAGE_UP: code = 57355; is_func = true; break;
                case KTERM_KEY_PAGE_DOWN: code = 57356; is_func = true; break;
                case KTERM_KEY_HOME: code = 57357; is_func = true; break;
                case KTERM_KEY_END: code = 57358; is_func = true; break;
                case KTERM_KEY_PRINT_SCREEN: code = 57367; is_func = true; break;
                case KTERM_KEY_PAUSE: code = 57368; is_func = true; break;
                case KTERM_KEY_MENU: code = 57369; is_func = true; break;
                default:
                    if (event->key_code >= KTERM_KEY_F1 && event->key_code <= KTERM_KEY_F12) {
                        code = 57370 + (event->key_code - KTERM_KEY_F1); is_func = true;
                    } else if (event->key_code >= KTERM_KEY_KP_0 && event->key_code <= KTERM_KEY_KP_EQUAL) {
                        code = 57418 + (event->key_code - KTERM_KEY_KP_0); is_func = true;
                    }
                    break;
            }
        }

        if (code != 0) {
            // Text or CSI u
            if (!is_func && mods == 1 && code >= 32 && code <= 126) {
                 event->sequence[0] = (char)code;
                 event->sequence[1] = '\0';
            } else {
                 snprintf(event->sequence, sizeof(event->sequence), "\x1B[%d;%du", code, mods);
            }
            return; // Done
        }
    }
}

void KTerm_QueueInputEvent(KTerm* term, KTermEvent event) {
    KTermSession* session = GET_SESSION(term);

    // Apply Key Translation (Kitty or Legacy)
    // If sequence is empty OR Kitty Mode is active (force override), translate
    if (event.sequence[0] == '\0' || session->input.kitty_keyboard_flags != 0) {
        KTerm_TranslateKey(session, &event);
    }

    // Apply 8-bit controls transform if enabled (S8C1T)
    if (session->input.use_8bit_controls && event.sequence[0] == '\x1B' && event.sequence[1] >= 0x40 && event.sequence[1] <= 0x5F && event.sequence[2] == '\0') {
        // Convert 7-bit ESC + Fe to 8-bit C1
        // ESC (0x1B) + char -> char + 0x40
        // e.g. ESC [ (CSI) -> 0x9B
        unsigned char c1 = (unsigned char)event.sequence[1] + 0x40;
        event.sequence[0] = c1;
        event.sequence[1] = '\0';
    }

    // Multiplexer Input Interceptor
    // 1. Activation: Check if prefix key (default Ctrl+B) is pressed and NOT already active
    if (!term->mux_input.active && event.ctrl && event.key_code == term->mux_input.prefix_key_code) {
        term->mux_input.active = true;
        return; // Consume the prefix key (don't send to host yet)
    }

    // 2. Processing: If active, interpret the NEXT key
    if (term->mux_input.active) {
        term->mux_input.active = false; // Reset state (one-shot)

        // Pass-through: Double prefix sends the prefix key itself
        if (event.ctrl && event.key_code == term->mux_input.prefix_key_code) {
            // Fall through to normal processing (send Ctrl+B to host)
        }
        else {
            // Intercept Action Keys
            KTermPane* current = (term->layout) ? term->layout->focused : NULL;
            if (!current && term->layout) current = term->layout->root;

            if (event.key_code == '"') {
                // Split Vertically (Top/Bottom)
                // Logic: Split current pane
                if (current->type == PANE_LEAF) {
                    KTermPane* new_pane = KTerm_SplitPane(term, current, PANE_SPLIT_VERTICAL, 0.5f);
                    if (new_pane) {
                        term->layout->focused = new_pane;
                        if (new_pane->session_index >= 0) KTerm_SetActiveSession(term, new_pane->session_index);
                    }
                }
            } else if (event.key_code == '%') {
                // Split Horizontally (Left/Right)
                 if (current->type == PANE_LEAF) {
                    KTermPane* new_pane = KTerm_SplitPane(term, current, PANE_SPLIT_HORIZONTAL, 0.5f);
                    if (new_pane) {
                        term->layout->focused = new_pane;
                        if (new_pane->session_index >= 0) KTerm_SetActiveSession(term, new_pane->session_index);
                    }
                }
            } else if (event.key_code == 'x') {
                KTerm_ClosePane(term, current);
            } else if (event.key_code == 'o' || event.key_code == 'n' ||
                       (event.sequence[0] == '\x1B' && (
                            event.sequence[1] == '[' || event.sequence[1] == 'O'
                       ) && (
                            event.sequence[2] == 'A' || event.sequence[2] == 'B' ||
                            event.sequence[2] == 'C' || event.sequence[2] == 'D'
                       ))) {
                // Cycle focus (Next) - Arrows also map to Next for basic cycling in Phase 5
                // DFS traversal to find next leaf
                KTermPane* stack[32];
                int top = 0;
                if (term->layout && term->layout->root) stack[top++] = term->layout->root;

                bool found_current = false;
                KTermPane* next_focus = NULL;
                KTermPane* first_leaf = NULL;

                while(top > 0) {
                    KTermPane* p = stack[--top];
                    if (p->type == PANE_LEAF) {
                        if (!first_leaf) first_leaf = p;
                        if (found_current) {
                            next_focus = p;
                            break;
                        }
                        if (p == current) found_current = true;
                    } else {
                        if (p->child_b) stack[top++] = p->child_b; // Push right first so we pop left
                        if (p->child_a) stack[top++] = p->child_a;
                    }
                }
                if (!next_focus) next_focus = first_leaf; // Wrap around

                if (next_focus) {
                    term->layout->focused = next_focus;
                    if (next_focus->session_index >= 0) KTerm_SetActiveSession(term, next_focus->session_index);
                }
            }
            return; // Consume command (action performed or invalid key in mux mode)
        }
    }

    // Route input to the focused pane's session if available
    if (term->layout && term->layout->focused && term->layout->focused->type == PANE_LEAF && term->layout->focused->session_index >= 0) {
        session = &term->sessions[term->layout->focused->session_index];
    } else {
        // Fallback to legacy active session
        session = GET_SESSION(term);
    }

    int next_head = (session->input.buffer_head + 1) % KEY_EVENT_BUFFER_SIZE;

    if (next_head != session->input.buffer_tail) {
        session->input.buffer[session->input.buffer_head] = event;
        atomic_store_explicit(&session->input.buffer_head, next_head, memory_order_release);
        session->input.total_events++;
    } else {
        session->input.dropped_events++;
    }
}

bool KTerm_GetKey(KTerm* term, KTermEvent* event) {
    if (!event) return false;
    // Just peek? Or remove? Traditionally GetKey removes.
    // But KTerm_Update is already consuming the buffer for processing sequences.
    // If the app calls GetKey, it probably wants to intercept events *before* they are processed?
    // Or does it want to see processed events?
    // If KTerm_Update consumes the buffer, then GetKey will likely return nothing unless called *before* Update.
    // Let's implement peek/copy behavior for inspection, or dequeue if intended for consuming.
    // Given the architecture "push to buffer -> buffer gets processed", GetKey might be for debugging or alternate handling.
    // Let's implement standard dequeue from the same buffer. If App consumes it, KTerm_Update won't see it.
    KTermSession* session = GET_SESSION(term);

    int current_tail = atomic_load_explicit(&session->input.buffer_tail, memory_order_relaxed);
    int current_head = atomic_load_explicit(&session->input.buffer_head, memory_order_acquire);

    if (current_tail == current_head) return false; // Empty

    *event = session->input.buffer[current_tail];

    current_tail = (current_tail + 1) % KEY_EVENT_BUFFER_SIZE;
    atomic_store_explicit(&session->input.buffer_tail, current_tail, memory_order_release);

    return true;
}

#ifdef KTERM_ENABLE_GATEWAY
#define KTERM_GATEWAY_IMPLEMENTATION
#include "kt_gateway.h"
#undef KTERM_GATEWAY_IMPLEMENTATION
#endif

#define KTERM_COMPOSITE_IMPLEMENTATION
#include "kt_composite_sit.h"
#undef KTERM_COMPOSITE_IMPLEMENTATION

#endif // KTERM_IMPLEMENTATION
#endif // KTERM_H
