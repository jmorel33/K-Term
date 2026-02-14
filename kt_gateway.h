#ifndef KT_GATEWAY_H
#define KT_GATEWAY_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations to avoid including kterm.h if possible,
// but kterm.h types are required.
// We assume kterm.h is included before this file or this is included BY kterm.h.
#ifndef KTERM_H
// If not included via kterm.h, we might need types.
// But this module is designed to be embedded.
#endif

// Include Networking API if available (via KTERM_NET_IMPLEMENTATION macro or similar)
// We rely on kt_net.h being included before or with kterm.h
#include "kt_net.h"

// Gateway Protocol Entry Point
// Parses and executes Gateway commands (DCS GATE ...)
// Format: DCS GATE <Class>;<ID>;<Command>[;<Params>] ST
// Example: DCS GATE MAT;1;SET;COLOR;RED ST
// This function replaces the internal handling in the main parser.
void KTerm_GatewayProcess(KTerm* term, KTermSession* session, const char* class_id, const char* id, const char* command, const char* params);

// Registers built-in extensions (called by KTerm_Init)
void KTerm_RegisterBuiltinExtensions(KTerm* term);

#ifdef __cplusplus
}
#endif

#endif // KT_GATEWAY_H

#ifdef KTERM_GATEWAY_IMPLEMENTATION
#ifndef KTERM_GATEWAY_IMPLEMENTATION_GUARD
#define KTERM_GATEWAY_IMPLEMENTATION_GUARD

// Extern Font Data (Usually provided by kterm.h/font_data.h)
extern const uint8_t ibm_font_8x8[256 * 8];

// Internal Structures
typedef struct {
    char text[256];
    char font_name[64];
    bool kerned;
    int align; // 0=LEFT, 1=CENTER, 2=RIGHT
    RGB_KTermColor gradient_start;
    RGB_KTermColor gradient_end;
    bool gradient_enabled;
} BannerOptions;

// Helper Macros for Safe String Handling
#ifndef SAFE_STRNCPY
#define SAFE_STRNCPY(dest, src, size) do { \
    if ((size) > 0) { \
        strncpy((dest), (src), (size) - 1); \
        (dest)[(size) - 1] = '\0'; \
    } \
} while(0)
#endif

// Internal Helper Declarations
static bool KTerm_ParseColor(const char* str, RGB_KTermColor* color);
static void KTerm_ProcessBannerOptions(const char* params, BannerOptions* options);
static void KTerm_GenerateBanner(KTerm* term, KTermSession* session, const BannerOptions* options);

// VT Pipe Helpers
static int KTerm_Base64Value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

// Decode Base64 to a raw buffer (caller must free if it returns a pointer, but here we use output buffer)
// Returns number of bytes written.
static size_t KTerm_Base64DecodeBuffer(const char* in, unsigned char* out, size_t max_len) {
    if (!in || !out) return 0;
    size_t len = strlen(in);
    unsigned int val = 0;
    int valb = -8;
    size_t out_pos = 0;
    for (size_t i = 0; i < len; i++) {
        if (in[i] == '=') break;
        int c = KTerm_Base64Value(in[i]);
        if (c == -1) continue;
        val = (val << 6) + c;
        valb += 6;
        if (valb >= 0) {
            if (out_pos < max_len) {
                out[out_pos++] = (unsigned char)((val >> valb) & 0xFF);
            }
            valb -= 8;
        }
    }
    return out_pos;
}

static void KTerm_Base64StreamDecode(KTerm* term, int session_idx, const char* in) {
    if (!term || !in) return;
    size_t len = strlen(in);
    unsigned int val = 0;
    int valb = -8;
    for (size_t i = 0; i < len; i++) {
        if (in[i] == '=') break;
        int c = KTerm_Base64Value(in[i]);
        if (c == -1) continue;
        val = (val << 6) + c;
        valb += 6;
        if (valb >= 0) {
            KTerm_WriteCharToSession(term, session_idx, (unsigned char)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
}

static int KTerm_HexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void KTerm_HexStreamDecode(KTerm* term, int session_idx, const char* in) {
    if (!term || !in) return;
    size_t len = strlen(in);
    for (size_t i = 0; i < len; i += 2) {
        if (i + 1 >= len) break;
        int h1 = KTerm_HexValue(in[i]);
        int h2 = KTerm_HexValue(in[i+1]);
        if (h1 != -1 && h2 != -1) {
            KTerm_WriteCharToSession(term, session_idx, (unsigned char)((h1 << 4) | h2));
        }
    }
}

static bool KTerm_Gateway_DecodePipePayload(KTerm* term, KTermSession* session, const char* id, const char* params) {
    if (!params) return false;
    if (strncmp(params, "VT;", 3) != 0) return false;

    const char* encoding_start = params + 3;
    const char* payload_start = strchr(encoding_start, ';');
    if (!payload_start) return false;

    char encoding[16];
    size_t enc_len = (size_t)(payload_start - encoding_start);
    if (enc_len >= sizeof(encoding)) enc_len = sizeof(encoding) - 1;
    strncpy(encoding, encoding_start, enc_len);
    encoding[enc_len] = '\0';

    const char* payload = payload_start + 1;

    int session_idx = -1;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (&term->sessions[i] == session) {
            session_idx = i;
            break;
        }
    }

    if (session_idx != -1) {
        if (KTerm_Strcasecmp(encoding, "B64") == 0) {
            KTerm_Base64StreamDecode(term, session_idx, payload);
        } else if (KTerm_Strcasecmp(encoding, "HEX") == 0) {
            KTerm_HexStreamDecode(term, session_idx, payload);
        } else if (KTerm_Strcasecmp(encoding, "RAW") == 0) {
            // RAW is simple injection
            for (size_t i = 0; payload[i] != '\0'; i++) {
                KTerm_WriteCharToSession(term, session_idx, (unsigned char)payload[i]);
            }
        }
    }

    return true;
}


static bool KTerm_ParseColor(const char* str, RGB_KTermColor* color) {
    if (!str || !color) return false;

    if (str[0] == '#') {
        StreamScanner scanner = { .ptr = str + 1, .len = strlen(str) - 1, .pos = 0 };
        unsigned int val;
        if (Stream_ReadHex(&scanner, &val)) {
             int consumed = scanner.pos;
             if (consumed == 6) {
                 color->r = (unsigned char)((val >> 16) & 0xFF);
                 color->g = (unsigned char)((val >> 8) & 0xFF);
                 color->b = (unsigned char)(val & 0xFF);
                 color->a = 255;
                 return true;
             } else if (consumed == 3) {
                 unsigned int r = (val >> 8) & 0xF;
                 unsigned int g = (val >> 4) & 0xF;
                 unsigned int b = val & 0xF;
                 color->r = (unsigned char)((r << 4) | r);
                 color->g = (unsigned char)((g << 4) | g);
                 color->b = (unsigned char)((b << 4) | b);
                 color->a = 255;
                 return true;
             }
        }
    } else {
        StreamScanner scanner = { .ptr = str, .len = strlen(str), .pos = 0 };
        int r, g, b;
        if (Stream_ReadInt(&scanner, &r) &&
            Stream_Expect(&scanner, ',') &&
            Stream_ReadInt(&scanner, &g) &&
            Stream_Expect(&scanner, ',') &&
            Stream_ReadInt(&scanner, &b)) {

            color->r = (unsigned char)r;
            color->g = (unsigned char)g;
            color->b = (unsigned char)b;
            color->a = 255;
            return true;
        }
    }

    // Try to parse named colors
    for (int i = 0; i < 16; i++) {
        // Simple named colors map could be added here, but for now rely on RGB/Hex
    }
    return false;
}

static uint32_t KTerm_ParseAttributeString(const char* str) {
    if (!str || str[0] == '\0') return 0;

    // Try numeric first
    char* endptr;
    unsigned long val = strtoul(str, &endptr, 0);
    if (*endptr == '\0') return (uint32_t)val;

    // String based
    uint32_t flags = 0;

    // We need a mutable copy to tokenize, or we can use the KTermLexer/Scanner
    // Simple tokenizer for '|'
    char buffer[256];
    strncpy(buffer, str, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';

    char* tok = strtok(buffer, "|");
    while (tok) {
        if (KTerm_Strcasecmp(tok, "BOLD") == 0) flags |= KTERM_ATTR_BOLD;
        else if (KTerm_Strcasecmp(tok, "DIM") == 0) flags |= KTERM_ATTR_FAINT;
        else if (KTerm_Strcasecmp(tok, "FAINT") == 0) flags |= KTERM_ATTR_FAINT;
        else if (KTerm_Strcasecmp(tok, "ITALIC") == 0) flags |= KTERM_ATTR_ITALIC;
        else if (KTerm_Strcasecmp(tok, "UNDERLINE") == 0) flags |= KTERM_ATTR_UNDERLINE;
        else if (KTerm_Strcasecmp(tok, "BLINK") == 0) flags |= KTERM_ATTR_BLINK;
        else if (KTerm_Strcasecmp(tok, "REVERSE") == 0) flags |= KTERM_ATTR_REVERSE;
        else if (KTerm_Strcasecmp(tok, "INVERSE") == 0) flags |= KTERM_ATTR_REVERSE;
        else if (KTerm_Strcasecmp(tok, "HIDDEN") == 0) flags |= KTERM_ATTR_CONCEAL;
        else if (KTerm_Strcasecmp(tok, "CONCEAL") == 0) flags |= KTERM_ATTR_CONCEAL;
        else if (KTerm_Strcasecmp(tok, "STRIKE") == 0) flags |= KTERM_ATTR_STRIKE;
        else if (KTerm_Strcasecmp(tok, "PROTECTED") == 0) flags |= KTERM_ATTR_PROTECTED;
        else if (KTerm_Strcasecmp(tok, "DIRTY") == 0) flags |= KTERM_FLAG_DIRTY;

        tok = strtok(NULL, "|");
    }

    return flags;
}

static void KTerm_ProcessBannerOptions(const char* params, BannerOptions* options) {
    // Default values
    memset(options, 0, sizeof(BannerOptions));
    options->align = 0; // LEFT
    options->kerned = false;
    options->gradient_enabled = false;

    if (!params) return;

    KTermLexer lexer;
    KTerm_LexerInit(&lexer, params);
    KTermToken token = KTerm_LexerNext(&lexer);
    bool first_token = true;

    while (token.type != KT_TOK_EOF) {
        if (token.type == KT_TOK_IDENTIFIER) {
            char keyBuffer[64];
            int len = token.length < 63 ? token.length : 63;
            strncpy(keyBuffer, token.start, len);
            keyBuffer[len] = '\0';

            // Check for legacy positional flags "KERNED" or "FIXED" at start
            if (first_token) {
                if (KTerm_Strcasecmp(keyBuffer, "KERNED") == 0) {
                    options->kerned = true;
                    token = KTerm_LexerNext(&lexer);
                    if (token.type == KT_TOK_SEMICOLON) token = KTerm_LexerNext(&lexer);
                    if (token.type != KT_TOK_EOF) {
                         const char* remainder = token.start;
                         SAFE_STRNCPY(options->text, remainder, sizeof(options->text));
                    }
                    return;
                } else if (KTerm_Strcasecmp(keyBuffer, "FIXED") == 0) {
                    options->kerned = false;
                    token = KTerm_LexerNext(&lexer);
                    if (token.type == KT_TOK_SEMICOLON) token = KTerm_LexerNext(&lexer);
                    if (token.type != KT_TOK_EOF) {
                         const char* remainder = token.start;
                         SAFE_STRNCPY(options->text, remainder, sizeof(options->text));
                    }
                    return;
                }
            }

            // Normal processing
            KTermToken next = KTerm_LexerNext(&lexer);
            if (next.type == KT_TOK_EQUALS) {
                // Key=Val
                KTermToken val = KTerm_LexerNext(&lexer);
                char valBuffer[256];
                int vlen = val.length < 255 ? val.length : 255;
                if (val.type == KT_TOK_STRING) {
                    KTerm_UnescapeString(valBuffer, val.start, vlen);
                } else {
                    strncpy(valBuffer, val.start, vlen);
                    valBuffer[vlen] = '\0';
                }

                if (KTerm_Strcasecmp(keyBuffer, "TEXT") == 0) {
                    SAFE_STRNCPY(options->text, valBuffer, sizeof(options->text));
                } else if (KTerm_Strcasecmp(keyBuffer, "FONT") == 0) {
                    SAFE_STRNCPY(options->font_name, valBuffer, sizeof(options->font_name));
                } else if (KTerm_Strcasecmp(keyBuffer, "ALIGN") == 0) {
                    if (KTerm_Strcasecmp(valBuffer, "CENTER") == 0) options->align = 1;
                    else if (KTerm_Strcasecmp(valBuffer, "RIGHT") == 0) options->align = 2;
                    else options->align = 0;
                } else if (KTerm_Strcasecmp(keyBuffer, "GRADIENT") == 0) {
                    // Check for composite value C1|C2
                    // First part is in valBuffer
                    // Peek next token
                    KTermToken sep = KTerm_LexerNext(&lexer);
                    if (sep.type == KT_TOK_UNKNOWN && sep.length == 1 && *sep.start == '|') {
                        KTermToken val2 = KTerm_LexerNext(&lexer);
                        char val2Buffer[64];
                        int v2len = val2.length < 63 ? val2.length : 63;
                        if (val2.type == KT_TOK_STRING) {
                            KTerm_UnescapeString(val2Buffer, val2.start, v2len);
                        } else {
                            strncpy(val2Buffer, val2.start, v2len);
                            val2Buffer[v2len] = '\0';
                        }

                        if (KTerm_ParseColor(valBuffer, &options->gradient_start) &&
                            KTerm_ParseColor(val2Buffer, &options->gradient_end)) {
                            options->gradient_enabled = true;
                        }
                        // Consume next token (semicolon or eof)
                        token = KTerm_LexerNext(&lexer);
                    } else {
                        // Not composite or using quotes handled by valBuffer
                        char* pipesep = strchr(valBuffer, '|');
                        if (pipesep) {
                            *pipesep = '\0';
                            if (KTerm_ParseColor(valBuffer, &options->gradient_start) &&
                                KTerm_ParseColor(pipesep + 1, &options->gradient_end)) {
                                options->gradient_enabled = true;
                            }
                        }
                        token = sep;
                    }
                    if (token.type == KT_TOK_SEMICOLON) token = KTerm_LexerNext(&lexer);
                    continue; // Loop
                } else if (KTerm_Strcasecmp(keyBuffer, "MODE") == 0) {
                     if (KTerm_Strcasecmp(valBuffer, "KERNED") == 0) options->kerned = true;
                }

                token = KTerm_LexerNext(&lexer); // Expecting Semicolon or EOF
                if (token.type == KT_TOK_SEMICOLON) token = KTerm_LexerNext(&lexer);
            } else {
                // No Equals? Positional Text
                SAFE_STRNCPY(options->text, keyBuffer, sizeof(options->text));
                if (next.type == KT_TOK_SEMICOLON) token = KTerm_LexerNext(&lexer);
                else token = next;
            }
        } else if (token.type == KT_TOK_STRING) {
             // Positional Text
             char valBuffer[256];
             int vlen = token.length < 255 ? token.length : 255;
             KTerm_UnescapeString(valBuffer, token.start, vlen);
             SAFE_STRNCPY(options->text, valBuffer, sizeof(options->text));
             token = KTerm_LexerNext(&lexer);
             if (token.type == KT_TOK_SEMICOLON) token = KTerm_LexerNext(&lexer);
        } else {
            // Skip
            token = KTerm_LexerNext(&lexer);
        }
    }
}


static void KTerm_GenerateBanner(KTerm* term, KTermSession* session, const BannerOptions* options) {
    const char* text = options->text;
    if (!text || strlen(text) == 0) return;
    int len = strlen(text);

    // Default Font
    const uint8_t* font_data = (const uint8_t*)term->current_font_data;
    bool is_16bit = term->current_font_is_16bit;
    int height = term->font_data_height;
    int width = term->font_data_width;

    KTermFontMetric temp_metrics[256];
    bool use_temp_metrics = false;

    // Check soft font
    if (session->soft_font.active) {
        font_data = (const uint8_t*)session->soft_font.font_data;
        height = session->soft_font.char_height;
        width = session->soft_font.char_width;
        is_16bit = (width > 8);
    }

    // Check requested font overrides
    if (strlen(options->font_name) > 0) {
        for (int i = 0; available_fonts[i].name != NULL; i++) {
             if (KTerm_Strcasecmp(available_fonts[i].name, options->font_name) == 0) {
                 font_data = (const uint8_t*)available_fonts[i].data;
                 width = available_fonts[i].data_width;
                 height = available_fonts[i].data_height;
                 is_16bit = available_fonts[i].is_16bit;

                 if (options->kerned) {
                      KTerm_CalculateFontMetrics(font_data, 256, width, height, 0, is_16bit, temp_metrics);
                      use_temp_metrics = true;
                 }
                 break;
             }
        }
    }

    // Calculate total width for alignment
    int total_width = 0;
    if (options->align != 0) {
        for (int i = 0; i < len; i++) {
            unsigned char c = (unsigned char)text[i];
            int w = width;
            if (options->kerned) {
                KTermFontMetric* m;
                if (use_temp_metrics) m = &temp_metrics[c];
                else if (session->soft_font.active && font_data == (const uint8_t*)session->soft_font.font_data) m = &session->soft_font.metrics[c];
                else m = &term->font_metrics[c];

                if (m->end_x >= m->begin_x) {
                     w = m->end_x - m->begin_x + 1;
                } else if (c == ' ') {
                     w = width / 2;
                } else {
                     w = 0;
                }
                // Kerning adds 1 space
                 if (w > 0) w++;
            }
            total_width += w;
        }
    }

    int padding = 0;
    if (options->align == 1) { // Center
        padding = (term->width - total_width) / 2;
    } else if (options->align == 2) { // Right
        padding = term->width - total_width;
    }
    if (padding < 0) padding = 0;

    // Allocate Line Buffer on Heap (Hardening)
    size_t line_buffer_size = 32768; // 32KB
    char* line_buffer = (char*)malloc(line_buffer_size);
    if (!line_buffer) return;

    for (int y = 0; y < height; y++) {
        int line_pos = 0;
        line_buffer[0] = '\0';

        // Padding
        for(int p=0; p<padding; p++) {
             if (line_pos < (int)line_buffer_size - 1) line_buffer[line_pos++] = ' ';
        }
        line_buffer[line_pos] = '\0';

        for (int i = 0; i < len; i++) {
            unsigned char c = (unsigned char)text[i];

            // Apply Gradient Color
            if (options->gradient_enabled) {
                float t = 0.0f;
                if (len > 1) t = (float)i / (float)(len - 1);

                unsigned char r = (unsigned char)(options->gradient_start.r + (options->gradient_end.r - options->gradient_start.r) * t);
                unsigned char g = (unsigned char)(options->gradient_start.g + (options->gradient_end.g - options->gradient_start.g) * t);
                unsigned char b = (unsigned char)(options->gradient_start.b + (options->gradient_end.b - options->gradient_start.b) * t);

                char color_seq[32];
                snprintf(color_seq, sizeof(color_seq), "\x1B[38;2;%d;%d;%dm", r, g, b);
                int seq_len = strlen(color_seq);

                if (line_pos + seq_len < (int)line_buffer_size) {
                    memcpy(line_buffer + line_pos, color_seq, seq_len);
                    line_pos += seq_len;
                    line_buffer[line_pos] = '\0';
                }
            }

            // Get Glyph Row Data
            uint32_t row_data = 0;

            if (is_16bit) {
                if (session->soft_font.active && font_data == (const uint8_t*)session->soft_font.font_data) {
                     uint8_t b1 = session->soft_font.font_data[c][y * 2];
                     uint8_t b2 = session->soft_font.font_data[c][y * 2 + 1];
                     row_data = (b1 << 8) | b2;
                } else {
                    const uint16_t* font_data16 = (const uint16_t*)font_data;
                    row_data = font_data16[c * height + y];
                }
            } else {
                if (session->soft_font.active && font_data == (const uint8_t*)session->soft_font.font_data) {
                    row_data = session->soft_font.font_data[c][y];
                } else {
                    row_data = font_data[c * height + y];
                }
            }

            // Determine render range
            int start_x = 0;
            int end_x = width - 1;

            if (options->kerned) {
                KTermFontMetric* m;
                if (use_temp_metrics) m = &temp_metrics[c];
                else if (session->soft_font.active && font_data == (const uint8_t*)session->soft_font.font_data) m = &session->soft_font.metrics[c];
                else m = &term->font_metrics[c];

                if (m->end_x >= m->begin_x) {
                    start_x = m->begin_x;
                    end_x = m->end_x;
                } else {
                    if (c == ' ') {
                        start_x = 0;
                        end_x = width / 2;
                    } else {
                         start_x = 0; end_x = -1; // Skip
                    }
                }
            }

            // Append bits
            for (int x = start_x; x <= end_x; x++) {
                if (line_pos >= (int)line_buffer_size - 5) break;

                bool bit_set = false;
                if ((row_data >> (width - 1 - x)) & 1) {
                     bit_set = true;
                }

                if (bit_set) {
                    line_buffer[line_pos++] = '\xE2';
                    line_buffer[line_pos++] = '\x96';
                    line_buffer[line_pos++] = '\x88';
                } else {
                    line_buffer[line_pos++] = ' ';
                }
            }

            // Spacing
            if (options->kerned) {
                if (line_pos < (int)line_buffer_size - 1) line_buffer[line_pos++] = ' ';
            }
        }

        // Reset Color at end of line if gradient
        if (options->gradient_enabled) {
             const char* reset = "\x1B[0m";
             int r_len = strlen(reset);
             if (line_pos + r_len < (int)line_buffer_size) {
                 memcpy(line_buffer + line_pos, reset, r_len);
                 line_pos += r_len;
             }
        }

        line_buffer[line_pos] = '\0';
        KTerm_WriteString(term, line_buffer);
        KTerm_WriteString(term, "\r\n");
    }

    free(line_buffer);
}

// =============================================================================
// GATEWAY COMMAND DISPATCHER
// =============================================================================

typedef void (*GatewayHandler)(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner);

typedef struct {
    const char* name;
    GatewayHandler handler;
} GatewayCommand;

// Helpers
static KTermSession* KTerm_GetTargetSession(KTerm* term, KTermSession* session) {
    if (term->gateway_target_session >= 0 && term->gateway_target_session < MAX_SESSIONS) {
        return &term->sessions[term->gateway_target_session];
    }
    return session;
}

static int KTerm_GetSessionIndex(KTerm* term, KTermSession* session) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (&term->sessions[i] == session) return i;
    }
    return -1;
}

// Handler Definitions
static void KTerm_Gateway_HandleExt(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner);
static void KTerm_Gateway_HandleGet(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner);
static void KTerm_Gateway_HandleInit(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner);
static void KTerm_Gateway_HandlePipeCmd(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner);
static void KTerm_Gateway_HandleRawDump(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner);
static void KTerm_Gateway_HandleReset(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner);
static void KTerm_Gateway_HandleSet(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner);
static void KTerm_Gateway_HandleAttach(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner);
static void KTerm_Gateway_HandleDNS(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner);
static void KTerm_Gateway_HandlePing(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner);
static void KTerm_Gateway_HandlePortScan(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner);
static void KTerm_Gateway_HandleWhois(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner);

static const GatewayCommand gateway_commands[] = {
    { "ATTACH", KTerm_Gateway_HandleAttach },
    { "DNS", KTerm_Gateway_HandleDNS },
    { "EXT", KTerm_Gateway_HandleExt },
    { "GET", KTerm_Gateway_HandleGet },
    { "INIT", KTerm_Gateway_HandleInit },
    { "PING", KTerm_Gateway_HandlePing },
    { "PIPE", KTerm_Gateway_HandlePipeCmd },
    { "PORTSCAN", KTerm_Gateway_HandlePortScan },
    { "RAWDUMP", KTerm_Gateway_HandleRawDump },
    { "RESET", KTerm_Gateway_HandleReset },
    { "SET", KTerm_Gateway_HandleSet },
    { "WHOIS", KTerm_Gateway_HandleWhois }
};

static int GatewayCommandCmp(const void* key, const void* elem) {
    const char* name = (const char*)key;
    const GatewayCommand* cmd = (const GatewayCommand*)elem;
    return strcmp(name, cmd->name);
}

// Handlers

static void KTerm_Gateway_HandleAttach(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner) {
#ifdef KTERM_DISABLE_NET
    char response[64];
    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;ATTACH;ERR;NET_DISABLED\x1B\\", id);
    KTerm_QueueResponse(term, response);
#else
    char subcmd[64];
    if (!Stream_ReadIdentifier(scanner, subcmd, sizeof(subcmd))) return;

    if (strcmp(subcmd, "SESSION") == 0) {
        if (Stream_Expect(scanner, '=')) {
            int s_idx;
            if (Stream_ReadInt(scanner, &s_idx)) {
                KTerm_Net_SetTargetSession(term, session, s_idx);
                char response[64];
                snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;ATTACH;OK;SESSION=%d\x1B\\", id, s_idx);
                KTerm_QueueResponse(term, response);
            }
        }
    }
#endif
}

// Forward declarations for callbacks
static void KTerm_Traceroute_Callback(KTerm* term, KTermSession* session, int hop, const char* ip, double rtt_ms, bool reached, void* user_data);
static void KTerm_ResponseTime_Callback(KTerm* term, KTermSession* session, const ResponseTimeResult* result, void* user_data);
static void KTerm_PortScan_Callback(KTerm* term, KTermSession* session, const char* host, int port, int status, void* user_data);
static void KTerm_Whois_Callback(KTerm* term, KTermSession* session, const char* data, size_t len, bool done, void* user_data);
static void KTerm_Speedtest_Callback(KTerm* term, KTermSession* session, const SpeedtestResult* result, void* user_data);

static void KTerm_Gateway_HandleDNS(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner) {
#ifdef KTERM_DISABLE_NET
    char response[64];
    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;DNS;ERR;NET_DISABLED\x1B\\", id);
    KTerm_QueueResponse(term, response);
#else
    // DNS;host
    if (Stream_Expect(scanner, ';')) {
        char host[256];
        // Read until ST or end
        const char* p = scanner->ptr + scanner->pos;
        size_t len = strlen(p);
        if (len > sizeof(host)-1) len = sizeof(host)-1;
        strncpy(host, p, len);
        host[len] = '\0';

        if (host[0]) {
            char ip[64];
            if (KTerm_Net_Resolve(host, ip, sizeof(ip))) {
                char response[128];
                snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;DNS;OK;IP=%s\x1B\\", id, ip);
                KTerm_QueueResponse(term, response);
            } else {
                char response[64];
                snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;DNS;ERR;RESOLVE_FAILED\x1B\\", id);
                KTerm_QueueResponse(term, response);
            }
        } else {
            char response[64];
            snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;DNS;ERR;MISSING_HOST\x1B\\", id);
            KTerm_QueueResponse(term, response);
        }
    }
#endif
}

static void KTerm_Gateway_HandlePing(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner) {
#ifdef KTERM_DISABLE_NET
    char response[64];
    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;PING;ERR;NET_DISABLED\x1B\\", id);
    KTerm_QueueResponse(term, response);
#else
    // PING;host;[count;interval;timeout]
    if (Stream_Expect(scanner, ';')) {
        char host[256];
        int count = 4;
        int interval = 1000;
        int timeout = 2000;

        // Manually parse args from remainder using strtok-like logic or StreamScanner
        // Host is first
        char* token = (char*)scanner->ptr + scanner->pos;
        char* next_semi = strchr(token, ';');
        size_t host_len = next_semi ? (size_t)(next_semi - token) : strlen(token);
        if (host_len >= sizeof(host)) host_len = sizeof(host)-1;
        strncpy(host, token, host_len);
        host[host_len] = '\0';

        if (next_semi) {
            token = next_semi + 1;
            // Count
            next_semi = strchr(token, ';');
            if (next_semi) {
                count = atoi(token);
                token = next_semi + 1;
                // Interval
                next_semi = strchr(token, ';');
                if (next_semi) {
                    interval = atoi(token);
                    token = next_semi + 1;
                    // Timeout
                    timeout = atoi(token);
                } else {
                    interval = atoi(token);
                }
            } else {
                count = atoi(token);
            }
        }

        if (host[0]) {
            char* id_copy = (char*)malloc(strlen(id) + 1);
            if (id_copy) {
                strcpy(id_copy, id);
                if (KTerm_Net_ResponseTime(term, session, host, count, interval, timeout, KTerm_ResponseTime_Callback, id_copy)) {
                    char response[64];
                    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;PING;OK;STARTED\x1B\\", id);
                    KTerm_QueueResponse(term, response);
                } else {
                    free(id_copy);
                    char response[64];
                    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;PING;ERR;START_FAILED\x1B\\", id);
                    KTerm_QueueResponse(term, response);
                }
            }
        } else {
            char response[64];
            snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;PING;ERR;MISSING_HOST\x1B\\", id);
            KTerm_QueueResponse(term, response);
        }
    }
#endif
}

static void KTerm_Gateway_HandlePortScan(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner) {
#ifdef KTERM_DISABLE_NET
    char response[64];
    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;PORTSCAN;ERR;NET_DISABLED\x1B\\", id);
    KTerm_QueueResponse(term, response);
#else
    // PORTSCAN;host;ports;[timeout]
    if (Stream_Expect(scanner, ';')) {
        char host[256];
        char ports[256];
        int timeout = 1000;

        char* token = (char*)scanner->ptr + scanner->pos;
        char* next_semi = strchr(token, ';');

        // Host
        size_t len = next_semi ? (size_t)(next_semi - token) : strlen(token);
        if (len >= sizeof(host)) len = sizeof(host)-1;
        strncpy(host, token, len);
        host[len] = '\0';

        if (next_semi) {
            token = next_semi + 1;
            next_semi = strchr(token, ';');
            // Ports
            len = next_semi ? (size_t)(next_semi - token) : strlen(token);
            if (len >= sizeof(ports)) len = sizeof(ports)-1;
            strncpy(ports, token, len);
            ports[len] = '\0';

            if (next_semi) {
                token = next_semi + 1;
                timeout = atoi(token);
            }
        } else {
            ports[0] = '\0';
        }

        if (host[0] && ports[0]) {
            char* id_copy = (char*)malloc(strlen(id) + 1);
            if (id_copy) {
                strcpy(id_copy, id);
                if (KTerm_Net_PortScan(term, session, host, ports, timeout, KTerm_PortScan_Callback, id_copy)) {
                    char response[64];
                    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;PORTSCAN;OK;STARTED\x1B\\", id);
                    KTerm_QueueResponse(term, response);
                } else {
                    free(id_copy);
                    char response[64];
                    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;PORTSCAN;ERR;START_FAILED\x1B\\", id);
                    KTerm_QueueResponse(term, response);
                }
            }
        } else {
            char response[64];
            snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;PORTSCAN;ERR;MISSING_ARGS\x1B\\", id);
            KTerm_QueueResponse(term, response);
        }
    }
#endif
}

static void KTerm_Gateway_HandleWhois(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner) {
#ifdef KTERM_DISABLE_NET
    char response[64];
    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;WHOIS;ERR;NET_DISABLED\x1B\\", id);
    KTerm_QueueResponse(term, response);
#else
    // WHOIS;host;[query]
    if (Stream_Expect(scanner, ';')) {
        char host[256];
        char query[256];

        char* token = (char*)scanner->ptr + scanner->pos;
        char* next_semi = strchr(token, ';');

        // Host
        size_t len = next_semi ? (size_t)(next_semi - token) : strlen(token);
        if (len >= sizeof(host)) len = sizeof(host)-1;
        strncpy(host, token, len);
        host[len] = '\0';

        if (next_semi) {
            token = next_semi + 1;
            len = strlen(token);
            if (len >= sizeof(query)) len = sizeof(query)-1;
            strncpy(query, token, len);
            query[len] = '\0';
        } else {
            // Default query = host
            strncpy(query, host, sizeof(query)-1);
            query[sizeof(query)-1] = '\0';
        }

        if (host[0]) {
            char* id_copy = (char*)malloc(strlen(id) + 1);
            if (id_copy) {
                strcpy(id_copy, id);
                if (KTerm_Net_Whois(term, session, host, query, KTerm_Whois_Callback, id_copy)) {
                    char response[64];
                    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;WHOIS;OK;STARTED\x1B\\", id);
                    KTerm_QueueResponse(term, response);
                } else {
                    free(id_copy);
                    char response[64];
                    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;WHOIS;ERR;START_FAILED\x1B\\", id);
                    KTerm_QueueResponse(term, response);
                }
            }
        } else {
            char response[64];
            snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;WHOIS;ERR;MISSING_HOST\x1B\\", id);
            KTerm_QueueResponse(term, response);
        }
    }
#endif
}

static void KTerm_Gateway_HandleSet(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner) {
    KTermSession* target_session = KTerm_GetTargetSession(term, session);

    char subcmd[64];
    if (!Stream_ReadIdentifier(scanner, subcmd, sizeof(subcmd))) return;

    // Check for "SESSION" etc.
    if (strcmp(subcmd, "SESSION") == 0) {
        if (Stream_Expect(scanner, ';')) {
            int s_idx;
            if (Stream_ReadInt(scanner, &s_idx)) {
                if (s_idx >= 0 && s_idx < MAX_SESSIONS) term->gateway_target_session = s_idx;
            }
        }
    } else if (strcmp(subcmd, "REGIS_SESSION") == 0) {
        if (Stream_Expect(scanner, ';')) {
             int s_idx;
             if (Stream_ReadInt(scanner, &s_idx)) {
                 if (s_idx >= 0 && s_idx < MAX_SESSIONS) term->regis_target_session = s_idx;
             }
        }
    } else if (strcmp(subcmd, "TEKTRONIX_SESSION") == 0) {
        if (Stream_Expect(scanner, ';')) {
             int s_idx;
             if (Stream_ReadInt(scanner, &s_idx)) {
                 if (s_idx >= 0 && s_idx < MAX_SESSIONS) term->tektronix_target_session = s_idx;
             }
        }
    } else if (strcmp(subcmd, "KITTY_SESSION") == 0) {
        if (Stream_Expect(scanner, ';')) {
             int s_idx;
             if (Stream_ReadInt(scanner, &s_idx)) {
                 if (s_idx >= 0 && s_idx < MAX_SESSIONS) term->kitty_target_session = s_idx;
             }
        }
    } else if (strcmp(subcmd, "SIXEL_SESSION") == 0) {
        if (Stream_Expect(scanner, ';')) {
             int s_idx;
             if (Stream_ReadInt(scanner, &s_idx)) {
                 if (s_idx >= 0 && s_idx < MAX_SESSIONS) term->sixel_target_session = s_idx;
             }
        }
    } else if (strcmp(subcmd, "CURSOR") == 0) {
        if (Stream_Expect(scanner, ';')) {
            KTermLexer lexer;
            KTerm_LexerInit(&lexer, scanner->ptr + scanner->pos);
            KTermToken token = KTerm_LexerNext(&lexer);
            while (token.type != KT_TOK_EOF) {
                if (token.type == KT_TOK_IDENTIFIER) {
                    char key[64];
                    int klen = token.length < 63 ? token.length : 63;
                    strncpy(key, token.start, klen);
                    key[klen] = '\0';
                    KTermToken next = KTerm_LexerNext(&lexer);
                    if (next.type == KT_TOK_EQUALS) {
                        KTermToken val = KTerm_LexerNext(&lexer);
                        int v = (val.type == KT_TOK_NUMBER) ? val.value.i : 0;
                        if (strcmp(key, "SKIP_PROTECT") == 0) {
                            target_session->skip_protect = (v != 0);
                        } else if (strcmp(key, "HOME_MODE") == 0) {
                            if (val.type == KT_TOK_IDENTIFIER) {
                                char valBuf[64];
                                int vlen = val.length < 63 ? val.length : 63;
                                strncpy(valBuf, val.start, vlen);
                                valBuf[vlen] = '\0';

                                if (KTerm_Strcasecmp(valBuf, "ABSOLUTE") == 0) target_session->home_mode = HOME_MODE_ABSOLUTE;
                                else if (KTerm_Strcasecmp(valBuf, "FIRST_UNPROTECTED") == 0) target_session->home_mode = HOME_MODE_FIRST_UNPROTECTED;
                                else if (KTerm_Strcasecmp(valBuf, "FIRST_UNPROTECTED_LINE") == 0) target_session->home_mode = HOME_MODE_FIRST_UNPROTECTED_LINE;
                                else if (KTerm_Strcasecmp(valBuf, "LAST_FOCUSED") == 0) target_session->home_mode = HOME_MODE_LAST_FOCUSED;
                            } else {
                                target_session->home_mode = (KTermHomeMode)v;
                            }
                        }
                        token = KTerm_LexerNext(&lexer);
                    } else token = next;
                } else token = KTerm_LexerNext(&lexer);
                if (token.type == KT_TOK_SEMICOLON) token = KTerm_LexerNext(&lexer);
            }
        }
    } else if (strcmp(subcmd, "ATTR") == 0) {
        if (Stream_Expect(scanner, ';')) {
            // Revert to lexer for complex ATTR parsing
            KTermLexer lexer;
            // KTermLexer needs string, StreamScanner provides ptr + pos
            KTerm_LexerInit(&lexer, scanner->ptr + scanner->pos);
            KTermToken token = KTerm_LexerNext(&lexer);

            while (token.type != KT_TOK_EOF) {
                if (token.type == KT_TOK_IDENTIFIER) {
                    char key[64];
                    int klen = token.length < 63 ? token.length : 63;
                    strncpy(key, token.start, klen);
                    key[klen] = '\0';

                    KTermToken next = KTerm_LexerNext(&lexer);
                    if (next.type == KT_TOK_EQUALS) {
                        KTermToken val = KTerm_LexerNext(&lexer);
                        int v = (val.type == KT_TOK_NUMBER) ? val.value.i : 0;

                        char valBuf[256] = {0};
                        if (val.type == KT_TOK_IDENTIFIER || val.type == KT_TOK_STRING || val.type == KT_TOK_NUMBER) {
                            int vlen = val.length < 255 ? val.length : 255;
                            if (val.type == KT_TOK_STRING) {
                                KTerm_UnescapeString(valBuf, val.start, vlen);
                            } else {
                                strncpy(valBuf, val.start, vlen);
                                valBuf[vlen] = '\0';
                            }
                            if (val.type != KT_TOK_NUMBER) {
                                char* endptr;
                                long parsed_v = strtol(valBuf, &endptr, 0);
                                if (endptr != valBuf) v = (int)parsed_v;
                            }
                        }

                        bool is_rgb = false;
                        int r=0, g=0, b=0;
                        KTermToken lookahead = KTerm_LexerNext(&lexer);

                        if ((strcmp(key, "UL") == 0 || strcmp(key, "ST") == 0) && lookahead.type == KT_TOK_COMMA) {
                             r = v;
                             KTermToken tok_g = KTerm_LexerNext(&lexer);
                             if (tok_g.type == KT_TOK_NUMBER) g = tok_g.value.i;
                             else g = (int)strtol(tok_g.start, NULL, 10);

                             KTermToken sep2 = KTerm_LexerNext(&lexer);
                             KTermToken tok_b = KTerm_LexerNext(&lexer);
                             if (tok_b.type == KT_TOK_NUMBER) b = tok_b.value.i;
                             else b = (int)strtol(tok_b.start, NULL, 10);

                             is_rgb = true;
                             token = KTerm_LexerNext(&lexer);
                        } else {
                            if (lookahead.type == KT_TOK_SEMICOLON) token = KTerm_LexerNext(&lexer);
                            else token = lookahead;
                        }

                        if (strcmp(key, "BOLD") == 0) {
                            if (v) target_session->current_attributes |= KTERM_ATTR_BOLD;
                            else target_session->current_attributes &= ~KTERM_ATTR_BOLD;
                        } else if (strcmp(key, "DIM") == 0) {
                             if (v) target_session->current_attributes |= KTERM_ATTR_FAINT;
                             else target_session->current_attributes &= ~KTERM_ATTR_FAINT;
                        } else if (strcmp(key, "ITALIC") == 0) {
                            if (v) target_session->current_attributes |= KTERM_ATTR_ITALIC;
                            else target_session->current_attributes &= ~KTERM_ATTR_ITALIC;
                        } else if (strcmp(key, "UNDERLINE") == 0) {
                            if (v) target_session->current_attributes |= KTERM_ATTR_UNDERLINE;
                            else target_session->current_attributes &= ~KTERM_ATTR_UNDERLINE;
                        } else if (strcmp(key, "BLINK") == 0) {
                            if (v) target_session->current_attributes |= KTERM_ATTR_BLINK;
                            else target_session->current_attributes &= ~KTERM_ATTR_BLINK;
                        } else if (strcmp(key, "REVERSE") == 0) {
                            if (v) target_session->current_attributes |= KTERM_ATTR_REVERSE;
                            else target_session->current_attributes &= ~KTERM_ATTR_REVERSE;
                        } else if (strcmp(key, "HIDDEN") == 0) {
                            if (v) target_session->current_attributes |= KTERM_ATTR_CONCEAL;
                            else target_session->current_attributes &= ~KTERM_ATTR_CONCEAL;
                        } else if (strcmp(key, "STRIKE") == 0) {
                            if (v) target_session->current_attributes |= KTERM_ATTR_STRIKE;
                            else target_session->current_attributes &= ~KTERM_ATTR_STRIKE;
                        } else if (strcmp(key, "FG") == 0) {
                            target_session->current_fg.color_mode = 0;
                            target_session->current_fg.value.index = v & 0xFF;
                        } else if (strcmp(key, "BG") == 0) {
                            target_session->current_bg.color_mode = 0;
                            target_session->current_bg.value.index = v & 0xFF;
                        } else if (strcmp(key, "UL") == 0) {
                            if (is_rgb) {
                                target_session->current_ul_color.color_mode = 1;
                                target_session->current_ul_color.value.rgb = (RGB_KTermColor){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
                            } else if (valBuf[0] && sscanf(valBuf, "%d,%d,%d", &r, &g, &b) == 3) {
                                target_session->current_ul_color.color_mode = 1;
                                target_session->current_ul_color.value.rgb = (RGB_KTermColor){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
                            } else {
                                target_session->current_ul_color.color_mode = 0;
                                target_session->current_ul_color.value.index = v & 0xFF;
                            }
                        } else if (strcmp(key, "ST") == 0) {
                            if (is_rgb) {
                                target_session->current_st_color.color_mode = 1;
                                target_session->current_st_color.value.rgb = (RGB_KTermColor){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
                            } else if (valBuf[0] && sscanf(valBuf, "%d,%d,%d", &r, &g, &b) == 3) {
                                target_session->current_st_color.color_mode = 1;
                                target_session->current_st_color.value.rgb = (RGB_KTermColor){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
                            } else {
                                target_session->current_st_color.color_mode = 0;
                                target_session->current_st_color.value.index = v & 0xFF;
                            }
                        }
                    } else token = next;
                } else token = KTerm_LexerNext(&lexer);
                if (token.type == KT_TOK_SEMICOLON) token = KTerm_LexerNext(&lexer);
            }
        }
    } else if (strcmp(subcmd, "KEYBOARD") == 0) {
        if (Stream_Expect(scanner, ';')) {
            KTermLexer lexer;
            KTerm_LexerInit(&lexer, scanner->ptr + scanner->pos);
            KTermToken token = KTerm_LexerNext(&lexer);
            while (token.type != KT_TOK_EOF) {
                if (token.type == KT_TOK_IDENTIFIER) {
                     char key[64];
                     int klen = token.length < 63 ? token.length : 63;
                     strncpy(key, token.start, klen);
                     key[klen] = '\0';
                     KTermToken next = KTerm_LexerNext(&lexer);
                     if (next.type == KT_TOK_EQUALS) {
                         KTermToken val = KTerm_LexerNext(&lexer);
                         int v = (val.type == KT_TOK_NUMBER) ? val.value.i : 0;
                         if (val.type == KT_TOK_IDENTIFIER) {
                             if (KTerm_TokenIs(val, "HOST")) {
                                 if (strcmp(key, "REPEAT") == 0) target_session->input.use_software_repeat = false;
                             } else if (KTerm_TokenIs(val, "SOFTWARE")) {
                                 if (strcmp(key, "REPEAT") == 0) target_session->input.use_software_repeat = true;
                             }
                         }
                         if (strcmp(key, "REPEAT_RATE") == 0) {
                            if (v < 0) v = 0; if (v > 31) v = 31;
                            target_session->auto_repeat_rate = v;
                         } else if (strcmp(key, "DELAY") == 0) {
                            if (v < 0) v = 0;
                            target_session->auto_repeat_delay = v;
                         }
                         token = KTerm_LexerNext(&lexer);
                     } else token = next;
                } else token = KTerm_LexerNext(&lexer);
                if (token.type == KT_TOK_SEMICOLON) token = KTerm_LexerNext(&lexer);
            }
        }
    } else if (strcmp(subcmd, "GRID") == 0) {
        if (Stream_Expect(scanner, ';')) {
            KTermLexer lexer;
            KTerm_LexerInit(&lexer, scanner->ptr + scanner->pos);
            KTermToken token = KTerm_LexerNext(&lexer);
            while (token.type != KT_TOK_EOF) {
                if (token.type == KT_TOK_IDENTIFIER) {
                    if (KTerm_TokenIs(token, "ON")) target_session->grid_enabled = true;
                    else if (KTerm_TokenIs(token, "OFF")) target_session->grid_enabled = false;
                    else {
                        char key[32];
                        int klen = token.length < 31 ? token.length : 31;
                        strncpy(key, token.start, klen);
                        key[klen] = '\0';
                        KTermToken next = KTerm_LexerNext(&lexer);
                        if (next.type == KT_TOK_EQUALS) {
                            KTermToken val = KTerm_LexerNext(&lexer);
                            int v = (val.type == KT_TOK_NUMBER) ? val.value.i : 0;
                            if (v < 0) v = 0; if (v > 255) v = 255;
                            if (strcmp(key, "R") == 0) target_session->grid_color.r = (unsigned char)v;
                            else if (strcmp(key, "G") == 0) target_session->grid_color.g = (unsigned char)v;
                            else if (strcmp(key, "B") == 0) target_session->grid_color.b = (unsigned char)v;
                            else if (strcmp(key, "A") == 0) target_session->grid_color.a = (unsigned char)v;
                            token = KTerm_LexerNext(&lexer);
                        } else token = next;
                    }
                } else token = KTerm_LexerNext(&lexer);
                if (token.type == KT_TOK_SEMICOLON) token = KTerm_LexerNext(&lexer);
            }
        }
    } else if (strcmp(subcmd, "CONCEAL") == 0) {
        if (Stream_Expect(scanner, ';')) {
            int v;
            if (Stream_ReadInt(scanner, &v)) {
                if (v >= 0) target_session->conceal_char_code = (uint32_t)v;
            }
        }
    } else if (strcmp(subcmd, "SHADER") == 0) {
        if (Stream_Expect(scanner, ';')) {
            KTermLexer lexer;
            KTerm_LexerInit(&lexer, scanner->ptr + scanner->pos);
            KTermToken token = KTerm_LexerNext(&lexer);
            while (token.type != KT_TOK_EOF) {
                if (token.type == KT_TOK_IDENTIFIER) {
                    char key[64];
                    int klen = token.length < 63 ? token.length : 63;
                    strncpy(key, token.start, klen);
                    key[klen] = '\0';
                    KTermToken next = KTerm_LexerNext(&lexer);
                    if (next.type == KT_TOK_EQUALS) {
                        KTermToken val = KTerm_LexerNext(&lexer);
                        float v = 0.0f;
                        if (val.type == KT_TOK_NUMBER) v = val.value.f;
                        else v = strtof(val.start, NULL);

                        if (strcmp(key, "CRT_CURVATURE") == 0) term->visual_effects.curvature = v;
                        else if (strcmp(key, "SCANLINE_INTENSITY") == 0) term->visual_effects.scanline_intensity = v;
                        else if (strcmp(key, "GLOW_INTENSITY") == 0) term->visual_effects.glow_intensity = v;
                        else if (strcmp(key, "NOISE_INTENSITY") == 0) term->visual_effects.noise_intensity = v;
                        else if (strcmp(key, "CRT_ENABLE") == 0) { if (v > 0.0f) term->visual_effects.flags |= SHADER_FLAG_CRT; else term->visual_effects.flags &= ~SHADER_FLAG_CRT; }
                        else if (strcmp(key, "SCANLINE_ENABLE") == 0) { if (v > 0.0f) term->visual_effects.flags |= SHADER_FLAG_SCANLINE; else term->visual_effects.flags &= ~SHADER_FLAG_SCANLINE; }
                        else if (strcmp(key, "GLOW_ENABLE") == 0) { if (v > 0.0f) term->visual_effects.flags |= SHADER_FLAG_GLOW; else term->visual_effects.flags &= ~SHADER_FLAG_GLOW; }
                        else if (strcmp(key, "NOISE_ENABLE") == 0) { if (v > 0.0f) term->visual_effects.flags |= SHADER_FLAG_NOISE; else term->visual_effects.flags &= ~SHADER_FLAG_NOISE; }

                        token = KTerm_LexerNext(&lexer);
                    } else token = next;
                } else token = KTerm_LexerNext(&lexer);
                if (token.type == KT_TOK_SEMICOLON) token = KTerm_LexerNext(&lexer);
            }
        }
    } else if (strcmp(subcmd, "BLINK") == 0) {
         if (Stream_Expect(scanner, ';')) {
             KTermLexer lexer;
             KTerm_LexerInit(&lexer, scanner->ptr + scanner->pos);
             KTermToken token = KTerm_LexerNext(&lexer);
             while (token.type != KT_TOK_EOF) {
                 if (token.type == KT_TOK_IDENTIFIER) {
                     char key[32];
                     int klen = token.length < 31 ? token.length : 31;
                     strncpy(key, token.start, klen);
                     key[klen] = '\0';
                     KTermToken next = KTerm_LexerNext(&lexer);
                     if (next.type == KT_TOK_EQUALS) {
                         KTermToken val = KTerm_LexerNext(&lexer);
                         int v = (val.type == KT_TOK_NUMBER) ? val.value.i : 0;
                         if (v > 0) {
                            if (strcmp(key, "FAST") == 0) target_session->fast_blink_rate = v;
                            else if (strcmp(key, "SLOW") == 0) target_session->slow_blink_rate = v;
                            else if (strcmp(key, "BG") == 0) target_session->bg_blink_rate = v;
                         }
                         token = KTerm_LexerNext(&lexer);
                     } else token = next;
                 } else token = KTerm_LexerNext(&lexer);
                 if (token.type == KT_TOK_SEMICOLON) token = KTerm_LexerNext(&lexer);
             }
         }
    } else {
        // Generic PARAM;VALUE
        const char* param = subcmd;
        if (Stream_Expect(scanner, ';')) {
             // Read Value using Lexer logic because it might be string/identifier
             // Or use primitives if possible.
             // Values: "XTERM" (Level), "ON" (Bool), "Arial" (Font), Number

             // I'll reuse lexer here too for robustness with existing complex values
             KTermLexer lexer;
             KTerm_LexerInit(&lexer, scanner->ptr + scanner->pos);
             KTermToken valTok = KTerm_LexerNext(&lexer);
             char val[256] = {0};
             if (valTok.type != KT_TOK_EOF && valTok.type != KT_TOK_SEMICOLON) {
                    int vlen = valTok.length < 255 ? valTok.length : 255;
                    if (valTok.type == KT_TOK_STRING) {
                        KTerm_UnescapeString(val, valTok.start, vlen);
                    } else {
                        strncpy(val, valTok.start, vlen);
                        val[vlen] = '\0';
                    }
             }

             if (strcmp(param, "LEVEL") == 0) {
                    int level = (valTok.type == KT_TOK_NUMBER) ? valTok.value.i : (int)strtol(val, NULL, 10);
                    if (strcmp(val, "XTERM") == 0) level = VT_LEVEL_XTERM;
                    KTerm_SetLevel(term, target_session, (VTLevel)level);
             } else if (strcmp(param, "DEBUG") == 0) {
                    bool enable = (strcmp(val, "ON") == 0 || strcmp(val, "1") == 0 || strcmp(val, "TRUE") == 0);
                    KTerm_EnableDebug(term, enable);
             } else if (strcmp(param, "OUTPUT") == 0) {
                    bool enable = (strcmp(val, "ON") == 0 || strcmp(val, "1") == 0 || strcmp(val, "TRUE") == 0);
                    target_session->response_enabled = enable;
             } else if (strcmp(param, "WIDE_CHARS") == 0) {
                    bool enable = (strcmp(val, "ON") == 0 || strcmp(val, "1") == 0 || strcmp(val, "TRUE") == 0);
                    target_session->enable_wide_chars = enable;
             } else if (strcmp(param, "FONT") == 0) {
                    KTerm_SetFont(term, val);
             } else if (strcmp(param, "WIDTH") == 0) {
                    int cols = (valTok.type == KT_TOK_NUMBER) ? valTok.value.i : (int)strtol(val, NULL, 10);
                    if (cols > 0) {
                        if (cols > KTERM_MAX_COLS) cols = KTERM_MAX_COLS;
                        KTerm_Resize(term, cols, term->height);
                    }
             } else if (strcmp(param, "HEIGHT") == 0) {
                    int rows = (valTok.type == KT_TOK_NUMBER) ? valTok.value.i : (int)strtol(val, NULL, 10);
                    if (rows > 0) {
                        if (rows > KTERM_MAX_ROWS) rows = KTERM_MAX_ROWS;
                        KTerm_Resize(term, term->width, rows);
                    }
             } else if (strcmp(param, "SIZE") == 0) {
                    // Expecting second value: SIZE;W;H
                    // valTok is W.
                    int cols = (valTok.type == KT_TOK_NUMBER) ? valTok.value.i : (int)strtol(val, NULL, 10);
                    KTermToken sep2 = KTerm_LexerNext(&lexer);
                    if (sep2.type == KT_TOK_SEMICOLON) {
                        KTermToken val2Tok = KTerm_LexerNext(&lexer);
                        char val2[256] = {0};
                         if (val2Tok.type != KT_TOK_EOF) {
                             int v2len = val2Tok.length < 255 ? val2Tok.length : 255;
                             if (val2Tok.type == KT_TOK_STRING) {
                                 KTerm_UnescapeString(val2, val2Tok.start, v2len);
                             } else {
                                 strncpy(val2, val2Tok.start, v2len);
                                 val2[v2len] = '\0';
                             }
                        }
                        int rows = (val2Tok.type == KT_TOK_NUMBER) ? val2Tok.value.i : (int)strtol(val2, NULL, 10);
                        if (cols > 0 && rows > 0) {
                            if (cols > KTERM_MAX_COLS) cols = KTERM_MAX_COLS;
                            if (rows > KTERM_MAX_ROWS) rows = KTERM_MAX_ROWS;
                            KTerm_Resize(term, cols, rows);
                        }
                    }
             }
        }
    }
}

static void KTerm_Gateway_HandlePipeCmd(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner) {
    KTermSession* target_session = KTerm_GetTargetSession(term, session);
    // Pipe payload parsing is special, it uses raw params string usually?
    // KTerm_Gateway_DecodePipePayload expects full params string "VT;..."
    // StreamScanner provides ptr + pos.

    // We pass the remainder of the string to DecodePipePayload
    if (KTerm_Gateway_DecodePipePayload(term, target_session, id, scanner->ptr + scanner->pos)) {
        return;
    }

    // Check for "BANNER;..."
    // StreamScanner approach:
    char subcmd[64];
    if (Stream_ReadIdentifier(scanner, subcmd, sizeof(subcmd))) {
         if (strcmp(subcmd, "BANNER") == 0) {
             if (Stream_Expect(scanner, ';')) {
                 BannerOptions options;
                 KTerm_ProcessBannerOptions(scanner->ptr + scanner->pos, &options);
                 KTerm_GenerateBanner(term, target_session, &options);
             }
         }
    }
}

static void KTerm_Gateway_HandleRawDump(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner) {
    // Params: START, STOP, TOGGLE, SESSION=n, FORCE_WOB=bool
    const char* args = scanner->ptr + scanner->pos;
    if (!args) return;

    char buffer[256];
    strncpy(buffer, args, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';

    bool start = false;
    bool stop = false;
    bool toggle = false;
    int target_id = -1;
    int force_wob = -1; // -1=unset

    char* token = strtok(buffer, ";");
    while (token) {
        if (strcmp(token, "START") == 0) start = true;
        else if (strcmp(token, "STOP") == 0) stop = true;
        else if (strcmp(token, "TOGGLE") == 0) toggle = true;
        else if (strncmp(token, "SESSION=", 8) == 0) {
            target_id = atoi(token + 8);
        } else if (strncmp(token, "FORCE_WOB=", 10) == 0) {
            const char* v = token + 10;
            if (strcmp(v, "1") == 0 || KTerm_Strcasecmp(v, "TRUE") == 0 || KTerm_Strcasecmp(v, "ON") == 0) force_wob = 1;
            else force_wob = 0;
        }
        token = strtok(NULL, ";");
    }

    if (target_id == -1) {
        target_id = term->active_session;
    }

    // Determine Action
    if (toggle) {
        if (session->raw_dump.raw_dump_mirror_active) stop = true;
        else start = true;
    }

    if (start && stop) start = false; // Stop wins

    if (stop) {
        session->raw_dump.raw_dump_mirror_active = false;

        char response[128];
        snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;RAWDUMP;STOPPED;SESSION=%d\x1B\\", id, target_id);
        KTerm_QueueResponse(term, response);
    } else if (start) {
        session->raw_dump.raw_dump_mirror_active = true;
        session->raw_dump.raw_dump_target_session_id = target_id;
        if (force_wob != -1) session->raw_dump.raw_dump_force_wob = (force_wob == 1);
        session->raw_dump.initialized = false; // Trigger clear

        char response[128];
        snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;RAWDUMP;ACTIVE;SESSION=%d\x1B\\", id, target_id);
        KTerm_QueueResponse(term, response);
    } else {
        // Just update params if neither start nor stop/toggle?
        // Or assume start if SESSION is provided but no verb?
        // Let's assume explicit verb is required for state change, but we can update target on the fly.
        if (target_id != -1 && session->raw_dump.raw_dump_mirror_active) session->raw_dump.raw_dump_target_session_id = target_id;
        if (force_wob != -1) session->raw_dump.raw_dump_force_wob = (force_wob == 1);

        char response[128];
        snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;RAWDUMP;UPDATED\x1B\\", id);
        KTerm_QueueResponse(term, response);
    }
}

static void KTerm_Gateway_HandleInit(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner) {
    // INIT;...
    char subcmd[64];
    if (!Stream_ReadIdentifier(scanner, subcmd, sizeof(subcmd))) return;

    // session finding logic
    int s_idx = -1;
    for(int i=0; i<MAX_SESSIONS; i++) if(&term->sessions[i] == session) { s_idx = i; break; }

    if (s_idx != -1) {
        if (strcmp(subcmd, "REGIS_SESSION") == 0) {
             term->regis_target_session = s_idx;
             KTerm_InitReGIS(term, session);
        } else if (strcmp(subcmd, "TEKTRONIX_SESSION") == 0) {
             term->tektronix_target_session = s_idx;
             KTerm_InitTektronix(term, session);
        } else if (strcmp(subcmd, "KITTY_SESSION") == 0) {
             term->kitty_target_session = s_idx;
             KTerm_InitKitty(term, session);
        } else if (strcmp(subcmd, "SIXEL_SESSION") == 0) {
             term->sixel_target_session = s_idx;
             KTerm_InitSixelGraphics(term, session);
        }
    }
}

static void KTerm_Gateway_HandleReset(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner) {
    KTermSession* target_session = KTerm_GetTargetSession(term, session);
    char subcmd[64];
    if (!Stream_ReadIdentifier(scanner, subcmd, sizeof(subcmd))) return;

    if (strcmp(subcmd, "GRAPHICS") == 0 || strcmp(subcmd, "ALL_GRAPHICS") == 0) {
        KTerm_ResetGraphics(term, target_session, GRAPHICS_RESET_ALL);
    } else if (strcmp(subcmd, "KITTY") == 0) {
        KTerm_ResetGraphics(term, target_session, GRAPHICS_RESET_KITTY);
    } else if (strcmp(subcmd, "REGIS") == 0) {
        KTerm_ResetGraphics(term, target_session, GRAPHICS_RESET_REGIS);
    } else if (strcmp(subcmd, "TEK") == 0 || strcmp(subcmd, "TEKTRONIX") == 0) {
        KTerm_ResetGraphics(term, target_session, GRAPHICS_RESET_TEK);
    } else if (strcmp(subcmd, "SIXEL") == 0) {
        KTerm_ResetGraphics(term, target_session, GRAPHICS_RESET_SIXEL);
    } else if (strcmp(subcmd, "SESSION") == 0) {
         term->gateway_target_session = -1;
    } else if (strcmp(subcmd, "REGIS_SESSION") == 0) {
         term->regis_target_session = -1;
    } else if (strcmp(subcmd, "TEKTRONIX_SESSION") == 0) {
         term->tektronix_target_session = -1;
    } else if (strcmp(subcmd, "KITTY_SESSION") == 0) {
         term->kitty_target_session = -1;
    } else if (strcmp(subcmd, "SIXEL_SESSION") == 0) {
         term->sixel_target_session = -1;
    } else if (strcmp(subcmd, "ATTR") == 0) {
         target_session->current_attributes = 0;
         target_session->current_fg.color_mode = 0;
         target_session->current_fg.value.index = COLOR_WHITE;
         target_session->current_bg.color_mode = 0;
         target_session->current_bg.value.index = COLOR_BLACK;
    } else if (strcmp(subcmd, "BLINK") == 0) {
         target_session->fast_blink_rate = 255;
         target_session->slow_blink_rate = 500;
         target_session->bg_blink_rate = 500;
    } else if (strcmp(subcmd, "TABS") == 0) {
         if (Stream_Expect(scanner, ';')) {
             char opt[64];
             if (Stream_ReadIdentifier(scanner, opt, sizeof(opt))) {
                 if (strcmp(opt, "DEFAULT8") == 0) {
                     KTerm_ClearAllTabStops(term);
                     for (int i = 8; i < term->width; i += 8) {
                         KTerm_SetTabStop(term, i);
                     }
                 }
             }
         } else {
             KTerm_ClearAllTabStops(term);
         }
    }
}

static void KTerm_Gateway_HandleGet(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner) {
    KTermSession* target_session = KTerm_GetTargetSession(term, session);
    char subcmd[64];
    if (!Stream_ReadIdentifier(scanner, subcmd, sizeof(subcmd))) return;

    if (strcmp(subcmd, "LEVEL") == 0) {
        char response[256];
        snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;REPORT;LEVEL=%d\x1B\\", id, KTerm_GetLevel(term));
        KTerm_QueueResponse(term, response);
    } else if (strcmp(subcmd, "VERSION") == 0) {
        char response[256];
        snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;REPORT;VERSION=%d.%d.%d\x1B\\", id, KTERM_VERSION_MAJOR, KTERM_VERSION_MINOR, KTERM_VERSION_PATCH);
        KTerm_QueueResponse(term, response);
    } else if (strcmp(subcmd, "OUTPUT") == 0) {
        char response[256];
        snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;REPORT;OUTPUT=%d\x1B\\", id, target_session->response_enabled ? 1 : 0);
        KTerm_QueueResponse(term, response);
    } else if (strcmp(subcmd, "FONTS") == 0) {
         char response[4096];
         snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;REPORT;FONTS=", id);
         size_t current_len = strlen(response);

         for (int i=0; available_fonts[i].name != NULL; i++) {
             size_t name_len = strlen(available_fonts[i].name);
             size_t remaining = sizeof(response) - current_len - 5;
             if (remaining > name_len) {
                 strcat(response, available_fonts[i].name);
                 current_len += name_len;
                 if (available_fonts[i+1].name != NULL) {
                     strcat(response, ",");
                     current_len++;
                 }
             } else {
                 break;
             }
         }
         if (current_len < sizeof(response) - 3) {
             strcat(response, "\x1B\\");
         }
         KTerm_QueueResponse(term, response);
    } else if (strcmp(subcmd, "UNDERLINE_COLOR") == 0) {
        char response[256];
        if (session->current_ul_color.color_mode == 1) {
            RGB_KTermColor c = session->current_ul_color.value.rgb;
            snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;REPORT;UNDERLINE_COLOR=%d,%d,%d\x1B\\", id, c.r, c.g, c.b);
        } else if (session->current_ul_color.color_mode == 2) {
            snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;REPORT;UNDERLINE_COLOR=DEFAULT\x1B\\", id);
        } else {
            snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;REPORT;UNDERLINE_COLOR=%d\x1B\\", id, session->current_ul_color.value.index);
        }
        KTerm_QueueResponse(term, response);
    } else if (strcmp(subcmd, "STRIKE_COLOR") == 0) {
        char response[256];
        if (session->current_st_color.color_mode == 1) {
            RGB_KTermColor c = session->current_st_color.value.rgb;
            snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;REPORT;STRIKE_COLOR=%d,%d,%d\x1B\\", id, c.r, c.g, c.b);
        } else if (session->current_st_color.color_mode == 2) {
            snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;REPORT;STRIKE_COLOR=DEFAULT\x1B\\", id);
        } else {
            snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;REPORT;STRIKE_COLOR=%d\x1B\\", id, session->current_st_color.value.index);
        }
        KTerm_QueueResponse(term, response);
    } else if (strcmp(subcmd, "SHADER") == 0) {
        char response[512];
        snprintf(response, sizeof(response),
            "\x1BPGATE;KTERM;%s;REPORT;SHADER=CRT_CURVATURE:%f,SCANLINE_INTENSITY:%f,GLOW_INTENSITY:%f,NOISE_INTENSITY:%f,FLAGS:%u\x1B\\",
            id, term->visual_effects.curvature, term->visual_effects.scanline_intensity, term->visual_effects.glow_intensity, term->visual_effects.noise_intensity, term->visual_effects.flags);
        KTerm_QueueResponse(term, response);
    } else if (strcmp(subcmd, "STATE") == 0) {
        char response[1024];
        // Packed State: CURSOR:x,y|SCROLL:t,b|MODES:dec,ansi|ATTR:fg,bg,flags
        int cx = target_session->cursor.x + 1;
        int cy = target_session->cursor.y + 1;
        int st = target_session->scroll_top + 1;
        int sb = target_session->scroll_bottom + 1;

        // Use unsigned int for modes to avoid warning format specifiers
        unsigned int dec_m = target_session->dec_modes;
        int ansi_m = target_session->ansi_modes.insert_replace ? 1 : 0;
        int fg = target_session->current_fg.value.index; // assuming indexed for simplicity in report, real state might be RGB
        int bg = target_session->current_bg.value.index;
        unsigned int attr = target_session->current_attributes;

        snprintf(response, sizeof(response),
            "\x1BPGATE;KTERM;%s;REPORT;STATE=CURSOR:%d,%d|SCROLL:%d,%d|MODES:%u,%d|ATTR:%d,%d,%u\x1B\\",
            id, cx, cy, st, sb, dec_m, ansi_m, fg, bg, attr);

        KTerm_QueueResponse(term, response);
    }
}

static void KTerm_Gateway_HandleExt(KTerm* term, KTermSession* session, const char* id, StreamScanner* scanner) {
    char ext_name[32];
    if (!Stream_ReadIdentifier(scanner, ext_name, sizeof(ext_name))) return;

    // Skip separator if present
    if (Stream_Expect(scanner, ';')) {
        // Args follow
    }
    const char* args = scanner->ptr + scanner->pos;

    // Find extension
    for (int i = 0; i < term->gateway_extension_count; i++) {
        if (strcmp(term->gateway_extensions[i].name, ext_name) == 0) {
            term->gateway_extensions[i].handler(term, session, id, args, KTerm_QueueSessionResponse);
            return;
        }
    }

    // Not found
    char err[128];
    snprintf(err, sizeof(err), "\x1BPGATE;KTERM;%s;ERR;UNKNOWN_EXTENSION=%s\x1B\\", id, ext_name);
    KTerm_QueueSessionResponse(term, session, err);
}

// Built-in Extension Handlers

static void KTerm_Ext_Broadcast(KTerm* term, KTermSession* session, const char* id, const char* args, GatewayResponseCallback respond) {
    (void)session;
    (void)respond;
    (void)id;
    // Broadcast text to all sessions
    // args: text to broadcast
    if (!args) return;

    // Simple broadcast: write chars to all sessions
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (term->sessions[i].session_open) {
            for (const char* p = args; *p; p++) {
                KTerm_WriteCharToSession(term, i, (unsigned char)*p);
            }
        }
    }
}

static void KTerm_Ext_Themes(KTerm* term, KTermSession* session, const char* id, const char* args, GatewayResponseCallback respond) {
    (void)id;
    // args: "set;bg=#123456" or "load;theme_name"
    if (!args) return;

    if (strncmp(args, "set;", 4) == 0) {
        const char* p = args + 4;
        if (strncmp(p, "bg=", 3) == 0) {
            RGB_KTermColor c;
            if (KTerm_ParseColor(p + 3, &c)) {
                // Set default BG via OSC 11
                char buf[64];
                snprintf(buf, sizeof(buf), "\x1B]11;rgb:%02x/%02x/%02x\x1B\\", c.r, c.g, c.b);
                int idx = KTerm_GetSessionIndex(term, session);
                if (idx != -1) {
                    for (int i=0; buf[i]; i++) KTerm_WriteCharToSession(term, idx, (unsigned char)buf[i]);
                }
                if (respond) respond(term, session, "OK");
            }
        }
    } else {
        if (respond) respond(term, session, "ERR;UNSUPPORTED_ACTION");
    }
}

static void KTerm_Ext_Clipboard(KTerm* term, KTermSession* session, const char* id, const char* args, GatewayResponseCallback respond) {
    (void)term; (void)session; (void)id;
    // args: "set;text" or "get"
    if (!args) return;
    if (strncmp(args, "set;", 4) == 0) {
        // Mock clipboard set
        // In real app, this calls platform clipboard API
        if (respond) respond(term, session, "OK");
    } else if (strcmp(args, "get") == 0) {
        // Mock clipboard get
        if (respond) respond(term, session, "MOCK_CLIPBOARD_DATA");
    }
}

static void KTerm_Ext_Icat(KTerm* term, KTermSession* session, const char* id, const char* args, GatewayResponseCallback respond) {
    (void)term; (void)session; (void)id;
    // args: "base64_data" or "file=path"
    if (!args) return;

    // Very basic pass-through for inline base64 image (Kitty format)
    // Assumes args is RAW base64 of the image file (e.g. PNG)
    // We wrap it in Kitty escape code: ESC _ G f=100, a=T, m=0; <data> ESC \

    // Inject header
    const char* header = "\x1B_Gf=100,a=T,m=0;"; // f=100 (PNG), a=T (Transmit), m=0 (Last chunk)

    int idx = KTerm_GetSessionIndex(term, session);
    if (idx != -1) {
        // header
        for (int i=0; header[i]; i++) KTerm_WriteCharToSession(term, idx, (unsigned char)header[i]);

        // data
        for (const char* p = args; *p; p++) KTerm_WriteCharToSession(term, idx, (unsigned char)*p);

        // footer (ST)
        KTerm_WriteCharToSession(term, idx, '\x1B');
        KTerm_WriteCharToSession(term, idx, '\\');
    }

    if (respond) respond(term, session, "OK");
}

static void KTerm_Ext_DirectInput(KTerm* term, KTermSession* session, const char* id, const char* args, GatewayResponseCallback respond) {
    if (!args) return;
    (void)id;

    KTermSession* target = KTerm_GetTargetSession(term, session);

    bool enable = false;
    if (strcmp(args, "1") == 0 || KTerm_Strcasecmp(args, "ON") == 0 || KTerm_Strcasecmp(args, "TRUE") == 0) {
        enable = true;
    }

    target->direct_input = enable;
    if (respond) respond(term, session, "OK");
}

// Callback for async traceroute responses
static void KTerm_Traceroute_Callback(KTerm* term, KTermSession* session, int hop, const char* ip, double rtt_ms, bool reached, void* user_data) {
    if (!term || !session) return;
    char* id = (char*)user_data;
    if (!id) id = "0";

    char payload[512];
    if (hop == 0) {
        // Error or Start
        // If hop 0 and error string is passed in ip
        snprintf(payload, sizeof(payload), "ERR;%s", ip ? ip : "UNKNOWN");
    } else {
        snprintf(payload, sizeof(payload), "HOP;%d;%s;%.3f%s", hop, ip ? ip : "*", rtt_ms, reached ? ";REACHED" : "");
    }

    // Format full Gateway response: ESC P GATE ; KTERM ; ID ; TRACEROUTE ; payload ST
    char response[1024];
    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;TRACEROUTE;%s\x1B\\", id, payload);

    // Send to session response (Host)
    KTerm_QueueSessionResponse(term, session, response);
}

// Callback for async response time results
static void KTerm_ResponseTime_Callback(KTerm* term, KTermSession* session, const ResponseTimeResult* result, void* user_data) {
    if (!term || !session || !result) return;
    char* id = (char*)user_data;
    if (!id) id = "0";

    char payload[1024];
    if (result->sent == 0) {
        snprintf(payload, sizeof(payload), "ERR;FAILED_TO_START");
    } else {
        // Format: OK;SENT=n;RECV=n;LOST=n;MIN=x;AVG=x;MAX=x;JITTER=x
        snprintf(payload, sizeof(payload), "OK;SENT=%d;RECV=%d;LOST=%d;MIN=%.3f;AVG=%.3f;MAX=%.3f;JITTER=%.3f",
                 result->sent, result->received, result->lost,
                 result->min_rtt_ms, result->avg_rtt_ms, result->max_rtt_ms, result->jitter_ms);
    }

    // Format full Gateway response: ESC P GATE ; KTERM ; ID ; RESPONSETIME ; payload ST
    char response[2048];
    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;RESPONSETIME;%s\x1B\\", id, payload);
    KTerm_QueueSessionResponse(term, session, response);
    // Note: user_data is owned by the Net context and will be freed when the context is destroyed/reset.
}

// Callback for async port scan results
static void KTerm_PortScan_Callback(KTerm* term, KTermSession* session, const char* host, int port, int status, void* user_data) {
    if (!term || !session) return;
    char* id = (char*)user_data;
    if (!id) id = "0";

    char payload[256];
    const char* status_str = "CLOSED";
    if (status == 1) status_str = "OPEN";
    else if (status == 0) status_str = "TIMEOUT"; // Or Closed/Refused

    snprintf(payload, sizeof(payload), "HOST=%s;PORT=%d;STATUS=%s", host ? host : "*", port, status_str);

    // Format full Gateway response: ESC P GATE ; KTERM ; ID ; PORTSCAN ; payload ST
    char response[512];
    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;PORTSCAN;%s\x1B\\", id, payload);
    KTerm_QueueSessionResponse(term, session, response);
}

// Callback for async whois results
static void KTerm_Whois_Callback(KTerm* term, KTermSession* session, const char* data, size_t len, bool done, void* user_data) {
    if (!term || !session) return;
    char* id = (char*)user_data;
    if (!id) id = "0";

    if (data && len > 0) {
        // Escape newlines and semicolons
        char* buf = (char*)malloc(len * 2 + 1);
        if (buf) {
            size_t j = 0;
            for(size_t i=0; i<len; i++) {
                if (data[i] == '\n') { buf[j++] = '|'; }
                else if (data[i] == '\r') {}
                else if (data[i] == ';') { buf[j++] = ':'; }
                else buf[j++] = data[i];
            }
            buf[j] = '\0';

            // Limit chunk size
            if (strlen(buf) > 1000) buf[1000] = '\0';

            char response[1500];
            snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;WHOIS;DATA;%s\x1B\\", id, buf);
            KTerm_QueueSessionResponse(term, session, response);
            free(buf);
        }
    }

    if (done) {
        char response[128];
        snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;WHOIS;DONE\x1B\\", id);
        KTerm_QueueSessionResponse(term, session, response);
    }
}

static void KTerm_Speedtest_Callback(KTerm* term, KTermSession* session, const SpeedtestResult* result, void* user_data) {
    if (!term || !session || !result) return;
    char* id = (char*)user_data;
    if (!id) id = "0";

    char payload[256];
    if (result->done) {
        // Final Result
        snprintf(payload, sizeof(payload), "RESULT;DL=%.2f;UL=%.2f;JITTER=%.2f",
                 result->dl_mbps, result->ul_mbps, result->jitter_ms);
    } else {
        // Progress
        if (result->phase == 1) { // DL
             snprintf(payload, sizeof(payload), "PROGRESS;PHASE=DL;VAL=%.2f;PCT=%.2f", result->dl_mbps, result->dl_progress);
        } else if (result->phase == 2) { // UL
             snprintf(payload, sizeof(payload), "PROGRESS;PHASE=UL;VAL=%.2f;PCT=%.2f", result->ul_mbps, result->ul_progress);
        } else {
             snprintf(payload, sizeof(payload), "PROGRESS;PHASE=INIT");
        }
    }

    char response[512];
    snprintf(response, sizeof(response), "\x1BPGATE;KTERM;%s;SPEEDTEST;%s\x1B\\", id, payload);
    KTerm_QueueSessionResponse(term, session, response);
}

static void KTerm_Ext_SSH(KTerm* term, KTermSession* session, const char* id, const char* args, GatewayResponseCallback respond) {
#ifdef KTERM_DISABLE_NET
    if (respond) respond(term, session, "ERR;NET_DISABLED");
#else
    if (!args) return;

    char buffer[512];
    strncpy(buffer, args, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';

    char* cmd = strtok(buffer, ";");
    if (!cmd) return;

    if (strcmp(cmd, "connect") == 0) {
        char* target = strtok(NULL, ";");
        char* password = strtok(NULL, ";"); // Optional Password

        if (target) {
            char user[64] = "root";
            char host[256] = {0};
            int port = 22;

            // Parse user@host:port
            char* at = strchr(target, '@');

            if (at) {
                *at = '\0';
                strncpy(user, target, sizeof(user)-1);
                target = at + 1; // Host part starts after @

                // Extract password from user:pass
                char* user_pass_sep = strchr(user, ':');
                if (user_pass_sep) {
                    *user_pass_sep = '\0';
                    password = user_pass_sep + 1;
                }
            }

            // Parse host:port (handle [ipv6]:port by finding last colon)
            char* colon = strrchr(target, ':');
            if (colon) {
                // If using brackets, ensure colon is outside
                char* bracket_close = strrchr(target, ']');
                if (bracket_close && colon < bracket_close) {
                    // Colon is inside brackets (e.g. [::1]), no port
                    colon = NULL;
                }
            }

            if (colon) {
                *colon = '\0';
                port = atoi(colon + 1);
            }

            strncpy(host, target, sizeof(host)-1);

            #if KTERM_ENABLE_DEBUG_OUTPUT
            fprintf(stderr, "[Gateway] SSH Connect: User='%s' Host='%s' Port=%d\n", user, host, port);
            #endif

            KTerm_Net_Connect(term, session, host, port, user, password);
            if (respond) respond(term, session, "OK;CONNECTING");
        } else {
            if (respond) respond(term, session, "ERR;MISSING_TARGET");
        }
    } else if (strcmp(cmd, "disconnect") == 0) {
        KTerm_Net_Disconnect(term, session);
        if (respond) respond(term, session, "OK;DISCONNECTED");
    } else if (strcmp(cmd, "status") == 0) {
        char status[64];
        KTerm_Net_GetStatus(term, session, status, sizeof(status));
        if (respond) {
            char msg[128];
            snprintf(msg, sizeof(msg), "OK;%s", status);
            respond(term, session, msg);
        }
    } else if (strcmp(cmd, "ping") == 0) {
        char* host = strtok(NULL, ";");
        if (host) {
             char output[1024];
             KTerm_Net_Ping(host, output, sizeof(output));
             // Sanitize output (replace newlines with | to fit in single response frame)
             for(int i=0; output[i]; i++) {
                 if(output[i] == '\n' || output[i] == '\r') output[i] = '|';
             }
             if (respond) respond(term, session, output);
        } else {
             if (respond) respond(term, session, "ERR;MISSING_HOST");
        }
    } else if (strcmp(cmd, "responsetime") == 0) {
        char* host = NULL;
        int count = 10;
        int interval_sec = 1;
        int timeout_ms = 2000;

        char* arg = strtok(NULL, ";");
        while(arg) {
            if (strncmp(arg, "host=", 5) == 0) host = arg + 5;
            else if (strncmp(arg, "count=", 6) == 0) count = atoi(arg+6);
            else if (strncmp(arg, "interval=", 9) == 0) interval_sec = atoi(arg+9);
            else if (strncmp(arg, "timeout=", 8) == 0) timeout_ms = atoi(arg+8);
            else if (!host) host = arg; // Fallback positional if not keyed
            arg = strtok(NULL, ";");
        }

        if (host) {
             // Pass ID as user_data (must duplicate it as it needs to persist)
             // KTerm_Net_ResponseTime will free it in DestroyContext
             char* id_copy = (char*)malloc(strlen(id) + 1);
             if (id_copy) {
                 strcpy(id_copy, id);
                 // Convert interval seconds to ms
                 if (KTerm_Net_ResponseTime(term, session, host, count, interval_sec * 1000, timeout_ms, KTerm_ResponseTime_Callback, id_copy)) {
                     if (respond) respond(term, session, "OK;STARTED");
                 } else {
                     // Initialization failed synchronously; ownership of id_copy was not transferred.
                     if (respond) respond(term, session, "ERR;INIT_FAILED");
                     free(id_copy);
                 }
             }
        } else {
             if (respond) respond(term, session, "ERR;MISSING_HOST");
        }
    } else if (strcmp(cmd, "myip") == 0) {
        char ip[64];
        KTerm_Net_GetLocalIP(ip, sizeof(ip));
        if (respond) respond(term, session, ip);
    } else if (strcmp(cmd, "traceroute") == 0) {
        char* host = NULL;
        int max_hops = 30;
        int timeout_ms = 2000;

        char* arg = strtok(NULL, ";");
        while(arg) {
            if (strncmp(arg, "host=", 5) == 0) host = arg + 5;
            else if (strncmp(arg, "maxhops=", 8) == 0) max_hops = atoi(arg+8);
            else if (strncmp(arg, "timeout=", 8) == 0) timeout_ms = atoi(arg+8);
            else if (!host) host = arg; // Fallback positional if not keyed
            arg = strtok(NULL, ";");
        }

        if (host) {
             // Pass ID as user_data (must duplicate it as it needs to persist)
             // KTerm_Net_Traceroute will free it in DestroyContext
             char* id_copy = (char*)malloc(strlen(id) + 1);
             if (id_copy) {
                 strcpy(id_copy, id);
                 KTerm_Net_Traceroute(term, session, host, max_hops, timeout_ms, KTerm_Traceroute_Callback, id_copy);
                 if (respond) respond(term, session, "OK;STARTED");
             }
        } else {
             if (respond) respond(term, session, "ERR;MISSING_HOST");
        }
    } else if (strcmp(cmd, "dns") == 0) {
        char* host = strtok(NULL, ";");
        if (host) {
            char ip[64];
            if (KTerm_Net_Resolve(host, ip, sizeof(ip))) {
                char msg[128];
                snprintf(msg, sizeof(msg), "OK;IP=%s", ip);
                if (respond) respond(term, session, msg);
            } else {
                if (respond) respond(term, session, "ERR;RESOLVE_FAILED");
            }
        } else {
            if (respond) respond(term, session, "ERR;MISSING_HOST");
        }
    } else if (strcmp(cmd, "portscan") == 0) {
        char* host = NULL;
        char* ports = NULL;
        int timeout_ms = 1000;

        char* arg = strtok(NULL, ";");
        while(arg) {
            if (strncmp(arg, "host=", 5) == 0) host = arg + 5;
            else if (strncmp(arg, "ports=", 6) == 0) ports = arg + 6;
            else if (strncmp(arg, "timeout=", 8) == 0) timeout_ms = atoi(arg+8);
            else if (!host) host = arg; // 1st Positional
            else if (!ports) ports = arg; // 2nd Positional
            arg = strtok(NULL, ";");
        }

        if (host && ports) {
             char* id_copy = (char*)malloc(strlen(id) + 1);
             if (id_copy) {
                 strcpy(id_copy, id);
                 if (KTerm_Net_PortScan(term, session, host, ports, timeout_ms, KTerm_PortScan_Callback, id_copy)) {
                     if (respond) respond(term, session, "OK;STARTED");
                 } else {
                     if (respond) respond(term, session, "ERR;START_FAILED");
                     free(id_copy);
                 }
             }
        } else {
             if (respond) respond(term, session, "ERR;MISSING_ARGS");
        }
    } else if (strcmp(cmd, "whois") == 0) {
        char* host = strtok(NULL, ";");
        if (host) {
             char* id_copy = (char*)malloc(strlen(id) + 1);
             if (id_copy) {
                 strcpy(id_copy, id);
                 // Default query is same as host
                 if (KTerm_Net_Whois(term, session, host, host, KTerm_Whois_Callback, id_copy)) {
                     if (respond) respond(term, session, "OK;STARTED");
                 } else {
                     if (respond) respond(term, session, "ERR;START_FAILED");
                     free(id_copy);
                 }
             }
        } else {
             if (respond) respond(term, session, "ERR;MISSING_HOST");
        }
    } else if (strcmp(cmd, "speedtest") == 0) {
        char* host = NULL;
        int port = 80;
        int streams = 4;
        char* path = NULL;

        char* arg = strtok(NULL, ";");
        while(arg) {
            if (strncmp(arg, "host=", 5) == 0) host = arg + 5;
            else if (strncmp(arg, "port=", 5) == 0) port = atoi(arg+5);
            else if (strncmp(arg, "streams=", 8) == 0) streams = atoi(arg+8);
            else if (strncmp(arg, "path=", 5) == 0) path = arg + 5;
            else if (!host) host = arg; // 1st Positional
            arg = strtok(NULL, ";");
        }

        // Default host: NULL implies auto-select in KTerm_Net_Speedtest

        char* id_copy = (char*)malloc(strlen(id) + 1);
        if (id_copy) {
            strcpy(id_copy, id);
            if (KTerm_Net_Speedtest(term, session, host, port, streams, path, KTerm_Speedtest_Callback, id_copy)) {
                if (respond) respond(term, session, "OK;STARTED");
            } else {
                if (respond) respond(term, session, "ERR;START_FAILED");
                free(id_copy);
            }
        }
    } else if (strcmp(cmd, "connections") == 0) {
        char list[1024];
        list[0] = '\0';
        for(int i=0; i<MAX_SESSIONS; i++) {
            char status[256];
            KTerm_Net_GetStatus(term, &term->sessions[i], status, sizeof(status));
            char entry[300];
            snprintf(entry, sizeof(entry), "[%d]:%s|", i, status);
            if (strlen(list) + strlen(entry) < sizeof(list) - 1) {
                strcat(list, entry);
            }
        }
        if (respond) {
            char msg[1200];
            snprintf(msg, sizeof(msg), "OK;%s", list);
            respond(term, session, msg);
        }
    } else {
        if (respond) respond(term, session, "ERR;UNKNOWN_CMD");
    }
#endif
}

static void KTerm_Ext_Net(KTerm* term, KTermSession* session, const char* id, const char* args, GatewayResponseCallback respond) {
    KTerm_Ext_SSH(term, session, id, args, respond);
}

static void KTerm_Ext_RawDump(KTerm* term, KTermSession* session, const char* id, const char* args, GatewayResponseCallback respond) {
    if (!args) return;
    (void)id;

    char buffer[256];
    strncpy(buffer, args, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';

    bool start = false;
    bool stop = false;
    bool toggle = false;
    int target_id = -1;
    int force_wob = -1;

    char* token = strtok(buffer, ";");
    while (token) {
        if (strcmp(token, "START") == 0) start = true;
        else if (strcmp(token, "STOP") == 0) stop = true;
        else if (strcmp(token, "TOGGLE") == 0) toggle = true;
        else if (strncmp(token, "SESSION=", 8) == 0) {
            target_id = atoi(token + 8);
        } else if (strncmp(token, "FORCE_WOB=", 10) == 0) {
            const char* v = token + 10;
            if (strcmp(v, "1") == 0 || KTerm_Strcasecmp(v, "TRUE") == 0 || KTerm_Strcasecmp(v, "ON") == 0) force_wob = 1;
            else force_wob = 0;
        }
        token = strtok(NULL, ";");
    }

    if (target_id == -1) {
        target_id = term->active_session;
    }

    if (toggle) {
        if (session->raw_dump.raw_dump_mirror_active) stop = true;
        else start = true;
    }

    if (start && stop) start = false;

    if (stop) {
        session->raw_dump.raw_dump_mirror_active = false;
        if (respond) respond(term, session, "STOPPED");
    } else if (start) {
        session->raw_dump.raw_dump_mirror_active = true;
        session->raw_dump.raw_dump_target_session_id = target_id;
        if (force_wob != -1) session->raw_dump.raw_dump_force_wob = (force_wob == 1);
        session->raw_dump.initialized = false;

        char msg[64];
        snprintf(msg, sizeof(msg), "ACTIVE;SESSION=%d", target_id);
        if (respond) respond(term, session, msg);
    } else {
        if (target_id != -1 && session->raw_dump.raw_dump_mirror_active) session->raw_dump.raw_dump_target_session_id = target_id;
        if (force_wob != -1) session->raw_dump.raw_dump_force_wob = (force_wob == 1);
        if (respond) respond(term, session, "UPDATED");
    }
}

static int KTerm_ParseGridCoord(KTerm* term, KTermSession* s, const char* str, int base_val) {
    if (!str || !*str) return 0;

    // Explicit relative sign ('+' or '-') triggers relative positioning.
    bool is_relative = (str[0] == '+' || str[0] == '-');
    int val = atoi(str);

    if (is_relative) {
        return base_val + val;
    }

    // Strict Mode Clamping: Negative absolute coordinates are clamped to 0
    if (term->config.strict_mode && val < 0) {
        return 0;
    }

    return val;
}

static bool KTerm_ParseGridColor(const char* str, ExtendedKTermColor* out) {
    if (!str || !out) return false;
    
    if (strncmp(str, "rgb:", 4) == 0) {
        RGB_KTermColor rgb;
        char buf[32];
        // Ensure # prefix for KTerm_ParseColor
        snprintf(buf, sizeof(buf), "#%s", str + 4);
        if (KTerm_ParseColor(buf, &rgb)) {
             out->color_mode = 1;
             out->value.rgb = rgb;
             return true;
        }
        // Try without # if it's already comma separated
        if (KTerm_ParseColor(str + 4, &rgb)) {
             out->color_mode = 1;
             out->value.rgb = rgb;
             return true;
        }
    } else if (strncmp(str, "pal:", 4) == 0) {
        int idx = atoi(str + 4);
        if (idx >= 0 && idx <= 255) {
            out->color_mode = 0;
            out->value.index = idx;
            return true;
        }
    } else if (strcmp(str, "def") == 0 || strcmp(str, "default") == 0) {
        out->color_mode = 2; // Default
        return true;
    }
    return false;
}

// Helper for Grid Extension
typedef struct {
    uint32_t mask;
    unsigned int ch;
    ExtendedKTermColor fg;
    ExtendedKTermColor bg;
    ExtendedKTermColor ul;
    ExtendedKTermColor st;
    uint32_t flags;
} GridStyle;

static int KTerm_QueueGridOp(KTermSession* s, int x, int y, int w, int h, const GridStyle* style) {
    if (w <= 0 || h <= 0) return 0;

    // Clip against session bounds
    if (x >= s->cols || y >= s->rows) return 0;
    if (x + w <= 0 || y + h <= 0) return 0;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > s->cols) w = s->cols - x;
    if (y + h > s->rows) h = s->rows - y;

    if (w <= 0 || h <= 0) return 0;

    KTermOp op;
    op.type = KTERM_OP_FILL_RECT_MASKED;
    op.u.fill_masked.rect.x = x;
    op.u.fill_masked.rect.y = y;
    op.u.fill_masked.rect.w = w;
    op.u.fill_masked.rect.h = h;
    op.u.fill_masked.mask = style->mask;
    op.u.fill_masked.fill_char.ch = style->ch;
    op.u.fill_masked.fill_char.fg_color = style->fg;
    op.u.fill_masked.fill_char.bg_color = style->bg;
    op.u.fill_masked.fill_char.ul_color = style->ul;
    op.u.fill_masked.fill_char.st_color = style->st;
    op.u.fill_masked.fill_char.flags = style->flags;

    KTerm_QueueOp(&s->op_queue, op);
    return w * h;
}

static int KTerm_Grid_FillCircle(KTermSession* s, int cx, int cy, int radius, const GridStyle* style) {
    if (radius < 0) return 0;
    int x = radius;
    int y = 0;
    int err = 0;
    int count = 0;

    while (x >= y) {
        // Draw horizontal spans
        count += KTerm_QueueGridOp(s, cx - x, cy + y, 2 * x + 1, 1, style);
        if (y != 0) {
            count += KTerm_QueueGridOp(s, cx - x, cy - y, 2 * x + 1, 1, style);
        }

        count += KTerm_QueueGridOp(s, cx - y, cy + x, 2 * y + 1, 1, style);
        if (x != 0) {
             count += KTerm_QueueGridOp(s, cx - y, cy - x, 2 * y + 1, 1, style);
        }

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
    return count;
}

static int KTerm_Grid_FillSpan(KTermSession* s, int sx, int sy, char dir, int len, bool wrap, const GridStyle* style) {
    if (len <= 0) return 0;
    int count = 0;

    if (dir == 'h' || dir == '0') {
        int x = sx;
        int y = sy;
        int remaining = len;

        while (remaining > 0) {
            if (y >= s->rows || y < 0) break;

            int w = remaining;
            if (wrap) {
                if (x + w > s->cols) {
                    w = s->cols - x;
                }
            }

            if (w < 0) w = 0; // Fix logic bug: x can be > cols

            count += KTerm_QueueGridOp(s, x, y, w, 1, style);

            remaining -= w;
            if (remaining > 0) {
                if (wrap) {
                    x = 0;
                    y++;
                } else {
                    break;
                }
            } else {
                if (w == 0) break; // Avoid infinite loop
            }
        }
    } else if (dir == 'v' || dir == '1') {
        count += KTerm_QueueGridOp(s, sx, sy, 1, len, style);
    } else if (dir == 'l' || dir == '2') {
        count += KTerm_QueueGridOp(s, sx - len + 1, sy, len, 1, style);
    } else if (dir == 'u' || dir == '3') {
        count += KTerm_QueueGridOp(s, sx, sy - len + 1, 1, len, style);
    }
    return count;
}

static int KTerm_Grid_Banner(KTermSession* s, int x, int y, const char* text, int scale, const GridStyle* style, char** opts, int opt_count) {
    if (!text || scale <= 0) return 0;
    int count = 0;

    int align = 0; // 0=Left, 1=Center, 2=Right
    bool use_kerning = false;
    // Simple options parsing
    for (int i = 0; i < opt_count; i++) {
        if (opts[i]) {
            if (strncmp(opts[i], "align=", 6) == 0) {
                if (strcmp(opts[i]+6, "center") == 0) align = 1;
                else if (strcmp(opts[i]+6, "right") == 0) align = 2;
            } else if (strncmp(opts[i], "kern=", 5) == 0) {
                if (opts[i][5] == '1') use_kerning = true;
            }
        }
    }

    // Font selection: fallback to ibm_font_8x8
    const uint8_t* font = ibm_font_8x8;
    int font_w = 8;
    int font_h = 8;

    int cur_y = y;
    const char* p = text;

    // Iterate line by line for proper alignment
    while (*p) {
        // Find line end
        const char* end = p;
        int line_width = 0;

        while (*end) {
            if (*end == '\n') break;
            if (*end == '\\' && *(end+1) == 'n') break;

            if (use_kerning) {
                int char_w = 0;
                unsigned char c = (unsigned char)*end;
                if (c == ' ') {
                    char_w = 4; // Kerned space width
                } else {
                    int max_col = -1;
                    for (int r = 0; r < font_h; r++) {
                        uint8_t bits = font[c * font_h + r];
                        for (int col = 0; col < 8; col++) {
                            if (bits & (1 << (7 - col))) {
                                if (col > max_col) max_col = col;
                            }
                        }
                    }
                    char_w = (max_col + 1) + 1; // +1 padding
                    if (char_w < 4) char_w = 4; // Min width
                }
                line_width += char_w * scale;
            } else {
                line_width += font_w * scale;
            }
            end++;
        }

        // Calculate X for this line based on alignment
        int cur_x = x;
        if (align == 1) cur_x = x - line_width / 2;
        else if (align == 2) cur_x = x - line_width;

        // Draw Characters in Line
        const char* q = p;
        while (q < end) {
            unsigned char c = (unsigned char)*q;
            int advance = font_w;

            if (use_kerning) {
                if (c == ' ') {
                    advance = 4;
                } else {
                    int max_col = -1;
                    for (int r = 0; r < font_h; r++) {
                        uint8_t bits = font[c * font_h + r];
                        for (int col = 0; col < 8; col++) {
                            if (bits & (1 << (7 - col))) {
                                if (col > max_col) max_col = col;
                            }
                        }
                    }
                    advance = (max_col + 1) + 1;
                    if (advance < 4) advance = 4;
                }
            }

            for (int row = 0; row < font_h; row++) {
                uint8_t bits = font[c * font_h + row];
                for (int col = 0; col < font_w; col++) {
                    if (bits & (1 << (7 - col))) {
                        count += KTerm_QueueGridOp(s, cur_x + col * scale, cur_y + row * scale, scale, scale, style);
                    }
                }
            }
            cur_x += advance * scale;
            q++;
        }

        // Advance to next line
        cur_y += font_h * scale;
        p = end;
        if (*p == '\n') p++;
        else if (*p == '\\' && *(p+1) == 'n') p += 2;
    }
    return count;
}

static void KTerm_Ext_Grid(KTerm* term, KTermSession* session, const char* id, const char* args, GatewayResponseCallback respond) {
    if (!args) return;
    (void)id;
    
    char buffer[2048]; // Increased buffer size for banners
    strncpy(buffer, args, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';
    
    char* tokens[32]; // Increased token limit
    int count = 0;
    char* ptr = buffer;
    char* token_start = buffer;

    // Custom Tokenizer to handle empty fields (;;)
    while (*ptr && count < 32) {
        if (*ptr == ';') {
            *ptr = '\0';
            tokens[count++] = token_start;
            token_start = ptr + 1;
        }
        ptr++;
    }
    // Last token
    if (count < 32) tokens[count++] = token_start;
    
    if (count == 0) return;

    // Resolve Target Session First
    KTermSession* target = session;
    if (count > 1 && tokens[1][0] != '\0') {
        int s_id = atoi(tokens[1]);
        if (s_id >= 0 && s_id < MAX_SESSIONS) target = &term->sessions[s_id];
    } else if (term->gateway_target_session >= 0) {
        target = &term->sessions[term->gateway_target_session];
    }

    // Initialize Style with Session Defaults (for optional params)
    GridStyle style;
    style.mask = 0;
    style.ch = 0;
    style.fg = target->current_fg;
    style.bg = target->current_bg;
    style.ul = target->current_ul_color;
    style.st = target->current_st_color;
    style.flags = target->current_attributes;

    int style_idx = -1;
    
    if (strcmp(tokens[0], "fill") == 0) {
        // Minimal args check reduced to allow optional trailing args if needed,
        // but style_idx is fixed offset.
        // fill;sid;x;y;w;h;mask... (index 6)
        if (count < 7) {
            if (respond) respond(term, session, "ERR;MISSING_ARGS");
            return;
        }
        style_idx = 6;
    } else if (strcmp(tokens[0], "fill_circle") == 0) {
        // fill_circle;sid;cx;cy;r;mask... (index 5)
        if (count < 6) {
            if (respond) respond(term, session, "ERR;MISSING_ARGS");
            return;
        }
        style_idx = 5;
    } else if (strcmp(tokens[0], "fill_line") == 0 || strcmp(tokens[0], "fill_span") == 0) {
        // fill_line;sid;sx;sy;dir;len;mask... (index 6)
        if (count < 7) {
            if (respond) respond(term, session, "ERR;MISSING_ARGS");
            return;
        }
        style_idx = 6;
    } else if (strcmp(tokens[0], "banner") == 0) {
        // banner;sid;x;y;text;scale;mask... (index 6)
        if (count < 7) {
            if (respond) respond(term, session, "ERR;MISSING_ARGS");
            return;
        }
        style_idx = 6;
    } else if (strcmp(tokens[0], "stream") == 0) {
        // stream;sid;x;y;w;h;mask;count;compress;data
        if (count < 10) {
            if (respond) respond(term, session, "ERR;MISSING_ARGS");
            return;
        }
        // stream handles mask differently (packed data), so style_idx logic below is skipped/custom
        style_idx = -1;
    } else if (strcmp(tokens[0], "copy") == 0 || strcmp(tokens[0], "move") == 0) {
        // copy;sid;sx;sy;dx;dy;w;h;mode
        if (count < 9) {
            if (respond) respond(term, session, "ERR;MISSING_ARGS");
            return;
        }
        style_idx = -1;
    } else {
        if (respond) respond(term, session, "ERR;UNKNOWN_SUBCOMMAND");
        return;
    }

    // Parse Style (Optional Params) for standard fill commands
    if (style_idx != -1) {
        if (count > style_idx && tokens[style_idx][0] != '\0')
            style.mask = (uint32_t)strtoul(tokens[style_idx], NULL, 0);

        // If mask is 0 (or omitted), it's a no-op (safe fail)
        if (style.mask == 0) {
            if (respond) respond(term, session, "OK;NOOP;MASK_ZERO");
            return;
        }

        if (count > style_idx+1 && tokens[style_idx+1][0] != '\0')
            style.ch = (unsigned int)strtoul(tokens[style_idx+1], NULL, 0);

        if (count > style_idx+2 && tokens[style_idx+2][0] != '\0')
            KTerm_ParseGridColor(tokens[style_idx+2], &style.fg);

        if (count > style_idx+3 && tokens[style_idx+3][0] != '\0')
            KTerm_ParseGridColor(tokens[style_idx+3], &style.bg);

        if (count > style_idx+4 && tokens[style_idx+4][0] != '\0')
            KTerm_ParseGridColor(tokens[style_idx+4], &style.ul);

        if (count > style_idx+5 && tokens[style_idx+5][0] != '\0')
            KTerm_ParseGridColor(tokens[style_idx+5], &style.st);

        if (count > style_idx+6 && tokens[style_idx+6][0] != '\0')
            style.flags = KTerm_ParseAttributeString(tokens[style_idx+6]);
    }


    // Dispatch
    int cells_applied = 0;

    if (strcmp(tokens[0], "fill") == 0) {
        int x = KTerm_ParseGridCoord(term, target, tokens[2], target->cursor.x);
        int y = KTerm_ParseGridCoord(term, target, tokens[3], target->cursor.y);
        int w = atoi(tokens[4]);
        int h = atoi(tokens[5]);

        // Handle negative W/H (Mirror/Reverse)
        if (w < 0) { x += w; w = -w; }
        if (h < 0) { y += h; h = -h; }

        if (w == 0) w = 1; // Legacy behavior
        if (h == 0) h = 1; // Legacy behavior
        cells_applied = KTerm_QueueGridOp(target, x, y, w, h, &style);
    } else if (strcmp(tokens[0], "fill_circle") == 0) {
        int cx = KTerm_ParseGridCoord(term, target, tokens[2], target->cursor.x);
        int cy = KTerm_ParseGridCoord(term, target, tokens[3], target->cursor.y);
        int r = atoi(tokens[4]);
        if (r < 0) r = -r; // Absolute radius
        cells_applied = KTerm_Grid_FillCircle(target, cx, cy, r, &style);
    } else if (strcmp(tokens[0], "fill_line") == 0 || strcmp(tokens[0], "fill_span") == 0) {
        int sx = KTerm_ParseGridCoord(term, target, tokens[2], target->cursor.x);
        int sy = KTerm_ParseGridCoord(term, target, tokens[3], target->cursor.y);
        char dir = tokens[4][0];
        int len = atoi(tokens[5]);

        // Handle negative length by reversing direction
        if (len < 0) {
            len = -len;
            // Toggle direction: Right <-> Left, Down <-> Up
            if (dir == 'h' || dir == '0') dir = 'l';
            else if (dir == 'l' || dir == '2') dir = 'h';
            else if (dir == 'v' || dir == '1') dir = 'u';
            else if (dir == 'u' || dir == '3') dir = 'v';
        }

        bool wrap = false;
        if (count > 13) {
            wrap = (atoi(tokens[13]) != 0);
        }
        cells_applied = KTerm_Grid_FillSpan(target, sx, sy, dir, len, wrap, &style);
    } else if (strcmp(tokens[0], "banner") == 0) {
        int x = KTerm_ParseGridCoord(term, target, tokens[2], target->cursor.x);
        int y = KTerm_ParseGridCoord(term, target, tokens[3], target->cursor.y);
        const char* text = tokens[4];
        int scale = atoi(tokens[5]);

        // Opts start at 13 (index 13, 14...)
        char** opts = NULL;
        int opt_count = 0;
        if (count > 13) {
            opts = &tokens[13];
            opt_count = count - 13;
        }
        cells_applied = KTerm_Grid_Banner(target, x, y, text, scale, &style, opts, opt_count);
    } else if (strcmp(tokens[0], "copy") == 0 || strcmp(tokens[0], "move") == 0) {
        // copy;sid;src_x;src_y;dst_x;dst_y;w;h;mode
        int sx = KTerm_ParseGridCoord(term, target, tokens[2], target->cursor.x);
        int sy = KTerm_ParseGridCoord(term, target, tokens[3], target->cursor.y);
        int dx = KTerm_ParseGridCoord(term, target, tokens[4], target->cursor.x);
        int dy = KTerm_ParseGridCoord(term, target, tokens[5], target->cursor.y);
        int w = atoi(tokens[6]);
        int h = atoi(tokens[7]);
        uint32_t mode = (uint32_t)strtoul(tokens[8], NULL, 0);

        if (strcmp(tokens[0], "move") == 0) {
            mode |= 0x2; // Set Clear Source
        }

        KTermRect src = {sx, sy, w, h};
        KTerm_QueueCopyRectWithMode(target, src, dx, dy, mode);
        cells_applied = w * h;
    } else if (strcmp(tokens[0], "stream") == 0) {
        // stream;sid;x;y;w;h;mask;count;compress;data
        int x = KTerm_ParseGridCoord(term, target, tokens[2], target->cursor.x);
        int y = KTerm_ParseGridCoord(term, target, tokens[3], target->cursor.y);
        int w = atoi(tokens[4]);
        int h = atoi(tokens[5]);
        uint32_t mask = (uint32_t)strtoul(tokens[6], NULL, 0);
        int count_cells = atoi(tokens[7]);
        int compress = atoi(tokens[8]);
        const char* b64_data = tokens[9];

        // Handle negative W/H (Mirroring logic consistent with fill)
        if (w < 0) { x += w; w = -w; }
        if (h < 0) { y += h; h = -h; }
        if (w == 0) w = 1;
        if (h == 0) h = 1;

        if (compress != 0) {
            if (respond) respond(term, session, "ERR;COMPRESSION_NOT_SUPPORTED");
            return;
        }

        // Flush pending ops to ensure we read up-to-date screen state
        // This prevents race conditions where a preceding operation (e.g. fill)
        // hasn't been applied to the screen buffer yet.
        KTerm_FlushOps(term, target);

        size_t b64_len = strlen(b64_data);
        size_t buffer_size = (b64_len * 3) / 4 + 4; // Approx size
        unsigned char* buffer = (unsigned char*)malloc(buffer_size);
        if (!buffer) {
            if (respond) respond(term, session, "ERR;OOM");
            return;
        }

        size_t data_len = KTerm_Base64DecodeBuffer(b64_data, buffer, buffer_size);
        unsigned char* ptr = buffer;
        unsigned char* end = buffer + data_len;

        for (int i = 0; i < count_cells; i++) {
            if (ptr >= end) break; // Unexpected end of data

            int cx = x + (i % w);
            int cy = y + (i / w);

            // Fetch current cell to modify partially
            EnhancedTermChar cell = {0};

            EnhancedTermChar* existing = GetActiveScreenCell(target, cy, cx);
            if (existing) {
                cell = *existing; // Copy current state
            } else {
                // Out of bounds or invalid?
                // If out of bounds, skip
                if (cx >= target->cols || cy >= target->rows) continue;
            }

            if (mask & GRID_MASK_CH) {
                if (ptr + 4 <= end) {
                    uint32_t val = (uint32_t)ptr[0] | ((uint32_t)ptr[1] << 8) | ((uint32_t)ptr[2] << 16) | ((uint32_t)ptr[3] << 24);
                    cell.ch = val;
                    ptr += 4;
                }
            }

            // Helper to read color
            // Format: 1 byte Mode + Data
            // Mode 0: Index (1 byte)
            // Mode 1: RGB (3 bytes)
            // Mode 2: Default (0 bytes)

            #define READ_COLOR_FROM_STREAM(c) do { \
                if (ptr + 1 > end) break; \
                int mode = *ptr++; \
                (c)->color_mode = mode; \
                if (mode == 0) { \
                    if (ptr + 1 <= end) { \
                        (c)->value.index = *ptr++; \
                    } \
                } else if (mode == 1) { \
                    if (ptr + 3 <= end) { \
                        (c)->value.rgb.r = ptr[0]; \
                        (c)->value.rgb.g = ptr[1]; \
                        (c)->value.rgb.b = ptr[2]; \
                        (c)->value.rgb.a = 255; \
                        ptr += 3; \
                    } \
                } \
            } while(0)

            if (mask & GRID_MASK_FG) READ_COLOR_FROM_STREAM(&cell.fg_color);
            if (mask & GRID_MASK_BG) READ_COLOR_FROM_STREAM(&cell.bg_color);
            if (mask & GRID_MASK_UL) READ_COLOR_FROM_STREAM(&cell.ul_color);
            if (mask & GRID_MASK_ST) READ_COLOR_FROM_STREAM(&cell.st_color);

            #undef READ_COLOR_FROM_STREAM

            if (mask & GRID_MASK_FLAGS) {
                if (ptr + 4 <= end) {
                    uint32_t val = (uint32_t)ptr[0] | ((uint32_t)ptr[1] << 8) | ((uint32_t)ptr[2] << 16) | ((uint32_t)ptr[3] << 24);
                    cell.flags = val;
                    ptr += 4;
                }
            }

            // Queue Op
            KTermOp op;
            op.type = KTERM_OP_SET_CELL;
            op.u.set_cell.x = cx;
            op.u.set_cell.y = cy;
            op.u.set_cell.cell = cell;
            // Ensure dirty flag is set
            op.u.set_cell.cell.flags |= KTERM_FLAG_DIRTY;
            KTerm_QueueOp(&target->op_queue, op);

            cells_applied++;
        }

        free(buffer);
    }

    if (respond) {
        char msg[64];
        snprintf(msg, sizeof(msg), "OK;QUEUED;%d", cells_applied);
        respond(term, session, msg);
    }
}

void KTerm_RegisterBuiltinExtensions(KTerm* term) {
    KTerm_RegisterGatewayExtension(term, "broadcast", KTerm_Ext_Broadcast);
    KTerm_RegisterGatewayExtension(term, "themes", KTerm_Ext_Themes);
    KTerm_RegisterGatewayExtension(term, "clipboard", KTerm_Ext_Clipboard);
    KTerm_RegisterGatewayExtension(term, "icat", KTerm_Ext_Icat);
    KTerm_RegisterGatewayExtension(term, "direct", KTerm_Ext_DirectInput);
    KTerm_RegisterGatewayExtension(term, "rawdump", KTerm_Ext_RawDump);
    KTerm_RegisterGatewayExtension(term, "grid", KTerm_Ext_Grid);
    KTerm_RegisterGatewayExtension(term, "ssh", KTerm_Ext_SSH);
    KTerm_RegisterGatewayExtension(term, "net", KTerm_Ext_Net);
}

void KTerm_GatewayProcess(KTerm* term, KTermSession* session, const char* class_id, const char* id, const char* command, const char* params) {
    printf("GatewayProcess: %s %s %s\n", class_id, id, command);
    // Input Hardening
    if (!term || !session || !class_id || !id || !command) return;
    if (!params) params = "";

    // Check Class ID
    if (strcmp(class_id, "KTERM") != 0) {
        goto call_user_callback;
    }

    const GatewayCommand* cmd = (const GatewayCommand*)bsearch(command, gateway_commands, sizeof(gateway_commands)/sizeof(GatewayCommand), sizeof(GatewayCommand), GatewayCommandCmp);

    if (cmd) {
        StreamScanner scanner = { .ptr = params, .len = strlen(params), .pos = 0 };
        cmd->handler(term, session, id, &scanner);
        return;
    }

call_user_callback:
    if (term->gateway_callback) {
        term->gateway_callback(term, class_id, id, command, params);
    } else {
        KTerm_ReportError(term, KTERM_LOG_WARNING, KTERM_SOURCE_API, "Unknown Gateway Command: Class=%s ID=%s Cmd=%s", class_id, id, command);
        // Also log via legacy method if needed, or rely on ReportError's fallback
        // KTerm_LogUnsupportedSequence(term, "Unknown Gateway Command");
    }
}

#endif // KTERM_GATEWAY_IMPLEMENTATION_GUARD
#endif // KTERM_GATEWAY_IMPLEMENTATION
