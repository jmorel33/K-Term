#define KTERM_IMPLEMENTATION
#include "../kterm.h"

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

// --- Fake SSH Implementation Skeleton ---

// SSH Message Types
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
    MySSHState pre_rekey_state; // To restore after re-keying
    char server_version[256];
    char client_version[256];

    // Auth Data
    char user[64];
    char password[64];

    // Buffers for Packet Reassembly (Framing)
    uint8_t in_buf[4096];
    int in_len;

    // Handshake RX State
    uint8_t hs_rx_buf[4096];
    int hs_rx_len;

    // Counters for quirks
    uint32_t window_size;
    uint32_t sequence_number;

    // Auth Toggles
    bool try_pubkey;
} MySSHContext;

// Helper: Simulate sending data
static void ssh_log(const char* msg) {
    printf("[SSH-Skeleton] %s\n", msg);
}

// Helper: Write Big Endian uint32
static void put_u32(uint8_t* buf, uint32_t v) {
    buf[0] = (v >> 24) & 0xFF;
    buf[1] = (v >> 16) & 0xFF;
    buf[2] = (v >> 8) & 0xFF;
    buf[3] = v & 0xFF;
}

// Helper: Read Big Endian uint32
static uint32_t get_u32(const uint8_t* buf) {
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

// Helper: Read Length-Prefixed String
// Returns length of string payload, or -1 if buffer too short
// Updates offset
static int ssh_read_string(const uint8_t* buf, size_t buf_len, size_t* offset, char* out, size_t out_max) {
    if (*offset + 4 > buf_len) return -1;
    uint32_t len = get_u32(buf + *offset);
    *offset += 4;

    if (*offset + len > buf_len) return -1;

    if (out && out_max > 0) {
        size_t copy = (len < out_max - 1) ? len : (out_max - 1);
        memcpy(out, buf + *offset, copy);
        out[copy] = '\0';
    }
    *offset += len;
    return len;
}

// Helper: Send SSH Binary Packet (Framed)
static int send_packet(int fd, uint8_t type, const void* payload, size_t len) {
    if (fd < 0) return 0;

    // RFC 4253: packet_length || padding_length || payload || padding || mac
    // packet_length = length of (padding_length + payload + padding)
    // padding_length >= 4
    // Payload includes the Message Type byte!

    uint8_t pad_len = 4; // Minimal padding for skeleton
    // Ensure total length is multiple of 8 (block size)
    // 4 (len) + 1 (pad_len) + 1 (type) + len + pad_len
    // We can adjust pad_len if needed, but for skeleton we keep it simple.

    uint32_t pkt_len = 1 + 1 + len + pad_len;

    uint8_t header[5];
    put_u32(header, pkt_len);
    header[4] = pad_len;

    // 1. Header
    if (send(fd, header, 5, 0) != 5) perror("send header failed");

    // 2. Payload Type
    if (send(fd, &type, 1, 0) != 1) perror("send type failed");

    // 3. Payload Data
    if (len > 0) {
        if (send(fd, payload, len, 0) != (ssize_t)len) perror("send payload failed");
    }

    // 4. Padding (zeros)
    uint8_t padding[16] = {0};
    if (send(fd, padding, pad_len, 0) != pad_len) perror("send padding failed");

    return 0;
}

// Packet Construction Helpers (for writing to a buffer before sending)
// Added bounds checking
static bool ssh_write_byte(uint8_t** ptr, size_t* rem, uint8_t val) {
    if (*rem < 1) return false;
    **ptr = val;
    (*ptr)++;
    (*rem)--;
    return true;
}

static bool ssh_write_bool(uint8_t** ptr, size_t* rem, bool val) {
    return ssh_write_byte(ptr, rem, val ? 1 : 0);
}

static bool ssh_write_u32(uint8_t** ptr, size_t* rem, uint32_t val) {
    if (*rem < 4) return false;
    put_u32(*ptr, val);
    (*ptr) += 4;
    (*rem) -= 4;
    return true;
}

static bool ssh_write_string(uint8_t** ptr, size_t* rem, const char* str, size_t len) {
    if (!ssh_write_u32(ptr, rem, (uint32_t)len)) return false;
    if (*rem < len) return false;
    memcpy(*ptr, str, len);
    (*ptr) += len;
    (*rem) -= len;
    return true;
}

static bool ssh_write_cstring(uint8_t** ptr, size_t* rem, const char* str) {
    return ssh_write_string(ptr, rem, str, strlen(str));
}

// Helper: Read next packet (Blocking-ish, returns type or -1 if incomplete)
// NOTE: This modifies ssh->hs_rx_buf
static int read_next_handshake_packet(MySSHContext* ssh, int fd, uint8_t* out_payload, size_t max_pay, size_t* out_pay_len) {
    // Read from socket into hs_rx_buf
    ssize_t n = recv(fd, ssh->hs_rx_buf + ssh->hs_rx_len, sizeof(ssh->hs_rx_buf) - ssh->hs_rx_len, 0);
    if (n > 0) ssh->hs_rx_len += n;

    if (ssh->hs_rx_len < 4) return -1; // Header incomplete

    uint32_t pkt_len = get_u32(ssh->hs_rx_buf);
    int total_frame = 4 + pkt_len;

    if (ssh->hs_rx_len < total_frame) return -1; // Frame incomplete

    uint8_t pad_len = ssh->hs_rx_buf[4];
    uint8_t type = ssh->hs_rx_buf[5];

    // Copy payload if requested
    int pay_len = pkt_len - 1 - pad_len;
    if (pay_len > 0 && out_payload) {
        size_t copy = (size_t)pay_len < max_pay ? (size_t)pay_len : max_pay;
        memcpy(out_payload, ssh->hs_rx_buf + 6, copy);
        if (out_pay_len) *out_pay_len = copy;
    }

    // Shift buffer
    memmove(ssh->hs_rx_buf, ssh->hs_rx_buf + total_frame, ssh->hs_rx_len - total_frame);
    ssh->hs_rx_len -= total_frame;

    return type;
}

static KTermSecResult my_ssh_handshake(void* ctx, KTermSession* session, int fd) {
    MySSHContext* ssh = (MySSHContext*)ctx;
    int type;
    uint8_t buf[1024];
    size_t len;
    uint8_t scratch[1024];
    uint8_t* p;
    size_t rem;

    switch (ssh->state) {
        case SSH_STATE_INIT:
            ssh_log("Starting Handshake...");
            KTerm_Net_GetCredentials(NULL, session, ssh->user, sizeof(ssh->user), ssh->password, sizeof(ssh->password));
            ssh->try_pubkey = true; // Enable Pubkey flow

            // Client Version
            snprintf(ssh->client_version, sizeof(ssh->client_version), "SSH-2.0-KTermSkeleton_1.0\r\n");
            if (fd >= 0) send(fd, ssh->client_version, strlen(ssh->client_version), 0);

            ssh->state = SSH_STATE_VERSION_EXCHANGE;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_VERSION_EXCHANGE:
            {
                char vbuf[256];
                ssize_t n = recv(fd, vbuf, sizeof(vbuf)-1, 0);
                if (n < 0) {
                    return KTERM_SEC_AGAIN;
                }

                if (n > 0) {
                    vbuf[n] = '\0';
                    strncpy(ssh->server_version, vbuf, sizeof(ssh->server_version));
                    ssh_log("Version Exchange Complete");
                    ssh->state = SSH_STATE_KEX_INIT;
                } else if (n == 0) return KTERM_SEC_ERROR;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_KEX_INIT:
            ssh_log("Sending SSH_MSG_KEXINIT...");
            uint8_t kex_cookie[16] = {0}; // Mock Cookie
            send_packet(fd, SSH_MSG_KEXINIT, kex_cookie, 16);
            ssh->state = SSH_STATE_WAIT_KEX_INIT;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_WAIT_KEX_INIT:
            type = read_next_handshake_packet(ssh, fd, buf, sizeof(buf), &len);
            if (type == SSH_MSG_KEXINIT) {
                ssh_log("Received SSH_MSG_KEXINIT. Verifying Host Key (Mock)... OK");
                // Mock KEX calculation...
                ssh->state = SSH_STATE_NEW_KEYS;
            } else if (type != -1) {
                // Ignore other packets or error
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_NEW_KEYS:
            ssh_log("Sending SSH_MSG_NEWKEYS...");
            send_packet(fd, SSH_MSG_NEWKEYS, NULL, 0);
            ssh->state = SSH_STATE_WAIT_NEW_KEYS;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_WAIT_NEW_KEYS:
            type = read_next_handshake_packet(ssh, fd, NULL, 0, NULL);
            if (type == SSH_MSG_NEWKEYS) {
                ssh_log("Received SSH_MSG_NEWKEYS.");
                ssh->state = SSH_STATE_SERVICE_REQUEST;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_SERVICE_REQUEST:
            ssh_log("Sending SSH_MSG_SERVICE_REQUEST (ssh-userauth)...");
            p = scratch; rem = sizeof(scratch);
            if (!ssh_write_cstring(&p, &rem, "ssh-userauth")) return KTERM_SEC_ERROR;
            send_packet(fd, SSH_MSG_SERVICE_REQUEST, scratch, p - scratch);
            ssh->state = SSH_STATE_WAIT_SERVICE_ACCEPT;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_WAIT_SERVICE_ACCEPT:
            type = read_next_handshake_packet(ssh, fd, NULL, 0, NULL);
            if (type == SSH_MSG_SERVICE_ACCEPT) {
                ssh_log("Received SSH_MSG_SERVICE_ACCEPT.");
                ssh->state = SSH_STATE_USERAUTH_PUBKEY_PROBE;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_USERAUTH_PUBKEY_PROBE:
            ssh_log("Auth: Probing Public Key (ssh-ed25519)...");
            // RFC 4252:
            // byte      SSH_MSG_USERAUTH_REQUEST
            // string    user name
            // string    service name ("ssh-connection")
            // string    "publickey"
            // boolean   FALSE
            // string    public key algorithm name
            // string    public key blob
            p = scratch; rem = sizeof(scratch);
            if (!ssh_write_cstring(&p, &rem, ssh->user)) return KTERM_SEC_ERROR;
            if (!ssh_write_cstring(&p, &rem, "ssh-connection")) return KTERM_SEC_ERROR;
            if (!ssh_write_cstring(&p, &rem, "publickey")) return KTERM_SEC_ERROR;
            if (!ssh_write_bool(&p, &rem, false)) return KTERM_SEC_ERROR; // Probe
            if (!ssh_write_cstring(&p, &rem, "ssh-ed25519")) return KTERM_SEC_ERROR;
            if (!ssh_write_string(&p, &rem, "dummy_key_blob", 14)) return KTERM_SEC_ERROR; // Mock key blob

            send_packet(fd, SSH_MSG_USERAUTH_REQUEST, scratch, p - scratch);
            ssh->state = SSH_STATE_WAIT_PK_OK;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_WAIT_PK_OK:
            type = read_next_handshake_packet(ssh, fd, NULL, 0, NULL);
            if (type == SSH_MSG_USERAUTH_PK_OK) {
                ssh_log("Auth: Public Key Accepted. Signing...");
                ssh->state = SSH_STATE_USERAUTH_PUBKEY_SIGN;
            } else if (type == SSH_MSG_USERAUTH_FAILURE) {
                ssh_log("Auth: Public Key Rejected. Falling back to Password (TODO).");
                return KTERM_SEC_ERROR;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_USERAUTH_PUBKEY_SIGN:
            ssh_log("Auth: Sending Signed Request...");
            // RFC 4252:
            // byte      SSH_MSG_USERAUTH_REQUEST
            // string    user name
            // string    service name ("ssh-connection")
            // string    "publickey"
            // boolean   TRUE
            // string    public key algorithm name
            // string    public key blob
            // string    signature
            p = scratch; rem = sizeof(scratch);
            if (!ssh_write_cstring(&p, &rem, ssh->user)) return KTERM_SEC_ERROR;
            if (!ssh_write_cstring(&p, &rem, "ssh-connection")) return KTERM_SEC_ERROR;
            if (!ssh_write_cstring(&p, &rem, "publickey")) return KTERM_SEC_ERROR;
            if (!ssh_write_bool(&p, &rem, true)) return KTERM_SEC_ERROR; // Sign
            if (!ssh_write_cstring(&p, &rem, "ssh-ed25519")) return KTERM_SEC_ERROR;
            if (!ssh_write_string(&p, &rem, "dummy_key_blob", 14)) return KTERM_SEC_ERROR;
            if (!ssh_write_string(&p, &rem, "dummy_signature", 15)) return KTERM_SEC_ERROR; // Mock signature

            send_packet(fd, SSH_MSG_USERAUTH_REQUEST, scratch, p - scratch);
            ssh->state = SSH_STATE_WAIT_AUTH_SUCCESS;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_WAIT_AUTH_SUCCESS:
            type = read_next_handshake_packet(ssh, fd, NULL, 0, NULL);
            if (type == SSH_MSG_USERAUTH_SUCCESS) {
                ssh_log("Auth: Success!");
                ssh->state = SSH_STATE_CHANNEL_OPEN;
            } else if (type == SSH_MSG_USERAUTH_FAILURE) {
                ssh_log("Auth: Signature Rejected.");
                return KTERM_SEC_ERROR;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_CHANNEL_OPEN:
            ssh_log("Sending SSH_MSG_CHANNEL_OPEN...");
            // byte      SSH_MSG_CHANNEL_OPEN
            // string    "session"
            // uint32    sender channel
            // uint32    initial window size
            // uint32    maximum packet size
            p = scratch; rem = sizeof(scratch);
            if (!ssh_write_cstring(&p, &rem, "session")) return KTERM_SEC_ERROR;
            if (!ssh_write_u32(&p, &rem, 0)) return KTERM_SEC_ERROR; // Local channel ID
            if (!ssh_write_u32(&p, &rem, 2097152)) return KTERM_SEC_ERROR; // Window size
            if (!ssh_write_u32(&p, &rem, 32768)) return KTERM_SEC_ERROR; // Max packet size
            send_packet(fd, SSH_MSG_CHANNEL_OPEN, scratch, p - scratch);
            ssh->state = SSH_STATE_WAIT_CHANNEL_OPEN;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_WAIT_CHANNEL_OPEN:
             type = read_next_handshake_packet(ssh, fd, NULL, 0, NULL);
             if (type == SSH_MSG_CHANNEL_OPEN_CONFIRMATION) {
                 ssh_log("Channel Opened.");
                 ssh->state = SSH_STATE_PTY_REQ;
             }
             return KTERM_SEC_AGAIN;

        case SSH_STATE_PTY_REQ:
            ssh_log("Sending PTY Request (xterm-256color)...");
            // byte      SSH_MSG_CHANNEL_REQUEST
            // uint32    recipient channel
            // string    "pty-req"
            // boolean   want_reply
            // string    TERM environment variable value (e.g., vt100)
            // uint32    terminal width, characters (e.g., 80)
            // uint32    terminal height, rows (e.g., 24)
            // uint32    terminal width, pixels (e.g., 640)
            // uint32    terminal height, pixels (e.g., 480)
            // string    encoded terminal modes
            p = scratch; rem = sizeof(scratch);
            if (!ssh_write_u32(&p, &rem, 0)) return KTERM_SEC_ERROR; // Remote channel
            if (!ssh_write_cstring(&p, &rem, "pty-req")) return KTERM_SEC_ERROR;
            if (!ssh_write_bool(&p, &rem, true)) return KTERM_SEC_ERROR;
            if (!ssh_write_cstring(&p, &rem, "xterm-256color")) return KTERM_SEC_ERROR;
            if (!ssh_write_u32(&p, &rem, 80)) return KTERM_SEC_ERROR;
            if (!ssh_write_u32(&p, &rem, 24)) return KTERM_SEC_ERROR;
            if (!ssh_write_u32(&p, &rem, 0)) return KTERM_SEC_ERROR;
            if (!ssh_write_u32(&p, &rem, 0)) return KTERM_SEC_ERROR;
            if (!ssh_write_string(&p, &rem, "", 0)) return KTERM_SEC_ERROR; // No modes

            // Note: In real packet, type is SSH_MSG_CHANNEL_REQUEST (98)
            send_packet(fd, 98, scratch, p - scratch);
            ssh->state = SSH_STATE_SHELL;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_SHELL:
            // Wait for PTY Success (optional in skeleton, but good practice)
            // Then send Shell request
            ssh_log("Sending Shell Request...");
            p = scratch; rem = sizeof(scratch);
            if (!ssh_write_u32(&p, &rem, 0)) return KTERM_SEC_ERROR;
            if (!ssh_write_cstring(&p, &rem, "shell")) return KTERM_SEC_ERROR;
            if (!ssh_write_bool(&p, &rem, true)) return KTERM_SEC_ERROR;
            send_packet(fd, 98, scratch, p - scratch); // SSH_MSG_CHANNEL_REQUEST

            ssh->state = SSH_STATE_READY;
            return KTERM_SEC_OK; // Handshake Done!

        case SSH_STATE_READY:
            return KTERM_SEC_OK;

        case SSH_STATE_REKEYING:
            return KTERM_SEC_OK;
    }

    return KTERM_SEC_ERROR;
}

// Filtered Read (Acts as Transport Layer)
static int my_ssh_read(void* ctx, int fd, char* buf, size_t len) {
    MySSHContext* ssh = (MySSHContext*)ctx;

    // 1. Read Raw Bytes into internal buffer
    ssize_t n = recv(fd, ssh->in_buf + ssh->in_len, sizeof(ssh->in_buf) - ssh->in_len, 0);
    if (n > 0) ssh->in_len += n;
    else if (n < 0) return -1; // EAGAIN handling assumed

    // 2. Process Packets (Loop to drain buffer)
    while (ssh->in_len > 4) {
        uint32_t pkt_len = get_u32(ssh->in_buf);
        int total_frame = 4 + pkt_len; // Length field + Body

        if (ssh->in_len < total_frame) break; // Incomplete packet

        // Full packet available
        uint8_t pad_len = ssh->in_buf[4];
        uint8_t type = ssh->in_buf[5];

        // Decrypt Payload (Mock: just offset)
        // Check bounds carefully
        if (pkt_len < 1 + pad_len) {
            // Invalid packet length
            return -1;
        }

        uint8_t* payload = ssh->in_buf + 6;
        int payload_len = pkt_len - 1 - pad_len; // -1 for pad_len byte itself

        // Handle Transport Messages (Quirks)
        if (type == SSH_MSG_IGNORE) {
            ssh_log("Received SSH_MSG_IGNORE (Keep-Alive)");
        }
        else if (type == SSH_MSG_DEBUG) {
            ssh_log("Received SSH_MSG_DEBUG");
        }
        else if (type == SSH_MSG_GLOBAL_REQUEST) {
            // "tcpip-forward", "cancel-tcpip-forward", "keepalive@openssh.com"
            // We should reply with FAILURE for unsupported global requests
            // Format: string request_name, bool want_reply
            size_t offset = 0;
            char req_name[64];
            if (ssh_read_string(payload, payload_len, &offset, req_name, sizeof(req_name)) > 0) {
                if (offset < (size_t)payload_len) {
                    bool want_reply = payload[offset] != 0;
                    ssh_log("Global Request:");
                    ssh_log(req_name);
                    if (want_reply) {
                        send_packet(fd, SSH_MSG_REQUEST_FAILURE, NULL, 0);
                    }
                }
            }
        }
        else if (type == SSH_MSG_KEXINIT) {
            // Quirk: Re-keying triggered by server
            ssh_log("Re-keying initiated by Server...");
            ssh->pre_rekey_state = ssh->state;
            ssh->state = SSH_STATE_REKEYING;
            // Reply with KEXINIT...
            send_packet(fd, SSH_MSG_KEXINIT, NULL, 0);
        }
        else if (type == SSH_MSG_CHANNEL_WINDOW_ADJUST) {
            // Quirk: Flow Control
            if (payload_len >= 4) {
                uint32_t bytes_to_add = get_u32(payload);
                ssh->window_size += bytes_to_add;
                ssh_log("Window Adjusted");
            }
        }
        else if (type == SSH_MSG_CHANNEL_DATA) {
            // Real Data!
            // Copy to output buffer
            int copy_len = (payload_len - 4 < (int)len) ? (payload_len - 4) : (int)len;
            if (copy_len > 0) {
                memcpy(buf, payload + 4, copy_len); // +4 skip channel id
            } else {
                copy_len = 0;
            }

            // Shift buffer
            memmove(ssh->in_buf, ssh->in_buf + total_frame, ssh->in_len - total_frame);
            ssh->in_len -= total_frame;

            return copy_len;
        }

        // Shift buffer (Packet consumed, loop again)
        memmove(ssh->in_buf, ssh->in_buf + total_frame, ssh->in_len - total_frame);
        ssh->in_len -= total_frame;
    }

    // If we processed packets but had no DATA to return:
    errno = EWOULDBLOCK;
    return -1;
}

static int my_ssh_write(void* ctx, int fd, const char* buf, size_t len) {
    // Encrypt & Frame outgoing terminal data
    // SSH_MSG_CHANNEL_DATA: uint32 recipient_channel, string data
    uint8_t payload[4096];
    uint8_t* p = payload;
    size_t rem = sizeof(payload);

    if (!ssh_write_u32(&p, &rem, 0)) return -1; // Channel 0
    if (!ssh_write_string(&p, &rem, buf, len)) return -1;

    send_packet(fd, SSH_MSG_CHANNEL_DATA, payload, p - payload);
    return len;
}

static void my_ssh_close(void* ctx) {
    ssh_log("Closing SSH Context");
    free(ctx);
}

// --- Main Example ---

void on_term_connect(KTerm* term, KTermSession* session) {
    printf("Callback: Session Connected!\n");
}

void on_term_error(KTerm* term, KTermSession* session, const char* msg) {
    printf("Callback: Session Error: %s\n", msg);
}

int main() {
    setbuf(stdout, NULL);
    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    KTerm* term = KTerm_Create(config);

    KTermSession* session = &term->sessions[0];

    // Setup Custom Security
    MySSHContext* ssh_ctx = (MySSHContext*)calloc(1, sizeof(MySSHContext));
    KTermNetSecurity sec = {
        .handshake = my_ssh_handshake,
        .read = my_ssh_read,
        .write = my_ssh_write,
        .close = my_ssh_close,
        .ctx = ssh_ctx
    };
    KTerm_Net_SetSecurity(term, session, sec);

    KTermNetCallbacks cbs = {0};
    cbs.on_connect = on_term_connect;
    cbs.on_error = on_term_error;
    KTerm_Net_SetCallbacks(term, session, cbs);

    printf("Simulating Gateway Command: connect;bob:secret@127.0.0.1:2222\n");

    KTerm_GatewayProcess(term, session, "KTERM", "1", "EXT", "ssh;connect;bob:secret@127.0.0.1:2222");

    // Run Loop (simulate a few frames)
    printf("Entering Main Loop (Mocking Server Responses via KTERM_TESTING macro logic)...\n");
    for (int i=0; i<50; i++) {
        KTerm_Net_Process(term);
        usleep(100000); // 100ms

        char status[64];
        KTerm_Net_GetStatus(term, session, status, sizeof(status));

        if (strstr(status, "ERROR")) {
             printf("Connection failed (expected if no server).\n");
             break;
        }
        if (strstr(status, "CONNECTED")) {
             printf("Connected! (Handshake completed)\n");
             break;
        }
    }

    KTerm_Destroy(term);
    return 0;
}
