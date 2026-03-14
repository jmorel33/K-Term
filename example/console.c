/**********************************************************************************************
 *
 * @file console.h
 *   (c) 2025 Jacques Morel
 * @brief Stub Command-Line Interface (CLI) for KaOS Terminal
 * @version 0.1
 *
 * @section Overview:
 *   console.h is a single-header stub for a command-line interface built on top of situation.h and kterm.h.
 *   It provides basic command processing, history navigation, tab completion, and integration with system queries via situation.h.
 *   This is currently a minimal implementation and will evolve into a more robust CLI with advanced features like scripting, plugins, and full system control.
 *
 * @section Key Features:
 *   - **Command Processing:** Tokenizes and executes commands like 'help', 'clear', 'echo', system info queries.
 *   - **Input Editing:** Supports line editing with backspace, arrows, history (up/down), tab completion.
 *   - **History Management:** Stores up to 32 recent commands for navigation.
 *   - **Tab Completion:** Context-aware completion for commands and arguments.
 *   - **Password Mode:** Masks input for sensitive commands.
 *   - **Integration with situation.h:** Commands for hardware info (CPU/GPU/RAM), displays, audio devices, user directory.
 *   - **KTerm Diagnostics:** Commands to query terminal status, VT level, device attributes, run tests.
 *   - **Performance Tools:** Set FPS/budget, run output tests.
 *
 * @section Design Principles:
 *   - **Stub Status:** This is a placeholder for a full-featured CLI. Future enhancements include proper error handling, modular commands, scripting support.
 *   - **Single-Header:** Define CONSOLE_IMPLEMENTATION in one .c file for full inclusion.
 *   - **Dependency on situation.h and kterm.h:** Leverages situation.h for platform abstraction and kterm.h for VT emulation and pipeline.
 *   - **Windows-Focused:** Primarily tested on Windows; POSIX fallbacks limited.
 *
 * @section Concurrency Model
 *   - **Single-Threaded:** This library is NOT THREAD-SAFE. All functions must be called from the main thread.
 *
 * @section Usage Models:
 *   A) Header-Only: #define CONSOLE_IMPLEMENTATION in one .c/.cpp file before including console.h.
 *
 * @section Dependencies:
 *   - **Required:** situation.h (with SITUATION_IMPLEMENTATION), kterm.h (with KTERM_IMPLEMENTATION), miniaudio.h (with MA_IMPLEMENTATION)
 *   - **Standard Libs:** <string.h>, <stdlib.h>, <stdio.h>, <math.h>, <stdint.h>
 *   - **Windows APIs:** Winsock2.h (for network info via situation.h)
**********************************************************************************************/
#if defined(_WIN32)
  #define NOGDI             // Keep this to protect Raylib's Rectangle struct
  #define NOMINMAX
#endif

#if defined(_WIN32)
    #include <winsock2.h> // Include before windows.h
#endif

#define MA_IMPLEMENTATION
#include "miniaudio.h" // Path to miniaudio.h

// --- situation.h Configuration ---
#if defined(_WIN32) && !defined(_MSC_VER) && defined(SITUATION_ENABLE_DXGI)
    #define INITGUID
#endif

#define SITUATION_IMPLEMENTATION
#include "situation.h"


#define KTERM_IMPLEMENTATION
// kterm.h should be included when NOUSER is effectively *defined* if it calls
// Raylib functions like CloseWindow/ShowCursor that would clash.
// This is the trickiest part. If NOUSER is undefined here, kterm.h might get Windows API versions.
// If NOUSER is defined here, situation.h might have issues if it's not robust.

// For kterm.h, let's assume it primarily uses Raylib APIs.
// So, we need NOUSER to be active for kterm.h
#if defined(_WIN32)
    #define NOUSER_FOR_TERMINAL
    #pragma push_macro("NOUSER")
    #define NOUSER
#endif

#include "kterm.h"

#if defined(_WIN32) && defined(NOUSER_FOR_TERMINAL)
    #pragma pop_macro("NOUSER")
    #undef NOUSER_FOR_TERMINAL
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define MAX_COMMAND_BUFFER 1024
#define MAX_TOKENS 64
#define MAX_HISTORY 32

typedef struct {
    bool waiting_for_position;
    bool position_received;
    int row;
    int col;
} CursorPositionTracker;

// Console state structure
typedef struct {
    char edit_buffer[MAX_COMMAND_BUFFER];
    int edit_pos;
    int edit_length;
    int edit_display_start;
    char command_buffer[MAX_COMMAND_BUFFER];
    bool line_ready;
    bool in_command;
    char command_history[MAX_HISTORY][MAX_COMMAND_BUFFER];
    int history_count;
    int history_pos;
    bool prompt_pending;
    bool waiting_for_pipeline; // Unused in provided code snippet
    int prompt_start_x;
    int prompt_line_y;
    bool echo_enabled;
    bool input_enabled;
    bool password_mode;
    bool waiting_for_prompt_cursor_pos;
} Console;

// Global exit flag
static bool should_exit = false;

// Forward declarations
static void ClearEditBuffer(void);
static void RedrawEditLine(void);
static void HandlePrintableKey(int key_code);
static void HandleBackspaceKey(void);
static void AddToHistory(const char* command);
static void NavigateHistory(int direction);
static int TokenizeCommand(const char* command, char* tokens[], char** buffer_to_free);
static void CompleteWord(const char* completion, int word_start);
static void ShowCompletionMatches(const char* matches[], int count, const char* partial);
static void CompleteCommonPrefix(const char* matches[], int count, const char* partial, int word_start);
static bool CompleteCommand(const char* partial, int word_start);
static bool AttemptTabCompletion(void);
static void HandleTabKey(void);
static void ShowPrompt(void);
static void ProcessCommand(const char* command);
static void HandleExtendedKeyInput(const char* sequence);
static void HandleEnterKey(void);
static void HandleKeyEvent(const char* sequence, int length);
static void HandleKTermResponse(void* ctx, const char* response, int length);
static void ProcessConsolePipeline(void);


// ***** NEW: Forward declarations for situation.h helper print functions *****
void SitHelperPrintDeviceInfo(const SituationDeviceInfo* info);
void SitHelperPrintDisplayInfo(SituationDisplayInfo* displays, int count);
void SitHelperPrintAudioDeviceInfo(SituationAudioDeviceInfo* devices, int count);
// *************************************************************************

// Global console instance
Console console = {0};
static KTerm* term = NULL;
static CursorPositionTracker cursor_tracker = {0};

static bool ParseCSIResponse(const char* response, int length) {
    // Check for cursor position report: ESC[n;mR
    if (length > 3 && response[0] == '\x1B' && response[1] == '[') {
        int i = 2;
        int row = 0, col = 0;
        
        // Parse row
        while (i < length && response[i] >= '0' && response[i] <= '9') {
            row = row * 10 + (response[i] - '0');
            i++;
        }
        
        if (i < length && response[i] == ';') {
            i++;
            // Parse column
            while (i < length && response[i] >= '0' && response[i] <= '9') {
                col = col * 10 + (response[i] - '0');
                i++;
            }
            
            if (i < length && response[i] == 'R') {
                // Valid cursor position response
                if (cursor_tracker.waiting_for_position) {
                    cursor_tracker.row = row;
                    cursor_tracker.col = col;
                    cursor_tracker.position_received = true;
                    cursor_tracker.waiting_for_position = false;
                    return true;
                }
            }
        }
    }
    return false;
}


// Command line editing functions
static void ClearEditBuffer(void) {
    memset(console.edit_buffer, 0, sizeof(console.edit_buffer));
    console.edit_pos = 0;
    console.edit_length = 0;
}

static void RedrawEditLine(void) {
    if (!console.echo_enabled) return;
    if (console.waiting_for_prompt_cursor_pos || console.prompt_line_y == 0 || console.prompt_start_x == 0) {
        return;
    }

    char move_cmd[32];
    snprintf(move_cmd, sizeof(move_cmd), "\x1B[%d;%dH", console.prompt_line_y, console.prompt_start_x);
    KTerm_WriteString(term, move_cmd);
    KTerm_WriteString(term, "\x1B[K");
    
    if (console.password_mode) {
        for (int i = 0; i < console.edit_length; i++) KTerm_WriteChar(term, '*');
    } else {
        for (int i = 0; i < console.edit_length; i++) KTerm_WriteChar(term, console.edit_buffer[i]);
    }
    
    snprintf(move_cmd, sizeof(move_cmd), "\x1B[%d;%dH", console.prompt_line_y, console.prompt_start_x + console.edit_pos);
    KTerm_WriteString(term, move_cmd);
}

static void HandlePrintableKey(int key_code) {
    if (!console.input_enabled) return;

    if (console.edit_length < MAX_COMMAND_BUFFER - 1) {
        // Insert character at current position in the CLI's buffer
        if (console.edit_pos < console.edit_length) {
            memmove(&console.edit_buffer[console.edit_pos + 1], &console.edit_buffer[console.edit_pos], console.edit_length - console.edit_pos);
        }
        console.edit_buffer[console.edit_pos] = (char)key_code;
        console.edit_length++;
        console.edit_pos++;
        console.edit_buffer[console.edit_length] = '\0';

        // The terminal library handles the actual visual echo if its local echo is on.
        // We just need to ensure our complete line is correctly displayed, especially
        // if the insertion happened in the middle of the line.
        RedrawEditLine();
    }
}

static void HandleBackspaceKey(void) {
    if (!console.input_enabled) return;

    if (console.edit_pos > 0) {
        console.edit_pos--;
        // Shift remaining characters left in the CLI's buffer
        memmove(&console.edit_buffer[console.edit_pos], &console.edit_buffer[console.edit_pos + 1], console.edit_length - console.edit_pos); // No +1 needed here if length is already reduced
        console.edit_length--;
        // console.edit_buffer[console.edit_length] = '\0'; // Ensure null termination

        // Redraw the entire line.
        // The terminal library (if local echo is on) might have just moved the cursor back.
        // RedrawEditLine ensures the character is visually erased and the line is correct.
        // The sequence "\b \b" is a common way for a HOST to tell a simple terminal
        // to backspace, erase, and backspace again if the terminal doesn't do it automatically
        // on just a BS character. Here, RedrawEditLine is more comprehensive.
        RedrawEditLine();
    }
}

// Command history management
static void AddToHistory(const char* command) {
    if (strlen(command) == 0) return;
    if (console.history_count > 0 && strcmp(command, console.command_history[console.history_count - 1]) == 0) return;
    
    if (console.history_count >= MAX_HISTORY) {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strncpy(console.command_history[i], console.command_history[i + 1], MAX_COMMAND_BUFFER - 1);
            console.command_history[i][MAX_COMMAND_BUFFER - 1] = '\0';
        }
        console.history_count = MAX_HISTORY - 1;
    }
    
    strncpy(console.command_history[console.history_count], command, MAX_COMMAND_BUFFER - 1);
    console.command_history[console.history_count][MAX_COMMAND_BUFFER - 1] = '\0';
    console.history_count++;
    console.history_pos = console.history_count;
}

static void NavigateHistory(int direction) {
    if (console.history_count == 0) return;
    
    if (direction < 0) { 
        if (console.history_pos > 0) {
            console.history_pos--;
        }
    } else { 
        if (console.history_pos < console.history_count - 1) {
            console.history_pos++;
        } else {
            ClearEditBuffer();
            RedrawEditLine();
            return;
        }
    }
    
    strncpy(console.edit_buffer, console.command_history[console.history_pos], MAX_COMMAND_BUFFER - 1);
    console.edit_buffer[MAX_COMMAND_BUFFER - 1] = '\0';
    console.edit_length = strlen(console.edit_buffer);
    console.edit_pos = console.edit_length;
    RedrawEditLine();
}

// Tokenization function
static int TokenizeCommand(const char* command, char* tokens[], char** buffer_to_free) {
    *buffer_to_free = NULL;
    if (command == NULL || strlen(command) == 0) return 0;

    // strtok_r modifies the string, so we need a writable copy.
    char* writable_command = strdup(command);
    if (writable_command == NULL) {
        fprintf(stderr, "Error: Memory allocation failed in TokenizeCommand for: %s\n", command);
        // KTerm_WriteString(term, "\n\x1B[31mError: Out of memory during tokenization.\x1B[0m\n"); // Optional: inform user
        return 0;
    }
    *buffer_to_free = writable_command; // Important: So ProcessCommand can free it
    int token_count = 0;
    char* context; // For strtok_r state

    // Delimiters: space and tab. Add others if needed (e.g., \n, \r if they can be in commands).
    const char* delimiters = " \t";
    char* token = strtok_r(writable_command, delimiters, &context);
    while (token != NULL && token_count < MAX_TOKENS) {
        tokens[token_count++] = token; // Store pointer to the token within writable_command
        token = strtok_r(NULL, delimiters, &context); // Get next token
    }
    return token_count;
}

// Tab completion system
static bool CompleteCommand(const char* partial, int word_start) {
    static const char* commands[] = {
        "clear", "cls", "echo", "test", "help", "graphics", "blink", "echo_on", "noecho", "password", "normal", "history", "exit", "quit",
        "pipeline_stats", "set_fps", "set_budget", "color_test", "cursor_test", "scroll_test", "performance", "demo", "rainbow",
        "term_status", "term_vtlevel", "term_da", "term_runtest", "term_showinfo", "term_diagbuffers", "sys_info", "sys_displays", "sys_audio", "sys_userdir"
    };
    static const int num_commands = sizeof(commands) / sizeof(commands[0]);

    if (word_start == 0) {
        const char* matches[32];
        int match_count = 0;
        int partial_len = strlen(partial);

        for (int i = 0; i < num_commands && match_count < 32; i++) {
            if (strncmp(commands[i], partial, partial_len) == 0) {
                matches[match_count++] = commands[i];
            }
        }

        if (match_count == 0) {
            return false;
        } else if (match_count == 1) {
            CompleteWord(matches[0], word_start);
            return true;
        } else {
            ShowCompletionMatches(matches, match_count, partial);
            CompleteCommonPrefix(matches, match_count, partial, word_start);
            return true;
        }
    } else {
        char first_word[MAX_COMMAND_BUFFER];
        int first_word_len = 0;
        while (first_word_len < console.edit_length && console.edit_buffer[first_word_len] != ' ') {
            first_word_len++;
        }
        
        if (first_word_len > 0 && first_word_len < MAX_COMMAND_BUFFER) {
            strncpy(first_word, console.edit_buffer, first_word_len);
            first_word[first_word_len] = '\0';

            if (strcmp(first_word, "set_fps") == 0) {
                static const char* fps_values[] = {"30", "60", "120"};
                const int num_fps = sizeof(fps_values) / sizeof(fps_values[0]);
                const char* matches[32];
                int match_count = 0;
                int partial_len = strlen(partial);

                for (int j = 0; j < num_fps && match_count < 32; j++) {
                    if (strncmp(fps_values[j], partial, partial_len) == 0) {
                        matches[match_count++] = fps_values[j];
                    }
                }

                if (match_count == 0) return false;
                if (match_count == 1) {
                    CompleteWord(matches[0], word_start);
                    return true;
                } else {
                    ShowCompletionMatches(matches, match_count, partial);
                    CompleteCommonPrefix(matches, match_count, partial, word_start);
                    return true;
                }
            } else if (strcmp(first_word, "set_budget") == 0) {
                static const char* budget_values[] = {"0.1", "0.5", "1.0"};
                const int num_budgets = sizeof(budget_values) / sizeof(budget_values[0]);
                const char* matches[32];
                int match_count = 0;
                int partial_len = strlen(partial);

                for (int j = 0; j < num_budgets && match_count < 32; j++) {
                    if (strncmp(budget_values[j], partial, partial_len) == 0) {
                        matches[match_count++] = budget_values[j];
                    }
                }

                if (match_count == 0) return false;
                if (match_count == 1) {
                    CompleteWord(matches[0], word_start);
                    return true;
                } else {
                    ShowCompletionMatches(matches, match_count, partial);
                    CompleteCommonPrefix(matches, match_count, partial, word_start);
                    return true;
                }
            }
        }
        return false;
    }
    return false; // Fallback
}

static void CompleteWord(const char* completion, int word_start) {
    int current_word_len = console.edit_pos - word_start;
    int completion_len = strlen(completion);
    int chars_to_add = completion_len - current_word_len;
    
    if (chars_to_add <= 0) return;
    if (console.edit_length + chars_to_add >= MAX_COMMAND_BUFFER - 1) return;
    
    if (console.edit_pos < console.edit_length) {
        memmove(&console.edit_buffer[console.edit_pos + chars_to_add], &console.edit_buffer[console.edit_pos], console.edit_length - console.edit_pos);
    }
    strncpy(&console.edit_buffer[console.edit_pos], completion + current_word_len, chars_to_add);
    console.edit_pos += chars_to_add;
    console.edit_length += chars_to_add;
    console.edit_buffer[console.edit_length] = '\0';
    
    if (word_start == 0 && console.edit_pos == console.edit_length && console.edit_length < MAX_COMMAND_BUFFER - 2) {
        console.edit_buffer[console.edit_length++] = ' ';
        console.edit_buffer[console.edit_length] = '\0';
        console.edit_pos++;
    }
    
    RedrawEditLine();
}

static void ShowCompletionMatches(const char* matches[], int count, const char* partial) {
    KTerm_WriteChar(term, '\n');
    int max_width = 0;
    for (int i = 0; i < count; i++) { int len = strlen(matches[i]); if (len > max_width) max_width = len; }
    max_width += 2;
    int cols = DEFAULT_TERM_WIDTH / max_width; if (cols == 0) cols = 1;
    for (int i = 0; i < count; i++) {
        KTerm_WriteString(term, matches[i]);
        if ((i + 1) % cols == 0 || i == count - 1) KTerm_WriteChar(term, '\n');
        else { int len = strlen(matches[i]); for (int j = len; j < max_width; j++) KTerm_WriteChar(term, ' '); }
    }
    ShowPrompt();
    if (console.edit_length > 0) RedrawEditLine();
}

static void CompleteCommonPrefix(const char* matches[], int count, const char* partial, int word_start) {
    if (count < 1) return;
    int common_len = strlen(matches[0]);
    for (int i = 1; i < count; i++) {
        int j = 0;
        while (j < common_len && j < strlen(matches[i]) && matches[0][j] == matches[i][j]) j++;
        common_len = j;
    }
    int current_len = strlen(partial);
    if (common_len > current_len) {
        char common_prefix[MAX_COMMAND_BUFFER];
        strncpy(common_prefix, matches[0], common_len);
        common_prefix[common_len] = '\0';
        CompleteWord(common_prefix, word_start);
    }
}

static bool AttemptTabCompletion(void) {
    int word_start = console.edit_pos;
    while (word_start > 0 && console.edit_buffer[word_start - 1] != ' ') word_start--;
    int word_len = console.edit_pos - word_start;
    char partial_word[MAX_COMMAND_BUFFER];
    if (word_len > 0) strncpy(partial_word, console.edit_buffer + word_start, word_len);
    partial_word[word_len] = '\0';
    return CompleteCommand(partial_word, word_start);
}

static void HandleTabKey(void) {
    if (GET_SESSION(term)->raw_mode) { HandlePrintableKey('\t'); return; }
    if (AttemptTabCompletion()) return;
    // Your original tab-to-spaces logic
    if (console.edit_length > 0) {
        int next_tab_pos = ((console.edit_pos / 4) + 1) * 4; // Assuming tab stop = 4
        int spaces_to_add = next_tab_pos - console.edit_pos;
        if (spaces_to_add == 0) spaces_to_add = 4;
        for (int i = 0; i < spaces_to_add && console.edit_length < MAX_COMMAND_BUFFER - 1; i++) {
            HandlePrintableKey(' ');
        }
    }
}

static void ShowPrompt(void) {
    KTerm_WriteString(term, "\x1B[32mKaOS>\x1B[0m ");
    console.waiting_for_prompt_cursor_pos = true;
    console.input_enabled = false;
    console.line_ready = false;
    cursor_tracker.waiting_for_position = true;
    cursor_tracker.position_received = false;
    fprintf(stderr, "CLI ShowPrompt: Sent DSR. waiting_for_prompt_cursor_pos=true, input_enabled=false.\n");
    KTerm_WriteString(term, "\x1B[6n"); // DSR Request Cursor Position
    console.prompt_pending = false;
}

static void ProcessCommand(const char* command) {
    char* tokens[MAX_TOKENS];
    char* buffer_to_free = NULL;
    int token_count = TokenizeCommand(command, tokens, &buffer_to_free);

    fprintf(stderr, "ProcessCommand - Input to Tokenize: '%s', Token count: %d\n", command, token_count);
    if (token_count == 0) {
        console.prompt_pending = true;
        if (buffer_to_free) free(buffer_to_free);
        return;
    }
    const char* cmd = tokens[0];

    if (strcmp(cmd, "cls") == 0 || strcmp(cmd, "clear") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'clear' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "\x1B[2J\x1B[H");
        }
    } else if (strcmp(cmd, "echo") == 0) {
        if (token_count == 1) {
            KTerm_WriteString(term, "\n");
        } else {
            for (int i = 1; i < token_count; i++) {
                KTerm_WriteString(term, tokens[i]);
                if (i < token_count - 1) KTerm_WriteChar(term, ' ');
            }
            KTerm_WriteString(term, "\n");
        }
    } else if (strcmp(cmd, "noecho") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'noecho' takes no arguments\x1B[0m\n");
        } else {
            console.echo_enabled = false;  // Update console's state
            KTerm_WriteString(term, "\x1B[?12l");  // Send DEC private mode reset for local echo
            KTerm_WriteString(term, "Echo disabled\n");
        }
    } else if (strcmp(cmd, "echo_on") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'echo_on' takes no arguments\x1B[0m\n");
        } else {
            console.echo_enabled = true;  // Update console's state
            KTerm_WriteString(term, "\x1B[?12h");  // Send DEC private mode set for local echo
            KTerm_WriteString(term, "Echo enabled\n");
        }
    } else if (strcmp(cmd, "password") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'password' takes no arguments\x1B[0m\n");
        } else {
            console.password_mode = true;  // Use console state
            KTerm_WriteString(term, "Password mode enabled (input will show as *)\n");
        }
    } else if (strcmp(cmd, "normal") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'normal' takes no arguments\x1B[0m\n");
        } else {
            console.password_mode = false;  // Use console state
            KTerm_WriteString(term, "Normal input mode\n");
        }
    } else if (strcmp(cmd, "test") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'test' takes no arguments\x1B[0m\n");
        } else {
            const char* test_seq = "\x1B[31mRed \x1B[32mGreen \x1B[33mYellow \x1B[34mBlue \x1B[35mMagenta \x1B[36mCyan \x1B[37mWhite\x1B[0m\n";
            KTerm_WriteString(term, test_seq);
        }
    } else if (strcmp(cmd, "color_test") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'color_test' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "Standard Colors:\n");
            for (int i = 0; i < 8; i++) KTerm_WriteFormat(term, "\x1B[%dm███ ", 30 + i);
            KTerm_WriteString(term, "\x1B[0m\nBright Colors:\n");
            for (int i = 0; i < 8; i++) KTerm_WriteFormat(term, "\x1B[%dm███ ", 90 + i);
            KTerm_WriteString(term, "\x1B[0m\n\n256-color palette (first 32):\n");
            for (int i = 0; i < 32; i++) {
                KTerm_WriteFormat(term, "\x1B[38;5;%dm█", i);
                if ((i + 1) % 16 == 0) KTerm_WriteString(term, "\n");
            }
            KTerm_WriteString(term, "\x1B[0m\n");
        }
    } else if (strcmp(cmd, "rainbow") == 0) {
        if (token_count == 1) {
            const char* text = "Rainbow colors using true color support!";
            int len = strlen(text);
            for (int i = 0; i < len; i++) {
                int r = (int)(127 * (1 + sin(i * 0.3)));
                int g = (int)(127 * (1 + sin(i * 0.3 + 2)));
                int b = (int)(127 * (1 + sin(i * 0.3 + 4)));
                KTerm_WriteFormat(term, "\x1B[38;2;%d;%d;%dm%c", r, g, b, text[i]);
            }
            KTerm_WriteString(term, "\x1B[0m\n");
        } else {
            int char_idx_overall = 0;
            for (int i = 1; i < token_count; i++) {
                const char* text_segment = tokens[i];
                int len = strlen(text_segment);
                for (int j = 0; j < len; j++) {
                    int r = (int)(127 * (1 + sin(char_idx_overall * 0.3)));
                    int g = (int)(127 * (1 + sin(char_idx_overall * 0.3 + 2.094395)));
                    int b = (int)(127 * (1 + sin(char_idx_overall * 0.3 + 4.188790)));
                    KTerm_WriteFormat(term, "\x1B[38;2;%d;%d;%dm%c", r, g, b, text_segment[j]);
                    char_idx_overall++;
                }
                if (i < token_count - 1) {
                    KTerm_WriteChar(term, ' ');
                    char_idx_overall++;
                }
            }
            KTerm_WriteString(term, "\x1B[0m\n");
        }
    } else if (strcmp(cmd, "cursor_test") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'cursor_test' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Cursor movement test:\nMoving cursor around...\n");
            KTerm_WriteString(term, "\x1B[10;10H*\x1B[12;15H*\x1B[8;20H*\x1B[15;5H*\x1B[H");
        }
    } else if (strcmp(cmd, "scroll_test") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'scroll_test' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Scroll test - generating many lines:\n");
            for (int i = 1; i <= 60; i++) KTerm_WriteFormat(term, "Line %d - This is a scrolling test\n", i);
        }
    } else if (strcmp(cmd, "performance") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'performance' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Performance test - sending large amount of data:\n");
            fprintf(stderr, "CLI ProcessCommand: Starting 'performance' test output loop.\n");
            for (int i = 0; i < 1000; i++) KTerm_WriteFormat(term, "Performance test line %d with some text content\n", i);
            fprintf(stderr, "CLI ProcessCommand: Finished 'performance' test output loop.\n");
        }
    } else if (strcmp(cmd, "demo") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'demo' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "\x1B[2J\x1B[H");
            KTerm_WriteString(term, "\x1B[1;36m╔══════════════════════════════════════╗\x1B[0m\n");
            KTerm_WriteString(term, "\x1B[1;36m║\x1B[1;33m          KaOS KTerm Demo          \x1B[1;36m║\x1B[0m\n");
            KTerm_WriteString(term, "\x1B[1;36m╚══════════════════════════════════════╝\x1B[0m\n\n");
            KTerm_WriteString(term, "\x1B[1;32mFeatures demonstrated:\x1B[0m\n");
            KTerm_WriteString(term, "• \x1B[33mFull ANSI color support\x1B[0m\n");
            KTerm_WriteString(term, "• \x1B[1mBold\x1B[0m, \x1B[4munderline\x1B[0m, \x1B[7minverse\x1B[0m text\n");
            KTerm_WriteString(term, "• \x1B[5mBlinking text\x1B[0m (if supported)\n");
            KTerm_WriteString(term, "• Box drawing: ┌─┬─┐ │ ├─┼─┤ │ └─┴─┘\n");
            KTerm_WriteString(term, "• Command history (↑/↓ arrows)\n");
            KTerm_WriteString(term, "• Tab completion\n");
            KTerm_WriteString(term, "• High-performance pipeline processing\n\n");
        }
    } else if (strcmp(cmd, "graphics") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'graphics' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Box drawing characters:\n");
            KTerm_WriteString(term, "┌─┬─┬─┐  ╔═╦═╦═╗  ╭─┬─┬─╮\n");
            KTerm_WriteString(term, "├─┼─┼─┤  ╠═╬═╬═╣  ├─┼─┼─┤\n");
            KTerm_WriteString(term, "└─┴─┴─┘  ╚═╩═╩═╝  ╰─┴─┴─╯\n");
            KTerm_WriteString(term, "Shades: ░░░ ▒▒▒ ▓▓▓ ███\n");
            KTerm_WriteString(term, "Blocks: ▀▀▀ ▄▄▄ █▌▐ ◄►▲▼\n");
        }
    } else if (strcmp(cmd, "blink") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'blink' takes no arguments\x1B[0m\n");
        else KTerm_WriteString(term, "This text should \x1B[5mblink\x1B[0m if blinking is supported.\n");
    } else if (strcmp(cmd, "history") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'history' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Command history:\n");
            for (int i = 0; i < console.history_count; i++) KTerm_WriteFormat(term, "%2d: %s\n", i + 1, console.command_history[i]);
        }
    } else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'exit/quit' takes no arguments\x1B[0m\n");
        else {
            KTerm_WriteString(term, "Goodbye!\n");
            should_exit = true;
        }
    } else if (strcmp(cmd, "pipeline_stats") == 0) {
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'pipeline_stats' takes no arguments\x1B[0m\n");
        else KTerm_ShowDiagnostics(term);
    } else if (strcmp(cmd, "set_fps") == 0) {
        if (token_count != 2) KTerm_WriteString(term, "\x1B[31mError: 'set_fps' requires one argument (FPS value)\x1B[0m\n");
        else {
            int fps = atoi(tokens[1]);
            if (fps > 0 && fps <= 120) {
                KTerm_SetPipelineTargetFPS(term, fps);
                KTerm_WriteFormat(term, "Target FPS set to %d\n", fps);
            } else KTerm_WriteString(term, "Invalid FPS value (1-120)\n");
        }
    } else if (strcmp(cmd, "set_budget") == 0) {
        if (token_count != 2) KTerm_WriteString(term, "\x1B[31mError: 'set_budget' requires one argument (percentage 0.0-1.0)\x1B[0m\n");
        else {
            double pct = atof(tokens[1]);
            if (pct > 0.0 && pct <= 1.0) {
                KTerm_SetPipelineTimeBudget(term, pct);
                KTerm_WriteFormat(term, "Pipeline time budget set to %.1f%%\n", pct * 100.0);
            } else KTerm_WriteString(term, "Invalid budget percentage (0.01-1.0)\n");
        }
    } else if (strcmp(cmd, "term_status") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_status' takes no arguments\x1B[0m\n");
        } else {
            // Get the status from the terminal library
            KTermStatus status = KTerm_GetStatus(term); // API call to kterm.h

            // CLI formats and displays the returned data
            KTerm_WriteString(term, "\n--- KTerm Library Status ---\n");
            KTerm_WriteFormat(term, "Input Pipeline Usage: %zu bytes\n", status.pipeline_usage);
            KTerm_WriteFormat(term, "Keyboard Event Usage: %zu events\n", status.key_usage); // Assuming this is from GET_SESSION(term)->vt_keyboard.buffer_count
            KTerm_WriteFormat(term, "Input Pipeline Overflowed: %s\n", status.overflow_detected ? "YES" : "NO");
            KTerm_WriteFormat(term, "Avg Char Process Time: %.6f ms\n", status.avg_process_time * 1000.0);
            KTerm_WriteString(term, "-----------------------------\n");
        }
    } else if (strcmp(cmd, "term_vtlevel") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_vtlevel' takes no arguments\x1B[0m\n");
        } else {
            // Get the level from the terminal library
            VTLevel level = KTerm_GetLevel(term); // API call to kterm.h

            // CLI formats and displays the returned data
            KTerm_WriteFormat(term, "\nCurrent KTerm VT Level: %d (", level);
            switch (level) {
                case VT_LEVEL_52:   KTerm_WriteString(term, "VT52"); break;
                case VT_LEVEL_100:  KTerm_WriteString(term, "VT100"); break;
                case VT_LEVEL_220:  KTerm_WriteString(term, "VT220"); break;
                case VT_LEVEL_320:  KTerm_WriteString(term, "VT320"); break;
                case VT_LEVEL_420:  KTerm_WriteString(term, "VT420"); break;
                case VT_LEVEL_XTERM: KTerm_WriteString(term, "XTERM"); break;
                default: KTerm_WriteString(term, "Unknown"); break;
            }
            KTerm_WriteString(term, ")\n");
        }
    } else if (strcmp(cmd, "term_da") == 0) { // This one remains largely the same
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_da' takes no arguments\x1B[0m\n");
        } else {
            KTerm_WriteString(term, "\nRequesting Primary DA (ESC[c)...\n");
            KTerm_WriteString(term, "\x1B[c"); // Send to terminal's input pipeline
            KTerm_WriteString(term, "Requesting Secondary DA (ESC[>c)...\n");
            KTerm_WriteString(term, "\x1B[>c"); // Send to terminal's input pipeline
            // The terminal library will respond via HandleKTermResponse
        }
    } else if (strcmp(cmd, "term_runtest") == 0) {
        if (token_count != 2) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_runtest' requires one argument (e.g., cursor, colors, all)\x1B[0m\n");
        } else {
            KTerm_WriteFormat(term, "\nRequesting terminal to run test: %s\n", tokens[1]);
            // Call the terminal library's function.
            // This function (in kterm.h) MUST write its own output to the pipeline.
            KTerm_RunTest(term, tokens[1]);
        }
    } else if (strcmp(cmd, "term_showinfo") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_showinfo' takes no arguments\x1B[0m\n");
        } else {
            // Call the terminal library's function.
            // This function (in kterm.h) MUST write its own output to the pipeline.
            KTerm_WriteString(term, "\nRequesting terminal to show its info:\n");
            KTerm_ShowInfo(term);
        }
    } else if (strcmp(cmd, "term_diagbuffers") == 0) {
        if (token_count > 1) {
            KTerm_WriteString(term, "\x1B[31mError: 'term_diagbuffers' takes no arguments\x1B[0m\n");
        } else {
            // Call the terminal library's function.
            // This function (in kterm.h) MUST write its own output to the pipeline.
            KTerm_WriteString(term, "\nRequesting terminal to show buffer diagnostics:\n");
            KTerm_ShowDiagnostics(term);
        }
    } else if (strcmp(cmd, "pipeline_stats") == 0) { // This CLI-specific alias can stay as is
        if (token_count > 1) KTerm_WriteString(term, "\x1B[31mError: 'pipeline_stats' takes no arguments\x1B[0m\n");
        else KTerm_ShowDiagnostics(term); // Or call the 'term_diagbuffers' logic if you prefer consistency

    }  else if (strcmp(cmd, "sys_info") == 0) {
        KTerm_WriteString(term, "\n\x1B[1;33m--- System Device Information ---\x1B[0m\n");
        SituationDeviceInfo dev_info = SituationGetDeviceInfo();
        SitHelperPrintDeviceInfo(&dev_info); // Use our new helper
    } else if (strcmp(cmd, "sys_displays") == 0) {
        KTerm_WriteString(term, "\n\x1B[1;33m--- Physical Display Information ---\x1B[0m\n");
        int display_count = 0;
        SituationDisplayInfo* displays = SituationGetDisplays(&display_count);
        if (displays) {
            SitHelperPrintDisplayInfo(displays, display_count);
            for (int i = 0; i < display_count; ++i) free(displays[i].available_modes);
            free(displays);
        } else {
            char* err_msg = SituationGetLastErrorMsg();
            KTerm_WriteFormat(term, "\x1B[31mError getting display info: %s\x1B[0m\n", err_msg ? err_msg : "Unknown");
            if (err_msg) free(err_msg);
        }
        KTerm_WriteFormat(term, "  Current Raylib Mon Index (from Situation): %d\n", SituationGetCurrentRaylibDisplayIndex());
    } else if (strcmp(cmd, "sys_audio") == 0) {
        KTerm_WriteString(term, "\n\x1B[1;33m--- Audio Playback Device Information ---\x1B[0m\n");
        int audio_device_count = 0;
        SituationAudioDeviceInfo* audio_devices = SituationGetAudioDevices(&audio_device_count);
        if (audio_devices) {
            SitHelperPrintAudioDeviceInfo(audio_devices, audio_device_count);
            free(audio_devices);
        } else {
            char* err_msg = SituationGetLastErrorMsg();
            KTerm_WriteFormat(term, "\x1B[31mError getting audio devices: %s\x1B[0m\n", err_msg ? err_msg : "No devices or error");
            if (err_msg) free(err_msg);
        }
    } else if (strcmp(cmd, "sys_userdir") == 0) {
        KTerm_WriteString(term, "\n\x1B[1;33m--- User Directory ---\x1B[0m\n");
        char* user_dir = SituationGetUserDirectory();
        if (user_dir) {
            KTerm_WriteFormat(term, "  User Profile Directory: %s\n", user_dir);
            free(user_dir);
        } else {
            char* err_msg = SituationGetLastErrorMsg();
            KTerm_WriteFormat(term, "\x1B[31mError getting user directory: %s\x1B[0m\n", err_msg ? err_msg : "Unknown");
            if (err_msg) free(err_msg);
        }
    } else if (strcmp(cmd, "help") == 0) {
        const char* help_text_page1 =
            "\x1B[1;36mKaOS KTerm Help - Page 1\x1B[0m\n"
            "\x1B[1;32mBasic Commands:\x1B[0m\n"
            "  \x1B[33mhelp\x1B[0m             - Show this help (use 'help 2' for more)\n"
            "  \x1B[33mcls/clear\x1B[0m        - Clear screen\n"
            "  \x1B[33mecho [text...]\x1B[0m   - Echo text (or newline)\n"
            "  \x1B[33mhistory\x1B[0m          - Show command history\n"
            "  \x1B[33mexit/quit\x1B[0m        - Exit console\n"
            "\x1B[1;32mKTerm Control:\x1B[0m\n"
            "  \x1B[33mecho_on/noecho\x1B[0m    - Toggle terminal's local echo (ESC[?12h/l)\n"
            "  \x1B[33mpassword/normal\x1B[0m  - Toggle CLI's password input display mode (*)\n"
            "  \x1B[33mmouse_on/mouse_off\x1B[0m - Toggle SGR mouse tracking (ESC[?1006h/l)\n"
            "\x1B[1;32mDemo Commands:\x1B[0m\n"
            "  \x1B[33mdemo\x1B[0m             - General features demo\n"
            "  \x1B[33mtest\x1B[0m             - Basic color test (old)\n"
            "  \x1B[33mcolor_test\x1B[0m       - ANSI & 256-color demo\n"
            "  \x1B[33mrainbow [txt...]\x1B[0m - True color rainbow text\n"
            "  \x1B[33mgraphics\x1B[0m         - Box drawing & block characters demo\n"
            "  \x1B[33mblink\x1B[0m            - Blinking text test\n"
            "  \x1B[33mscroll_test\x1B[0m      - Multi-line scrolling demo\n"
            "\x1B[90mShortcuts: \x1B[33m↑/↓\x1B[90m History, \x1B[33mTab\x1B[90m Complete, \x1B[33mCtrl+C\x1B[90m Interrupt, \x1B[33mCtrl+L\x1B[90m Clear\x1B[0m\n";
        const char* help_text_page2 =
            "\x1B[1;36mKaOS KTerm Help - Page 2\x1B[0m\n"
            "\x1B[1;32mKTerm Library Diagnostics:\x1B[0m\n"
            "  \x1B[33mterm_status\x1B[0m      - Show terminal library's KTerm_GetStatus(term)\n"
            "  \x1B[33mterm_vtlevel\x1B[0m     - Display current VT compatibility level\n"
            "  \x1B[33mterm_da\x1B[0m          - Request Primary & Secondary Device Attributes\n"
            "  \x1B[33mterm_diagbuffers\x1B[0m - Show terminal's internal buffer diagnostics\n"
            "  \x1B[33mterm_showinfo\x1B[0m    - Display terminal's full internal info screen\n"
            "  \x1B[33mterm_runtest \x1B[36m<name>\x1B[0m - Run internal terminal test suite\n"
            "     \x1B[36m<name>\x1B[0m: \x1B[90mcursor, colors, charset, mouse, modes, all\x1B[0m\n"
            "\x1B[1;32mPerformance Related:\x1B[0m\n"
            "  \x1B[33mperformance\x1B[0m      - Run CLI's high-volume output test\n"
            "  \x1B[33mpipeline_stats\x1B[0m   - Alias for term_diagbuffers (CLI specific)\n"
            "  \x1B[33mset_fps <val>\x1B[0m      - Set terminal's pipeline target FPS (1-120)\n"
            "  \x1B[33mset_budget <pct>\x1B[0m  - Set term's pipeline time budget (0.01-1.0)\n"
            "\x1B[1;32mSystem Information (via situation.h):\x1B[0m\n"
            "  \x1B[33msys_info\x1B[0m         - Show detailed hardware/OS information\n"
            "  \x1B[33msys_displays\x1B[0m     - List physical display monitors and modes\n"
            "  \x1B[33msys_audio\x1B[0m        - List available audio playback devices\n"
            "  \x1B[33msys_userdir\x1B[0m      - Show current user's profile directory\n"
            "\x1B[90mNote: KTerm diagnostic commands query/use the terminal library's features.\x1B[0m\n";
        if (token_count == 1 || (token_count == 2 && strcmp(tokens[1], "1") == 0)) {
            KTerm_WriteString(term, help_text_page1);
        } else if (token_count == 2 && strcmp(tokens[1], "2") == 0) {
            KTerm_WriteString(term, help_text_page2);
        } else {
            KTerm_WriteString(term, "\x1B[31mUsage: help [1|2]\x1B[0m\n");
        }

    } else {
        KTerm_WriteString(term, "\x1B[31mUnknown command: \x1B[0m");
        KTerm_WriteString(term, cmd);
        KTerm_WriteString(term, "\n\x1B[90mType 'help' for available commands.\x1B[0m\n");
    }
    
    console.prompt_pending = true;
    console.in_command = false;
    
    if (buffer_to_free) {
        free(buffer_to_free);
    }
}

// Keyboard event handling
static void HandleExtendedKeyInput(const char* sequence) {
    if (strcmp(sequence, "\x1B[A") == 0 || strcmp(sequence, "\x1BOA") == 0) NavigateHistory(-1);
    else if (strcmp(sequence, "\x1B[B") == 0 || strcmp(sequence, "\x1BOB") == 0) NavigateHistory(1);
    else if (strcmp(sequence, "\x1B[D") == 0 || strcmp(sequence, "\x1BOD") == 0) { if (console.edit_pos > 0) { console.edit_pos--; RedrawEditLine(); } }
    else if (strcmp(sequence, "\x1B[C") == 0 || strcmp(sequence, "\x1BOC") == 0) { if (console.edit_pos < console.edit_length) { console.edit_pos++; RedrawEditLine(); } }
    else if (strcmp(sequence, "\x1B[H") == 0) { console.edit_pos = 0; RedrawEditLine(); } // Home
    else if (strcmp(sequence, "\x1B[F") == 0) { console.edit_pos = console.edit_length; RedrawEditLine(); } // End
    else if (strcmp(sequence, "\x1B[3~") == 0) { // Delete
        if (console.edit_pos < console.edit_length) {
            memmove(&console.edit_buffer[console.edit_pos],
                    &console.edit_buffer[console.edit_pos + 1],
                    console.edit_length - console.edit_pos); // Corrected memmove
            console.edit_length--;
            console.edit_buffer[console.edit_length] = '\0'; // Ensure null term
            RedrawEditLine();
        }
    }
}

static void HandleEnterKey(void) {
    if (!console.input_enabled) return;
    KTerm_WriteChar(term, '\n');
    if (console.edit_length > 0) {
        AddToHistory(console.edit_buffer);
        strncpy(console.command_buffer, console.edit_buffer, MAX_COMMAND_BUFFER - 1);
        console.command_buffer[MAX_COMMAND_BUFFER - 1] = '\0';
        ClearEditBuffer();
        console.input_enabled = false;
        console.in_command = true;
        ProcessCommand(console.command_buffer);
    } else {
        console.prompt_pending = true;
        console.input_enabled = false;
    }
}

static void HandleKeyEvent(const char* sequence, int length) {
    // This function is now called by HandleKTermResponse AFTER DSR processing.
    // It receives fully processed VT sequences or characters.

    // Critical check: Only process key events if CLI input is enabled
    // AND we are not in the middle of processing a command (console.in_command).
    // Ctrl+C is a special case that should often bypass this.
    if (!(console.input_enabled && !console.in_command) &&
        !(length == 1 && sequence[0] == 0x03)) { // Allow Ctrl+C (0x03)
        return;
    }

    // REMOVE THE DEBUG KTerm_WriteFormat from here, as it will go to the terminal screen.
    // If you need this debug, print it to stderr or a log file.
    // fprintf(stderr, "[DEBUG HandleKeyEvent: Seq='%.*s' (Len:%d) input_enabled:%d]\n", length, sequence, length, console.input_enabled);


    // --- Handle specific key sequences for line editing ---

    if (length == 1 && (sequence[0] == '\r' || sequence[0] == '\n')) { // Enter
        HandleEnterKey();
        return;
    }
    if (length == 1 && (sequence[0] == '\b' || sequence[0] == 0x7F)) { // Backspace or DEL
        HandleBackspaceKey(); // This function should redraw
        return;
    }
    if (length == 1 && sequence[0] == '\t') { // Tab
        HandleTabKey(); // This function should redraw if changes are made
        return;
    }

    // Handle Ctrl+<char> sequences (0x01 to 0x1A)
    if (length == 1 && sequence[0] >= 0x01 && sequence[0] <= 0x1A) {
        int ctrl_char_code = sequence[0];
        switch (ctrl_char_code) {
            case 0x01:  // Ctrl+A - Move to beginning of line
                if (console.input_enabled) { // Check again, as Ctrl+C might have changed it
                    console.edit_pos = 0;
                    RedrawEditLine();
                }
                break;
            case 0x02:  // Ctrl+B - Move back one character
                if (console.input_enabled && console.edit_pos > 0) {
                    console.edit_pos--;
                    RedrawEditLine();
                }
                break;
            case 0x03:  // Ctrl+C - Interrupt / Clear line
                // console.input_enabled might already be false if called from Ctrl+C itself.
                KTerm_WriteChar(term, '^'); // Visually show ^C
                KTerm_WriteChar(term, 'C');
                KTerm_WriteChar(term, '\n');

                ClearEditBuffer();
                // RedrawEditLine(); // Not needed, new prompt will be shown
                
                console.in_command = false; // Ensure not stuck in command processing
                console.waiting_for_prompt_cursor_pos = false; // Cancel any pending DSR wait
                console.prompt_pending = true; // Request a new prompt
                console.input_enabled = false; // Disable input until new prompt DSR is back
                break;
            case 0x04:  // Ctrl+D - Delete char or EOF
                if (console.edit_length == 0) {
                    ProcessCommand("exit");
                } else if (console.edit_pos < console.edit_length) {
                    for (int i = console.edit_pos; i < console.edit_length - 1; i++) {
                        console.edit_buffer[i] = console.edit_buffer[i + 1];
                    }
                    console.edit_length--;
                    console.edit_buffer[console.edit_length] = '\0';
                    RedrawEditLine();
                }
                break;
            case 0x05:  // Ctrl+E - Move to end of line
                console.edit_pos = console.edit_length;
                RedrawEditLine();
                break;
            case 0x06:  // Ctrl+F - Move forward one character
                if (console.edit_pos < console.edit_length) {
                    console.edit_pos++;
                    RedrawEditLine();
                }
                break;
            case 0x08:  // Ctrl+H - Backspace
                HandleBackspaceKey();
                break;
            case 0x0A: // LF - Line Feed
                // case 0x0B: // VT - Vertical Tab (often treated same as LF)
                // case 0x0C: // FF - Form Feed (often treated same as LF or clears screen + home)
                GET_SESSION(term)->cursor.y++;
                if (GET_SESSION(term)->cursor.y > GET_SESSION(term)->scroll_bottom) { // Key condition
                    GET_SESSION(term)->cursor.y = GET_SESSION(term)->scroll_bottom;   // Clamp cursor to last line of region
                    KTerm_ScrollUpRegion(term, GET_SESSION(term)->scroll_top, GET_SESSION(term)->scroll_bottom, 1); // Scroll content
                }
                if (GET_SESSION(term)->ansi_modes.line_feed_new_line) { // LNM mode
                    GET_SESSION(term)->cursor.x = GET_SESSION(term)->left_margin; // Move to left margin of current line
                }
                break;
            case 0x0B:  // Ctrl+K - Kill to end of line
                console.edit_length = console.edit_pos;
                console.edit_buffer[console.edit_length] = '\0';
                RedrawEditLine();
                break;
            case 0x0C:  // Ctrl+L - Clear screen
                KTerm_WriteString(term, "\x1B[2J\x1B[H"); // Send clear screen and home to terminal
                console.prompt_pending = true;        // A new prompt will be needed
                console.input_enabled = false;        // Wait for DSR for new prompt
                console.waiting_for_prompt_cursor_pos = false; // Reset DSR wait state
                // No need to RedrawEditLine() here as the screen is cleared.
                break;
            case 0x0E:  // Ctrl+N - Next history
                NavigateHistory(1);
                break;
            case 0x10:  // Ctrl+P - Previous history
                NavigateHistory(-1);
                break;
            case 0x15:  // Ctrl+U - Clear line
                ClearEditBuffer();
                RedrawEditLine();
                break;
            case 0x17:  // Ctrl+W - Delete word
                if (console.edit_pos == 0) break;
                int end_pos = console.edit_pos;
                int start_pos = console.edit_pos;
                while (start_pos > 0 && console.edit_buffer[start_pos - 1] == ' ') start_pos--;
                while (start_pos > 0 && console.edit_buffer[start_pos - 1] != ' ') start_pos--;
                
                int num_to_delete = end_pos - start_pos;
                if (num_to_delete > 0) {
                    memmove(&console.edit_buffer[start_pos], 
                            &console.edit_buffer[end_pos], 
                            console.edit_length - end_pos + 1);
                    console.edit_length -= num_to_delete;
                    console.edit_pos = start_pos;
                    RedrawEditLine();
                }
                break;
            default:
                // Ignore other control characters
                break;
        }
        return;
    }

    // Handle extended key sequences (like arrows, Home, End - usually multi-byte ESC sequences)
    // These arrive as full sequences like "\x1B[A"
    if (length > 1 && sequence[0] == '\x1B') { // Common start for escape sequences
        // DSR would have been caught by HandleKTermResponse already.
        if (!GET_SESSION(term)->raw_mode) { // Assuming GET_SESSION(term)->raw_mode is from your terminal library
            HandleExtendedKeyInput(sequence); // This function should call RedrawEditLine if needed
        } else {
            // In raw mode, the application might want to see the raw sequence.
            // For a CLI, raw mode usually means the line editor is bypassed.
            // If your terminal library sends raw sequences in raw_mode,
            // you'd probably not call HandleKeyEvent in that case, or HandleKeyEvent
            // would just buffer it for a different kind of processing.
            // For now, let's assume HandleExtendedKeyInput knows what to do.
        }
        return;
    }

    // Handle printable characters
    // This assumes single-byte printable chars. For UTF-8, length could be > 1.
    // Your current HandlePrintableKey takes an `int key_code`.
    // If `sequence` can be multi-byte for a single character (UTF-8), this needs adjustment.
    // Assuming ASCII for now based on `HandlePrintableKey`'s signature.
    if (length == 1 && sequence[0] >= 32) { // ASCII 32 (space) and up
        HandlePrintableKey(sequence[0]);
        // RedrawEditLine() is NOT called here because HandlePrintableKey should
        // ideally send the character to the pipeline IF local echo is OFF.
        // If terminal local echo is ON, the terminal library handles display.
        // If your HandlePrintableKey also calls RedrawEditLine, that's fine.
    }

    // Unhandled sequences could be logged if necessary
    // else {
    //     fprintf(stderr, "CLI: Unhandled key sequence in HandleKeyEvent: '%.*s'\n", length, sequence);
    // }
}

// Response callback (Sink)
static void HandleKTermResponse(void* ctx, const char* response_data, int length) {
    KTerm* term = (KTerm*)ctx;
    // fprintf(stderr, "CLI: HandleKTermResponse received (len %d): '", length);
    for(int k=0; k<length; ++k) {
        if (response_data[k] >= 32 && response_data[k] < 127) fputc(response_data[k], stderr);
        else if (response_data[k] == '\x1B') fprintf(stderr, "ESC");
        else fprintf(stderr, "[%02X]", (unsigned char)response_data[k]);
    }
    fprintf(stderr, "'. waiting_for_prompt_DSR=%d\n", console.waiting_for_prompt_cursor_pos);

    const char* current_pos = response_data;
    int remaining_length = length;

    while (remaining_length > 0) {
        // Try to parse DSR for cursor position: ESC[<row>;<col>R
        // Need a way for ParseCSIResponse to indicate how many bytes it consumed if successful.
        // Let's modify ParseCSIResponse or create a new helper.

        int consumed_bytes = 0;

        // --- Attempt to parse Cursor Position Report (CPR) ---
        if (console.waiting_for_prompt_cursor_pos) { // Only look for CPR if we are expecting it for the prompt
            int cpr_row, cpr_col;
            // Helper function to find and parse CPR:
            // int find_and_parse_cpr(const char* buffer, int buffer_len, int* out_row, int* out_col, int* out_consumed_len)
            // Returns 1 if CPR found and parsed, 0 otherwise.
            // out_consumed_len is how much of 'buffer' the CPR took.
            // For simplicity now, let's assume if it starts with ESC[ and ends with R and contains ;
            // Note: This is a simplified check for this specific scenario. A full parser is more robust.
            
            // Try to match ESC[...R at the current_pos
            if (remaining_length >= 3 && current_pos[0] == '\x1B' && current_pos[1] == '[') {
                const char* r_char = (const char*)memchr(current_pos, 'R', remaining_length);
                if (r_char != NULL && (r_char - current_pos < remaining_length)) {
                    int cpr_len = (r_char - current_pos) + 1;
                    // Now try to parse just this segment as a DSR
                    if (ParseCSIResponse(current_pos, cpr_len)) { // ParseCSIResponse needs to be robust enough
                                                                  // or modified to take a max_len for this segment
                        if (cursor_tracker.position_received) { // ParseCSIResponse sets this
                            console.prompt_line_y = cursor_tracker.row;
                            console.prompt_start_x = cursor_tracker.col;
                            console.waiting_for_prompt_cursor_pos = false;
                            console.input_enabled = true;
                            cursor_tracker.position_received = false; // Consume
                            fprintf(stderr, "CLI HandleKTermResponse: DSR FOR PROMPT HANDLED from chunk. input_enabled=true. Y=%d, X=%d\n", console.prompt_line_y, console.prompt_start_x);
                            RedrawEditLine();
                            consumed_bytes = cpr_len;
                        } else {
                             fprintf(stderr, "CLI HandleKTermResponse: Chunk looked like CPR but ParseCSIResponse didn't confirm position.\n");
                             // It was some other ESC[...R sequence, or ParseCSIResponse needs adjustment
                        }
                    }
                }
            }
        }

        // --- Attempt to parse Device Attributes ---
        if (consumed_bytes == 0 && remaining_length >= 3 && current_pos[0] == '\x1B' && current_pos[1] == '[') {
            char end_char_da = current_pos[remaining_length -1]; // This is problematic if DA isn't last
            const char* c_char = (const char*)memchr(current_pos, 'c', remaining_length);

            if (c_char != NULL && (c_char - current_pos < remaining_length) &&
                (current_pos[2] == '?' || current_pos[2] == '>' || current_pos[2] == '=' || current_pos[2] >= '0' && current_pos[2] <='9')) { // Basic DA check
                
                int da_len = (c_char - current_pos) + 1;
                KTerm_WriteString(term, "\n\x1B[36mKTerm DA:\x1B[0m ");
                for(int i=0; i<da_len; ++i) {
                     if (current_pos[i] >= 32 && current_pos[i] < 127) KTerm_WriteChar(term, current_pos[i]);
                     else if (current_pos[i] == '\x1B') KTerm_WriteString(term, "ESC");
                     else KTerm_WriteFormat(term, "[%02X]", (unsigned char)current_pos[i]);
                }
                KTerm_WriteString(term, "\n");
                // If we just printed DA, we likely need a new prompt display cycle
                if (!console.waiting_for_prompt_cursor_pos) { // Avoid if we are already waiting for prompt DSR
                    console.prompt_pending = true;
                    console.input_enabled = false;
                }
                consumed_bytes = da_len;
            }
        }

        if (remaining_length > 2 && current_pos[0] == '\x1B' && current_pos[1] == '[') {
            if (current_pos[2] == 'M' || (current_pos[2] == '<' && strchr(current_pos, 'M') != NULL) || (current_pos[2] == '<' && strchr(current_pos, 'm') != NULL) ) {
                fprintf(stderr, "CLI: Detected Mouse Report: '%.*s'\n", remaining_length, current_pos);
                // Optionally, display it on terminal if you want to see it during testing
                // KTerm_WriteString(term, "\nMouse Report: ");
                // for(int k=0; k<remaining_length; ++k) KTerm_WriteChar(term, current_pos[k]);
                // KTerm_WriteString(term, "\n");
                // console.prompt_pending = true; // Need new prompt after this
                // console.input_enabled = false;

                consumed_bytes = remaining_length; // Consume the whole mouse report
                                                // so it doesn't go to HandleKeyEvent
            }
        }

        if (consumed_bytes == 0) {
            HandleKeyEvent(current_pos, 1); // Or pass remaining_length if you expect multi-byte key events
            consumed_bytes = 1; // Or remaining_length
        }

        current_pos += consumed_bytes;
        remaining_length -= consumed_bytes;
    }
}

// Pipeline management
static void ProcessConsolePipeline(void) {
    // Process any pending output
    if (console.prompt_pending && !cursor_tracker.waiting_for_position) {
        ShowPrompt();
    }
}

// ***** NEW: Helper functions to print Situation.h info via PipelineWrite... *****
void SitHelperPrintDeviceInfo(const SituationDeviceInfo* info) {
    if (!info) {
        KTerm_WriteString(term, "  \x1B[31mError: Device info pointer is NULL.\x1B[0m\n");
        return;
    }

    KTerm_WriteString(term, "  \x1B[1;34mCPU:\x1B[0m\n");
    KTerm_WriteFormat(term, "    Name: \x1B[37m%s\x1B[0m\n", info->cpu_name);
    KTerm_WriteFormat(term, "    Cores: \x1B[37m%d\x1B[0m\n", info->cpu_cores);
    KTerm_WriteFormat(term, "    Clock Speed: \x1B[37m%.2f GHz\x1B[0m\n", info->cpu_clock_speed_ghz);

    KTerm_WriteString(term, "  \x1B[1;34mGPU:\x1B[0m\n");
    KTerm_WriteFormat(term, "    Name: \x1B[37m%s\x1B[0m\n", info->gpu_name);
    KTerm_WriteFormat(term, "    Dedicated VRAM: \x1B[37m%llu MB\x1B[0m\n", info->gpu_dedicated_memory_bytes / (1024 * 1024));

    KTerm_WriteString(term, "  \x1B[1;34mRAM:\x1B[0m\n");
    KTerm_WriteFormat(term, "    Total: \x1B[37m%llu MB\x1B[0m\n", info->total_ram_bytes / (1024 * 1024));
    KTerm_WriteFormat(term, "    Available: \x1B[37m%llu MB\x1B[0m\n", info->available_ram_bytes / (1024 * 1024));

    KTerm_WriteFormat(term, "  \x1B[1;34mStorage Devices (%d found):\x1B[0m\n", info->storage_device_count);
    for (int i = 0; i < info->storage_device_count; ++i) {
        KTerm_WriteFormat(term, "    [%d] Name: \x1B[37m%s\x1B[0m\n", i, info->storage_device_names[i]);
        KTerm_WriteFormat(term, "        Capacity: \x1B[37m%llu GB\x1B[0m\n", info->storage_capacity_bytes[i] / (1024 * 1024 * 1024));
        KTerm_WriteFormat(term, "        Free Space: \x1B[37m%llu GB\x1B[0m\n", info->storage_free_bytes[i] / (1024 * 1024 * 1024));
    }

    KTerm_WriteFormat(term, "  \x1B[1;34mNetwork Adapters (%d found):\x1B[0m\n", info->network_adapter_count);
    for (int i = 0; i < info->network_adapter_count; ++i) {
        KTerm_WriteFormat(term, "    [%d] Name: \x1B[37m%s\x1B[0m\n", i, info->network_adapter_names[i]);
    }

    KTerm_WriteFormat(term, "  \x1B[1;34mInput Devices (%d found):\x1B[0m\n", info->input_device_count);
    for (int i = 0; i < info->input_device_count; ++i) {
        KTerm_WriteFormat(term, "    [%d] Name: \x1B[37m%s\x1B[0m\n", i, info->input_device_names[i]);
    }
    KTerm_WriteString(term, "\x1B[0m"); // Reset color
}

void SitHelperPrintDisplayInfo(SituationDisplayInfo* displays, int count) {
    if (!displays || count == 0) {
        KTerm_WriteString(term, "  \x1B[31mNo display information available.\x1B[0m\n");
        return;
    }
    KTerm_WriteFormat(term, "  Found \x1B[1;37m%d\x1B[0m physical display(s):\n", count);
    for (int i = 0; i < count; ++i) {
        KTerm_WriteFormat(term, "  \x1B[1;34mDisplay [%d]:\x1B[0m \x1B[37m%s\x1B[0m (Raylib Idx: \x1B[37m%d\x1B[0m)\n", i, displays[i].name, displays[i].raylib_monitor_index);
        KTerm_WriteFormat(term, "    Primary: \x1B[37m%s\x1B[0m\n", displays[i].is_primary ? "Yes" : "No");
        KTerm_WriteFormat(term, "    Current Mode: \x1B[37m%dx%d @ %dHz, %d-bit\x1B[0m\n",
               displays[i].current_mode.width, displays[i].current_mode.height,
               displays[i].current_mode.refresh_rate, displays[i].current_mode.color_depth);
        KTerm_WriteFormat(term, "    Available Modes (\x1B[37m%d\x1B[0m found):\n", displays[i].available_mode_count);
        for (int j = 0; j < displays[i].available_mode_count; ++j) {
            if (j < 3 || j > displays[i].available_mode_count - 2) { // Print first 3 and last 1
                 KTerm_WriteFormat(term, "      - \x1B[37m%dx%d @ %dHz, %d-bit\x1B[0m\n",
                       displays[i].available_modes[j].width, displays[i].available_modes[j].height,
                       displays[i].available_modes[j].refresh_rate, displays[i].available_modes[j].color_depth);
            } else if (j == 3 && displays[i].available_mode_count > 4) {
                KTerm_WriteFormat(term, "      - \x1B[90m... (and %d more)\x1B[0m\n", displays[i].available_mode_count - 4);
            }
        }
    }
    KTerm_WriteString(term, "\x1B[0m"); // Reset color
}

void SitHelperPrintAudioDeviceInfo(SituationAudioDeviceInfo* devices, int count) {
    if (!devices || count == 0) {
        KTerm_WriteString(term, "  \x1B[31mNo audio device information available.\x1B[0m\n");
        return;
    }
    KTerm_WriteFormat(term, "  Found \x1B[1;37m%d\x1B[0m audio playback device(s):\n", count);
    for (int i = 0; i < count; ++i) {
        KTerm_WriteFormat(term, "  \x1B[1;34mDevice [%d]\x1B[0m (Sit. ID: \x1B[37m%d\x1B[0m): \x1B[37m%s\x1B[0m\n", i, devices[i].situation_internal_id, devices[i].name);
        KTerm_WriteFormat(term, "    Default Playback: \x1B[37m%s\x1B[0m\n", devices[i].is_default_playback ? "Yes" : "No");
    }
    KTerm_WriteString(term, "\x1B[0m"); // Reset color
}

// Main application
int main(void) {
    int argc = 1; // Dummy if no real args; pass from main if available
    char* argv[] = {"console"}; // Dummy
    const char* window_title = "KaOS - Kaizen Operating System v0.1 (Situation-Aware)";
    int target_fps = 60; // Or get from GetPipelineTargetFPS() if kterm.h exposes it early

    SituationInitInfo init_info = {0};
    init_info.window_width = DEFAULT_WINDOW_WIDTH; // Use your define
    init_info.window_height = DEFAULT_WINDOW_HEIGHT;
    init_info.window_title = window_title;
    init_info.initial_active_window_flags = SITUATION_WINDOW_STATE_RESIZABLE | SITUATION_WINDOW_STATE_VSYNC_HINT | SITUATION_WINDOW_STATE_ALWAYS_RUN;
    init_info.initial_inactive_window_flags = SITUATION_WINDOW_STATE_ALWAYS_RUN;

    SituationError sit_init_err = SituationInit(
        argc,
        argv,
        &init_info
    );

    if (sit_init_err != SITUATION_SUCCESS) {
        char* err_msg = SituationGetLastErrorMsg();
        fprintf(stderr, "FATAL: SituationInit failed: %s\n", err_msg ? err_msg : "Unknown error");
        if (err_msg) free(err_msg);
        // If SituationInit fails (which includes Raylib's InitWindow), we probably can't continue
        return 1; 
    }
    SituationSetTargetFPS(target_fps); // Set after init
    fprintf(stderr, "Situation.h initialized successfully.\n");
    // No longer call InitWindow() directly here, SituationInit() handles it.
    // No longer call SetTargetFPS() directly here, SituationInit() handles it.
    // **********************************************

    KTermConfig term_config = {
        .width = DEFAULT_TERM_WIDTH,
        .height = DEFAULT_TERM_HEIGHT
        // .response_callback = HandleKTermResponse // Legacy
    };
    term = KTerm_Create(term_config);
    if (!term) return 1;

    // Use modern Sink Output for zero-copy performance
    KTerm_SetOutputSink(term, HandleKTermResponse, term);

    console.prompt_pending = false;
    console.in_command = false;
    console.line_ready = false;
    console.history_count = 0;
    console.history_pos = 0;
    console.echo_enabled = true;
    console.input_enabled = false;
    ClearEditBuffer();
    
    // Welcome Message
    KTerm_WriteString(term, "   \x1B[36m"); KTerm_WriteChar(term, 201); for(int i=0;i<74;i++) KTerm_WriteChar(term, 205); KTerm_WriteChar(term, 187); KTerm_WriteString(term, "\x1B[0m\n");
    KTerm_WriteString(term, "   \x1B[36m"); KTerm_WriteChar(term, 186); KTerm_WriteString(term, "\x1B[1;33m                    KaOS - Kaizen Operating System                    "); KTerm_WriteString(term, "\x1B[36m"); KTerm_WriteChar(term, 186); KTerm_WriteString(term, "\x1B[0m\n");
    KTerm_WriteString(term, "   \x1B[36m"); KTerm_WriteChar(term, 186); KTerm_WriteString(term, "\x1B[32m                     Version 0.1 - K-Term                  "); KTerm_WriteString(term, "\x1B[36m"); KTerm_WriteChar(term, 186); KTerm_WriteString(term, "\x1B[0m\n");
    KTerm_WriteString(term, "   \x1B[36m"); KTerm_WriteChar(term, 200); for(int i=0;i<74;i++) KTerm_WriteChar(term, 205); KTerm_WriteChar(term, 188); KTerm_WriteString(term, "\x1B[0m\n\n");
    KTerm_WriteString(term, "\x1B[1;37mWelcome to KaOS K-Term v0.1\x1B[0m\n");
    KTerm_WriteString(term, "\x1B[96m\x1B[0m Full ANSI support \x1B[0m 256 colors \x1B[0m Command history \x1B[0m Tab completion \x1B[0m High performance\x1B[0m\n");
    KTerm_WriteString(term, "\x1B[90mType '\x1B[33mhelp\x1B[90m' for commands, '\x1B[33mdemo\x1B[90m' for features, or '\x1B[33mtest\x1B[90m' for colors.\x1B[0m\n\n");
    
    GET_SESSION(term)->input_enabled = false;
    console.prompt_pending = true;
    
    // Main loop
    while (!SituationWindowShouldClose() && !should_exit) { // Use Situation's wrapper
        // ***** NEW: Call SituationUpdate() *****
        SituationUpdate();
        // **************************************

        if (SituationIsWindowResized()) {
            int w, h;
            SituationGetWindowSize(&w, &h);
            int cols = w / (DEFAULT_CHAR_WIDTH * DEFAULT_WINDOW_SCALE);
            int rows = h / (DEFAULT_CHAR_HEIGHT * DEFAULT_WINDOW_SCALE);
            KTerm_Resize(term, cols, rows);
        }

        if (console.prompt_pending && !console.in_command && !console.waiting_for_prompt_cursor_pos) {
            fprintf(stderr, "CLI MainLoop: Calling ShowPrompt.\n");
            ShowPrompt();
        }
        
        // KTerm_Update(term) likely handles input polling, processing, and drawing the terminal content
        KTerm_Update(term);

        // ***** NEW: Raylib Drawing Block (if kterm.h doesn't manage Begin/EndDrawing) *****
        // If KTerm_Update(term) calls BeginDrawing/EndDrawing internally, this block is not needed here.
        // If KTerm_Update(term) just "draws" characters to a buffer that is then rendered by Raylib's
        // DrawText or similar, then you need this block.
        // For now, I'll assume kterm.h draws as part of KTerm_Update(term) OR you have a separate
        // TerminalRender() function. Let's wrap it conceptually.
        
        // SituationBeginDrawing(); // Start Raylib drawing
        //     // ClearBackground(BLACK); // Or let kterm.h handle background
        //     // TerminalRender(); // Hypothetical function to draw terminal content
        //     // You could draw other Situation-managed elements here if needed (e.g., FPS counter from Raylib)
        //     DrawFPS(10,10); // Example Raylib direct draw
        // SituationEndDrawing(); // End Raylib drawing
        //
        //  NOTE: Your kterm.h likely ALREADY calls BeginDrawing/EndDrawing.
        //  If so, this explicit block might be redundant or conflict. Ensure kterm.h uses Situation equivalents.
        //  The key is that *somewhere* in your loop, BeginDrawing and EndDrawing must happen.
        //  If kterm.h does it, ensure SituationUpdate() is called before that part of kterm.h.
        //  For now, I'll remove this explicit block assuming kterm.h handles its own drawing.
        //  The critical part is `SituationUpdate()` is called once per frame.
    }
    
    // ***** NEW: Call SituationShutdown() *****
    SituationShutdown();
    // ****************************************

    KTerm_Destroy(term);
    // CloseWindow(); // SituationShutdown() calls CloseWindow()
    
    return 0;
}
