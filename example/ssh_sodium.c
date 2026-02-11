// KTerm SSH Client with LibSodium Integration Example
// A graphical SSH client demonstrating how to hook libsodium for crypto.
//
// Requires: libsodium (https://doc.libsodium.org/)
// Compile: gcc ssh_sodium.c -o ssh_sodium -lsituation -lsodium -lm
//
// Note: This is a reference implementation showing where to hook sodium primitives.
// A full SSH implementation requires careful handling of packet numbers,
// key derivation (RFC 4253), and strict state validation.

#define KTERM_IMPLEMENTATION
#include "../kterm.h"

// Integrate Situation Input Adapter
#define KTERM_IO_SIT_IMPLEMENTATION
#include "../kt_io_sit.h"

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

// --- LibSodium Integration ---
// If libsodium is not available in your build environment, this example
// defaults to the skeleton logic but shows the calls in comments/inactive blocks.
#ifdef HAVE_LIBSODIUM
#include <sodium.h>
#endif

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
    SSH_STATE_READY
} MySSHState;

typedef struct {
    MySSHState state;
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
    uint32_t local_channel_id;
    uint32_t remote_channel_id;
    bool try_pubkey;

    // Crypto State
    bool encrypted;
#ifdef HAVE_LIBSODIUM
    unsigned char kex_pk[crypto_scalarmult_curve25519_BYTES];
    unsigned char kex_sk[crypto_scalarmult_curve25519_BYTES];
    unsigned char shared_secret[crypto_scalarmult_curve25519_BYTES];
    unsigned char session_id[32]; // Hash(H)

    // Keys (Derived from K, H, SessionID)
    unsigned char enc_key_c2s[32]; // Client to Server
    unsigned char enc_key_s2c[32]; // Server to Client
    unsigned char mac_key_c2s[32];
    unsigned char mac_key_s2c[32];

    // Nonces/Counters
    uint64_t seq_c2s;
    uint64_t seq_s2c;
#endif

    // UI State
    char status_text[256];
} MySSHContext;

static MySSHContext global_ssh_ctx = {0};

void update_status(const char* msg) {
    snprintf(global_ssh_ctx.status_text, sizeof(global_ssh_ctx.status_text), "%s", msg);
}

static void put_u32(uint8_t* buf, uint32_t v) {
    buf[0] = (v >> 24) & 0xFF; buf[1] = (v >> 16) & 0xFF; buf[2] = (v >> 8) & 0xFF; buf[3] = v & 0xFF;
}

static uint32_t get_u32(const uint8_t* buf) {
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

// Send Framed Packet
static int send_packet(int fd, uint8_t type, const void* payload, size_t len) {
    if (fd < 0) return 0;

    // 1. Construct Plaintext Payload
    // RFC 4253: packet_length (4) + padding_length (1) + payload (len) + padding (pad_len)

    uint8_t pad_len = 4; // Minimal padding (dummy for example)
    // In real SSH, pad_len satisfies block size alignment

    uint32_t pkt_len = 1 + 1 + len + pad_len; // 1(pad_len_byte) + 1(type) + body + pad
    uint8_t packet[4096];

    put_u32(packet, pkt_len);
    packet[4] = pad_len;
    packet[5] = type;
    if (len > 0) memcpy(packet + 6, payload, len);

    // Random padding
#ifdef HAVE_LIBSODIUM
    randombytes_buf(packet + 6 + len, pad_len);
#else
    memset(packet + 6 + len, 0, pad_len);
#endif

    // 2. Encrypt if keys negotiated
    if (global_ssh_ctx.encrypted) {
#ifdef HAVE_LIBSODIUM
        // crypto_stream_chacha20_xor(...) on packet + 4 (length is usually encrypted too in some modes, or not)
        // Note: Standard SSH uses specific modes (ctr) or AEAD (chacha20-poly1305)
        // If chacha20-poly1305@openssh.com:
        //   packet length is encrypted with a separate header key
        //   payload is encrypted with main key + mac tag appended

        // This is a placeholder for where the Sodium call goes
        // crypto_aead_chacha20poly1305_ietf_encrypt(...)
#endif
    }

    send(fd, packet, 4 + pkt_len, 0); // Header(4) + Body
    return 0;
}

static int read_next_handshake_packet(MySSHContext* ssh, int fd, uint8_t* out_payload, size_t max_pay) {
    // Read raw
    ssize_t n = recv(fd, ssh->hs_rx_buf + ssh->hs_rx_len, sizeof(ssh->hs_rx_buf) - ssh->hs_rx_len, 0);
    if (n > 0) ssh->hs_rx_len += n;

    if (ssh->hs_rx_len < 4) return -1;
    uint32_t pkt_len = get_u32(ssh->hs_rx_buf);
    int total_frame = 4 + pkt_len;

    if (ssh->hs_rx_len < total_frame) return -1;

    // Decrypt would happen here if encrypted

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
    int type;

    switch (ssh->state) {
        case SSH_STATE_INIT:
#ifdef HAVE_LIBSODIUM
            if (sodium_init() < 0) return KTERM_SEC_ERROR;
            update_status("Sodium Init OK");
#endif
            update_status("Sending Version...");
            KTerm_Net_GetCredentials(NULL, session, ssh->user, sizeof(ssh->user), ssh->password, sizeof(ssh->password));
            snprintf(ssh->client_version, sizeof(ssh->client_version), "SSH-2.0-KTermSodium_1.0\r\n");
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
                    if (strncmp(vbuf, "SSH-", 4) != 0) {
                        // Skip banner
                    } else {
                        update_status("Exchange KEXINIT...");
                        ssh->state = SSH_STATE_KEX_INIT;
                    }
                } else if (n == 0) return KTERM_SEC_ERROR;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_KEX_INIT:
            // Send KEXINIT with 'curve25519-sha256' and 'chacha20-poly1305@openssh.com' preferred
            memset(scratch, 0, 16); // Cookie
#ifdef HAVE_LIBSODIUM
            randombytes_buf(scratch, 16);
#endif
            // Construct KEXINIT packet...
            send_packet(fd, SSH_MSG_KEXINIT, scratch, 16);
            ssh->state = SSH_STATE_WAIT_KEX_INIT;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_WAIT_KEX_INIT:
            type = read_next_handshake_packet(ssh, fd, buf, sizeof(buf));
            if (type == SSH_MSG_KEXINIT) {
                // Generate Ephemeral Keypair
#ifdef HAVE_LIBSODIUM
                crypto_scalarmult_curve25519_base(ssh->kex_pk, ssh->kex_sk);
                // Send ECDH Init
                // Packet: SSH_MSG_KEX_ECDH_INIT (30) + Public Key String
                // ... (Implement packet construction)
#endif
                update_status("KEX: Computing Secret...");
                ssh->state = SSH_STATE_NEW_KEYS;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_NEW_KEYS:
            // Verify Host Key (Signature check using sodium)
            // Compute Shared Secret: crypto_scalarmult_curve25519(shared, sk, server_pk)
            // Compute Exchange Hash H
            // Verify Signature on H

            update_status("Sending NEWKEYS...");
            send_packet(fd, SSH_MSG_NEWKEYS, NULL, 0);
            ssh->state = SSH_STATE_WAIT_NEW_KEYS;
            return KTERM_SEC_AGAIN;

        case SSH_STATE_WAIT_NEW_KEYS:
            type = read_next_handshake_packet(ssh, fd, NULL, 0);
            if (type == SSH_MSG_NEWKEYS) {
                // Enable Encryption
                // Derive Keys (IV + Key for C2S and S2C) using H, K, SessionID
                ssh->encrypted = true;
                ssh->state = SSH_STATE_SERVICE_REQUEST;
            }
            return KTERM_SEC_AGAIN;

        case SSH_STATE_SERVICE_REQUEST:
            // ... (Same as ssh_client.c: request ssh-userauth) ...
            ssh->state = SSH_STATE_USERAUTH_PASSWORD; // Simplified flow
            return KTERM_SEC_AGAIN;

        case SSH_STATE_USERAUTH_PASSWORD:
            // Send Password Auth Request
            // ...
            ssh->state = SSH_STATE_READY; // Skip full implementation for brevity
            return KTERM_SEC_OK;
    }
    return KTERM_SEC_ERROR;
}

static int my_ssh_read(void* ctx, int fd, char* buf, size_t len) {
    // Decrypt and read loop
    // crypto_stream_chacha20_xor(...)
    return recv(fd, buf, len, 0);
}

static int my_ssh_write(void* ctx, int fd, const char* buf, size_t len) {
    // Encrypt and write loop
    return send(fd, buf, len, 0);
}

static void my_ssh_close(void* ctx) {
    // Cleanup sodium
    // sodium_memzero(...)
}

int main(int argc, char** argv) {
    const char* host = "127.0.0.1";
    int port = 2222;
    const char* user = "root";
    const char* pass = "toor";

    // 1. Platform Init
    KTermInitInfo init_info = {
        .window_width = 1024,
        .window_height = 768,
        .window_title = "K-Term SSH (Sodium)",
        .initial_active_window_flags = KTERM_WINDOW_STATE_RESIZABLE
    };
    if (KTerm_Platform_Init(0, NULL, &init_info) != KTERM_SUCCESS) return 1;

    // 2. Terminal Init
    KTermConfig config = {0};
    KTerm* term = KTerm_Create(config);
    KTerm_Net_Init(term);
    KTermSession* session = &term->sessions[0];

    // 3. SSH Security Init
    global_ssh_ctx.state = SSH_STATE_INIT;
    strncpy(global_ssh_ctx.user, user, 63);
    strncpy(global_ssh_ctx.password, pass, 63);

    KTermNetSecurity sec = {
        .handshake = my_ssh_handshake,
        .read = my_ssh_read,
        .write = my_ssh_write,
        .close = my_ssh_close,
        .ctx = &global_ssh_ctx
    };
    KTerm_Net_SetSecurity(term, session, sec);

    // Connect
    KTerm_Net_Connect(term, session, host, port, user, pass);

    // 4. Main Loop
    while (!WindowShouldClose()) {
        KTermSit_ProcessInput(term);
        KTerm_Update(term);
        KTerm_BeginFrame();
            ClearBackground((Color){0, 0, 0, 255});
            KTerm_Draw(term);
        KTerm_EndFrame();
    }

    KTerm_Net_Disconnect(term, session);
    KTerm_Destroy(term);
    KTerm_Platform_Shutdown();
    return 0;
}
