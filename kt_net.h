#ifndef KT_NET_H
#define KT_NET_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct KTerm_T KTerm;
typedef struct KTermSession_T KTermSession;

// --- Networking Context & State ---

typedef enum {
    KTERM_NET_STATE_DISCONNECTED = 0,
    KTERM_NET_STATE_RESOLVING,
    KTERM_NET_STATE_CONNECTING,
    KTERM_NET_STATE_HANDSHAKE,
    KTERM_NET_STATE_AUTH,
    KTERM_NET_STATE_CONNECTED,
    KTERM_NET_STATE_ERROR
} KTermNetState;

#define NET_BUFFER_SIZE 16384

// Callbacks for Async Networking
typedef struct {
    void (*on_connect)(KTerm* term, KTermSession* session);
    void (*on_disconnect)(KTerm* term, KTermSession* session);
    void (*on_data)(KTerm* term, KTermSession* session, const char* data, size_t len);
    void (*on_error)(KTerm* term, KTermSession* session, const char* msg);
    void* user_data;
} KTermNetCallbacks;

// Security Interface (TLS/SSL Hooks)
typedef enum {
    KTERM_SEC_OK = 0,
    KTERM_SEC_AGAIN = 1,
    KTERM_SEC_ERROR = -1
} KTermSecResult;

typedef struct {
    // Perform handshake (called repeatedly until OK or ERROR)
    KTermSecResult (*handshake)(void* ctx, int socket_fd);
    // Read decrypted data (returns bytes read, or -1/AGAIN)
    int (*read)(void* ctx, int socket_fd, char* buf, size_t len);
    // Write encrypted data (returns bytes written, or -1/AGAIN)
    int (*write)(void* ctx, int socket_fd, const char* buf, size_t len);
    // Cleanup context
    void (*close)(void* ctx);
    void* ctx;
} KTermNetSecurity;

// Protocol Mode
typedef enum {
    KTERM_NET_PROTO_RAW = 0,
    KTERM_NET_PROTO_FRAMED = 1
} KTermNetProtocol;

// Packet Types for Framed Mode
#define KTERM_PKT_DATA    0x01
#define KTERM_PKT_RESIZE  0x02 // Payload: [Width:4][Height:4] (Big Endian)
#define KTERM_PKT_GATEWAY 0x03 // Payload: Gateway Command String
#define KTERM_PKT_ATTACH  0x04 // Payload: [SessionID:1]

// Initialization
void KTerm_Net_Init(KTerm* term);
void KTerm_Net_Process(KTerm* term);

// API for Gateway Integration
void KTerm_Net_Connect(KTerm* term, KTermSession* session, const char* host, int port, const char* user, const char* password);
void KTerm_Net_Disconnect(KTerm* term, KTermSession* session);
void KTerm_Net_GetStatus(KTerm* term, KTermSession* session, char* buffer, size_t max_len);

// Async API / Hardening
void KTerm_Net_SetCallbacks(KTerm* term, KTermSession* session, KTermNetCallbacks callbacks);
void KTerm_Net_SetSecurity(KTerm* term, KTermSession* session, KTermNetSecurity security);
void KTerm_Net_SetProtocol(KTerm* term, KTermSession* session, KTermNetProtocol protocol);
intptr_t KTerm_Net_GetSocket(KTerm* term, KTermSession* session); // Returns socket_fd or -1

// Session Control
void KTerm_Net_SetTargetSession(KTerm* term, KTermSession* session, int target_idx);

// Send Framed Packet (Helper)
void KTerm_Net_SendPacket(KTerm* term, KTermSession* session, uint8_t type, const void* payload, size_t len);

#ifdef __cplusplus
}
#endif

#endif // KT_NET_H

#ifdef KTERM_NET_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef KTERM_USE_LIBSSH
#include <libssh/libssh.h>
#endif

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET closesocket
    #define IS_VALID_SOCKET(s) ((s) != INVALID_SOCKET)
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <sys/select.h>
    typedef int socket_t;
    #define CLOSE_SOCKET close
    #define IS_VALID_SOCKET(s) ((s) >= 0)
    #define INVALID_SOCKET -1
#endif

typedef struct {
    KTermNetState state;
    char host[256];
    int port;
    char user[64];
    char password[64]; // Simple storage for auth

    // Fallback / Raw TCP Mode
    socket_t socket_fd;

#ifdef KTERM_USE_LIBSSH
    ssh_session ssh_session;
    ssh_channel ssh_channel;
#endif

    // TX Buffer (Terminal -> Network)
    char tx_buffer[NET_BUFFER_SIZE];
    int tx_head;
    int tx_tail;

    // RX Buffer (Network -> Terminal)
    // Used for Reassembly in Framed Mode
    char rx_buffer[NET_BUFFER_SIZE];
    int rx_len;
    int expected_frame_len; // 0 = Reading Header, >0 = Reading Payload

    // Callbacks
    KTermNetCallbacks callbacks;

    // Security Hooks
    KTermNetSecurity security;

    // Protocol Mode
    KTermNetProtocol protocol;

    // Target Routing
    int target_session_index; // Default: owner session index

} KTermNetSession;

// --- Helper Functions ---

static KTermNetSession* KTerm_Net_GetContext(KTermSession* session) {
    if (!session) return NULL;
    return (KTermNetSession*)session->user_data;
}

static KTermNetSession* KTerm_Net_CreateContext(KTermSession* session) {
    if (!session) return NULL;
    if (session->user_data) return (KTermNetSession*)session->user_data;

    KTermNetSession* net = (KTermNetSession*)malloc(sizeof(KTermNetSession));
    if (net) {
        memset(net, 0, sizeof(KTermNetSession));
        net->socket_fd = INVALID_SOCKET;
#ifdef KTERM_USE_LIBSSH
        net->ssh_session = NULL;
        net->ssh_channel = NULL;
#endif
        net->target_session_index = -1;
        session->user_data = net;
    }
    return net;
}

static void KTerm_Net_DestroyContext(KTermSession* session) {
    if (!session || !session->user_data) return;
    KTermNetSession* net = (KTermNetSession*)session->user_data;

    if (net->security.close) {
        net->security.close(net->security.ctx);
    }

#ifdef KTERM_USE_LIBSSH
    if (net->ssh_channel) {
        ssh_channel_send_eof(net->ssh_channel);
        ssh_channel_close(net->ssh_channel);
        ssh_channel_free(net->ssh_channel);
    }
    if (net->ssh_session) {
        ssh_disconnect(net->ssh_session);
        ssh_free(net->ssh_session);
    }
#endif

    if (IS_VALID_SOCKET(net->socket_fd)) {
        CLOSE_SOCKET(net->socket_fd);
    }
    free(net);
    session->user_data = NULL;
}

static void KTerm_Net_Log(KTerm* term, int session_idx, const char* msg) {
    // Write Status Message to Terminal
    KTerm_WriteCharToSession(term, session_idx, '\r');
    KTerm_WriteCharToSession(term, session_idx, '\n');
    KTerm_WriteString(term, "\x1B[33m[NET] ");
    for(int i=0; msg[i]; i++) KTerm_WriteCharToSession(term, session_idx, msg[i]);
    KTerm_WriteString(term, "\x1B[0m\r\n");
}

static void KTerm_Net_TriggerError(KTerm* term, KTermSession* session, KTermNetSession* net, const char* msg) {
    KTerm_Net_Log(term, (int)(session - term->sessions), msg);
    net->state = KTERM_NET_STATE_ERROR;
    if (net->callbacks.on_error) {
        net->callbacks.on_error(term, session, msg);
    }
}

// Helper: Process a received frame
static void KTerm_Net_ProcessFrame(KTerm* term, KTermSession* session, KTermNetSession* net, uint8_t type, const char* payload, size_t len) {
    int target_idx = (net->target_session_index != -1) ? net->target_session_index : (int)(session - term->sessions);

    // 1. DATA
    if (type == KTERM_PKT_DATA) {
        if (net->callbacks.on_data) net->callbacks.on_data(term, session, payload, len);
        for (size_t i = 0; i < len; i++) {
            KTerm_WriteCharToSession(term, target_idx, payload[i]);
        }
    }
    // 2. RESIZE
    else if (type == KTERM_PKT_RESIZE && len >= 8) {
        uint32_t w = ((uint8_t)payload[0] << 24) | ((uint8_t)payload[1] << 16) | ((uint8_t)payload[2] << 8) | (uint8_t)payload[3];
        uint32_t h = ((uint8_t)payload[4] << 24) | ((uint8_t)payload[5] << 16) | ((uint8_t)payload[6] << 8) | (uint8_t)payload[7];
        KTerm_Resize(term, (int)w, (int)h); // Note: Resizes WHOLE terminal for now, maybe session specific later?
        KTerm_Net_Log(term, (int)(session - term->sessions), "Remote Resize Request Applied");
    }
    // 3. GATEWAY
    else if (type == KTERM_PKT_GATEWAY) {
        // Inject Gateway Command (simulated DCS)
        KTerm_WriteCharToSession(term, target_idx, 0x1B);
        KTerm_WriteCharToSession(term, target_idx, 'P');
        KTerm_WriteCharToSession(term, target_idx, 'G');
        KTerm_WriteCharToSession(term, target_idx, 'A');
        KTerm_WriteCharToSession(term, target_idx, 'T');
        KTerm_WriteCharToSession(term, target_idx, 'E');
        KTerm_WriteCharToSession(term, target_idx, ';');
        for (size_t i = 0; i < len; i++) {
            KTerm_WriteCharToSession(term, target_idx, payload[i]);
        }
        KTerm_WriteCharToSession(term, target_idx, 0x1B);
        KTerm_WriteCharToSession(term, target_idx, '\\');
    }
    // 4. ATTACH
    else if (type == KTERM_PKT_ATTACH && len >= 1) {
        int new_session_id = (unsigned char)payload[0];
        if (new_session_id >= 0 && new_session_id < 4) { // MAX_SESSIONS check
            net->target_session_index = new_session_id;
            char msg[64];
            snprintf(msg, sizeof(msg), "Attached to Session %d", new_session_id);
            KTerm_Net_Log(term, (int)(session - term->sessions), msg);
        }
    }
}

// --- Output Sink (Term -> Net) ---
static void KTerm_Net_Sink(void* user_data, KTermSession* session, const char* data, size_t len) {
    KTerm* term = (KTerm*)user_data;
    if (!term || !session) return;

    KTermNetSession* net = KTerm_Net_GetContext(session);

    if (net && (net->state == KTERM_NET_STATE_CONNECTED || net->state == KTERM_NET_STATE_AUTH)) {
        if (net->protocol == KTERM_NET_PROTO_FRAMED) {
            // Encode as DATA packet immediately
            uint8_t header[5];
            header[0] = KTERM_PKT_DATA;
            header[1] = (len >> 24) & 0xFF;
            header[2] = (len >> 16) & 0xFF;
            header[3] = (len >> 8) & 0xFF;
            header[4] = len & 0xFF;

            for(int i=0; i<5; i++) {
                net->tx_buffer[net->tx_head] = header[i];
                net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE;
                if(net->tx_head == net->tx_tail) net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
            }
        }

        for (size_t i = 0; i < len; i++) {
            net->tx_buffer[net->tx_head] = data[i];
            net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE;
            if (net->tx_head == net->tx_tail) {
                net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE; // Drop oldest
            }
        }
    } else {
        if (term->response_callback) {
            term->response_callback(term, data, (int)len);
        }
    }
}

// --- API Implementation ---

void KTerm_Net_Connect(KTerm* term, KTermSession* session, const char* host, int port, const char* user, const char* password) {
    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (!net) return;

    if (IS_VALID_SOCKET(net->socket_fd)) { CLOSE_SOCKET(net->socket_fd); net->socket_fd = INVALID_SOCKET; }
#ifdef KTERM_USE_LIBSSH
    if (net->ssh_channel) { ssh_channel_free(net->ssh_channel); net->ssh_channel = NULL; }
    if (net->ssh_session) { ssh_free(net->ssh_session); net->ssh_session = NULL; }
#endif

    strncpy(net->user, user ? user : "root", sizeof(net->user)-1);
    net->port = (port > 0) ? port : 22;
    strncpy(net->host, host, sizeof(net->host)-1);

    if (password) strncpy(net->password, password, sizeof(net->password)-1);
    else net->password[0] = '\0';

    net->state = KTERM_NET_STATE_RESOLVING;
    net->tx_head = 0;
    net->tx_tail = 0;
    net->rx_len = 0;
    net->expected_frame_len = 0;
    net->target_session_index = (int)(session - term->sessions); // Default to self
}

void KTerm_Net_Disconnect(KTerm* term, KTermSession* session) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (net) {
        if (net->state == KTERM_NET_STATE_CONNECTED) {
            if (net->callbacks.on_disconnect) net->callbacks.on_disconnect(term, session);
        }
    }
    KTerm_Net_DestroyContext(session);
}

void KTerm_Net_GetStatus(KTerm* term, KTermSession* session, char* buffer, size_t max_len) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    const char* state_str = "DISCONNECTED";
    if (net) {
        switch(net->state) {
            case KTERM_NET_STATE_RESOLVING: state_str = "RESOLVING"; break;
            case KTERM_NET_STATE_CONNECTING: state_str = "CONNECTING"; break;
            case KTERM_NET_STATE_HANDSHAKE: state_str = "HANDSHAKE"; break;
            case KTERM_NET_STATE_AUTH: state_str = "AUTH"; break;
            case KTERM_NET_STATE_CONNECTED: state_str = "CONNECTED"; break;
            case KTERM_NET_STATE_ERROR: state_str = "ERROR"; break;
            default: break;
        }
    }
    snprintf(buffer, max_len, "STATE=%s", state_str);
}

void KTerm_Net_SetCallbacks(KTerm* term, KTermSession* session, KTermNetCallbacks callbacks) {
    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (net) net->callbacks = callbacks;
}

void KTerm_Net_SetSecurity(KTerm* term, KTermSession* session, KTermNetSecurity security) {
    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (net) net->security = security;
}

void KTerm_Net_SetProtocol(KTerm* term, KTermSession* session, KTermNetProtocol protocol) {
    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (net) net->protocol = protocol;
}

intptr_t KTerm_Net_GetSocket(KTerm* term, KTermSession* session) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net) return -1;
#ifdef KTERM_USE_LIBSSH
    if (net->ssh_session) return -1;
#endif
    return (intptr_t)net->socket_fd;
}

void KTerm_Net_SetTargetSession(KTerm* term, KTermSession* session, int target_idx) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (net && target_idx >= 0 && target_idx < 4) {
        net->target_session_index = target_idx;
        // Optionally log or notify?
    }
}

void KTerm_Net_SendPacket(KTerm* term, KTermSession* session, uint8_t type, const void* payload, size_t len) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || net->protocol != KTERM_NET_PROTO_FRAMED) return;

    // Header: [Type:1][Len:4]
    uint8_t header[5];
    header[0] = type;
    header[1] = (len >> 24) & 0xFF;
    header[2] = (len >> 16) & 0xFF;
    header[3] = (len >> 8) & 0xFF;
    header[4] = len & 0xFF;

    // Inject into TX Buffer
    for (int i=0; i<5; i++) {
        net->tx_buffer[net->tx_head] = header[i];
        net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE;
        if (net->tx_head == net->tx_tail) net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
    }
    const char* p = (const char*)payload;
    for (size_t i=0; i<len; i++) {
        net->tx_buffer[net->tx_head] = p[i];
        net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE;
        if (net->tx_head == net->tx_tail) net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
    }
}

// --- Initialization ---

void KTerm_Net_Init(KTerm* term) {
    if (!term) return;
    KTerm_SetOutputSink(term, KTerm_Net_Sink, term);
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

// --- Process Loop ---

static void KTerm_Net_ProcessSession(KTerm* term, int session_idx) {
    KTermSession* session = &term->sessions[session_idx];
    KTermNetSession* net = KTerm_Net_GetContext(session);

    if (!net || net->state == KTERM_NET_STATE_DISCONNECTED || net->state == KTERM_NET_STATE_ERROR) return;

    if (net->target_session_index == -1) net->target_session_index = session_idx;

    if (net->state == KTERM_NET_STATE_RESOLVING) {
#ifdef KTERM_USE_LIBSSH
        net->ssh_session = ssh_new();
        if (!net->ssh_session) { KTerm_Net_TriggerError(term, session, net, "Error creating SSH session"); return; }
        ssh_options_set(net->ssh_session, SSH_OPTIONS_HOST, net->host);
        ssh_options_set(net->ssh_session, SSH_OPTIONS_PORT, &net->port);
        ssh_options_set(net->ssh_session, SSH_OPTIONS_USER, net->user);
        int rc = ssh_connect(net->ssh_session);
        if (rc != SSH_OK) { KTerm_Net_TriggerError(term, session, net, ssh_get_error(net->ssh_session)); return; }
        net->state = KTERM_NET_STATE_AUTH;
        rc = ssh_userauth_none(net->ssh_session, NULL);
        if (rc == SSH_AUTH_SUCCESS) {
            net->state = KTERM_NET_STATE_CONNECTED;
            if (net->callbacks.on_connect) net->callbacks.on_connect(term, session);
        }
#else
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", net->port);
        if (getaddrinfo(net->host, port_str, &hints, &res) != 0) { KTerm_Net_TriggerError(term, session, net, "DNS Failed"); return; }
        net->socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (!IS_VALID_SOCKET(net->socket_fd)) { KTerm_Net_TriggerError(term, session, net, "Socket Failed"); freeaddrinfo(res); return; }
#ifdef _WIN32
        u_long mode = 1; ioctlsocket(net->socket_fd, FIONBIO, &mode);
#else
        fcntl(net->socket_fd, F_SETFL, fcntl(net->socket_fd, F_GETFL, 0) | O_NONBLOCK);
#endif
        if (connect(net->socket_fd, res->ai_addr, res->ai_addrlen) == 0) {
            if (net->security.handshake) { net->state = KTERM_NET_STATE_HANDSHAKE; }
            else { net->state = KTERM_NET_STATE_CONNECTED; if (net->callbacks.on_connect) net->callbacks.on_connect(term, session); }
        } else {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK)
#else
            if (errno == EINPROGRESS)
#endif
                net->state = KTERM_NET_STATE_CONNECTING;
            else { KTerm_Net_TriggerError(term, session, net, "Connection Failed"); CLOSE_SOCKET(net->socket_fd); net->socket_fd = INVALID_SOCKET; }
        }
        freeaddrinfo(res);
#endif
    }
    else if (net->state == KTERM_NET_STATE_CONNECTING) {
#ifndef KTERM_USE_LIBSSH
        fd_set wfds; struct timeval tv = {0, 0}; FD_ZERO(&wfds); FD_SET(net->socket_fd, &wfds);
        if (select(net->socket_fd + 1, NULL, &wfds, NULL, &tv) > 0) {
            int opt = 0; socklen_t len = sizeof(opt);
            if (getsockopt(net->socket_fd, SOL_SOCKET, SO_ERROR, (char*)&opt, &len) == 0 && opt == 0) {
                if (net->security.handshake) { net->state = KTERM_NET_STATE_HANDSHAKE; }
                else { net->state = KTERM_NET_STATE_CONNECTED; if (net->callbacks.on_connect) net->callbacks.on_connect(term, session); }
            } else {
                KTerm_Net_TriggerError(term, session, net, "Async Connect Failed"); CLOSE_SOCKET(net->socket_fd); net->socket_fd = INVALID_SOCKET;
            }
        }
#endif
    }
    else if (net->state == KTERM_NET_STATE_HANDSHAKE) {
        if (net->security.handshake) {
            KTermSecResult res = net->security.handshake(net->security.ctx, net->socket_fd);
            if (res == KTERM_SEC_OK) {
                net->state = KTERM_NET_STATE_CONNECTED;
                if (net->callbacks.on_connect) net->callbacks.on_connect(term, session);
            } else if (res == KTERM_SEC_ERROR) {
                KTerm_Net_TriggerError(term, session, net, "Handshake Failed");
            }
        } else { net->state = KTERM_NET_STATE_CONNECTED; }
    }
    else if (net->state == KTERM_NET_STATE_AUTH) {
#ifdef KTERM_USE_LIBSSH
        net->state = KTERM_NET_STATE_CONNECTED;
#endif
    }

    else if (net->state == KTERM_NET_STATE_CONNECTED) {
#ifdef KTERM_USE_LIBSSH
        if (!net->ssh_channel) return;
        char chunk[1024];
        int chunk_len = 0;
        while (net->tx_head != net->tx_tail && chunk_len < 1024) {
            chunk[chunk_len++] = net->tx_buffer[net->tx_tail];
            net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
        }
        if (chunk_len > 0) {
            ssh_channel_write(net->ssh_channel, chunk, chunk_len);
        }
        if (ssh_channel_is_open(net->ssh_channel) && !ssh_channel_is_eof(net->ssh_channel)) {
            char rx[1024];
            int nbytes = ssh_channel_read_nonblocking(net->ssh_channel, rx, sizeof(rx), 0);
            if (nbytes > 0) {
                if (net->callbacks.on_data) net->callbacks.on_data(term, session, rx, nbytes);
                int target = (net->target_session_index != -1) ? net->target_session_index : session_idx;
                for (int i=0; i<nbytes; i++) {
                    KTerm_WriteCharToSession(term, target, rx[i]);
                }
            }
        }
#else
        if (!IS_VALID_SOCKET(net->socket_fd)) return;

        // 1. Write TX Buffer
        // In RAW mode, raw data in buffer. In FRAMED mode, valid mixed frames in buffer.
        // We just flush regardless.
        char chunk[1024];
        int chunk_len = 0;
        while (net->tx_head != net->tx_tail && chunk_len < 1024) {
            chunk[chunk_len++] = net->tx_buffer[net->tx_tail];
            net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
        }
        if (chunk_len > 0) {
            if (net->security.write) net->security.write(net->security.ctx, net->socket_fd, chunk, chunk_len);
            else send(net->socket_fd, chunk, chunk_len, 0);
        }

        // 2. Read RX
        char rx[1024];
        int nbytes = 0;
        if (net->security.read) {
            nbytes = net->security.read(net->security.ctx, net->socket_fd, rx, sizeof(rx));
        } else {
            nbytes = recv(net->socket_fd, rx, sizeof(rx), 0);
        }

        if (nbytes > 0) {
            if (net->protocol == KTERM_NET_PROTO_FRAMED) {
                for (int i = 0; i < nbytes; i++) {
                    if (net->rx_len < NET_BUFFER_SIZE) {
                        net->rx_buffer[net->rx_len++] = rx[i];
                    }

                    if (net->expected_frame_len == 0) {
                        if (net->rx_len >= 5) {
                            uint32_t len = ((uint8_t)net->rx_buffer[1] << 24) |
                                           ((uint8_t)net->rx_buffer[2] << 16) |
                                           ((uint8_t)net->rx_buffer[3] << 8) |
                                           (uint8_t)net->rx_buffer[4];

                            // Check against buffer limits (NET_BUFFER_SIZE - 5 for header)
                            if (len > NET_BUFFER_SIZE - 5) {
                                KTerm_Net_TriggerError(term, session, net, "Packet too large");
                                CLOSE_SOCKET(net->socket_fd);
                                net->socket_fd = INVALID_SOCKET;
                                net->state = KTERM_NET_STATE_ERROR;
                                return;
                            }
                            net->expected_frame_len = len;
                        }
                    }

                    if (net->expected_frame_len > 0) {
                        if (net->rx_len >= 5 + net->expected_frame_len) {
                            uint8_t type = net->rx_buffer[0];
                            KTerm_Net_ProcessFrame(term, session, net, type, net->rx_buffer + 5, net->expected_frame_len);

                            int frame_total = 5 + net->expected_frame_len;
                            int remaining = net->rx_len - frame_total;
                            if (remaining > 0) {
                                memmove(net->rx_buffer, net->rx_buffer + frame_total, remaining);
                            }
                            net->rx_len = remaining;
                            net->expected_frame_len = 0;
                        }
                    }
                }
            } else {
                if (net->callbacks.on_data) net->callbacks.on_data(term, session, rx, nbytes);
                int target = (net->target_session_index != -1) ? net->target_session_index : session_idx;
                for (int i=0; i<nbytes; i++) {
                    KTerm_WriteCharToSession(term, target, rx[i]);
                }
            }
        } else if (nbytes == 0 && !net->security.read) {
            KTerm_Net_Log(term, session_idx, "Connection Closed");
            net->state = KTERM_NET_STATE_DISCONNECTED;
            if (net->callbacks.on_disconnect) net->callbacks.on_disconnect(term, session);
            CLOSE_SOCKET(net->socket_fd);
            net->socket_fd = INVALID_SOCKET;
        }
#endif
    }
}

void KTerm_Net_Process(KTerm* term) {
    if (!term) return;
    for (int i = 0; i < 4; i++) {
        KTerm_Net_ProcessSession(term, i);
    }
}

#endif // KTERM_NET_IMPLEMENTATION
