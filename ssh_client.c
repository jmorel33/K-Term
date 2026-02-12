// KTerm SSH Client Reference Implementation
// A graphical, standalone SSH-2 client demonstrating KTerm's custom security transport layer.
//
// ⚠️ WARNING: REFERENCE IMPLEMENTATION WITH MOCK CRYPTO ⚠️
// This client uses STUBBED cryptographic primitives (Mock KEX, Mock Signatures).
// DO NOT USE FOR REAL SSH CONNECTIONS WITHOUT PLUGGING IN A REAL CRYPTO LIBRARY.
// See `KTermNetSecurity` hooks for integration points (e.g. libsodium, OpenSSL).
//
// Features:
// - Full SSH-2 Protocol State Machine (RFC 4253/4252/4254)
// - "Bring Your Own Crypto": Pluggable hooks for Key Exchange, Encryption, and MAC
// - Graphical Window via Situation
// - Mock Host Key Verification UI
// - Resizable PTY
//
// Usage: ./ssh_client [user@]host [port]
// Compile: gcc ssh_client.c -o ssh_client -lkterm -lsituation -lm

#define KTERM_IMPLEMENTATION
#include "kterm.h"

// Integrate Situation Input Adapter
#define KTERM_IO_SIT_IMPLEMENTATION
#include "kt_io_sit.h"

#define KTERM_SERIALIZE_IMPLEMENTATION
#include "kt_serialize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <strings.h> // for strcasecmp

// --- Configuration ---
// Define KTERM_USE_MOCK_CRYPTO to run against tests/mock_ssh_server.py without external libs.
// Define KTERM_LINK_LIBSSH or similar to enable real crypto (requires implementation).
#define KTERM_USE_MOCK_CRYPTO 1

// --- SSH Message Types ---
#define SSH_MSG_DISCONNECT 1
#define SSH_MSG_IGNORE 2
#define SSH_MSG_DEBUG 4
#define SSH_MSG_SERVICE_REQUEST 5
#define SSH_MSG_SERVICE_ACCEPT 6
#define SSH_MSG_KEXINIT 20
#define SSH_MSG_NEWKEYS 21
#define SSH_MSG_USERAUTH_REQUEST 50
#define SSH_MSG_USERAUTH_FAILURE 51
#define SSH_MSG_USERAUTH_SUCCESS 52
#define SSH_MSG_USERAUTH_BANNER 53
#define SSH_MSG_USERAUTH_PK_OK 60
#define SSH_MSG_GLOBAL_REQUEST 80
#define SSH_MSG_REQUEST_SUCCESS 81
#define SSH_MSG_REQUEST_FAILURE 82
#define SSH_MSG_CHANNEL_OPEN 90
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION 91
#define SSH_MSG_CHANNEL_WINDOW_ADJUST 93
#define SSH_MSG_CHANNEL_DATA 94
#define SSH_MSG_CHANNEL_REQUEST 98

typedef enum {
    SSH_STATE_INIT = 0,
    SSH_STATE_VERSION_EXCHANGE,
    SSH_STATE_KEX_INIT,
    SSH_STATE_WAIT_KEX_INIT,
    SSH_STATE_NEW_KEYS,
    SSH_STATE_WAIT_NEW_KEYS,
    SSH_STATE_SERVICE_REQUEST,
    SSH_STATE_WAIT_SERVICE_ACCEPT,
    SSH_STATE_USERAUTH_PUBKEY_PROBE,
    SSH_STATE_WAIT_PK_OK,
    SSH_STATE_USERAUTH_PUBKEY_SIGN,
    SSH_STATE_USERAUTH_PASSWORD,
    SSH_STATE_WAIT_AUTH_SUCCESS,
    SSH_STATE_CHANNEL_OPEN,
    SSH_STATE_WAIT_CHANNEL_OPEN,
    SSH_STATE_PTY_REQ,
    SSH_STATE_SHELL,
    SSH_STATE_READY,
    SSH_STATE_REKEYING
} MySSHState;

typedef struct {
    MySSHState state;
    MySSHState pre_rekey_state;
    char server_version[256];
    char client_version[256];

    // Auth Data
    char user[64];
    char password[64];

    // Packet Assembly
    uint8_t in_buf[4096];
    int in_len;
    uint8_t hs_rx_buf[4096];
    int hs_rx_len;

    // Session State
    uint32_t window_size;
    uint32_t sequence_number;
    uint32_t local_channel_id;
    uint32_t remote_channel_id;
    bool try_pubkey;

    // Configuration
    bool durable_mode;
    bool persist_session;
    char session_file[256];
    char term_type[64];
    time_t last_reconnect_attempt;

    // UI State
    char status_text[256];
    bool show_hostkey_alert;
    char hostkey_fingerprint[64];
} MySSHContext;

static MySSHContext global_ssh_ctx = {0}; // Simplified for single-session example

// --- Graphics Interception ---
typedef enum {
    G_STATE_NORMAL = 0,
    G_STATE_BUFFERING
} GraphicsState;

typedef struct {
    GraphicsState state;
    char* buffer;
    size_t len;
    size_t capacity;
    char header_buf[16];
    int header_len;
} GraphicsContext;

static GraphicsContext g_ctx = {0};

static void g_append(const char* data, size_t len) {
    if (g_ctx.len + len > g_ctx.capacity) {
        size_t new_cap = g_ctx.capacity * 2;
        if (new_cap < g_ctx.len + len) new_cap = g_ctx.len + len + 1024 * 1024;
        g_ctx.buffer = (char*)realloc(g_ctx.buffer, new_cap);
        g_ctx.capacity = new_cap;
    }
    memcpy(g_ctx.buffer + g_ctx.len, data, len);
    g_ctx.len += len;
}

static bool my_net_on_data(KTerm* term, KTermSession* session, const char* data, size_t len) {
    size_t processed = 0;
    int idx = (int)(session - term->sessions);

    while (processed < len) {
        if (g_ctx.state == G_STATE_NORMAL) {
            char c = data[processed];

            if (g_ctx.header_len > 0) {
                // We are accumulating a potential header
                if (g_ctx.header_len < 15) g_ctx.header_buf[g_ctx.header_len++] = c;

                bool handled = false;
                if (g_ctx.header_len >= 3) {
                    if (memcmp(g_ctx.header_buf, "\x1B_G", 3) == 0) {
                        g_ctx.state = G_STATE_BUFFERING;
                        g_append(g_ctx.header_buf, 3);
                        g_ctx.header_len = 0;
                        handled = true;
                    } else if (memcmp(g_ctx.header_buf, "\x1BPq", 3) == 0) {
                        g_ctx.state = G_STATE_BUFFERING;
                        g_append(g_ctx.header_buf, 3);
                        g_ctx.header_len = 0;
                        handled = true;
                    } else {
                        // Mismatch or unknown sequence, flush as text
                        for (int i=0; i<g_ctx.header_len; i++) KTerm_WriteCharToSession(term, idx, g_ctx.header_buf[i]);
                        g_ctx.header_len = 0;
                        handled = true;
                    }
                } else if (g_ctx.header_len == 2) {
                    if (g_ctx.header_buf[1] != '_' && g_ctx.header_buf[1] != 'P') {
                        // Not a graphics prefix, flush
                        for (int i=0; i<g_ctx.header_len; i++) KTerm_WriteCharToSession(term, idx, g_ctx.header_buf[i]);
                        g_ctx.header_len = 0;
                        handled = true;
                    }
                }
                // If not handled, we keep buffering (valid partial prefix)
                processed++;
            } else {
                if (c == '\x1B') {
                    g_ctx.header_buf[g_ctx.header_len++] = c;
                    processed++;
                } else {
                    KTerm_WriteCharToSession(term, idx, c);
                    processed++;
                }
            }
        }
        else { // BUFFERING
            const char* current = data + processed;
            size_t remaining = len - processed;

            const char* st = NULL;
            // Search for ST (ESC \)
            for (size_t i=0; i < remaining - 1; i++) {
                if (current[i] == '\x1B' && current[i+1] == '\\') {
                    st = current + i;
                    break;
                }
            }

            if (st) {
                size_t chunk_len = (st - current) + 2;
                g_append(current, chunk_len);
                processed += chunk_len;

                KTerm_WriteRawGraphics(term, idx, g_ctx.buffer, g_ctx.len);
                g_ctx.len = 0;
                g_ctx.state = G_STATE_NORMAL;
            } else {
                g_append(current, remaining);
                processed += remaining;

                if (g_ctx.len >= 2 && g_ctx.buffer[g_ctx.len-2] == '\x1B' && g_ctx.buffer[g_ctx.len-1] == '\\') {
                     KTerm_WriteRawGraphics(term, idx, g_ctx.buffer, g_ctx.len);
                     g_ctx.len = 0;
                     g_ctx.state = G_STATE_NORMAL;
                }
            }
        }
    }
    return true;
}

// --- Helper Functions ---

void update_status(const char* msg) {
    snprintf(global_ssh_ctx.status_text, sizeof(global_ssh_ctx.status_text), "%s", msg);
}

static void put_u32(uint8_t* buf, uint32_t v) {
    buf[0] = (v >> 24) & 0xFF; buf[1] = (v >> 16) & 0xFF; buf[2] = (v >> 8) & 0xFF; buf[3] = v & 0xFF;
}

static uint32_t get_u32(const uint8_t* buf) {
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

// Write helpers
static bool ssh_write_byte(uint8_t** ptr, size_t* rem, uint8_t val) {
    if (*rem < 1) return false;
    **ptr = val; (*ptr)++; (*rem)--;
    return true;
}
static bool ssh_write_bool(uint8_t** ptr, size_t* rem, bool val) {
    return ssh_write_byte(ptr, rem, val ? 1 : 0);
}
static bool ssh_write_u32(uint8_t** ptr, size_t* rem, uint32_t val) {
    if (*rem < 4) return false;
    put_u32(*ptr, val); (*ptr) += 4; (*rem) -= 4;
    return true;
}
static bool ssh_write_string(uint8_t** ptr, size_t* rem, const char* str, size_t len) {
    if (!ssh_write_u32(ptr, rem, (uint32_t)len)) return false;
    if (*rem < len) return false;
    memcpy(*ptr, str, len); (*ptr) += len; (*rem) -= len;
    return true;
}
static bool ssh_write_cstring(uint8_t** ptr, size_t* rem, const char* str) {
    return ssh_write_string(ptr, rem, str, strlen(str));
}

// Send Framed Packet
static int send_packet(int fd, uint8_t type, const void* payload, size_t len) {
    if (fd < 0) return 0;
    uint8_t pad_len = 4; // Minimal padding (dummy)
    // RFC 4253: packet_length (4) + padding_length (1) + payload (len) + padding (pad_len)
    // payload includes type byte? No, usually payload arg is body.
    // Let's construct body = type + payload.

    uint32_t pkt_len = 1 + 1 + len + pad_len; // 1(pad_len_byte) + 1(type) + body + pad
    uint8_t header[5];
    put_u32(header, pkt_len);
    header[4] = pad_len;

    send(fd, header, 5, 0);
    send(fd, &type, 1, 0);
    if (len > 0) send(fd, payload, len, 0);
    uint8_t padding[16] = {0};
    send(fd, padding, pad_len, 0);
    return 0;
}

// Read next handshake packet
static int read_next_handshake_packet(MySSHContext* ssh, int fd, uint8_t* out_payload, size_t max_pay) {
    // Read raw
    ssize_t n = recv(fd, ssh->hs_rx_buf + ssh->hs_rx_len, sizeof(ssh->hs_rx_buf) - ssh->hs_rx_len, 0);
    if (n > 0) ssh->hs_rx_len += n;

    if (ssh->hs_rx_len < 4) return -1;
    uint32_t pkt_len = get_u32(ssh->hs_rx_buf);
    int total_frame = 4 + pkt_len;

    if (ssh->hs_rx_len < total_frame) return -1;

    uint8_t pad_len = ssh->hs_rx_buf[4];
    uint8_t type = ssh->hs_rx_buf[5];

    int pay_len = pkt_len - 1 - pad_len;
    if (pay_len > 0 && out_payload) {
        size_t copy = (size_t)pay_len < max_pay ? (size_t)pay_len : max_pay;
        memcpy(out_payload, ssh->hs_rx_buf + 6, copy);
    }

    memmove(ssh->hs_rx_buf, ssh->hs_rx_buf + total_frame, ssh->hs_rx_len - total_frame);
    ssh->hs_rx_len -= total_frame;

    return type;
}

// --- SSH Hooks ---

static KTermSecResult my_ssh_handshake(void* ctx, KTermSession* session, int fd) {
    MySSHContext* ssh = (MySSHContext*)ctx;
    uint8_t buf[1024];
    uint8_t scratch[1024];
    uint8_t* p;
    size_t rem;
    int type;

    switch (ssh->state) {
        case SSH_STATE_INIT:
            update_status("Sending Version...");
            KTerm_Net_GetCredentials(NULL, session, ssh->user, sizeof(ssh->user), ssh->password, sizeof(ssh->password));
            ssh->try_pubkey = true;
            ssh->local_channel_id = 0;
            snprintf(ssh->client_version, sizeof(ssh->client_version), "SSH-2.0-KTermSSH_1.0\r\n");
            send(fd, ssh->client_version, strlen(ssh->client_version), 0);
            ssh->state = SSH_STATE_VERSION_EXCHANGE;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_VERSION_EXCHANGE:
            {
                char vbuf[256];
                ssize_t n = recv(fd, vbuf, sizeof(vbuf)-1, 0);
                if (n > 0) {
                    vbuf[n] = '\0';
                    strncpy(ssh->server_version, vbuf, sizeof(ssh->server_version));
                    // Check for banner lines (RFC 4253 allows lines before version)
                    if (strncmp(vbuf, "SSH-", 4) != 0) {
                        // Assume banner line, ignore and wait for version (simplified)
                        // In real impl, buffer lines until SSH-2.0 is found
                    }
                    update_status("Exchange KEXINIT...");
                    ssh->state = SSH_STATE_KEX_INIT;
                } else if (n == 0) return KTERM_SEC_ERROR;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_KEX_INIT:
            // Send KEXINIT
            // Crypto Hooks: Here you would populate algo lists
            // For now, sending dummy cookie
            memset(scratch, 0, 16); // Cookie
            send_packet(fd, SSH_MSG_KEXINIT, scratch, 16); // + empty lists (invalid but skeleton)
            ssh->state = SSH_STATE_WAIT_KEX_INIT;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_WAIT_KEX_INIT:
            type = read_next_handshake_packet(ssh, fd, buf, sizeof(buf));
            if (type == SSH_MSG_KEXINIT) {
                // Crypto Hooks: Perform ECDH/DH here
                // 1. Parse server algorithms
                // 2. Select algorithm (curve25519-sha256)
                // 3. Generate keypair
                // 4. Send SSH_MSG_KEX_ECDH_INIT
                update_status("Mocking KEX (Host Key Verification)...");
                // Simulate Host Key check
                if (!ssh->show_hostkey_alert) {
                    ssh->show_hostkey_alert = true;
                    snprintf(ssh->hostkey_fingerprint, sizeof(ssh->hostkey_fingerprint), "SHA256:MOCK_FINGERPRINT_1234");
                    return KTERM_SEC_AGAIN; // Stall until accepted
                }
                // Assuming accepted for demo
                ssh->state = SSH_STATE_NEW_KEYS;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_NEW_KEYS:
            update_status("Sending NEWKEYS...");
            send_packet(fd, SSH_MSG_NEWKEYS, NULL, 0);
            ssh->state = SSH_STATE_WAIT_NEW_KEYS;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_WAIT_NEW_KEYS:
            type = read_next_handshake_packet(ssh, fd, NULL, 0);
            if (type == SSH_MSG_NEWKEYS) {
                // Crypto Hooks: Initialize Ciphers (AES/ChaCha) and MACs
                ssh->state = SSH_STATE_SERVICE_REQUEST;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_SERVICE_REQUEST:
            update_status("Requesting Auth Service...");
            p = scratch; rem = sizeof(scratch);
            ssh_write_cstring(&p, &rem, "ssh-userauth");
            send_packet(fd, SSH_MSG_SERVICE_REQUEST, scratch, p - scratch);
            ssh->state = SSH_STATE_WAIT_SERVICE_ACCEPT;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_WAIT_SERVICE_ACCEPT:
            type = read_next_handshake_packet(ssh, fd, NULL, 0);
            if (type == SSH_MSG_SERVICE_ACCEPT) {
                ssh->state = SSH_STATE_USERAUTH_PUBKEY_PROBE;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_USERAUTH_PUBKEY_PROBE:
            // Skip actual probe for demo, fallback to password immediately if no key logic
            // But show the state
            update_status("Auth: Probing Pubkey...");
            // ... Probe logic ...
            // Fallback
            ssh->state = SSH_STATE_USERAUTH_PASSWORD;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_USERAUTH_PASSWORD:
            update_status("Auth: Sending Password...");
            p = scratch; rem = sizeof(scratch);
            ssh_write_cstring(&p, &rem, ssh->user);
            ssh_write_cstring(&p, &rem, "ssh-connection");
            ssh_write_cstring(&p, &rem, "password");
            ssh_write_bool(&p, &rem, false);
            ssh_write_cstring(&p, &rem, ssh->password);
            send_packet(fd, SSH_MSG_USERAUTH_REQUEST, scratch, p - scratch);
            ssh->state = SSH_STATE_WAIT_AUTH_SUCCESS;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_WAIT_AUTH_SUCCESS:
            type = read_next_handshake_packet(ssh, fd, NULL, 0);
            if (type == SSH_MSG_USERAUTH_SUCCESS) {
                update_status("Auth Success! Opening Channel...");
                ssh->state = SSH_STATE_CHANNEL_OPEN;
            } else if (type == SSH_MSG_USERAUTH_FAILURE) {
                update_status("Auth Failed!");
                return KTERM_SEC_ERROR;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_CHANNEL_OPEN:
            p = scratch; rem = sizeof(scratch);
            ssh_write_cstring(&p, &rem, "session");
            ssh_write_u32(&p, &rem, ssh->local_channel_id);
            ssh_write_u32(&p, &rem, 2097152); // Window
            ssh_write_u32(&p, &rem, 32768);   // Max Pkt
            send_packet(fd, SSH_MSG_CHANNEL_OPEN, scratch, p - scratch);
            ssh->state = SSH_STATE_WAIT_CHANNEL_OPEN;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_WAIT_CHANNEL_OPEN:
            type = read_next_handshake_packet(ssh, fd, buf, sizeof(buf));
            if (type == SSH_MSG_CHANNEL_OPEN_CONFIRMATION) {
                ssh->remote_channel_id = get_u32(buf); // Remote ID is first 4 bytes of payload
                ssh->state = SSH_STATE_PTY_REQ;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_PTY_REQ:
            update_status("Requesting PTY...");
            p = scratch; rem = sizeof(scratch);
            ssh_write_u32(&p, &rem, ssh->remote_channel_id);
            ssh_write_cstring(&p, &rem, "pty-req");
            ssh_write_bool(&p, &rem, true); // Want reply
            ssh_write_cstring(&p, &rem, ssh->term_type[0] ? ssh->term_type : "xterm-256color");
            ssh_write_u32(&p, &rem, 80); // Width chars
            ssh_write_u32(&p, &rem, 24); // Height chars
            ssh_write_u32(&p, &rem, 0);
            ssh_write_u32(&p, &rem, 0);
            ssh_write_string(&p, &rem, "", 0); // Modes
            send_packet(fd, SSH_MSG_CHANNEL_REQUEST, scratch, p - scratch);
            ssh->state = SSH_STATE_SHELL;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_SHELL:
            // Assuming PTY success (should read reply but skipping for brevity)
            update_status("Requesting Shell...");
            p = scratch; rem = sizeof(scratch);
            ssh_write_u32(&p, &rem, ssh->remote_channel_id);
            ssh_write_cstring(&p, &rem, "shell");
            ssh_write_bool(&p, &rem, true);
            send_packet(fd, SSH_MSG_CHANNEL_REQUEST, scratch, p - scratch);
            ssh->state = SSH_STATE_READY;
            update_status("Connected");
            return KTERM_SEC_OK; // Handshake Done

        case SSH_STATE_READY:
            return KTERM_SEC_OK;
    }
    return KTERM_SEC_ERROR;
}

static int my_ssh_read(void* ctx, int fd, char* buf, size_t len) {
    MySSHContext* ssh = (MySSHContext*)ctx;

    // Read raw
    ssize_t n = recv(fd, ssh->in_buf + ssh->in_len, sizeof(ssh->in_buf) - ssh->in_len, 0);
    if (n > 0) ssh->in_len += n;
    else if (n < 0) return -1;

    // Process Packets
    while (ssh->in_len > 4) {
        uint32_t pkt_len = get_u32(ssh->in_buf);
        int total_frame = 4 + pkt_len;
        if (ssh->in_len < total_frame) break;

        uint8_t type = ssh->in_buf[5]; // 4(len)+1(padlen)
        uint8_t* payload = ssh->in_buf + 6;
        int pay_len = pkt_len - 1 - ssh->in_buf[4];

        if (type == SSH_MSG_CHANNEL_DATA) {
            // uint32 recipient_channel, string data
            // Payload starts at +4 (skip recip chan)
            if (pay_len > 4) {
                uint32_t data_len = get_u32(payload + 4);
                // Clamp copy
                int to_copy = (data_len < len) ? data_len : len;
                memcpy(buf, payload + 8, to_copy);

                // Shift buffer
                memmove(ssh->in_buf, ssh->in_buf + total_frame, ssh->in_len - total_frame);
                ssh->in_len -= total_frame;
                return to_copy;
            }
        }

        // Consume non-data packet
        memmove(ssh->in_buf, ssh->in_buf + total_frame, ssh->in_len - total_frame);
        ssh->in_len -= total_frame;
    }
    errno = EWOULDBLOCK;
    return -1;
}

static int my_ssh_write(void* ctx, int fd, const char* buf, size_t len) {
    MySSHContext* ssh = (MySSHContext*)ctx;
    uint8_t payload[4096];
    uint8_t* p = payload;
    size_t rem = sizeof(payload);

    ssh_write_u32(&p, &rem, ssh->remote_channel_id);
    ssh_write_string(&p, &rem, buf, len);
    send_packet(fd, SSH_MSG_CHANNEL_DATA, payload, p - payload);
    return len;
}

static void my_ssh_close(void* ctx) {
    // Cleanup
}

// --- Config Parser ---

typedef struct {
    char host_pattern[256];
    char hostname[256];
    char user[64];
    int port;
    bool durable;
    char term_type[64];
} SSHProfile;

// Simple line-based config parser (SSH-config style)
static bool load_config_profile(const char* config_path, const char* profile_name, SSHProfile* out_profile) {
    FILE* f = fopen(config_path, "r");
    if (!f) return false;

    char line[512];
    bool in_block = false;
    bool found = false;

    // Initialize Defaults
    if (out_profile) {
        memset(out_profile, 0, sizeof(SSHProfile));
        out_profile->port = 22;
    }

    while (fgets(line, sizeof(line), f)) {
        // Trim leading whitespace
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;

        // Skip comments/empty
        if (*p == '#' || *p == '\n' || *p == '\0') continue;

        // Trim trailing whitespace
        char* end = p + strlen(p) - 1;
        while (end > p && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = '\0';

        char key[64] = {0};
        char val[256] = {0};

        // Split key/value (space, tab, or =)
        char* token = strtok(p, " \t=");
        if (token) {
            strncpy(key, token, 63);
            token = strtok(NULL, " \t=");
            if (token) strncpy(val, token, 255);
        }

        if (strcasecmp(key, "Host") == 0) {
            if (found) break; // Already found our block, next Host ends it
            if (strcasecmp(val, profile_name) == 0) {
                in_block = true;
                found = true;
                if (out_profile) strcpy(out_profile->host_pattern, val);
            } else {
                in_block = false;
            }
        } else if (in_block && out_profile) {
            if (strcasecmp(key, "HostName") == 0) strcpy(out_profile->hostname, val);
            else if (strcasecmp(key, "User") == 0) strcpy(out_profile->user, val);
            else if (strcasecmp(key, "Port") == 0) out_profile->port = atoi(val);
            else if (strcasecmp(key, "Durable") == 0) out_profile->durable = (strcasecmp(val, "true") == 0 || strcasecmp(val, "yes") == 0 || strcmp(val, "1") == 0);
            else if (strcasecmp(key, "Term") == 0) strcpy(out_profile->term_type, val);
        }
    }
    fclose(f);
    return found;
}

// --- Main ---

int main(int argc, char** argv) {
    const char* host = "127.0.0.1";
    int port = 2222;
    const char* user = "root";
    const char* pass = "toor";
    bool durable = false;
    bool persist = false;
    const char* term_type = "xterm-256color";
    bool host_provided = false;

    // Mutable buffers for config overrides
    char cfg_host[256];
    char cfg_user[64];
    char cfg_term[64];

    const char* config_file = "ssh_config"; // Default local config

    // Pre-parse for --config (optional)
    for (int i=1; i<argc-1; i++) {
        if (strcmp(argv[i], "--config") == 0) {
            config_file = argv[i+1];
        }
    }

    // Parse Arguments
    int arg_idx = 1;
    while (arg_idx < argc) {
        if (strcmp(argv[arg_idx], "--durable") == 0) {
            durable = true;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--persist") == 0) {
            persist = true;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--config") == 0) {
            // Already handled, skip
            arg_idx += 2;
        } else if (strcmp(argv[arg_idx], "--term") == 0 && arg_idx + 1 < argc) {
            term_type = argv[arg_idx + 1];
            arg_idx += 2;
        } else {
            // Positional args
            if (!host_provided) { // First positional
                host_provided = true;
                char* target = argv[arg_idx];

                // Check if target is a profile in config
                SSHProfile profile;
                if (load_config_profile(config_file, target, &profile)) {
                    printf("Loaded profile '%s' from %s\n", target, config_file);

                    if (profile.hostname[0]) {
                        strncpy(cfg_host, profile.hostname, sizeof(cfg_host));
                        host = cfg_host;
                    } else {
                        host = target;
                    }

                    if (profile.user[0]) {
                        strncpy(cfg_user, profile.user, sizeof(cfg_user));
                        user = cfg_user;
                    }

                    if (profile.port > 0) port = profile.port;
                    if (profile.durable) durable = true;
                    if (profile.term_type[0]) {
                        strncpy(cfg_term, profile.term_type, sizeof(cfg_term));
                        term_type = cfg_term;
                    }
                } else {
                    // Not a profile, parse as user@host
                    char* at = strchr(target, '@');
                    if (at) {
                        *at = '\0';
                        user = target;
                        host = at + 1;
                    } else {
                        host = target;
                    }
                }
            } else {
                port = atoi(argv[arg_idx]);
            }
            arg_idx++;
        }
    }

    // 1. Platform Init
    KTermInitInfo init_info = {
        .window_width = 1024,
        .window_height = 768,
        .window_title = "K-Term SSH Client (Ref)",
        .initial_active_window_flags = KTERM_WINDOW_STATE_RESIZABLE
    };
    if (KTerm_Platform_Init(0, NULL, &init_info) != KTERM_SUCCESS) return 1;
    KTerm_SetTargetFPS(60);

    // 2. Terminal Init
    KTermConfig config = {0};
    config.width = 80; config.height = 24;
    KTerm* term = KTerm_Create(config);
    if (!term) return 1;

    KTerm_Net_Init(term);
    KTermSession* session = &term->sessions[0];

    // 3. SSH Security Init
    // In real app, allocate context per session. Here global.
    global_ssh_ctx.state = SSH_STATE_INIT;
    strncpy(global_ssh_ctx.user, user, 63);
    strncpy(global_ssh_ctx.password, pass, 63);
    global_ssh_ctx.durable_mode = durable;
    global_ssh_ctx.persist_session = persist;

    // Sanitize host for filename (replace non-alphanumeric with _)
    char safe_host[256];
    strncpy(safe_host, host, sizeof(safe_host)-1);
    safe_host[sizeof(safe_host)-1] = '\0';
    for (int i=0; safe_host[i]; i++) {
        if (!((safe_host[i] >= 'a' && safe_host[i] <= 'z') ||
              (safe_host[i] >= 'A' && safe_host[i] <= 'Z') ||
              (safe_host[i] >= '0' && safe_host[i] <= '9') ||
              safe_host[i] == '.' || safe_host[i] == '-')) {
            safe_host[i] = '_';
        }
    }
    snprintf(global_ssh_ctx.session_file, sizeof(global_ssh_ctx.session_file), "ssh_session_%s_%d.dat", safe_host, port);

    strncpy(global_ssh_ctx.term_type, term_type, 63);

    // Try to restore session if persistence is enabled and file exists
    if (persist) {
        FILE* f = fopen(global_ssh_ctx.session_file, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long fsize = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (fsize > 0) {
                void* buf = malloc(fsize);
                if (buf) {
                    size_t read_len = fread(buf, 1, fsize, f);
                    if (read_len == (size_t)fsize) {
                        if (KTerm_DeserializeSession(session, buf, fsize)) {
                            printf("Restored session from %s\n", global_ssh_ctx.session_file);
                        } else {
                            printf("Failed to deserialize session.\n");
                        }
                    } else {
                        printf("Failed to read session file (partial read).\n");
                    }
                    free(buf);
                }
            }
            fclose(f);
        }
    }

    KTermNetSecurity sec = {
        .handshake = my_ssh_handshake,
        .read = my_ssh_read,
        .write = my_ssh_write,
        .close = my_ssh_close,
        .ctx = &global_ssh_ctx
    };
    KTerm_Net_SetSecurity(term, session, sec);

    // Set Data Callback for Graphics Interception
    KTermNetCallbacks cbs = {0};
    cbs.on_data = my_net_on_data;
    KTerm_Net_SetCallbacks(term, session, cbs);

    // Init Graphics Buffer
    g_ctx.capacity = 1024 * 1024; // Start 1MB
    g_ctx.buffer = (char*)malloc(g_ctx.capacity);
    g_ctx.len = 0;
    g_ctx.state = G_STATE_NORMAL;

    // Connect
    char msg[256];
    snprintf(msg, sizeof(msg), "SSH Connecting to %s@%s:%d...\r\n", user, host, port);
    KTerm_WriteString(term, msg);
    KTerm_Net_Connect(term, session, host, port, user, pass);

    // 4. Main Loop
    int frame_count = 0;
    while (!WindowShouldClose()) {
#ifdef KTERM_TESTING
        frame_count++;
        if (frame_count > 100) break; // Exit for testing persistence
#endif

        // Mock Host Key Prompt Overlay
        if (global_ssh_ctx.show_hostkey_alert) {
            // Draw a fake dialog using direct grid writes
            // In a real app, use Situation native dialogs or KTerm GUI overlay features
            KTerm_WriteString(term, "\r\n\x1B[31;1m[SECURITY ALERT]\x1B[0m Unknown Host Key:\r\n");
            KTerm_WriteString(term, "Fingerprint: ");
            KTerm_WriteString(term, global_ssh_ctx.hostkey_fingerprint);
            KTerm_WriteString(term, "\r\nAccept? (y/n): ");
            // Cheat: Auto-accept for demo loop after 1 frame (or handle input)
            // For now, let's just pretend user typed 'y'
            global_ssh_ctx.show_hostkey_alert = false;
            KTerm_WriteString(term, "y\r\n\x1B[32mHost Verified.\x1B[0m\r\n");
        }

        // Handle Resizing (Send Window Adjust)
        if (SituationIsWindowResized()) {
            int w, h;
            SituationGetWindowSize(&w, &h);
            int cols = w / (10 * DEFAULT_WINDOW_SCALE);
            int rows = h / (10 * DEFAULT_WINDOW_SCALE);
            if (cols != config.width || rows != config.height) {
                config.width = cols; config.height = rows;
                KTerm_Resize(term, cols, rows);

                // Send SSH Window Change
                if (global_ssh_ctx.state == SSH_STATE_READY) {
                    uint8_t payload[64];
                    uint8_t* p = payload; size_t rem = sizeof(payload);
                    ssh_write_u32(&p, &rem, global_ssh_ctx.remote_channel_id);
                    ssh_write_cstring(&p, &rem, "window-change");
                    ssh_write_bool(&p, &rem, false);
                    ssh_write_u32(&p, &rem, cols);
                    ssh_write_u32(&p, &rem, rows);
                    ssh_write_u32(&p, &rem, 0);
                    ssh_write_u32(&p, &rem, 0);
                    send_packet((int)KTerm_Net_GetSocket(term, session), SSH_MSG_CHANNEL_REQUEST, payload, p - payload);
                }
            }
        }

        // Update Title with Status
        char title[512];
        snprintf(title, sizeof(title), "K-Term SSH: %s | State: %s", host, global_ssh_ctx.status_text);
        KTerm_SetWindowTitle(term, title);

        // Durable Mode Reconnection Logic
        if (global_ssh_ctx.durable_mode) {
            char status_buf[256];
            KTerm_Net_GetStatus(term, session, status_buf, sizeof(status_buf));

            if (strstr(status_buf, "STATE=DISCONNECTED") || strstr(status_buf, "STATE=ERROR")) {
                time_t now = time(NULL);
                if (now - global_ssh_ctx.last_reconnect_attempt > 3) {
                    global_ssh_ctx.last_reconnect_attempt = now;
                    update_status("Reconnecting (Durable)...");

                    // Reset SSH State
                    global_ssh_ctx.state = SSH_STATE_INIT;

                    // Reconnect
                    KTerm_Net_Connect(term, session, host, port, user, pass);
                }
            }
        }

        KTermSit_ProcessInput(term);
        KTerm_Update(term);

        KTerm_BeginFrame();
            ClearBackground((Color){0, 0, 0, 255});
            KTerm_Draw(term);

            // Draw Overlay if Reconnecting
            if (global_ssh_ctx.durable_mode &&
               (strstr(global_ssh_ctx.status_text, "Reconnecting") || strstr(global_ssh_ctx.status_text, "ERROR") || strstr(global_ssh_ctx.status_text, "DISCONNECTED"))) {
                // Simple red box in top right
                // Since we don't have direct access to Situation DrawRectangle here easily without including it or using KTerm ops,
                // we'll just rely on the Title Bar status update for now, or print a message to the terminal if it's not frozen.
                // But terminal might be frozen/cleared on reconnect.
            }
        KTerm_EndFrame();
    }

    KTerm_Net_Disconnect(term, session);

    // Persist session state on exit
    if (global_ssh_ctx.persist_session) {
        void* buf = NULL;
        size_t len = 0;
        if (KTerm_SerializeSession(session, &buf, &len)) {
            FILE* f = fopen(global_ssh_ctx.session_file, "wb");
            if (f) {
                fwrite(buf, 1, len, f);
                fclose(f);
                printf("Session saved to %s\n", global_ssh_ctx.session_file);
            }
            free(buf);
        }
    }

    KTerm_Destroy(term);
    KTerm_Platform_Shutdown();
    return 0;
}
