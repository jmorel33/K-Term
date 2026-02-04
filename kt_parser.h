#ifndef KT_PARSER_H
#define KT_PARSER_H

#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

typedef enum {
    KT_TOK_EOF = 0,
    KT_TOK_ERROR,
    KT_TOK_IDENTIFIER,  // e.g., CREATE, TYPE, X, Y
    KT_TOK_STRING,      // e.g., "Hello World"
    KT_TOK_NUMBER,      // e.g., 10, -5.5, 0xFF
    KT_TOK_EQUALS,      // =
    KT_TOK_SEMICOLON,   // ;
    KT_TOK_COMMA,       // ,
    KT_TOK_UNKNOWN      // ?
} KTermTokenType;

typedef struct {
    KTermTokenType type;
    const char* start;  // Pointer into the source string (zero-copy)
    int length;
    struct {
        int i;
        float f;
    } value;            // Pre-parsed numeric value
} KTermToken;

typedef struct {
    const char* src;
    size_t pos;
} KTermLexer;

// Initialize the lexer
static inline void KTerm_LexerInit(KTermLexer* lexer, const char* input) {
    lexer->src = input;
    lexer->pos = 0;
}

// Get the next token
static inline KTermToken KTerm_LexerNext(KTermLexer* lexer) {
    KTermToken token = {0};
    const char* p = lexer->src + lexer->pos;

    // 1. Skip Whitespace
    while (*p && isspace((unsigned char)*p)) p++;

    token.start = p;

    // 2. Check End of String
    if (*p == '\0') {
        token.type = KT_TOK_EOF;
        lexer->pos = p - lexer->src;
        return token;
    }

    // 3. Operators
    if (*p == '=') { token.type = KT_TOK_EQUALS;    token.length = 1; p++; }
    else if (*p == ';') { token.type = KT_TOK_SEMICOLON; token.length = 1; p++; }
    else if (*p == ',') { token.type = KT_TOK_COMMA;     token.length = 1; p++; }

    // 4. Strings ("..." or '...')
    else if (*p == '"' || *p == '\'') {
        char quote = *p++; // Remember quote type and skip it
        token.start = p;   // Content starts after quote
        token.type = KT_TOK_STRING;
        while (*p && *p != quote) {
            if (*p == '\\' && *(p+1)) p++; // Skip escape char so next char is treated as content
            p++;
        }
        token.length = (int)(p - token.start);
        if (*p == quote) {
            p++; // Skip closing quote
        } else {
            // Unterminated string
            token.type = KT_TOK_ERROR;
        }
    }

    // 5. Numbers (Integer, Float, Hex)
    else if (isdigit((unsigned char)*p) || (*p == '-' && isdigit((unsigned char)*(p+1)))) {
        token.type = KT_TOK_NUMBER;
        char* endptr;
        // Check for Hex
        if (*p == '0' && (*(p+1) == 'x' || *(p+1) == 'X')) {
            token.value.i = (int)strtol(p, &endptr, 16);
            token.value.f = (float)token.value.i;
        } else {
            token.value.f = strtof(p, &endptr);
            token.value.i = (int)token.value.f;
        }
        token.length = (int)(endptr - p);
        p = endptr;
    }

    // 6. Identifiers (Alpha + _ + # for colors)
    else if (isalpha((unsigned char)*p) || *p == '_' || *p == '#') {
        token.type = KT_TOK_IDENTIFIER;
        while (isalnum((unsigned char)*p) || *p == '_' || *p == '#') p++;
        token.length = (int)(p - token.start);
    }

    // 7. Unknown
    else {
        token.type = KT_TOK_UNKNOWN;
        token.length = 1;
        p++;
    }

    lexer->pos = p - lexer->src;
    return token;
}

// Helper to check if identifier matches string
static inline bool KTerm_TokenIs(KTermToken t, const char* str) {
    if (t.type != KT_TOK_IDENTIFIER) return false;
    if (strlen(str) != (size_t)t.length) return false;
    return strncmp(t.start, str, t.length) == 0;
}

// Helper to unescape a string token content in-place or to a buffer
static inline void KTerm_UnescapeString(char* dest, const char* src, int length) {
    const char* end = src + length;
    while (src < end) {
        if (*src == '\\' && src + 1 < end) {
            src++; // Skip backslash
            char c = *src++;
            switch (c) {
                case 'n': *dest++ = '\n'; break;
                case 't': *dest++ = '\t'; break;
                case 'r': *dest++ = '\r'; break;
                case '\\': *dest++ = '\\'; break;
                case '"': *dest++ = '"'; break;
                case '\'': *dest++ = '\''; break;
                default: *dest++ = c; break; // Fallback: just the character
            }
        } else {
            *dest++ = *src++;
        }
    }
    *dest = '\0';
}

// =============================================================================
// STREAM SCANNER (Centralized)
// =============================================================================

typedef struct {
    const char* ptr;
    size_t len;
    size_t pos;
} StreamScanner;

// Helper: Peek next character
static inline char Stream_Peek(StreamScanner* scanner) {
    if (scanner->pos >= scanner->len) return 0;
    return scanner->ptr[scanner->pos];
}

// Helper: Consume next character
static inline char Stream_Consume(StreamScanner* scanner) {
    if (scanner->pos >= scanner->len) return 0;
    return scanner->ptr[scanner->pos++];
}

static inline void Stream_SkipWhitespace(StreamScanner* scanner) {
    while (scanner->pos < scanner->len && isspace((unsigned char)Stream_Peek(scanner))) {
        Stream_Consume(scanner);
    }
}

static inline bool Stream_Expect(StreamScanner* scanner, char expected) {
    Stream_SkipWhitespace(scanner);
    if (Stream_Peek(scanner) == expected) {
        Stream_Consume(scanner);
        return true;
    }
    return false;
}

static inline bool Stream_ReadInt(StreamScanner* scanner, int* out_val) {
    Stream_SkipWhitespace(scanner);
    if (scanner->pos >= scanner->len) return false;

    int sign = 1;
    char ch = Stream_Peek(scanner);
    if (ch == '-') {
        sign = -1;
        Stream_Consume(scanner);
    } else if (ch == '+') {
        Stream_Consume(scanner);
    }

    if (!isdigit((unsigned char)Stream_Peek(scanner))) return false;

    long long val = 0;
    while (scanner->pos < scanner->len && isdigit((unsigned char)Stream_Peek(scanner))) {
        int digit = Stream_Consume(scanner) - '0';

        // Check for overflow (assuming int is 32-bit typically, but using robust check)
        if (val > (INT_MAX - digit) / 10) {
            val = (sign == 1) ? INT_MAX : INT_MIN;
            // Consume remaining digits
            while (scanner->pos < scanner->len && isdigit((unsigned char)Stream_Peek(scanner))) {
                Stream_Consume(scanner);
            }
            *out_val = (int)val;
            return true;
        }
        val = val * 10 + digit;
    }
    *out_val = (int)(val * sign);
    return true;
}

static inline bool Stream_ReadHex(StreamScanner* scanner, unsigned int* out_val) {
    Stream_SkipWhitespace(scanner);
    if (scanner->pos >= scanner->len) return false;

    // Optional 0x prefix
    if (scanner->pos + 1 < scanner->len && Stream_Peek(scanner) == '0' &&
        (scanner->ptr[scanner->pos+1] == 'x' || scanner->ptr[scanner->pos+1] == 'X')) {
        Stream_Consume(scanner);
        Stream_Consume(scanner);
    }

    if (!isxdigit((unsigned char)Stream_Peek(scanner))) return false;

    unsigned int val = 0;
    while (scanner->pos < scanner->len && isxdigit((unsigned char)Stream_Peek(scanner))) {
        char c = Stream_Consume(scanner);
        int d = 0;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;

        // Check for overflow: val * 16 + d > UINT_MAX
        if (val > (UINT_MAX - d) / 16) {
             val = UINT_MAX;
             // Consume remaining
             while (scanner->pos < scanner->len && isxdigit((unsigned char)Stream_Peek(scanner))) Stream_Consume(scanner);
             *out_val = val;
             return true;
        }

        val = (val << 4) | d;
    }
    *out_val = val;
    return true;
}

static inline bool Stream_ReadFloat(StreamScanner* scanner, float* out_val) {
    Stream_SkipWhitespace(scanner);
    if (scanner->pos >= scanner->len) return false;
    char* endptr;
    *out_val = strtof(scanner->ptr + scanner->pos, &endptr);
    if (endptr == scanner->ptr + scanner->pos) return false;
    scanner->pos = endptr - scanner->ptr;
    return true;
}

static inline char Stream_PeekChar(StreamScanner* scanner) {
    return Stream_Peek(scanner);
}

static inline int Stream_Strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

static inline bool Stream_ReadIdentifier(StreamScanner* scanner, char* buffer, size_t max_len) {
    Stream_SkipWhitespace(scanner);
    if (scanner->pos >= scanner->len) return false;
    char c = Stream_Peek(scanner);
    if (!isalpha((unsigned char)c) && c != '_') return false;

    size_t i = 0;
    while (scanner->pos < scanner->len) {
        c = Stream_Peek(scanner);
        if (isalnum((unsigned char)c) || c == '_') {
            if (i < max_len - 1) {
                buffer[i++] = c;
            }
            Stream_Consume(scanner);
        } else {
            break;
        }
    }
    buffer[i] = '\0';
    return i > 0;
}

static inline bool Stream_PeekIdentifier(StreamScanner* scanner, char* buffer, size_t max_len) {
    size_t original_pos = scanner->pos;
    bool result = Stream_ReadIdentifier(scanner, buffer, max_len);
    scanner->pos = original_pos;
    return result;
}

static inline bool Stream_ReadBool(StreamScanner* scanner, bool* out_val) {
    Stream_SkipWhitespace(scanner);
    if (scanner->pos >= scanner->len) return false;

    char c = Stream_Peek(scanner);
    if (c == '1') {
        Stream_Consume(scanner);
        *out_val = true;
        return true;
    }
    if (c == '0') {
        Stream_Consume(scanner);
        *out_val = false;
        return true;
    }

    size_t start = scanner->pos;
    char buf[16];
    if (Stream_ReadIdentifier(scanner, buf, sizeof(buf))) {
         if (Stream_Strcasecmp(buf, "ON") == 0 || Stream_Strcasecmp(buf, "TRUE") == 0) {
             *out_val = true;
             return true;
         }
         if (Stream_Strcasecmp(buf, "OFF") == 0 || Stream_Strcasecmp(buf, "FALSE") == 0) {
             *out_val = false;
             return true;
         }
    }

    scanner->pos = start;
    return false;
}

static inline bool Stream_MatchToken(StreamScanner* scanner, const char* token) {
    size_t start = scanner->pos;
    char buf[64];
    if (Stream_ReadIdentifier(scanner, buf, sizeof(buf))) {
        if (Stream_Strcasecmp(buf, token) == 0) {
            return true;
        }
    }
    scanner->pos = start;
    return false;
}

#endif // KT_PARSER_H
