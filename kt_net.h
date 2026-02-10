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

// Initialization
void KTerm_Net_Init(KTerm* term);
void KTerm_Net_Process(KTerm* term);

// API for Gateway Integration
void KTerm_Net_Connect(KTerm* term, KTermSession* session, const char* host, int port, const char* user, const char* password);
void KTerm_Net_Disconnect(KTerm* term, KTermSession* session);
void KTerm_Net_GetStatus(KTerm* term, KTermSession* session, char* buffer, size_t max_len);

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

    // RX Buffer (Network -> Terminal) handled directly via KTerm_WriteCharToSession usually
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
        session->user_data = net;
    }
    return net;
}

static void KTerm_Net_DestroyContext(KTermSession* session) {
    if (!session || !session->user_data) return;
    KTermNetSession* net = (KTermNetSession*)session->user_data;

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

// --- Output Sink (Term -> Net) ---
static void KTerm_Net_Sink(void* user_data, KTermSession* session, const char* data, size_t len) {
    KTerm* term = (KTerm*)user_data;
    if (!term || !session) return;

    KTermNetSession* net = KTerm_Net_GetContext(session);

    if (net && (net->state == KTERM_NET_STATE_CONNECTED || net->state == KTERM_NET_STATE_AUTH)) {
        for (size_t i = 0; i < len; i++) {
            net->tx_buffer[net->tx_head] = data[i];
            net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE;
            if (net->tx_head == net->tx_tail) {
                net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE; // Drop oldest
            }
        }
    } else {
        // Fallback to response_callback if not connected
        if (term->response_callback) {
            term->response_callback(term, data, (int)len);
        }
    }
}

// --- API Implementation ---

void KTerm_Net_Connect(KTerm* term, KTermSession* session, const char* host, int port, const char* user, const char* password) {
    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (!net) return;

    // Clean previous state
    if (IS_VALID_SOCKET(net->socket_fd)) { CLOSE_SOCKET(net->socket_fd); net->socket_fd = INVALID_SOCKET; }
#ifdef KTERM_USE_LIBSSH
    if (net->ssh_channel) { ssh_channel_free(net->ssh_channel); net->ssh_channel = NULL; }
    if (net->ssh_session) { ssh_free(net->ssh_session); net->ssh_session = NULL; }
#endif

    strncpy(net->user, user ? user : "root", sizeof(net->user)-1);
    net->port = (port > 0) ? port : 22;
    strncpy(net->host, host, sizeof(net->host)-1);

    // Optional password storage (simplified)
    if (password) strncpy(net->password, password, sizeof(net->password)-1);
    else net->password[0] = '\0';

    net->state = KTERM_NET_STATE_RESOLVING;
    net->tx_head = 0;
    net->tx_tail = 0;
}

void KTerm_Net_Disconnect(KTerm* term, KTermSession* session) {
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

// --- Initialization ---

void KTerm_Net_Init(KTerm* term) {
    if (!term) return;
    // Note: We do NOT register the gateway extension here anymore.
    // That is now handled by kt_gateway.h using KTerm_Net_* API.
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

    // --- State Machine ---

    if (net->state == KTERM_NET_STATE_RESOLVING) {
#ifdef KTERM_USE_LIBSSH
        // Use libssh to connect
        net->ssh_session = ssh_new();
        if (!net->ssh_session) {
            KTerm_Net_Log(term, session_idx, "Error creating SSH session");
            net->state = KTERM_NET_STATE_ERROR;
            return;
        }
        ssh_options_set(net->ssh_session, SSH_OPTIONS_HOST, net->host);
        ssh_options_set(net->ssh_session, SSH_OPTIONS_PORT, &net->port);
        ssh_options_set(net->ssh_session, SSH_OPTIONS_USER, net->user);

        KTerm_Net_Log(term, session_idx, "SSH: Connecting...");
        int rc = ssh_connect(net->ssh_session);
        if (rc != SSH_OK) {
            KTerm_Net_Log(term, session_idx, ssh_get_error(net->ssh_session));
            net->state = KTERM_NET_STATE_ERROR;
            return;
        }

        // Connected TCP/Handshake
        net->state = KTERM_NET_STATE_AUTH; // LibSSH handles Kex internally in connect usually
        KTerm_Net_Log(term, session_idx, "SSH: Connected. Authenticating...");

        // Try None/Pubkey first?
        rc = ssh_userauth_none(net->ssh_session, NULL);
        if (rc == SSH_AUTH_SUCCESS) {
            net->state = KTERM_NET_STATE_CONNECTED;
        } else {
            // Need password
            KTerm_Net_Log(term, session_idx, "SSH: Password required.");
            // We stay in AUTH state and wait for password in tx_buffer
        }
#else
        // Fallback TCP Socket
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", net->port);

        int status = getaddrinfo(net->host, port_str, &hints, &res);
        if (status != 0) {
            KTerm_Net_Log(term, session_idx, "DNS Resolution Failed");
            fprintf(stderr, "[KT_NET] DNS Resolution Failed: %s\n", gai_strerror(status));
            net->state = KTERM_NET_STATE_ERROR;
            return;
        }

        net->socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (!IS_VALID_SOCKET(net->socket_fd)) {
            KTerm_Net_Log(term, session_idx, "Socket Creation Failed");
            perror("[KT_NET] Socket Creation Failed");
            freeaddrinfo(res);
            net->state = KTERM_NET_STATE_ERROR;
            return;
        }

        // Non-blocking Connect
        KTerm_Net_Log(term, session_idx, "Connecting (TCP)...");

        // Set Non-Blocking immediately
#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(net->socket_fd, FIONBIO, &mode);
#else
        int flags = fcntl(net->socket_fd, F_GETFL, 0);
        fcntl(net->socket_fd, F_SETFL, flags | O_NONBLOCK);
#endif

        int connect_res = connect(net->socket_fd, res->ai_addr, res->ai_addrlen);
        if (connect_res == 0) {
            KTerm_Net_Log(term, session_idx, "Connected (TCP)");
            net->state = KTERM_NET_STATE_CONNECTED;
        } else {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
#else
            if (errno == EINPROGRESS) {
#endif
                net->state = KTERM_NET_STATE_CONNECTING;
            } else {
                KTerm_Net_Log(term, session_idx, "Connection Failed");
                perror("[KT_NET] Connection Failed");
                CLOSE_SOCKET(net->socket_fd);
                net->socket_fd = INVALID_SOCKET;
                net->state = KTERM_NET_STATE_ERROR;
            }
        }
        freeaddrinfo(res);
#endif
    }

    else if (net->state == KTERM_NET_STATE_CONNECTING) {
#ifndef KTERM_USE_LIBSSH
        fd_set wfds;
        struct timeval tv = {0, 0}; // Immediate poll
        FD_ZERO(&wfds);
        FD_SET(net->socket_fd, &wfds);

        int res = select(net->socket_fd + 1, NULL, &wfds, NULL, &tv);
        if (res > 0) {
            // Check for SO_ERROR
            int optval = 0;
            socklen_t optlen = sizeof(optval);
            if (getsockopt(net->socket_fd, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen) == 0 && optval == 0) {
                KTerm_Net_Log(term, session_idx, "Connected (TCP)");
                net->state = KTERM_NET_STATE_CONNECTED;
            } else {
                KTerm_Net_Log(term, session_idx, "Connection Failed (Async)");
                CLOSE_SOCKET(net->socket_fd);
                net->socket_fd = INVALID_SOCKET;
                net->state = KTERM_NET_STATE_ERROR;
            }
        } else if (res < 0) {
            // Select error
             KTerm_Net_Log(term, session_idx, "Connection Select Error");
             CLOSE_SOCKET(net->socket_fd);
             net->socket_fd = INVALID_SOCKET;
             net->state = KTERM_NET_STATE_ERROR;
        }
        // If res == 0, still connecting
#endif
    }

    else if (net->state == KTERM_NET_STATE_AUTH) {
#ifdef KTERM_USE_LIBSSH
        // Check for password in TX Buffer
        // In AUTH state, we treat input as password
        char pwd_buf[256];
        int pwd_len = 0;

        // Simple line buffered read for password
        while (net->tx_head != net->tx_tail && pwd_len < 255) {
            char c = net->tx_buffer[net->tx_tail];
            net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
            if (c == '\r' || c == '\n') break;
            pwd_buf[pwd_len++] = c;
        }
        pwd_buf[pwd_len] = '\0';

        if (pwd_len > 0) {
            int rc = ssh_userauth_password(net->ssh_session, NULL, pwd_buf);
            if (rc == SSH_AUTH_SUCCESS) {
                KTerm_Net_Log(term, session_idx, "SSH: Authentication Successful");
                net->state = KTERM_NET_STATE_CONNECTED;

                // Open Channel
                net->ssh_channel = ssh_channel_new(net->ssh_session);
                if (ssh_channel_open_session(net->ssh_channel) != SSH_OK) {
                    KTerm_Net_Log(term, session_idx, "SSH: Channel Open Failed");
                    net->state = KTERM_NET_STATE_ERROR;
                    return;
                }
                if (ssh_channel_request_pty(net->ssh_channel) != SSH_OK) {
                    KTerm_Net_Log(term, session_idx, "SSH: PTY Request Failed");
                }
                if (ssh_channel_request_shell(net->ssh_channel) != SSH_OK) {
                    KTerm_Net_Log(term, session_idx, "SSH: Shell Request Failed");
                }

            } else {
                KTerm_Net_Log(term, session_idx, "SSH: Authentication Failed");
            }
        }
#else
        // TCP mode has no auth logic here
#endif
    }

    else if (net->state == KTERM_NET_STATE_CONNECTED) {
#ifdef KTERM_USE_LIBSSH
        if (!net->ssh_channel) return;

        // 1. Write TX Buffer to SSH
        char chunk[1024];
        int chunk_len = 0;
        while (net->tx_head != net->tx_tail && chunk_len < 1024) {
            chunk[chunk_len++] = net->tx_buffer[net->tx_tail];
            net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
        }
        if (chunk_len > 0) {
            ssh_channel_write(net->ssh_channel, chunk, chunk_len);
        }

        // 2. Read from SSH to KTerm
        if (ssh_channel_is_open(net->ssh_channel) && !ssh_channel_is_eof(net->ssh_channel)) {
            char rx[1024];
            int nbytes = ssh_channel_read_nonblocking(net->ssh_channel, rx, sizeof(rx), 0);
            if (nbytes > 0) {
                for (int i=0; i<nbytes; i++) {
                    KTerm_WriteCharToSession(term, session_idx, rx[i]);
                }
            }
        }
#else
        if (!IS_VALID_SOCKET(net->socket_fd)) return;

        // 1. Write TX Buffer to Socket
        char chunk[1024];
        int chunk_len = 0;
        while (net->tx_head != net->tx_tail && chunk_len < 1024) {
            chunk[chunk_len++] = net->tx_buffer[net->tx_tail];
            net->tx_tail = (net->tx_tail + 1) % NET_BUFFER_SIZE;
        }
        if (chunk_len > 0) {
            send(net->socket_fd, chunk, chunk_len, 0);
        }

        // 2. Read from Socket to KTerm
        char rx[1024];
        int nbytes = recv(net->socket_fd, rx, sizeof(rx), 0); // Non-blocking
        if (nbytes > 0) {
            for (int i=0; i<nbytes; i++) {
                KTerm_WriteCharToSession(term, session_idx, rx[i]);
            }
        } else if (nbytes == 0) {
            // Closed
            KTerm_Net_Log(term, session_idx, "Connection Closed");
            net->state = KTERM_NET_STATE_DISCONNECTED;
            CLOSE_SOCKET(net->socket_fd);
            net->socket_fd = INVALID_SOCKET;
        }
#endif
    }
}

void KTerm_Net_Process(KTerm* term) {
    if (!term) return;
    for (int i = 0; i < 4; i++) { // MAX_SESSIONS
        KTerm_Net_ProcessSession(term, i);
    }
}

#endif // KTERM_NET_IMPLEMENTATION
