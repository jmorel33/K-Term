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
#define SSH_MSG_USERAUTH_SUCCESS 52
#define SSH_MSG_CHANNEL_OPEN 90
#define SSH_MSG_CHANNEL_WINDOW_ADJUST 93
#define SSH_MSG_CHANNEL_DATA 94

typedef enum {
    SSH_STATE_INIT = 0,
    SSH_STATE_VERSION_EXCHANGE,
    SSH_STATE_KEX_INIT,
    SSH_STATE_NEW_KEYS,
    SSH_STATE_SERVICE_REQUEST,
    SSH_STATE_USERAUTH,
    SSH_STATE_CHANNEL_OPEN,
    SSH_STATE_PTY_REQ,
    SSH_STATE_SHELL,
    SSH_STATE_READY,
    SSH_STATE_REKEYING // New state for re-keying quirk
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
    int packet_len; // Parsed from current header

    // Counters for quirks
    uint32_t window_size;
    uint32_t sequence_number;
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

// Helper: Send SSH Binary Packet (Framed)
static int send_packet(int fd, uint8_t type, const void* payload, size_t len) {
    if (fd < 0) return 0;

    // RFC 4253: packet_length || padding_length || payload || padding || mac
    // packet_length = length of (padding_length + payload + padding)
    // padding_length >= 4
    // Payload includes the Message Type byte!

    uint8_t pad_len = 4; // Minimal padding for skeleton
    uint32_t pkt_len = 1 + 1 + len + pad_len;
    // 1 (padding_length byte) + 1 (Message Type) + len (payload data) + pad_len (padding bytes)

    uint8_t header[5];
    put_u32(header, pkt_len);
    header[4] = pad_len;

    // 1. Header
    send(fd, header, 5, 0);

    // 2. Payload Type
    send(fd, &type, 1, 0);

    // 3. Payload Data
    if (len > 0) send(fd, payload, len, 0);

    // 4. Padding (zeros)
    uint8_t padding[16] = {0}; // max padding usually 255 but we use fixed 4
    send(fd, padding, pad_len, 0);

    // 5. Mock MAC (omitted or dummy 4 bytes)
    // send(fd, padding, 4, 0);

    return 0;
}

static KTermSecResult my_ssh_handshake(void* ctx, KTermSession* session, int fd) {
    MySSHContext* ssh = (MySSHContext*)ctx;

    switch (ssh->state) {
        case SSH_STATE_INIT:
            ssh_log("Starting Handshake...");
            KTerm_Net_GetCredentials(NULL, session, ssh->user, sizeof(ssh->user), ssh->password, sizeof(ssh->password));

            // Client Version
            snprintf(ssh->client_version, sizeof(ssh->client_version), "SSH-2.0-KTermSkeleton_1.0\r\n");
            if (fd >= 0) send(fd, ssh->client_version, strlen(ssh->client_version), 0);

            ssh->state = SSH_STATE_VERSION_EXCHANGE;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_VERSION_EXCHANGE:
            // ... (Version Exchange Logic kept simple text-based as it is not packetized) ...
            {
                char buf[256];
                ssize_t n = -1;
                if (fd >= 0) n = recv(fd, buf, sizeof(buf)-1, 0);

                if (n < 0) {
                    // MOCK
                    const char* mock_srv = "SSH-2.0-OpenSSH_Mock\r\n";
                    strncpy(buf, mock_srv, sizeof(buf));
                    n = strlen(mock_srv);
                }

                if (n > 0) {
                    buf[n] = '\0';
                    ssh_log("Version Exchange Complete");
                    ssh->state = SSH_STATE_KEX_INIT;
                } else if (n == 0) return KTERM_SEC_ERROR;
                else return KTERM_SEC_AGAIN;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_KEX_INIT:
            ssh_log("Sending SSH_MSG_KEXINIT (Framed)...");
            // Dummy KEX payload
            uint8_t kex_cookie[16] = {0};
            send_packet(fd, SSH_MSG_KEXINIT, kex_cookie, 16); // + lists...
            ssh->state = SSH_STATE_NEW_KEYS;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_NEW_KEYS:
            ssh_log("Sending SSH_MSG_NEWKEYS...");
            send_packet(fd, SSH_MSG_NEWKEYS, NULL, 0);
            ssh->state = SSH_STATE_SERVICE_REQUEST;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_SERVICE_REQUEST:
            ssh_log("Sending SSH_MSG_SERVICE_REQUEST...");
            send_packet(fd, SSH_MSG_SERVICE_REQUEST, "ssh-userauth", 12);
            ssh->state = SSH_STATE_USERAUTH;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_USERAUTH:
            printf("[SSH-Skeleton] Authenticating '%s' with password...\n", ssh->user);
            // Payload would be complex struct, assume sent
            send_packet(fd, SSH_MSG_USERAUTH_REQUEST, ssh->user, strlen(ssh->user));
            ssh->state = SSH_STATE_CHANNEL_OPEN;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_CHANNEL_OPEN:
            ssh_log("Sending SSH_MSG_CHANNEL_OPEN...");
            send_packet(fd, SSH_MSG_CHANNEL_OPEN, "session", 7);
            ssh->state = SSH_STATE_PTY_REQ;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_PTY_REQ:
            ssh_log("Sending PTY Request...");
            // ...
            ssh->state = SSH_STATE_SHELL;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_SHELL:
            ssh_log("Sending Shell Request...");
            ssh->state = SSH_STATE_READY;
            return KTERM_SEC_OK;

        case SSH_STATE_READY:
            return KTERM_SEC_OK;

        case SSH_STATE_REKEYING:
            // Logic handled in read/write
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
            // Quirk: Ignore/Debug messages
        }
        else if (type == SSH_MSG_DEBUG) {
            ssh_log("Received SSH_MSG_DEBUG");
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

    // MOCK: If in handshake and no server, inject data to progress state
    // But since this is a skeleton for integration, we focus on structure.
    return -1; // EAGAIN
}

static int my_ssh_write(void* ctx, int fd, const char* buf, size_t len) {
    // Encrypt & Frame outgoing terminal data
    send_packet(fd, SSH_MSG_CHANNEL_DATA, buf, len);
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

int main() {
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
    KTerm_Net_SetCallbacks(term, session, cbs);

    printf("Simulating Gateway Command: connect;bob:secret@localhost:2222\n");

    KTerm_GatewayProcess(term, session, "KTERM", "1", "EXT", "ssh;connect;bob:secret@localhost:2222");

    // Run Loop (simulate a few frames)
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
