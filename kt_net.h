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
    KTERM_NET_STATE_LISTENING,   // Server Mode
    KTERM_NET_STATE_HANDSHAKE,
    KTERM_NET_STATE_AUTH,        // Client or Server Auth
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

    // Telnet Negotiation Callback (Return true to handle, false for default rejection)
    bool (*on_telnet_command)(KTerm* term, KTermSession* session, unsigned char command, unsigned char option);

    // Telnet Subnegotiation Callback
    void (*on_telnet_sb)(KTerm* term, KTermSession* session, unsigned char option, const char* data, size_t len);

    // Server-Side Auth Callback (Return true if valid)
    bool (*on_auth)(KTerm* term, KTermSession* session, const char* user, const char* pass);

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
    KTermSecResult (*handshake)(void* ctx, KTermSession* session, int socket_fd);
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
    KTERM_NET_PROTO_FRAMED = 1,
    KTERM_NET_PROTO_TELNET = 2
} KTermNetProtocol;

// Packet Types for Framed Mode
#define KTERM_PKT_DATA    0x01
#define KTERM_PKT_RESIZE  0x02 // Payload: [Width:4][Height:4] (Big Endian)
#define KTERM_PKT_GATEWAY 0x03 // Payload: Gateway Command String
#define KTERM_PKT_ATTACH  0x04 // Payload: [SessionID:1]

// Telnet Commands (RFC 854)
#define KTERM_TELNET_SE   240
#define KTERM_TELNET_NOP  241
#define KTERM_TELNET_DM   242
#define KTERM_TELNET_BRK  243
#define KTERM_TELNET_IP   244
#define KTERM_TELNET_AO   245
#define KTERM_TELNET_AYT  246
#define KTERM_TELNET_EC   247
#define KTERM_TELNET_EL   248
#define KTERM_TELNET_GA   249
#define KTERM_TELNET_SB   250
#define KTERM_TELNET_WILL 251
#define KTERM_TELNET_WONT 252
#define KTERM_TELNET_DO   253
#define KTERM_TELNET_DONT 254
#define KTERM_TELNET_IAC  255
#define KTERM_TELNET_ECHO 1

// Initialization
void KTerm_Net_Init(KTerm* term);
void KTerm_Net_Process(KTerm* term);

// API for Gateway Integration
void KTerm_Net_Connect(KTerm* term, KTermSession* session, const char* host, int port, const char* user, const char* password);
void KTerm_Net_Disconnect(KTerm* term, KTermSession* session);
void KTerm_Net_GetStatus(KTerm* term, KTermSession* session, char* buffer, size_t max_len);
void KTerm_Net_GetCredentials(KTerm* term, KTermSession* session, char* user_out, size_t user_max, char* pass_out, size_t pass_max);

// Server API
void KTerm_Net_Listen(KTerm* term, KTermSession* session, int port);

// Async API / Hardening
void KTerm_Net_SetCallbacks(KTerm* term, KTermSession* session, KTermNetCallbacks callbacks);
void KTerm_Net_SetSecurity(KTerm* term, KTermSession* session, KTermNetSecurity security);
void KTerm_Net_SetProtocol(KTerm* term, KTermSession* session, KTermNetProtocol protocol);
void KTerm_Net_SetKeepAlive(KTerm* term, KTermSession* session, bool enable, int idle_sec);
intptr_t KTerm_Net_GetSocket(KTerm* term, KTermSession* session); // Returns socket_fd or -1

// Session Control
void KTerm_Net_SetTargetSession(KTerm* term, KTermSession* session, int target_idx);

// Send Framed Packet (Helper)
void KTerm_Net_SendPacket(KTerm* term, KTermSession* session, uint8_t type, const void* payload, size_t len);

// Send Telnet Command (Helper)
void KTerm_Net_SendTelnetCommand(KTerm* term, KTermSession* session, uint8_t command, uint8_t option);

#ifdef __cplusplus
}
#endif

#endif // KT_NET_H

#ifdef KTERM_NET_IMPLEMENTATION
#ifndef KTERM_NET_IMPLEMENTATION_GUARD
#define KTERM_NET_IMPLEMENTATION_GUARD

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

// Telnet Parse State
typedef enum {
    TELNET_STATE_NORMAL = 0,
    TELNET_STATE_IAC,
    TELNET_STATE_WILL,
    TELNET_STATE_WONT,
    TELNET_STATE_DO,
    TELNET_STATE_DONT,
    TELNET_STATE_SB,
    TELNET_STATE_SB_IAC
} TelnetParseState;

// Auth State for Server
typedef enum {
    AUTH_STATE_NONE = 0,
    AUTH_STATE_USER,
    AUTH_STATE_PASS
} NetAuthState;

typedef struct {
    KTermNetState state;
    char host[256];
    int port;
    char user[64];
    char password[64];

    socket_t socket_fd;
    socket_t listener_fd; // For Server Mode
    bool is_server;

#ifdef KTERM_USE_LIBSSH
    ssh_session ssh_session;
    ssh_channel ssh_channel;
#endif

    // TX Buffer (Terminal -> Network)
    char tx_buffer[NET_BUFFER_SIZE];
    int tx_head;
    int tx_tail;

    // RX Buffer
    char rx_buffer[NET_BUFFER_SIZE];
    int rx_len;
    int expected_frame_len;

    KTermNetCallbacks callbacks;
    KTermNetSecurity security;
    KTermNetProtocol protocol;

    bool keep_alive;
    int keep_alive_idle;

    // Telnet State
    TelnetParseState telnet_state;
    char sb_buffer[1024];
    int sb_len;
    unsigned char sb_option;

    // Auth State
    NetAuthState auth_state;
    char auth_input_buf[64];
    int auth_input_len;
    char auth_user_temp[64];

    int target_session_index;

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
        net->listener_fd = INVALID_SOCKET;
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
    if (IS_VALID_SOCKET(net->listener_fd)) {
        CLOSE_SOCKET(net->listener_fd);
    }
    free(net);
    session->user_data = NULL;
}

static void KTerm_Net_Log(KTerm* term, int session_idx, const char* msg) {
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

static void KTerm_Net_ProcessFrame(KTerm* term, KTermSession* session, KTermNetSession* net, uint8_t type, const char* payload, size_t len) {
    int target_idx = (net->target_session_index != -1) ? net->target_session_index : (int)(session - term->sessions);

    if (type == KTERM_PKT_DATA) {
        if (net->callbacks.on_data) net->callbacks.on_data(term, session, payload, len);
        if (!net->is_server) {
            for (size_t i = 0; i < len; i++) {
                KTerm_WriteCharToSession(term, target_idx, payload[i]);
            }
        }
    }
    else if (type == KTERM_PKT_RESIZE && len >= 8) {
        uint32_t w = ((uint8_t)payload[0] << 24) | ((uint8_t)payload[1] << 16) | ((uint8_t)payload[2] << 8) | (uint8_t)payload[3];
        uint32_t h = ((uint8_t)payload[4] << 24) | ((uint8_t)payload[5] << 16) | ((uint8_t)payload[6] << 8) | (uint8_t)payload[7];
        KTerm_Resize(term, (int)w, (int)h);
        KTerm_Net_Log(term, (int)(session - term->sessions), "Remote Resize Request Applied");
    }
    else if (type == KTERM_PKT_GATEWAY) {
        // Inject Gateway Command
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
    else if (type == KTERM_PKT_ATTACH && len >= 1) {
        int new_session_id = (unsigned char)payload[0];
        if (new_session_id >= 0 && new_session_id < 4) {
            net->target_session_index = new_session_id;
            char msg[64];
            snprintf(msg, sizeof(msg), "Attached to Session %d", new_session_id);
            KTerm_Net_Log(term, (int)(session - term->sessions), msg);
        }
    }
}

void KTerm_Net_SendTelnetCommand(KTerm* term, KTermSession* session, uint8_t command, uint8_t option) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net) return;
    char buf[3];
    buf[0] = (char)KTERM_TELNET_IAC; buf[1] = (char)command; buf[2] = (char)option;
    for(int i=0; i<3; i++) {
        net->tx_buffer[net->tx_head] = buf[i];
        net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE;
        if(net->tx_head == net->tx_tail) net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
    }
}

static void KTerm_Net_Sink(void* user_data, KTermSession* session, const char* data, size_t len) {
    KTerm* term = (KTerm*)user_data;
    if (!term || !session) return;

    KTermNetSession* net = KTerm_Net_GetContext(session);

    if (net && (net->state == KTERM_NET_STATE_CONNECTED || net->state == KTERM_NET_STATE_AUTH)) {
        if (net->protocol == KTERM_NET_PROTO_FRAMED) {
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
        else if (net->protocol == KTERM_NET_PROTO_TELNET) {
            for (size_t i = 0; i < len; i++) {
                net->tx_buffer[net->tx_head] = data[i];
                net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE;
                if (net->tx_head == net->tx_tail) net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
                if ((unsigned char)data[i] == KTERM_TELNET_IAC) {
                    net->tx_buffer[net->tx_head] = (char)KTERM_TELNET_IAC;
                    net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE;
                    if (net->tx_head == net->tx_tail) net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
                }
            }
            return;
        }

        for (size_t i = 0; i < len; i++) {
            net->tx_buffer[net->tx_head] = data[i];
            net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE;
            if (net->tx_head == net->tx_tail) net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
        }
    } else {
        if (term->response_callback) term->response_callback(term, data, (int)len);
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
    net->is_server = false;
    net->tx_head = 0; net->tx_tail = 0;
    net->rx_len = 0; net->expected_frame_len = 0;
    net->telnet_state = TELNET_STATE_NORMAL;
    net->target_session_index = (int)(session - term->sessions);
}

void KTerm_Net_Listen(KTerm* term, KTermSession* session, int port) {
    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (!net) return;

    // Clean up existing if any (except preserve callbacks/config if desired? No, full reset safer)
    // But we want to preserve callbacks set by user.
    // Reset network state only.
    if (IS_VALID_SOCKET(net->listener_fd)) { CLOSE_SOCKET(net->listener_fd); }
    if (IS_VALID_SOCKET(net->socket_fd)) { CLOSE_SOCKET(net->socket_fd); }

    // Zero out buffers and state
    net->tx_head = 0; net->tx_tail = 0;
    net->rx_len = 0; net->expected_frame_len = 0;
    net->telnet_state = TELNET_STATE_NORMAL;
    net->auth_state = AUTH_STATE_NONE;
    net->socket_fd = INVALID_SOCKET;
    net->listener_fd = INVALID_SOCKET;
    net->is_server = true;
    net->port = port;
    net->target_session_index = (int)(session - term->sessions);

    net->listener_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (!IS_VALID_SOCKET(net->listener_fd)) { KTerm_Net_TriggerError(term, session, net, "Socket Creation Failed"); return; }

    int opt = 1;
    setsockopt(net->listener_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(net->listener_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        KTerm_Net_TriggerError(term, session, net, "Bind Failed");
        CLOSE_SOCKET(net->listener_fd); net->listener_fd = INVALID_SOCKET;
        return;
    }

    if (listen(net->listener_fd, 1) < 0) {
        KTerm_Net_TriggerError(term, session, net, "Listen Failed");
        CLOSE_SOCKET(net->listener_fd); net->listener_fd = INVALID_SOCKET;
        return;
    }

#ifdef _WIN32
    u_long mode = 1; ioctlsocket(net->listener_fd, FIONBIO, &mode);
#else
    fcntl(net->listener_fd, F_SETFL, fcntl(net->listener_fd, F_GETFL, 0) | O_NONBLOCK);
#endif

    net->state = KTERM_NET_STATE_LISTENING;
    KTerm_Net_Log(term, (int)(session - term->sessions), "Listening...");
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
            case KTERM_NET_STATE_LISTENING: state_str = "LISTENING"; break;
            case KTERM_NET_STATE_HANDSHAKE: state_str = "HANDSHAKE"; break;
            case KTERM_NET_STATE_AUTH: state_str = "AUTH"; break;
            case KTERM_NET_STATE_CONNECTED: state_str = "CONNECTED"; break;
            case KTERM_NET_STATE_ERROR: state_str = "ERROR"; break;
            default: break;
        }
    }
    snprintf(buffer, max_len, "STATE=%s", state_str);
}

void KTerm_Net_GetCredentials(KTerm* term, KTermSession* session, char* user_out, size_t user_max, char* pass_out, size_t pass_max) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (net) {
        if (user_out && user_max > 0) {
            strncpy(user_out, net->user, user_max - 1);
            user_out[user_max - 1] = '\0';
        }
        if (pass_out && pass_max > 0) {
            strncpy(pass_out, net->password, pass_max - 1);
            pass_out[pass_max - 1] = '\0';
        }
    }
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

void KTerm_Net_SetKeepAlive(KTerm* term, KTermSession* session, bool enable, int idle_sec) {
    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (net) {
        net->keep_alive = enable;
        net->keep_alive_idle = idle_sec;
    }
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
    }
}

void KTerm_Net_SendPacket(KTerm* term, KTermSession* session, uint8_t type, const void* payload, size_t len) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || net->protocol != KTERM_NET_PROTO_FRAMED) return;

    uint8_t header[5];
    header[0] = type;
    header[1] = (len >> 24) & 0xFF; header[2] = (len >> 16) & 0xFF; header[3] = (len >> 8) & 0xFF; header[4] = len & 0xFF;

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

void KTerm_Net_Init(KTerm* term) {
    if (!term) return;
    KTerm_SetOutputSink(term, KTerm_Net_Sink, term);
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

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
        // ... (Standard Client Connect Logic) ...
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", net->port);
        int gai_err = getaddrinfo(net->host, port_str, &hints, &res);
        if (gai_err != 0) {
            char err_buf[256];
            snprintf(err_buf, sizeof(err_buf), "DNS Failed: %s", gai_strerror(gai_err));
            KTerm_Net_TriggerError(term, session, net, err_buf); return;
        }
        net->socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (!IS_VALID_SOCKET(net->socket_fd)) { KTerm_Net_TriggerError(term, session, net, "Socket Failed"); freeaddrinfo(res); return; }
#ifdef _WIN32
        u_long mode = 1; ioctlsocket(net->socket_fd, FIONBIO, &mode);
#else
        fcntl(net->socket_fd, F_SETFL, fcntl(net->socket_fd, F_GETFL, 0) | O_NONBLOCK);
#endif
        if (net->keep_alive) {
            int opt = 1; setsockopt(net->socket_fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&opt, sizeof(opt));
#ifdef TCP_KEEPIDLE
            if (net->keep_alive_idle > 0) { int idle = net->keep_alive_idle; setsockopt(net->socket_fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)); }
#endif
        }
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
    }
    else if (net->state == KTERM_NET_STATE_LISTENING) {
        // Accept incoming connection
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client = accept(net->listener_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client >= 0) {
            if (IS_VALID_SOCKET(net->socket_fd)) CLOSE_SOCKET(net->socket_fd); // Should not happen in 1:1 mode but safety
            net->socket_fd = client;

#ifdef _WIN32
            u_long mode = 1; ioctlsocket(net->socket_fd, FIONBIO, &mode);
#else
            fcntl(net->socket_fd, F_SETFL, fcntl(net->socket_fd, F_GETFL, 0) | O_NONBLOCK);
#endif

            KTerm_Net_Log(term, session_idx, "Client Connected");

            // Negotiate Echo if Telnet
            if (net->protocol == KTERM_NET_PROTO_TELNET) {
                KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_WILL, KTERM_TELNET_ECHO);
            }

            if (net->callbacks.on_auth) {
                net->state = KTERM_NET_STATE_AUTH;
                net->auth_state = AUTH_STATE_USER;
                net->auth_input_len = 0;
                // Send Login Prompt
                const char* msg = "\r\nLogin: ";
                for(int i=0; msg[i]; i++) {
                    net->tx_buffer[net->tx_head] = msg[i];
                    net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE;
                }
            } else {
                net->state = KTERM_NET_STATE_CONNECTED;
                if (net->callbacks.on_connect) net->callbacks.on_connect(term, session);
            }
        }
    }
    else if (net->state == KTERM_NET_STATE_HANDSHAKE) {
        if (net->security.handshake) {
            KTermSecResult res = net->security.handshake(net->security.ctx, session, net->socket_fd);
            if (res == KTERM_SEC_OK) {
                net->state = KTERM_NET_STATE_CONNECTED;
                if (net->callbacks.on_connect) net->callbacks.on_connect(term, session);
            } else if (res == KTERM_SEC_ERROR) {
                KTerm_Net_TriggerError(term, session, net, "Handshake Failed");
            }
        } else { net->state = KTERM_NET_STATE_CONNECTED; }
    }
    else if (net->state == KTERM_NET_STATE_AUTH) {
        // Server Side Authentication Handling
        // We need to read from socket to process Auth, so we fall through to CONNECTED logic but with state check
    }

    // Combined Read/Write for CONNECTED and AUTH
    if (net->state == KTERM_NET_STATE_CONNECTED || net->state == KTERM_NET_STATE_AUTH) {
#ifdef KTERM_USE_LIBSSH
        if (net->ssh_channel) {
            char chunk[1024]; int chunk_len = 0;
            while (net->tx_head != net->tx_tail && chunk_len < 1024) {
                chunk[chunk_len++] = net->tx_buffer[net->tx_tail];
                net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
            }
            if (chunk_len > 0) ssh_channel_write(net->ssh_channel, chunk, chunk_len);
            if (ssh_channel_is_open(net->ssh_channel) && !ssh_channel_is_eof(net->ssh_channel)) {
                char rx[1024];
                int nbytes = ssh_channel_read_nonblocking(net->ssh_channel, rx, sizeof(rx), 0);
                if (nbytes > 0) {
                    if (net->callbacks.on_data) net->callbacks.on_data(term, session, rx, nbytes);
                    int target = (net->target_session_index != -1) ? net->target_session_index : session_idx;
                    for (int i=0; i<nbytes; i++) KTerm_WriteCharToSession(term, target, rx[i]);
                }
            }
            return;
        }
#endif
        if (!IS_VALID_SOCKET(net->socket_fd)) return;

        // 1. Write TX
        char chunk[1024]; int chunk_len = 0;
        while (net->tx_head != net->tx_tail && chunk_len < 1024) {
            chunk[chunk_len++] = net->tx_buffer[net->tx_tail];
            net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
        }
        if (chunk_len > 0) {
            ssize_t sent = -1;
            if (net->security.write) sent = net->security.write(net->security.ctx, net->socket_fd, chunk, chunk_len);
            else sent = send(net->socket_fd, chunk, chunk_len, 0);

            if (sent < 0) {
#ifdef _WIN32
                if (WSAGetLastError() == WSAEWOULDBLOCK)
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK)
#endif
                { net->tx_tail = (net->tx_tail - chunk_len + NET_BUFFER_SIZE) % NET_BUFFER_SIZE; }
                else { KTerm_Net_TriggerError(term, session, net, "Write Failed"); CLOSE_SOCKET(net->socket_fd); net->socket_fd = INVALID_SOCKET; }
            } else if (sent < chunk_len) {
                int unsent = chunk_len - (int)sent;
                net->tx_tail = (net->tx_tail - unsent + NET_BUFFER_SIZE) % NET_BUFFER_SIZE;
            }
        }

        // 2. Read RX
        char rx[1024]; int nbytes = 0;
        if (net->security.read) nbytes = net->security.read(net->security.ctx, net->socket_fd, rx, sizeof(rx));
        else nbytes = recv(net->socket_fd, rx, sizeof(rx), 0);

        if (nbytes > 0) {
            if (net->state == KTERM_NET_STATE_AUTH) {
                 // Simple Auth State Machine
                 for(int i=0; i<nbytes; i++) {
                     char c = rx[i];
                     // Handle Telnet Commands even during Auth? Yes, needed for IAC WILL ECHO logic
                     if (net->protocol == KTERM_NET_PROTO_TELNET) {
                         if (net->telnet_state == TELNET_STATE_NORMAL && c == KTERM_TELNET_IAC) { net->telnet_state = TELNET_STATE_IAC; continue; }
                         if (net->telnet_state != TELNET_STATE_NORMAL) {
                             // Process Telnet State machine (Simplified Copy)
                             switch (net->telnet_state) {
                                case TELNET_STATE_IAC:
                                    if (c == KTERM_TELNET_IAC) { /* Literal 255 */ }
                                    else if (c == KTERM_TELNET_DO || c == KTERM_TELNET_DONT || c == KTERM_TELNET_WILL || c == KTERM_TELNET_WONT) {
                                        net->telnet_state = (TelnetParseState)(c - KTERM_TELNET_SE + TELNET_STATE_NORMAL); // Mapping is tricky, let's use direct
                                        if (c==KTERM_TELNET_WILL) net->telnet_state = TELNET_STATE_WILL;
                                        if (c==KTERM_TELNET_WONT) net->telnet_state = TELNET_STATE_WONT;
                                        if (c==KTERM_TELNET_DO) net->telnet_state = TELNET_STATE_DO;
                                        if (c==KTERM_TELNET_DONT) net->telnet_state = TELNET_STATE_DONT;
                                        continue;
                                    } else { net->telnet_state = TELNET_STATE_NORMAL; continue; }
                                    break;
                                case TELNET_STATE_WILL: case TELNET_STATE_WONT: case TELNET_STATE_DO: case TELNET_STATE_DONT:
                                    net->telnet_state = TELNET_STATE_NORMAL; continue;
                                default: net->telnet_state = TELNET_STATE_NORMAL; continue;
                             }
                         }
                     }

                     if (c == '\r' || c == '\n') {
                         net->auth_input_buf[net->auth_input_len] = '\0';
                         if (net->auth_input_len > 0) {
                             if (net->auth_state == AUTH_STATE_USER) {
                                 strncpy(net->auth_user_temp, net->auth_input_buf, 64);
                                 net->auth_state = AUTH_STATE_PASS;
                                 net->auth_input_len = 0;
                                 const char* msg = "\r\nPassword: ";
                                 for(int j=0; msg[j]; j++) { net->tx_buffer[net->tx_head] = msg[j]; net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE; }
                             } else if (net->auth_state == AUTH_STATE_PASS) {
                                 bool ok = false;
                                 if (net->callbacks.on_auth) ok = net->callbacks.on_auth(term, session, net->auth_user_temp, net->auth_input_buf);
                                 if (ok) {
                                     net->state = KTERM_NET_STATE_CONNECTED;
                                     const char* msg = "\r\nWelcome.\r\n";
                                     for(int j=0; msg[j]; j++) { net->tx_buffer[net->tx_head] = msg[j]; net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE; }
                                     if (net->callbacks.on_connect) net->callbacks.on_connect(term, session);
                                 } else {
                                     const char* msg = "\r\nAuth Failed.\r\n";
                                     for(int j=0; msg[j]; j++) { net->tx_buffer[net->tx_head] = msg[j]; net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE; }
                                     net->state = KTERM_NET_STATE_DISCONNECTED; // Or loop back to login?
                                     CLOSE_SOCKET(net->socket_fd); net->socket_fd = INVALID_SOCKET;
                                 }
                             }
                         }
                     } else if (c == 0x7F || c == 0x08) { // Backspace
                         if (net->auth_input_len > 0) {
                             net->auth_input_len--;
                             // Echo BS if echoing
                             if (net->auth_state == AUTH_STATE_USER) {
                                 char bs[] = "\x08 \x08";
                                 for(int j=0; j<3; j++) { net->tx_buffer[net->tx_head] = bs[j]; net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE; }
                             }
                         }
                     } else {
                         if (net->auth_input_len < 63) {
                             net->auth_input_buf[net->auth_input_len++] = c;
                             if (net->auth_state == AUTH_STATE_USER) {
                                 // Echo user
                                 net->tx_buffer[net->tx_head] = c; net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE;
                             }
                         }
                     }
                 }
                 return;
            }

            if (net->protocol == KTERM_NET_PROTO_FRAMED) {
                 // ... (Framed logic kept mostly same) ...
                 for (int i = 0; i < nbytes; i++) {
                    if (net->rx_len < NET_BUFFER_SIZE) {
                        net->rx_buffer[net->rx_len++] = rx[i];
                    }
                    if (net->expected_frame_len == 0 && net->rx_len >= 5) {
                         uint32_t len = ((uint8_t)net->rx_buffer[1] << 24) | ((uint8_t)net->rx_buffer[2] << 16) | ((uint8_t)net->rx_buffer[3] << 8) | (uint8_t)net->rx_buffer[4];
                         if (len > NET_BUFFER_SIZE - 5) { KTerm_Net_TriggerError(term, session, net, "Packet too large"); CLOSE_SOCKET(net->socket_fd); return; }
                         net->expected_frame_len = len;
                    }
                    if (net->expected_frame_len > 0 && net->rx_len >= 5 + net->expected_frame_len) {
                        KTerm_Net_ProcessFrame(term, session, net, net->rx_buffer[0], net->rx_buffer + 5, net->expected_frame_len);
                        int frame_total = 5 + net->expected_frame_len;
                        int remaining = net->rx_len - frame_total;
                        if (remaining > 0) memmove(net->rx_buffer, net->rx_buffer + frame_total, remaining);
                        net->rx_len = remaining; net->expected_frame_len = 0;
                    }
                 }
            } else if (net->protocol == KTERM_NET_PROTO_TELNET) {
                if (net->callbacks.on_data) net->callbacks.on_data(term, session, rx, nbytes);
                int target = (net->target_session_index != -1) ? net->target_session_index : session_idx;

                for (int i = 0; i < nbytes; i++) {
                    unsigned char c = (unsigned char)rx[i];

                    switch (net->telnet_state) {
                        case TELNET_STATE_NORMAL:
                            if (c == KTERM_TELNET_IAC) { net->telnet_state = TELNET_STATE_IAC; }
                            else { if (!net->is_server) KTerm_WriteCharToSession(term, target, c); }
                            break;

                        case TELNET_STATE_IAC:
                            if (c == KTERM_TELNET_IAC) { if (!net->is_server) KTerm_WriteCharToSession(term, target, c); net->telnet_state = TELNET_STATE_NORMAL; }
                            else if (c == KTERM_TELNET_WILL) { net->telnet_state = TELNET_STATE_WILL; }
                            else if (c == KTERM_TELNET_WONT) { net->telnet_state = TELNET_STATE_WONT; }
                            else if (c == KTERM_TELNET_DO) { net->telnet_state = TELNET_STATE_DO; }
                            else if (c == KTERM_TELNET_DONT) { net->telnet_state = TELNET_STATE_DONT; }
                            else if (c == KTERM_TELNET_SB) { net->telnet_state = TELNET_STATE_SB; net->sb_len = 0; }
                            else { net->telnet_state = TELNET_STATE_NORMAL; }
                            break;

                        case TELNET_STATE_WILL:
                            if (!net->callbacks.on_telnet_command || !net->callbacks.on_telnet_command(term, session, KTERM_TELNET_WILL, c)) {
                                KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_DONT, c);
                            }
                            net->telnet_state = TELNET_STATE_NORMAL;
                            break;

                        case TELNET_STATE_WONT:
                            if (net->callbacks.on_telnet_command) net->callbacks.on_telnet_command(term, session, KTERM_TELNET_WONT, c);
                            net->telnet_state = TELNET_STATE_NORMAL;
                            break;

                        case TELNET_STATE_DO:
                            if (!net->callbacks.on_telnet_command || !net->callbacks.on_telnet_command(term, session, KTERM_TELNET_DO, c)) {
                                KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_WONT, c);
                            }
                            net->telnet_state = TELNET_STATE_NORMAL;
                            break;

                        case TELNET_STATE_DONT:
                            if (net->callbacks.on_telnet_command) net->callbacks.on_telnet_command(term, session, KTERM_TELNET_DONT, c);
                            net->telnet_state = TELNET_STATE_NORMAL;
                            break;

                        case TELNET_STATE_SB:
                            if (c == KTERM_TELNET_IAC) { net->telnet_state = TELNET_STATE_SB_IAC; }
                            else {
                                if (net->sb_len == 0) net->sb_option = c;
                                else if (net->sb_len < 1024) net->sb_buffer[net->sb_len] = c;
                                net->sb_len++;
                            }
                            break;

                        case TELNET_STATE_SB_IAC:
                            if (c == KTERM_TELNET_SE) {
                                // End of SB
                                if (net->sb_len > 0) {
                                    if (net->callbacks.on_telnet_sb) {
                                        net->callbacks.on_telnet_sb(term, session, net->sb_option, net->sb_buffer + 1, net->sb_len - 1);
                                    }
                                    // Default NEW-ENVIRON handling
                                    if (net->sb_option == 39) { // NEW-ENVIRON
                                        // Simple Parsing logic could go here or user callback
                                        // For now we assume user callback handles it if set
                                    }
                                }
                                net->telnet_state = TELNET_STATE_NORMAL;
                            } else if (c == KTERM_TELNET_IAC) {
                                // Escaped IAC in SB data
                                if (net->sb_len < 1024) net->sb_buffer[net->sb_len++] = c;
                                net->telnet_state = TELNET_STATE_SB;
                            } else {
                                // Malformed? Back to SB
                                net->telnet_state = TELNET_STATE_SB;
                            }
                            break;
                    }
                }
            } else {
                if (net->callbacks.on_data) net->callbacks.on_data(term, session, rx, nbytes);
                int target = (net->target_session_index != -1) ? net->target_session_index : session_idx;
                if (!net->is_server) {
                    for (int i=0; i<nbytes; i++) {
                        KTerm_WriteCharToSession(term, target, rx[i]);
                    }
                }
            }
        } else if (nbytes == 0 && !net->security.read) {
            KTerm_Net_Log(term, session_idx, "Connection Closed");
            net->state = KTERM_NET_STATE_DISCONNECTED;
            if (net->callbacks.on_disconnect) net->callbacks.on_disconnect(term, session);
            CLOSE_SOCKET(net->socket_fd);
            net->socket_fd = INVALID_SOCKET;
        } else if (nbytes < 0) {
#ifdef _WIN32
             if (WSAGetLastError() != WSAEWOULDBLOCK)
#else
             if (errno != EAGAIN && errno != EWOULDBLOCK)
#endif
             {
                 KTerm_Net_TriggerError(term, session, net, "Read Error");
                 CLOSE_SOCKET(net->socket_fd);
                 net->socket_fd = INVALID_SOCKET;
             }
        }
    }
}

void KTerm_Net_Process(KTerm* term) {
    if (!term) return;
    for (int i = 0; i < 4; i++) {
        KTerm_Net_ProcessSession(term, i);
    }
}

#endif // KTERM_NET_IMPLEMENTATION_GUARD
#endif // KTERM_NET_IMPLEMENTATION
