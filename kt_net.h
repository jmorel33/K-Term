#ifndef KT_NET_H
#define KT_NET_H

#ifndef KTERM_DISABLE_NET

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct KTerm_T KTerm;
typedef struct KTermSession_T KTermSession;

// Context Typedefs
typedef struct KTermTracerouteContext KTermTracerouteContext;
typedef struct KTermResponseTimeContext KTermResponseTimeContext;
typedef struct KTermPortScanContext KTermPortScanContext;
typedef struct KTermWhoisContext KTermWhoisContext;
typedef struct KTermSpeedtestContext KTermSpeedtestContext;
typedef struct KTermHttpProbeContext KTermHttpProbeContext;
typedef struct KTermMtuProbeContext KTermMtuProbeContext;
typedef struct KTermFragTestContext KTermFragTestContext;
typedef struct KTermPingExtContext KTermPingExtContext;
typedef struct KTermLiveWireContext KTermLiveWireContext;

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
    bool (*on_data)(KTerm* term, KTermSession* session, const char* data, size_t len); // Return true if handled/consumed
    void (*on_error)(KTerm* term, KTermSession* session, const char* msg);

#ifndef KTERM_DISABLE_TELNET
    // Telnet Negotiation Callback (Return true to handle, false for default rejection)
    bool (*on_telnet_command)(KTerm* term, KTermSession* session, unsigned char command, unsigned char option);

    // Telnet Subnegotiation Callback
    void (*on_telnet_sb)(KTerm* term, KTermSession* session, unsigned char option, const char* data, size_t len);
#endif

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
#ifndef KTERM_DISABLE_TELNET
    KTERM_NET_PROTO_TELNET = 2
#endif
} KTermNetProtocol;

// Packet Types for Framed Mode
#define KTERM_PKT_DATA    0x01
#define KTERM_PKT_RESIZE  0x02 // Payload: [Width:4][Height:4] (Big Endian)
#define KTERM_PKT_GATEWAY 0x03 // Payload: Gateway Command String
#define KTERM_PKT_ATTACH  0x04 // Payload: [SessionID:1]
#define KTERM_PKT_AUDIO_VOICE   0x10 // Compressed or raw PCM voice data
#define KTERM_PKT_AUDIO_COMMAND 0x11 // Voice command (recognized text or raw waveform)
#define KTERM_PKT_AUDIO_STREAM  0x12 // High-quality audio stream between sessions

#ifndef KTERM_DISABLE_TELNET
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

// Telnet Options (Common)
#define KTERM_TELNET_SGA         3
#define KTERM_TELNET_NAWS       31
#define KTERM_TELNET_ENVIRON    36
#define KTERM_TELNET_NEW_ENVIRON 39
#endif

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
void KTerm_Net_SetAutoReconnect(KTerm* term, KTermSession* session, bool enable, int max_retries, int delay_ms);
intptr_t KTerm_Net_GetSocket(KTerm* term, KTermSession* session); // Returns socket_fd or -1

// Session Control
void KTerm_Net_SetTargetSession(KTerm* term, KTermSession* session, int target_idx);

// Send Framed Packet (Helper)
void KTerm_Net_SendPacket(KTerm* term, KTermSession* session, uint8_t type, const void* payload, size_t len);

#ifndef KTERM_DISABLE_TELNET
// Send Telnet Command (Helper)
void KTerm_Net_SendTelnetCommand(KTerm* term, KTermSession* session, uint8_t command, uint8_t option);
#endif

// Utilities
void KTerm_Net_GetLocalIP(char* buffer, size_t max_len);
void KTerm_Net_Ping(const char* host, char* output, size_t max_len);
bool KTerm_Net_Resolve(const char* host, char* output_ip, size_t max_len); // Sync Resolve
void KTerm_Net_DumpConnections(KTerm* term, char* buffer, size_t max_len); // Detailed socket list

// Traceroute Callback
typedef void (*KTermTracerouteCallback)(KTerm* term, KTermSession* session, int hop, const char* ip, double rtt_ms, bool reached, void* user_data);

// Starts an async traceroute. Use NULL callback for default Gateway output if integrated.
void KTerm_Net_Traceroute(KTerm* term, KTermSession* session, const char* host, int max_hops, int timeout_ms, KTermTracerouteCallback cb, void* user_data);
void KTerm_Net_TracerouteContinuous(KTerm* term, KTermSession* session, const char* host, int max_hops, int timeout_ms, bool continuous, KTermTracerouteCallback cb, void* user_data);

// Response Time / Latency Test Result
typedef struct {
    double min_rtt_ms;
    double avg_rtt_ms;
    double max_rtt_ms;
    double jitter_ms; // Standard Deviation
    int sent;
    int received;
    int lost;
} ResponseTimeResult;

typedef void (*KTermResponseTimeCallback)(KTerm* term, KTermSession* session, const ResponseTimeResult* result, void* user_data);

// Starts an async response time test (Ping-like).
bool KTerm_Net_ResponseTime(KTerm* term, KTermSession* session, const char* host, int count, int interval_ms, int timeout_ms, KTermResponseTimeCallback cb, void* user_data);

// Port Scan Callback
// Status: 0=CLOSED/TIMEOUT, 1=OPEN
typedef void (*KTermPortScanCallback)(KTerm* term, KTermSession* session, const char* host, int port, int status, void* user_data);

// Starts an async port scan. Ports string can be comma separated (e.g., "80,443,22,8080").
// Timeout is per port.
bool KTerm_Net_PortScan(KTerm* term, KTermSession* session, const char* host, const char* ports, int timeout_ms, KTermPortScanCallback cb, void* user_data);

// Whois Callback
typedef void (*KTermWhoisCallback)(KTerm* term, KTermSession* session, const char* data, size_t len, bool done, void* user_data);

// Starts an async whois query.
bool KTerm_Net_Whois(KTerm* term, KTermSession* session, const char* host, const char* query, KTermWhoisCallback cb, void* user_data);

// Speedtest Result
typedef struct {
    double dl_mbps;
    double ul_mbps;
    double jitter_ms;
    bool done;
    // Progress
    double dl_progress;
    double ul_progress;
    int phase; // 0=INIT, 1=DL, 2=UL, 3=DONE
} SpeedtestResult;

typedef void (*KTermSpeedtestCallback)(KTerm* term, KTermSession* session, const SpeedtestResult* result, void* user_data);

// Starts an async speedtest (Download/Upload).
// Streams: number of parallel connections (default 4).
// Path: URL path for download test (default "/100MB.zip").
bool KTerm_Net_Speedtest(KTerm* term, KTermSession* session, const char* host, int port, int streams, const char* path, KTermSpeedtestCallback cb, void* user_data);

// HTTP Probe Result
typedef struct {
    int status_code;
    double dns_ms;
    double connect_ms;
    double ttfb_ms;
    double download_ms;
    double total_ms;
    uint64_t size_bytes;
    double speed_mbps;
    bool error;
    char error_msg[64];
} KTermHttpProbeResult;

typedef void (*KTermHttpProbeCallback)(KTerm* term, KTermSession* session, const KTermHttpProbeResult* result, void* user_data);

// Starts an async HTTP probe.
bool KTerm_Net_HttpProbe(KTerm* term, KTermSession* session, const char* url, KTermHttpProbeCallback cb, void* user_data);

// MTU Probe Result
typedef struct {
    int path_mtu;
    int local_mtu;
    bool done;
    bool error;
    char msg[64];
} KTermMtuProbeResult;

typedef void (*KTermMtuProbeCallback)(KTerm* term, KTermSession* session, const KTermMtuProbeResult* result, void* user_data);

// Starts an async MTU/PMTU probe.
// df: Don't Fragment flag (PMTUD)
// start_size/max_size: Probing range
bool KTerm_Net_MTUProbe(KTerm* term, KTermSession* session, const char* host, bool df, int start_size, int max_size, KTermMtuProbeCallback cb, void* user_data);

// Fragmentation Test Result
typedef struct {
    int fragments_sent;
    bool reassembly_success;
    bool done;
    bool error;
    char msg[64];
} KTermFragTestResult;

typedef void (*KTermFragTestCallback)(KTerm* term, KTermSession* session, const KTermFragTestResult* result, void* user_data);

// Starts an async Fragmentation test.
// size: Total payload size (will be fragmented)
// fragments: Expected number of fragments (informational/target)
bool KTerm_Net_FragTest(KTerm* term, KTermSession* session, const char* host, int size, int fragments, KTermFragTestCallback cb, void* user_data);

// Extended Ping Stats
typedef struct {
    int min_rtt;
    int avg_rtt;
    int max_rtt;
    int stddev_rtt;
    int sent;
    int received;
    int lost;
    float loss_percent;
    // Histogram buckets (0-10, 10-20, 20-50, 50-100, 100+)
    int hist_0_10;
    int hist_10_20;
    int hist_20_50;
    int hist_50_100;
    int hist_100_plus;
    bool done;
    // ASCII Graph line (e.g. ".....X..")
    char graph_line[64];
} KTermPingExtResult;

typedef void (*KTermPingExtCallback)(KTerm* term, KTermSession* session, const KTermPingExtResult* result, void* user_data);

// Starts an async Extended Ping.
// count: Number of packets
// interval_ms: Interval between packets
// size: Packet size
// graph: Enable ASCII graph generation
bool KTerm_Net_PingExt(KTerm* term, KTermSession* session, const char* host, int count, int interval_ms, int size, bool graph, KTermPingExtCallback cb, void* user_data);

// Starts the LiveWire packet sniffer.
// params: Key-value pairs (interface, filter, snaplen, count, promisc, timeout)
bool KTerm_Net_LiveWire_Start(KTerm* term, KTermSession* session, const char* params);
void KTerm_Net_LiveWire_Stop(KTerm* term, KTermSession* session);
void KTerm_Net_LiveWire_GetStatus(KTerm* term, KTermSession* session, char* buffer, size_t max_len);

void KTerm_Net_LiveWire_Pause(KTerm* term, KTermSession* session);
void KTerm_Net_LiveWire_Resume(KTerm* term, KTermSession* session);
bool KTerm_Net_LiveWire_SetFilter(KTerm* term, KTermSession* session, const char* filter);
bool KTerm_Net_LiveWire_GetDetail(KTerm* term, KTermSession* session, int packet_id, char* out, size_t max);
bool KTerm_Net_LiveWire_Follow(KTerm* term, KTermSession* session, uint32_t flow_id);
bool KTerm_Net_LiveWire_GetStats(KTerm* term, KTermSession* session, char* out, size_t max);
bool KTerm_Net_LiveWire_GetFlows(KTerm* term, KTermSession* session, char* out, size_t max);

// Cleanup Functions (Internal/Advanced use)
void KTerm_Net_FreeTraceroute(KTermTracerouteContext* ctx);
void KTerm_Net_FreeResponseTime(KTermResponseTimeContext* ctx);
void KTerm_Net_FreePortScan(KTermPortScanContext* ctx);
void KTerm_Net_FreeWhois(KTermWhoisContext* ctx);
void KTerm_Net_FreeSpeedtest(KTermSpeedtestContext* ctx);
void KTerm_Net_FreeHttpProbe(KTermHttpProbeContext* ctx);
void KTerm_Net_FreeMtuProbe(KTermMtuProbeContext* ctx);
void KTerm_Net_FreeFragTest(KTermFragTestContext* ctx);
void KTerm_Net_FreePingExt(KTermPingExtContext* ctx);
void KTerm_Net_FreeLiveWire(KTermLiveWireContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // KTERM_DISABLE_NET

#endif // KT_NET_H

#ifdef KTERM_NET_IMPLEMENTATION
#ifndef KTERM_NET_IMPLEMENTATION_GUARD
#define KTERM_NET_IMPLEMENTATION_GUARD

#ifndef KTERM_DISABLE_NET

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#ifndef _WIN32
#include <pthread.h>
#endif

#ifdef KTERM_ENABLE_LIVEWIRE
// Include pcap.h if LiveWire is enabled.
// Use "deps/pcap.h" if bundled, otherwise assume system path.
#ifdef KTERM_USE_BUNDLED_PCAP
    #include "deps/pcap.h"
#else
    #include <pcap.h>
#endif
#endif

#ifdef KTERM_USE_LIBSSH
#include <libssh/libssh.h>
#endif

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #include <icmpapi.h>
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
    #ifdef __linux__
        #include <linux/errqueue.h>
        #include <sys/uio.h>
        #include <sys/time.h>
        #include <netinet/ip_icmp.h>
    #endif
    typedef int socket_t;
    #define CLOSE_SOCKET close
    #define IS_VALID_SOCKET(s) ((s) >= 0)
    #define INVALID_SOCKET -1
#endif

#ifdef _WIN32
    #define KTERM_MSG_DONTWAIT 0
#else
    #define KTERM_MSG_DONTWAIT MSG_DONTWAIT
#endif

// Forward declarations
static double KTerm_GetTime(void);
static unsigned short KTerm_Checksum(void *b, int len);

#ifndef KTERM_DISABLE_VOICE
typedef struct {
    KTerm* term;
    KTermSession* session;
} KTermVoiceSendCtx;

static void _KTerm_Net_VoiceSendCallback(void* user_data, const void* data, size_t len) {
    KTermVoiceSendCtx* ctx = (KTermVoiceSendCtx*)user_data;
    KTerm_Net_SendPacket(ctx->term, ctx->session, KTERM_PKT_AUDIO_VOICE, data, len);
}
#endif
struct KTermWhoisContext;
struct KTermSpeedtestContext;
struct KTermHttpProbeContext;
struct KTermMtuProbeContext;
struct KTermFragTestContext;
struct KTermPingExtContext;
struct KTermLiveWireContext;

#ifndef KTERM_DISABLE_TELNET
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
#endif

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

    // Auto-Reconnect
    bool auto_reconnect;
    int max_retries;
    int retry_delay_ms;

#ifndef KTERM_DISABLE_TELNET
    // Telnet State
    TelnetParseState telnet_state;
    char sb_buffer[1024];
    int sb_len;
    unsigned char sb_option;
#endif

    // Auth State
    NetAuthState auth_state;
    char auth_input_buf[64];
    int auth_input_len;
    char auth_user_temp[64];

    struct KTermTracerouteContext* traceroute;
    struct KTermResponseTimeContext* response_time;
    struct KTermPortScanContext* port_scan;
    struct KTermWhoisContext* whois;
    struct KTermSpeedtestContext* speedtest;
    struct KTermHttpProbeContext* http_probe;
    struct KTermMtuProbeContext* mtu_probe;
    struct KTermFragTestContext* frag_test;
    struct KTermPingExtContext* ping_ext;
    struct KTermLiveWireContext* livewire;

    int target_session_index;

    // Hardening: Timeouts & Retries
    time_t connect_start_time;
    int retry_count;
    char last_error[256];

} KTermNetSession;

typedef struct KTermTracerouteContext {
    int state; // 0=IDLE, 1=RESOLVE, 2=SEND, 3=WAIT, 4=DONE
    char host[256];
    struct sockaddr_in dest_addr;
    int current_ttl;
    int max_hops;
    int current_probe;
    int timeout_ms;

    socket_t sockfd;
    struct timeval probe_start_time;

    KTermTracerouteCallback callback;
    void* user_data;
    bool continuous;

#ifdef _WIN32
    HANDLE icmp_handle;
    HANDLE icmp_event;
    char reply_buffer[1024];
#endif
} KTermTracerouteContext;

typedef struct KTermResponseTimeContext {
    int state; // 0=IDLE, 1=RESOLVE, 2=SEND, 3=WAIT, 4=DONE
    char host[256];
    struct sockaddr_in dest_addr;
    int count;
    int interval_ms;
    int timeout_ms;

    int sent_count;
    int recv_count;

    double rtt_sum;
    double rtt_min;
    double rtt_max;
    double rtt_sq_sum;

    struct timeval probe_start_time;
    struct timeval last_complete_time;

    socket_t sockfd;
    bool is_raw;

    KTermResponseTimeCallback callback;
    void* user_data;

#ifdef _WIN32
    HANDLE icmp_handle;
    HANDLE icmp_event;
    char reply_buffer[4096];
#endif
} KTermResponseTimeContext;

typedef struct KTermPortScanContext {
    int state; // 0=IDLE, 1=CONNECTING, 2=NEXT
    char host[256];
    char ports_str[256];
    int current_port;
    int timeout_ms;

    // Parser state for ports_str
    char* ports_ptr;
    char* ports_next;

    socket_t sockfd;
    struct timeval start_time;
    struct sockaddr_in dest_addr;

    KTermPortScanCallback callback;
    void* user_data;
} KTermPortScanContext;

typedef struct KTermWhoisContext {
    int state; // 0=IDLE, 1=CONNECTING, 2=SENDING, 3=RECEIVING, 4=DONE
    char host[256];
    char query[256];
    socket_t sockfd;
    struct sockaddr_in dest_addr;
    char buffer[4096];

    int timeout_ms;
    struct timeval start_time;

    KTermWhoisCallback callback;
    void* user_data;
} KTermWhoisContext;

typedef struct {
    socket_t fd;
    bool connected;
    uint64_t bytes;
} SpeedtestStream;

#define MAX_ST_STREAMS 8

typedef struct KTermSpeedtestContext {
    int state; // 0=AUTO_SELECT, 1=LATENCY, 2=CONNECT_DL, 3=RUN_DL, 4=CONNECT_UL, 5=RUN_UL, 6=DONE
    char host[256];
    int port;
    char dl_path[256];
    int num_streams;
    struct sockaddr_in dest_addr;

    // Auto-Select State
    int auto_state; // 0=CONNECT, 1=SEND, 2=READ, 3=PARSE
    socket_t config_fd;
    char config_buffer[16384];
    int config_len;

    SpeedtestStream streams[MAX_ST_STREAMS];
    int connected_count;

    double start_time;
    double phase_start_time;
    double duration_sec;

    double dl_mbps;
    double ul_mbps;
    double jitter_ms;

    bool latency_started;
    bool latency_done;

    KTermSpeedtestCallback callback;
    void* user_data;
} KTermSpeedtestContext;

typedef struct KTermHttpProbeContext {
    int state; // 0=IDLE, 1=RESOLVE, 2=CONNECT, 3=SEND, 4=WAIT_HEAD, 5=READ_BODY, 6=DONE
    char host[256];
    int port;
    char path[1024];
    struct sockaddr_in dest_addr;
    socket_t sockfd;

    double start_time;
    double dns_start_time;
    double connect_start_time;
    double request_start_time;
    double first_byte_time;

    // Metrics
    double dns_ms;
    double connect_ms;
    double ttfb_ms;

    char buffer[8192];
    int buffer_len;

    int status_code;
    uint64_t size_bytes;
    uint64_t content_length; // For keep-alive support

    KTermHttpProbeCallback callback;
    void* user_data;
} KTermHttpProbeContext;

typedef struct KTermMtuProbeContext {
    int state; // 0=IDLE, 1=RESOLVE, 2=SOCKET, 3=SEND_PROBE, 4=WAIT_PROBE, 5=DONE
    char host[256];
    struct sockaddr_in dest_addr;
    socket_t sockfd;

    bool df;
    int current_size;
    int min_size;
    int max_size;
    int known_good_size;
    int path_mtu;
    int local_mtu;

    struct timeval probe_start_time;
    int retry_count;

    KTermMtuProbeCallback callback;
    void* user_data;

#ifdef _WIN32
    HANDLE icmp_handle;
    HANDLE icmp_event;
    char reply_buffer[1024];
#endif
} KTermMtuProbeContext;

typedef struct KTermFragTestContext {
    int state; // 0=IDLE, 1=RESOLVE, 2=SOCKET, 3=SEND, 4=WAIT, 5=DONE
    char host[256];
    struct sockaddr_in dest_addr;
    socket_t sockfd;

    int size;
    int fragments;

    int sent_count;

    struct timeval start_time;

    KTermFragTestCallback callback;
    void* user_data;

#ifdef _WIN32
    HANDLE icmp_handle;
    HANDLE icmp_event;
    char reply_buffer[1024];
#endif
} KTermFragTestContext;

typedef struct KTermPingExtContext {
    int state;
    char host[256];
    struct sockaddr_in dest_addr;
    socket_t sockfd;
    bool is_raw;

    int count;
    int interval_ms;
    int size;
    bool graph;

    int sent;
    int received;

    // Stats
    double rtt_min;
    double rtt_max;
    double rtt_sum;
    double rtt_sq_sum;

    // Hist
    int h_0_10;
    int h_10_20;
    int h_20_50;
    int h_50_100;
    int h_100_plus;

    // Graph
    char graph_buf[64];
    int graph_idx;

    struct timeval probe_start_time;
    struct timeval last_complete_time;

    KTermPingExtCallback callback;
    void* user_data;

#ifdef _WIN32
    HANDLE icmp_handle;
    HANDLE icmp_event;
    char reply_buffer[4096];
#endif
} KTermPingExtContext;

typedef struct {
    struct timeval ts;
    int len;
    uint8_t data[1500];
} CapturedPacket;

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t proto;
} LiveWireFlowKey;

typedef struct {
    uint32_t seq;
    uint32_t ack;
    uint32_t last_ts;
    // Buffer for reassembly
    char buffer[4096];
    int buf_len;
} LiveWireStream;

typedef struct {
    uint64_t packets;
    uint64_t bytes;
    uint64_t lost;
    double jitter;
    double last_jitter_ts;
    double prev_delta;
    // Protocol specific
    bool is_rtp;
    uint32_t ssrc;
} LiveWireFlowStats;

typedef struct LiveWireFlow {
    LiveWireFlowKey key;
    LiveWireStream stream;
    LiveWireFlowStats stats;
    struct LiveWireFlow* next; // Chaining for hash collisions
    uint32_t id; // Unique ID for referencing
} LiveWireFlow;

typedef struct KTermLiveWireContext {
    void* pcap_handle; // void* to avoid pcap dependency in header if possible, but we included pcap.h in impl
    // Actually this struct is in IMPLEMENTATION block, so we can use pcap_t if included
#ifdef KTERM_ENABLE_LIVEWIRE
    pcap_t* handle;
#else
    void* handle;
#endif
    char dev[64];
    char filter_exp[256];
    int snaplen;
    int promisc;
    int count;
    int timeout_ms;

    // Control
    bool paused;
    bool trigger_mtu_probe;
    char last_frag_ip[64];

    // Stats
    int captured_count;
    int error_count;
    bool running;

    // Threading
#ifndef _WIN32
    pthread_t thread;
    pthread_mutex_t mutex;
#else
    HANDLE thread;
    CRITICAL_SECTION mutex;
#endif

    // Output Ring Buffer (Text)
    char out_buf[65536];
    int buf_head;
    int buf_tail;

    // Packet Ring Buffer (Raw)
    CapturedPacket packet_ring[128];
    int ring_head;
    int ring_count; // Total packets processed (redundant with captured_count but used for relative index)

    // Flow Tracking & Stats
    struct LiveWireFlow* flow_table[256]; // Hash table
    uint32_t next_flow_id;
    uint32_t follow_flow_id; // 0 = None

    struct {
        uint64_t total_packets;
        uint64_t total_bytes;
        uint64_t tcp_packets;
        uint64_t udp_packets;
        uint64_t icmp_packets;
        uint64_t other_packets;
    } stats;

    // Target
    KTerm* term;
    int session_index;

} KTermLiveWireContext;

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
        net->max_retries = 3; // Default
        net->retry_delay_ms = 1000;
        session->user_data = net;
    }
    return net;
}

void KTerm_Net_FreeTraceroute(KTermTracerouteContext* ctx) {
    if (!ctx) return;
    if (IS_VALID_SOCKET(ctx->sockfd)) CLOSE_SOCKET(ctx->sockfd);
#ifdef _WIN32
    if (ctx->icmp_handle != INVALID_HANDLE_VALUE) IcmpCloseHandle(ctx->icmp_handle);
    if (ctx->icmp_event) CloseHandle(ctx->icmp_event);
#endif
    if (ctx->user_data) free(ctx->user_data);
    free(ctx);
}

void KTerm_Net_FreeResponseTime(KTermResponseTimeContext* ctx) {
    if (!ctx) return;
    if (IS_VALID_SOCKET(ctx->sockfd)) CLOSE_SOCKET(ctx->sockfd);
#ifdef _WIN32
    if (ctx->icmp_handle != INVALID_HANDLE_VALUE) IcmpCloseHandle(ctx->icmp_handle);
    if (ctx->icmp_event) CloseHandle(ctx->icmp_event);
#endif
    if (ctx->user_data) free(ctx->user_data);
    free(ctx);
}

void KTerm_Net_FreePortScan(KTermPortScanContext* ctx) {
    if (!ctx) return;
    if (IS_VALID_SOCKET(ctx->sockfd)) CLOSE_SOCKET(ctx->sockfd);
    if (ctx->user_data) free(ctx->user_data);
    free(ctx);
}

void KTerm_Net_FreeWhois(KTermWhoisContext* ctx) {
    if (!ctx) return;
    if (IS_VALID_SOCKET(ctx->sockfd)) CLOSE_SOCKET(ctx->sockfd);
    if (ctx->user_data) free(ctx->user_data);
    free(ctx);
}

void KTerm_Net_FreeSpeedtest(KTermSpeedtestContext* ctx) {
    if (!ctx) return;
    for(int i=0; i<ctx->num_streams; i++) {
        if (IS_VALID_SOCKET(ctx->streams[i].fd)) CLOSE_SOCKET(ctx->streams[i].fd);
    }
    if (IS_VALID_SOCKET(ctx->config_fd)) CLOSE_SOCKET(ctx->config_fd);
    if (ctx->user_data) free(ctx->user_data);
    free(ctx);
}

void KTerm_Net_FreeHttpProbe(KTermHttpProbeContext* ctx) {
    if (!ctx) return;
    if (IS_VALID_SOCKET(ctx->sockfd)) CLOSE_SOCKET(ctx->sockfd);
    if (ctx->user_data) free(ctx->user_data);
    free(ctx);
}

void KTerm_Net_FreeMtuProbe(KTermMtuProbeContext* ctx) {
    if (!ctx) return;
    if (IS_VALID_SOCKET(ctx->sockfd)) CLOSE_SOCKET(ctx->sockfd);
#ifdef _WIN32
    if (ctx->icmp_handle != INVALID_HANDLE_VALUE) IcmpCloseHandle(ctx->icmp_handle);
    if (ctx->icmp_event) CloseHandle(ctx->icmp_event);
#endif
    if (ctx->user_data) free(ctx->user_data);
    free(ctx);
}

void KTerm_Net_FreeFragTest(KTermFragTestContext* ctx) {
    if (!ctx) return;
    if (IS_VALID_SOCKET(ctx->sockfd)) CLOSE_SOCKET(ctx->sockfd);
#ifdef _WIN32
    if (ctx->icmp_handle != INVALID_HANDLE_VALUE) IcmpCloseHandle(ctx->icmp_handle);
    if (ctx->icmp_event) CloseHandle(ctx->icmp_event);
#endif
    if (ctx->user_data) free(ctx->user_data);
    free(ctx);
}

void KTerm_Net_FreePingExt(KTermPingExtContext* ctx) {
    if (!ctx) return;
    if (IS_VALID_SOCKET(ctx->sockfd)) CLOSE_SOCKET(ctx->sockfd);
#ifdef _WIN32
    if (ctx->icmp_handle != INVALID_HANDLE_VALUE) IcmpCloseHandle(ctx->icmp_handle);
    if (ctx->icmp_event) CloseHandle(ctx->icmp_event);
#endif
    if (ctx->user_data) free(ctx->user_data);
    free(ctx);
}

void KTerm_Net_FreeLiveWire(KTermLiveWireContext* ctx) {
    if (!ctx) return;
    if (ctx->running) {
        // Should have been stopped, but just in case
        ctx->running = false;
        // Handle closing in Stop
    }

    // Cleanup Flows
    for (int i = 0; i < 256; i++) {
        LiveWireFlow* flow = ctx->flow_table[i];
        while (flow) {
            LiveWireFlow* next = flow->next;
            free(flow);
            flow = next;
        }
        ctx->flow_table[i] = NULL;
    }

    // Handle is closed in Stop
    free(ctx);
}

static void KTerm_Net_DestroyContext(KTermSession* session) {
    if (!session || !session->user_data) return;
    KTermNetSession* net = (KTermNetSession*)session->user_data;

    if (net->traceroute) { KTerm_Net_FreeTraceroute(net->traceroute); net->traceroute = NULL; }
    if (net->response_time) { KTerm_Net_FreeResponseTime(net->response_time); net->response_time = NULL; }
    if (net->port_scan) { KTerm_Net_FreePortScan(net->port_scan); net->port_scan = NULL; }
    if (net->whois) { KTerm_Net_FreeWhois(net->whois); net->whois = NULL; }
    if (net->speedtest) { KTerm_Net_FreeSpeedtest(net->speedtest); net->speedtest = NULL; }
    if (net->http_probe) { KTerm_Net_FreeHttpProbe(net->http_probe); net->http_probe = NULL; }
    if (net->mtu_probe) { KTerm_Net_FreeMtuProbe(net->mtu_probe); net->mtu_probe = NULL; }
    if (net->frag_test) { KTerm_Net_FreeFragTest(net->frag_test); net->frag_test = NULL; }
    if (net->ping_ext) { KTerm_Net_FreePingExt(net->ping_ext); net->ping_ext = NULL; }
    if (net->livewire) {
        KTerm_Net_LiveWire_Stop(NULL, session); // Stop if active
        KTerm_Net_FreeLiveWire(net->livewire);
        net->livewire = NULL;
    }

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

    // Secure Cleanup: Wipe credentials
    volatile char* p_pass = (volatile char*)net->password;
    while(*p_pass) *p_pass++ = 0;

    volatile char* p_user = (volatile char*)net->user;
    while(*p_user) *p_user++ = 0;

    volatile char* p_auth = (volatile char*)net->auth_input_buf;
    while(*p_auth) *p_auth++ = 0;

    volatile char* p_tmp = (volatile char*)net->auth_user_temp;
    while(*p_tmp) *p_tmp++ = 0;

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
    if (net) snprintf(net->last_error, sizeof(net->last_error), "%s", msg);

    // Retry Logic for Connection Errors
    if ((net->state == KTERM_NET_STATE_CONNECTING || net->state == KTERM_NET_STATE_RESOLVING) && net->auto_reconnect) {
        if (net->retry_count < net->max_retries) {
            net->retry_count++;
            net->state = KTERM_NET_STATE_RESOLVING;
            if (IS_VALID_SOCKET(net->socket_fd)) {
                CLOSE_SOCKET(net->socket_fd);
                net->socket_fd = INVALID_SOCKET;
            }
            char retry_msg[64];
            snprintf(retry_msg, sizeof(retry_msg), "Retrying (%d/%d)...", net->retry_count, net->max_retries);
            KTerm_Net_Log(term, (int)(session - term->sessions), retry_msg);

            // Reset start time to give retry a fresh window
            net->connect_start_time = time(NULL);
            return;
        }
    }

    net->state = KTERM_NET_STATE_ERROR;
    if (net->callbacks.on_error) {
        net->callbacks.on_error(term, session, msg);
    }
}

static void KTerm_Net_ProcessPingExt(KTerm* term, KTermSession* session) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->ping_ext) return;
    KTermPingExtContext* ctx = net->ping_ext;

    if (ctx->state == 5) return; // DONE

    if (ctx->state == 2) { // SOCKET
#ifdef __linux__
        ctx->sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
        ctx->is_raw = false;
        if (ctx->sockfd < 0) {
             ctx->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
             ctx->is_raw = true;
        }
        if (ctx->sockfd < 0) { ctx->state = 5; if(ctx->callback){ KTermPingExtResult r={0}; r.done=true; ctx->callback(term, session, &r, ctx->user_data); } return; }
        fcntl(ctx->sockfd, F_SETFL, fcntl(ctx->sockfd, F_GETFL, 0) | O_NONBLOCK);
#elif defined(_WIN32)
        ctx->icmp_handle = IcmpCreateFile();
        if (ctx->icmp_handle == INVALID_HANDLE_VALUE) { ctx->state = 5; return; }
        ctx->icmp_event = CreateEvent(NULL, TRUE, FALSE, NULL);
#endif
        ctx->state = 3; // SEND
        return;
    }

    if (ctx->state == 3) { // SEND
        if (ctx->sent >= ctx->count) {
             ctx->state = 5; // DONE
             if (ctx->callback) {
                 KTermPingExtResult r = {0};
                 r.done = true;
                 r.sent = ctx->sent;
                 r.received = ctx->received;
                 r.lost = ctx->sent - ctx->received;
                 if (ctx->sent > 0) r.loss_percent = (float)r.lost / ctx->sent * 100.0f;
                 if (ctx->received > 0) {
                     r.min_rtt = (int)ctx->rtt_min;
                     r.max_rtt = (int)ctx->rtt_max;
                     r.avg_rtt = (int)(ctx->rtt_sum / ctx->received);
                     double mean = ctx->rtt_sum / ctx->received;
                     double var = (ctx->rtt_sq_sum / ctx->received) - (mean * mean);
                     if (var < 0) var = 0;
                     r.stddev_rtt = (int)sqrt(var);
                 }
                 r.hist_0_10 = ctx->h_0_10;
                 r.hist_10_20 = ctx->h_10_20;
                 r.hist_20_50 = ctx->h_20_50;
                 r.hist_50_100 = ctx->h_50_100;
                 r.hist_100_plus = ctx->h_100_plus;
                 if (ctx->graph) strncpy(r.graph_line, ctx->graph_buf, sizeof(r.graph_line));

                 ctx->callback(term, session, &r, ctx->user_data);
             }
             return;
        }

        // Check Interval
        if (ctx->sent > 0) {
             double now = KTerm_GetTime();
#ifdef __linux__
             double last = (double)ctx->last_complete_time.tv_sec + (double)ctx->last_complete_time.tv_usec / 1000000.0;
#elif defined(_WIN32)
             double last = (double)ctx->last_complete_time.tv_sec / 1000.0; // Stored as ticks but cast to double time
             // Wait, KTerm_GetTime uses GetTickCount()/1000.0 on windows.
             // But my struct stores last_complete_time as timeval for linux compatibility,
             // but I used it as ticks in other funcs.
             // Let's assume tv_sec holds ticks on Windows.
             last = (double)ctx->last_complete_time.tv_sec / 1000.0;
#endif
             if ((now - last) * 1000.0 < ctx->interval_ms) return;
        }

        // Send Packet
#ifdef __linux__
        int payload_len = ctx->size;
        if (payload_len < 8) payload_len = 8;
        char* packet = (char*)calloc(1, payload_len);
        struct icmphdr* icmp = (struct icmphdr*)packet;
        icmp->type = ICMP_ECHO;
        icmp->code = 0;
        icmp->un.echo.id = htons(getpid() & 0xFFFF);
        icmp->un.echo.sequence = htons(ctx->sent + 1);
        if (ctx->is_raw) icmp->checksum = KTerm_Checksum(packet, payload_len);

        gettimeofday(&ctx->probe_start_time, NULL);
        sendto(ctx->sockfd, packet, payload_len, 0, (struct sockaddr*)&ctx->dest_addr, sizeof(ctx->dest_addr));
        free(packet);
        ctx->sent++;
        ctx->state = 4; // WAIT
#elif defined(_WIN32)
        int payload_len = ctx->size;
        void* payload = calloc(1, payload_len > 0 ? payload_len : 1);
        ResetEvent(ctx->icmp_event);
        DWORD res = IcmpSendEcho2(
            ctx->icmp_handle, ctx->icmp_event, NULL, NULL,
            ctx->dest_addr.sin_addr.s_addr,
            payload, payload_len,
            NULL,
            ctx->reply_buffer, 4096,
            1000
        );
        free(payload);
        if (res == 0 && GetLastError() == ERROR_IO_PENDING) {
            ctx->probe_start_time.tv_sec = (long)GetTickCount();
            ctx->sent++;
            ctx->state = 4;
        } else if (res > 0) {
            ctx->probe_start_time.tv_sec = (long)GetTickCount();
            SetEvent(ctx->icmp_event);
            ctx->sent++;
            ctx->state = 4;
        } else {
            // Error
            ctx->sent++;
            ctx->state = 3; // Retry next
            // Update Graph with Loss
            if (ctx->graph && ctx->graph_idx < 63) ctx->graph_buf[ctx->graph_idx++] = 'X';
        }
#endif
    }
    else if (ctx->state == 4) { // WAIT
        bool received = false;
        double rtt = 0;
        bool timeout = false;

#ifdef __linux__
        char buf[1024];
        struct sockaddr_in r_addr;
        socklen_t addr_len = sizeof(r_addr);
        int n = recvfrom(ctx->sockfd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr*)&r_addr, &addr_len);
        if (n > 0) {
             struct icmphdr* icmp = NULL;
             if (ctx->is_raw) {
                 if (n >= 20) icmp = (struct icmphdr*)(buf + 20);
             } else {
                 icmp = (struct icmphdr*)buf;
             }
             if (icmp && icmp->type == ICMP_ECHOREPLY) {
                 struct timeval now; gettimeofday(&now, NULL);
                 rtt = (now.tv_sec - ctx->probe_start_time.tv_sec) * 1000.0 + (now.tv_usec - ctx->probe_start_time.tv_usec) / 1000.0;
                 received = true;
                 ctx->last_complete_time = now;
             }
        } else {
             struct timeval now; gettimeofday(&now, NULL);
             if ((now.tv_sec - ctx->probe_start_time.tv_sec) * 1000.0 + (now.tv_usec - ctx->probe_start_time.tv_usec) / 1000.0 > 1000.0) {
                 timeout = true;
                 ctx->last_complete_time = now;
             }
        }
#elif defined(_WIN32)
        if (WaitForSingleObject(ctx->icmp_event, 0) == WAIT_OBJECT_0) {
             PICMP_ECHO_REPLY reply = (PICMP_ECHO_REPLY)ctx->reply_buffer;
             if (reply->Status == IP_SUCCESS) {
                 rtt = (double)reply->RoundTripTime;
                 if (rtt < 1.0) rtt = 1.0;
                 received = true;
             } else {
                 timeout = true; // Error reply treated as loss for now
             }
             ctx->last_complete_time.tv_sec = (long)GetTickCount();
        } else {
             if (GetTickCount() - (DWORD)ctx->probe_start_time.tv_sec > 1000) {
                 timeout = true;
                 ctx->last_complete_time.tv_sec = (long)GetTickCount();
             }
        }
#endif

        if (received) {
            ctx->received++;
            if (rtt < ctx->rtt_min) ctx->rtt_min = rtt;
            if (rtt > ctx->rtt_max) ctx->rtt_max = rtt;
            ctx->rtt_sum += rtt;
            ctx->rtt_sq_sum += (rtt * rtt);

            if (rtt <= 10) ctx->h_0_10++;
            else if (rtt <= 20) ctx->h_10_20++;
            else if (rtt <= 50) ctx->h_20_50++;
            else if (rtt <= 100) ctx->h_50_100++;
            else ctx->h_100_plus++;

            if (ctx->graph && ctx->graph_idx < 63) ctx->graph_buf[ctx->graph_idx++] = '.';

            ctx->state = 3; // Next
        } else if (timeout) {
            if (ctx->graph && ctx->graph_idx < 63) ctx->graph_buf[ctx->graph_idx++] = 'X';
            ctx->state = 3; // Next
        }
    }
}

static void KTerm_Net_ProcessMtuProbe(KTerm* term, KTermSession* session) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->mtu_probe) return;
    KTermMtuProbeContext* ctx = net->mtu_probe;

    if (ctx->state == 5) return; // DONE

    if (ctx->state == 2) { // SOCKET
#ifdef __linux__
        ctx->sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
        if (ctx->sockfd < 0) ctx->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

        if (ctx->sockfd < 0) {
             ctx->state = 5;
             if (ctx->callback) { KTermMtuProbeResult r={0}; r.error=true; snprintf(r.msg, sizeof(r.msg), "Socket Init Failed"); ctx->callback(term, session, &r, ctx->user_data); }
             return;
        }

        if (ctx->df) {
            int val = IP_PMTUDISC_DO;
            setsockopt(ctx->sockfd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
        }

        fcntl(ctx->sockfd, F_SETFL, fcntl(ctx->sockfd, F_GETFL, 0) | O_NONBLOCK);
#elif defined(_WIN32)
        ctx->icmp_handle = IcmpCreateFile();
        if (ctx->icmp_handle == INVALID_HANDLE_VALUE) { ctx->state = 5; return; }
        ctx->icmp_event = CreateEvent(NULL, TRUE, FALSE, NULL);
#endif
        ctx->current_size = ctx->min_size; // Start low
        ctx->state = 3; // SEND
        return;
    }

    if (ctx->state == 3) { // SEND
        // Binary search logic
        if (ctx->min_size > ctx->max_size) {
            ctx->path_mtu = ctx->known_good_size;
            ctx->state = 5; // DONE
            if (ctx->callback) {
                KTermMtuProbeResult r = {0};
                r.done = true;
                r.path_mtu = ctx->path_mtu;
                r.local_mtu = 0; // TODO: Get local MTU
                ctx->callback(term, session, &r, ctx->user_data);
            }
            return;
        }

        ctx->current_size = (ctx->min_size + ctx->max_size) / 2;

        // Send
#ifdef __linux__
        int payload_len = ctx->current_size - 28;
        if (payload_len < 0) payload_len = 0;

        char* packet = (char*)calloc(1, payload_len + 8);
        struct icmphdr* icmp = (struct icmphdr*)packet;
        icmp->type = ICMP_ECHO;
        icmp->code = 0;
        icmp->un.echo.id = htons(getpid() & 0xFFFF);
        icmp->un.echo.sequence = htons(ctx->current_size);
        if (ctx->sockfd > 0) { // Should check if RAW
             // For RAW, we need checksum. For DGRAM, kernel does it?
             // Safest to always calc.
             icmp->checksum = KTerm_Checksum(packet, payload_len + 8);
        }

        gettimeofday(&ctx->probe_start_time, NULL);
        int sent = sendto(ctx->sockfd, packet, payload_len + 8, 0, (struct sockaddr*)&ctx->dest_addr, sizeof(ctx->dest_addr));
        free(packet);

        if (sent < 0) {
            if (errno == EMSGSIZE) {
                ctx->max_size = ctx->current_size - 1;
                return;
            }
        }
        ctx->state = 4; // WAIT
#elif defined(_WIN32)
        int payload_len = ctx->current_size - 28;
        if (payload_len < 0) payload_len = 0;
        void* payload = calloc(1, payload_len > 0 ? payload_len : 1);

        IP_OPTION_INFORMATION ip_opts = {0};
        ip_opts.Ttl = 128;
        if (ctx->df) ip_opts.Flags = IP_FLAG_DF;

        ResetEvent(ctx->icmp_event);
        DWORD res = IcmpSendEcho2(ctx->icmp_handle, ctx->icmp_event, NULL, NULL, ctx->dest_addr.sin_addr.s_addr, payload, payload_len, &ip_opts, ctx->reply_buffer, 1024, 1000);
        free(payload);

        if (res == 0 && GetLastError() == ERROR_IO_PENDING) {
            ctx->probe_start_time.tv_sec = (long)GetTickCount();
            ctx->state = 4;
        } else if (res > 0) {
            ctx->probe_start_time.tv_sec = (long)GetTickCount();
            SetEvent(ctx->icmp_event);
            ctx->state = 4;
        } else {
             ctx->max_size = ctx->current_size - 1;
             ctx->state = 3;
        }
#endif
    }
    else if (ctx->state == 4) { // WAIT
        bool success = false;
        bool too_big = false;
        bool timeout = false;

#ifdef __linux__
        char buf[1024];
        struct sockaddr_in r_addr;
        socklen_t addr_len = sizeof(r_addr);
        int n = recvfrom(ctx->sockfd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr*)&r_addr, &addr_len);
        if (n > 0) {
             // Need to check type. Assuming DGRAM strips header?
             // If RAW, 20 bytes IP.
             // Simplified check:
             success = true; // Assume echo reply for now
        } else {
             struct timeval now; gettimeofday(&now, NULL);
             if ((now.tv_sec - ctx->probe_start_time.tv_sec) * 1000.0 + (now.tv_usec - ctx->probe_start_time.tv_usec) / 1000.0 > 1000.0) {
                 timeout = true;
             }
        }
#elif defined(_WIN32)
        if (WaitForSingleObject(ctx->icmp_event, 0) == WAIT_OBJECT_0) {
             PICMP_ECHO_REPLY reply = (PICMP_ECHO_REPLY)ctx->reply_buffer;
             if (reply->Status == IP_SUCCESS) success = true;
             else if (reply->Status == IP_PACKET_TOO_BIG) too_big = true;
             else timeout = true;
        } else {
             if (GetTickCount() - (DWORD)ctx->probe_start_time.tv_sec > 1000) timeout = true;
        }
#endif

        if (success) {
            ctx->known_good_size = ctx->current_size;
            ctx->min_size = ctx->current_size + 1;
            ctx->state = 3;
        } else if (too_big || timeout) {
            ctx->max_size = ctx->current_size - 1;
            ctx->state = 3;
        }
    }
}

static void KTerm_Net_ProcessFragTest(KTerm* term, KTermSession* session) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->frag_test) return;
    KTermFragTestContext* ctx = net->frag_test;

    if (ctx->state == 5) return;

    if (ctx->state == 2) { // SOCKET
#ifdef __linux__
        // Need RAW socket to manually fragment or send large packet
        ctx->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (!IS_VALID_SOCKET(ctx->sockfd)) {
             // Fallback DGRAM won't help with explicit fragmentation testing usually,
             // but if we just send large packet, OS might fragment it.
             ctx->sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
        }
#endif
        if (!IS_VALID_SOCKET(ctx->sockfd)) {
#ifdef _WIN32
             ctx->icmp_handle = IcmpCreateFile();
             if (ctx->icmp_handle == INVALID_HANDLE_VALUE) {
                 ctx->state = 5;
                 if (ctx->callback) { KTermFragTestResult r={0}; r.error=true; snprintf(r.msg, sizeof(r.msg), "ICMP Init Failed"); ctx->callback(term, session, &r, ctx->user_data); }
                 return;
             }
             ctx->icmp_event = CreateEvent(NULL, TRUE, FALSE, NULL);
#else
             ctx->state = 5;
             if (ctx->callback) { KTermFragTestResult r={0}; r.error=true; snprintf(r.msg, sizeof(r.msg), "Socket Failed"); ctx->callback(term, session, &r, ctx->user_data); }
             return;
#endif
        }
#ifdef __linux__
        fcntl(ctx->sockfd, F_SETFL, fcntl(ctx->sockfd, F_GETFL, 0) | O_NONBLOCK);
#endif
        ctx->state = 3; // SEND
        return;
    }

    if (ctx->state == 3) { // SEND
        // We just send one large packet and see if it comes back reassembled.
        // OS handles fragmentation usually.

        int payload_len = ctx->size - 28;
        if (payload_len < 0) payload_len = 0;

#ifdef __linux__
        char* packet = (char*)calloc(1, payload_len + 8);
        struct icmphdr* icmp = (struct icmphdr*)packet;
        icmp->type = ICMP_ECHO;
        icmp->code = 0;
        icmp->un.echo.id = htons(getpid() & 0xFFFF);
        icmp->un.echo.sequence = htons(1);
        icmp->checksum = KTerm_Checksum(packet, payload_len + 8);

        gettimeofday(&ctx->start_time, NULL);
        if (sendto(ctx->sockfd, packet, payload_len + 8, 0, (struct sockaddr*)&ctx->dest_addr, sizeof(ctx->dest_addr)) < 0) {
             // If EMSGSIZE, it means DF is set or local interface MTU restriction prevents fragmentation (sometimes)
             if (errno == EMSGSIZE) {
                 ctx->state = 5;
                 if (ctx->callback) { KTermFragTestResult r={0}; r.error=true; snprintf(r.msg, sizeof(r.msg), "Send Failed (EMSGSIZE)"); ctx->callback(term, session, &r, ctx->user_data); }
                 free(packet);
                 return;
             }
        }
        free(packet);
        ctx->state = 4; // WAIT
#elif defined(_WIN32)
        void* payload = calloc(1, payload_len > 0 ? payload_len : 1);
        IP_OPTION_INFORMATION ip_opts = {0};
        ip_opts.Ttl = 128;
        // Ensure DF is NOT set
        ip_opts.Flags = 0;

        ResetEvent(ctx->icmp_event);
        DWORD res = IcmpSendEcho2(
            ctx->icmp_handle, ctx->icmp_event, NULL, NULL,
            ctx->dest_addr.sin_addr.s_addr,
            payload, payload_len,
            &ip_opts,
            ctx->reply_buffer, 1024,
            2000
        );
        free(payload);

        if (res == 0 && GetLastError() == ERROR_IO_PENDING) {
            ctx->start_time.tv_sec = (long)GetTickCount();
            ctx->state = 4;
        } else if (res > 0) {
            ctx->state = 4;
            SetEvent(ctx->icmp_event);
        } else {
             ctx->state = 5;
             if (ctx->callback) { KTermFragTestResult r={0}; r.error=true; snprintf(r.msg, sizeof(r.msg), "Send Failed"); ctx->callback(term, session, &r, ctx->user_data); }
        }
#endif
    }
    else if (ctx->state == 4) { // WAIT
        bool success = false;
        bool done = false;

#ifdef __linux__
        char buf[65536]; // Need big buffer for reassembled packet
        struct sockaddr_in r_addr;
        socklen_t addr_len = sizeof(r_addr);
        int n = recvfrom(ctx->sockfd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr*)&r_addr, &addr_len);
        if (n > 0) {
             struct icmphdr* icmp = NULL;
             if (n >= 20) icmp = (struct icmphdr*)(buf + 20);
             if (icmp && icmp->type == ICMP_ECHOREPLY) {
                 success = true;
                 done = true;
             }
        } else {
             struct timeval now; gettimeofday(&now, NULL);
             if ((now.tv_sec - ctx->start_time.tv_sec) * 1000.0 + (now.tv_usec - ctx->start_time.tv_usec) / 1000.0 > 2000.0) {
                 success = false;
                 done = true;
             }
        }
#elif defined(_WIN32)
        if (WaitForSingleObject(ctx->icmp_event, 0) == WAIT_OBJECT_0) {
             PICMP_ECHO_REPLY reply = (PICMP_ECHO_REPLY)ctx->reply_buffer;
             if (reply->Status == IP_SUCCESS) success = true;
             done = true;
        } else {
             if (GetTickCount() - (DWORD)ctx->start_time.tv_sec > 2000) {
                 success = false;
                 done = true;
             }
        }
#endif

        if (done) {
            if (ctx->callback) {
                KTermFragTestResult r = {0};
                r.done = true;
                r.reassembly_success = success;
                r.fragments_sent = (ctx->size + 1499) / 1500; // Approx
                ctx->callback(term, session, &r, ctx->user_data);
            }
            ctx->state = 5;
        }
    }
}

static void KTerm_Net_ProcessFrame(KTerm* term, KTermSession* session, KTermNetSession* net, uint8_t type, const char* payload, size_t len) {
    int target_idx = (net->target_session_index != -1) ? net->target_session_index : (int)(session - term->sessions);

    if (type == KTERM_PKT_DATA) {
        bool handled = false;
        if (net->callbacks.on_data) handled = net->callbacks.on_data(term, session, payload, len);
        if (!handled && !net->is_server) {
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
    else if (type == KTERM_PKT_AUDIO_VOICE) {
#ifndef KTERM_DISABLE_VOICE
        KTermSession* target_session = &term->sessions[target_idx];
        KTerm_Voice_ProcessPlayback(target_session, payload, len);
#endif
    }
    else if (type == KTERM_PKT_AUDIO_COMMAND) {
#ifndef KTERM_DISABLE_VOICE
        // Payload is null-terminated string
        char cmd[1024];
        if (len < sizeof(cmd)) {
            memcpy(cmd, payload, len);
            cmd[len] = '\0';
            KTerm_Voice_InjectCommand(term, cmd);
        }
#endif
    }
}

#ifndef KTERM_DISABLE_TELNET
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
#endif

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
#ifndef KTERM_DISABLE_TELNET
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
#endif

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
#ifndef KTERM_DISABLE_TELNET
    net->telnet_state = TELNET_STATE_NORMAL;
#endif
    net->target_session_index = (int)(session - term->sessions);

    // Hardening Init
    net->connect_start_time = time(NULL);
    net->retry_count = 0;
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
#ifndef KTERM_DISABLE_TELNET
    net->telnet_state = TELNET_STATE_NORMAL;
#endif
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
        snprintf(buffer, max_len, "STATE=%s;HOST=%s;PORT=%d;RETRY=%d;LAST_ERROR=%s",
                 state_str, net->host, net->port, net->retry_count, net->last_error);
    } else {
        snprintf(buffer, max_len, "STATE=%s", state_str);
    }
}

void KTerm_Net_DumpConnections(KTerm* term, char* buffer, size_t max_len) {
    if (!buffer || max_len == 0) return;
    buffer[0] = '\0';
    size_t offset = 0;

    for (int i = 0; i < MAX_SESSIONS; i++) {
        KTermSession* session = &term->sessions[i];
        KTermNetSession* net = KTerm_Net_GetContext(session);
        if (!net) continue;

        // Main Connection
        if (net->state != KTERM_NET_STATE_DISCONNECTED) {
            char status[256];
            KTerm_Net_GetStatus(term, session, status, sizeof(status));
            int n = snprintf(buffer + offset, max_len - offset, "[%d:MAIN] %s|", i, status);
            if (n > 0) offset += n;
            if (offset >= max_len) return;
        }

        // Sub-contexts
        if (net->traceroute) {
            int n = snprintf(buffer + offset, max_len - offset, "[%d:TRACE] HOST=%s;TTL=%d;STATE=%d|", i, net->traceroute->host, net->traceroute->current_ttl, net->traceroute->state);
            if (n > 0) offset += n;
            if (offset >= max_len) return;
        }
        if (net->response_time) {
            int n = snprintf(buffer + offset, max_len - offset, "[%d:PING] HOST=%s;SENT=%d;RECV=%d|", i, net->response_time->host, net->response_time->sent_count, net->response_time->recv_count);
            if (n > 0) offset += n;
            if (offset >= max_len) return;
        }
        if (net->port_scan) {
            int n = snprintf(buffer + offset, max_len - offset, "[%d:SCAN] HOST=%s;PORT=%d;STATE=%d|", i, net->port_scan->host, net->port_scan->current_port, net->port_scan->state);
            if (n > 0) offset += n;
            if (offset >= max_len) return;
        }
        if (net->whois) {
            int n = snprintf(buffer + offset, max_len - offset, "[%d:WHOIS] HOST=%s;STATE=%d|", i, net->whois->host, net->whois->state);
            if (n > 0) offset += n;
            if (offset >= max_len) return;
        }
        if (net->http_probe) {
            int n = snprintf(buffer + offset, max_len - offset, "[%d:HTTP] HOST=%s;STATE=%d|", i, net->http_probe->host, net->http_probe->state);
            if (n > 0) offset += n;
            if (offset >= max_len) return;
        }
        if (net->speedtest) {
            int n = snprintf(buffer + offset, max_len - offset, "[%d:SPEED] HOST=%s;STATE=%d;DL=%.2f;UL=%.2f|", i, net->speedtest->host, net->speedtest->state, net->speedtest->dl_mbps, net->speedtest->ul_mbps);
            if (n > 0) offset += n;
            if (offset >= max_len) return;
            // Streams
            for (int s = 0; s < net->speedtest->num_streams; s++) {
                if (IS_VALID_SOCKET(net->speedtest->streams[s].fd)) {
                    int n = snprintf(buffer + offset, max_len - offset, "[%d:SPEED:S%d] SOCKET=%d;BYTES=%llu|", i, s, (int)net->speedtest->streams[s].fd, (unsigned long long)net->speedtest->streams[s].bytes);
                    if (n > 0) offset += n;
                    if (offset >= max_len) return;
                }
            }
        }
    }
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

void KTerm_Net_SetAutoReconnect(KTerm* term, KTermSession* session, bool enable, int max_retries, int delay_ms) {
    (void)term;
    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (net) {
        net->auto_reconnect = enable;
        net->max_retries = (max_retries > 0) ? max_retries : 3;
        net->retry_delay_ms = (delay_ms > 0) ? delay_ms : 1000;
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

static unsigned short KTerm_Checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;
    for (sum = 0; len > 1; len -= 2) sum += *buf++;
    if (len == 1) sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

static void KTerm_Net_ProcessResponseTime(KTerm* term, KTermSession* session) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->response_time) return;
    KTermResponseTimeContext* rt = net->response_time;

    if (rt->state == 4) return; // DONE

#ifdef __linux__
    // Linux Implementation using RAW/DGRAM Socket (ICMP)
    if (rt->state == 2) { // SEND
        if (rt->sent_count >= rt->count) {
             rt->state = 4; // DONE
             // Finalize
             ResponseTimeResult res = {0};
             res.sent = rt->sent_count;
             res.received = rt->recv_count;
             res.lost = rt->sent_count - rt->recv_count;
             if (rt->recv_count > 0) {
                 res.min_rtt_ms = rt->rtt_min;
                 res.max_rtt_ms = rt->rtt_max;
                 res.avg_rtt_ms = rt->rtt_sum / rt->recv_count;
                 // Jitter (StdDev) = sqrt(E[x^2] - (E[x])^2)
                 double mean = res.avg_rtt_ms;
                 double variance = (rt->rtt_sq_sum / rt->recv_count) - (mean * mean);
                 if (variance < 0) variance = 0; // Floating point error
                 res.jitter_ms = sqrt(variance);
             }
             if (rt->callback) rt->callback(term, session, &res, rt->user_data);
             return;
        }

        // Check Interval
        struct timeval now;
        gettimeofday(&now, NULL);
        if (rt->sent_count > 0) {
            double elapsed = (now.tv_sec - rt->last_complete_time.tv_sec) * 1000.0 + (now.tv_usec - rt->last_complete_time.tv_usec) / 1000.0;
            if (elapsed < rt->interval_ms) return;
        }

        struct icmphdr icmp_hdr;
        memset(&icmp_hdr, 0, sizeof(icmp_hdr));
        icmp_hdr.type = ICMP_ECHO;
        icmp_hdr.code = 0;
        icmp_hdr.un.echo.id = htons(getpid() & 0xFFFF);
        icmp_hdr.un.echo.sequence = htons(rt->sent_count + 1);
        icmp_hdr.checksum = 0;

        if (rt->is_raw) {
            icmp_hdr.checksum = KTerm_Checksum(&icmp_hdr, sizeof(icmp_hdr));
        }

        gettimeofday(&rt->probe_start_time, NULL);

        if (sendto(rt->sockfd, &icmp_hdr, sizeof(icmp_hdr), 0, (struct sockaddr*)&rt->dest_addr, sizeof(rt->dest_addr)) < 0) {
             // Handle error (e.g. Permission Denied)
             // Retry or fail?
             rt->state = 4;
             ResponseTimeResult res = {0};
             res.sent = rt->sent_count;
             res.received = rt->recv_count;
             res.lost = rt->count; // All lost/failed
             if (rt->callback) rt->callback(term, session, &res, rt->user_data);
             return;
        }
        rt->sent_count++;
        rt->state = 3; // WAIT
    }
    else if (rt->state == 3) { // WAIT
        char buf[1024];
        struct sockaddr_in r_addr;
        socklen_t addr_len = sizeof(r_addr);

        // Non-blocking recv
        int n = recvfrom(rt->sockfd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr*)&r_addr, &addr_len);
        if (n > 0) {
            struct icmphdr* icmp_hdr = NULL;

            if (rt->is_raw) {
                // SOCK_RAW includes IP header
                if (n >= 20) { // Min IP header
                    int ip_len = (buf[0] & 0x0F) * 4;
                    if (n >= ip_len + (int)sizeof(struct icmphdr)) {
                        icmp_hdr = (struct icmphdr*)(buf + ip_len);
                    }
                }
            } else {
                // SOCK_DGRAM strips IP header
                if (n >= (int)sizeof(struct icmphdr)) {
                    icmp_hdr = (struct icmphdr*)buf;
                }
            }

            if (icmp_hdr && icmp_hdr->type == ICMP_ECHOREPLY) {
                 // Success
                 struct timeval now;
                 gettimeofday(&now, NULL);
                 double rtt = (now.tv_sec - rt->probe_start_time.tv_sec) * 1000.0 + (now.tv_usec - rt->probe_start_time.tv_usec) / 1000.0;

                 if (rt->recv_count == 0) {
                     rt->rtt_min = rtt;
                     rt->rtt_max = rtt;
                 } else {
                     if (rtt < rt->rtt_min) rt->rtt_min = rtt;
                     if (rtt > rt->rtt_max) rt->rtt_max = rtt;
                 }
                 rt->rtt_sum += rtt;
                 rt->rtt_sq_sum += (rtt * rtt);
                 rt->recv_count++;

                 rt->last_complete_time = now;
                 rt->state = 2; // Next Probe
            }
        } else {
             struct timeval now;
             gettimeofday(&now, NULL);
             double elapsed = (now.tv_sec - rt->probe_start_time.tv_sec) * 1000.0 + (now.tv_usec - rt->probe_start_time.tv_usec) / 1000.0;
             if (elapsed > rt->timeout_ms) {
                 // Timeout
                 rt->last_complete_time = now;
                 rt->state = 2; // Next Probe
             }
        }
    }
#elif defined(_WIN32)
    if (rt->state == 2) { // SEND
        if (rt->sent_count >= rt->count) {
             rt->state = 4; // DONE
             ResponseTimeResult res = {0};
             res.sent = rt->sent_count;
             res.received = rt->recv_count;
             res.lost = rt->sent_count - rt->recv_count;
             if (rt->recv_count > 0) {
                 res.min_rtt_ms = rt->rtt_min;
                 res.max_rtt_ms = rt->rtt_max;
                 res.avg_rtt_ms = rt->rtt_sum / rt->recv_count;
                 double mean = res.avg_rtt_ms;
                 double variance = (rt->rtt_sq_sum / rt->recv_count) - (mean * mean);
                 if (variance < 0) variance = 0;
                 res.jitter_ms = sqrt(variance);
             }
             if (rt->callback) rt->callback(term, session, &res, rt->user_data);
             return;
        }

        // Check Interval
        DWORD now_ticks = GetTickCount();
        if (rt->sent_count > 0) {
            DWORD diff = now_ticks - (DWORD)rt->last_complete_time.tv_sec; // Stored in tv_sec
            if (diff < (DWORD)rt->interval_ms) return;
        }

        ResetEvent(rt->icmp_event);

        DWORD res = IcmpSendEcho2(
            rt->icmp_handle,
            rt->icmp_event,
            NULL, NULL,
            rt->dest_addr.sin_addr.s_addr,
            "KTermPing", 9,
            NULL, // Default options
            rt->reply_buffer, 4096,
            rt->timeout_ms
        );

        if (res == 0 && GetLastError() == ERROR_IO_PENDING) {
            rt->probe_start_time.tv_sec = (long)GetTickCount();
            rt->state = 3; // WAIT
            rt->sent_count++;
        } else if (res > 0) {
            // Immediate success (sync?)
            rt->probe_start_time.tv_sec = (long)GetTickCount();
            SetEvent(rt->icmp_event); // Manually set to trigger wait logic
            rt->state = 3;
            rt->sent_count++;
        } else {
             // Error
             rt->state = 4;
             if (rt->callback) {
                 ResponseTimeResult res = {0};
                 res.sent = rt->sent_count + 1; // Count this one
                 res.received = rt->recv_count;
                 res.lost = res.sent - res.received;
                 rt->callback(term, session, &res, rt->user_data);
             }
             return;
        }
    }
    else if (rt->state == 3) { // WAIT
        if (WaitForSingleObject(rt->icmp_event, 0) == WAIT_OBJECT_0) {
             PICMP_ECHO_REPLY reply = (PICMP_ECHO_REPLY)rt->reply_buffer;
             if (reply->Status == IP_SUCCESS) {
                 double rtt = (double)reply->RoundTripTime;
                 // Windows timing is low res (ms), but sufficient for basic stats
                 if (rtt < 1.0) rtt = 1.0; // clamp to 1ms if 0

                 if (rt->recv_count == 0) {
                     rt->rtt_min = rtt;
                     rt->rtt_max = rtt;
                 } else {
                     if (rtt < rt->rtt_min) rt->rtt_min = rtt;
                     if (rtt > rt->rtt_max) rt->rtt_max = rtt;
                 }
                 rt->rtt_sum += rtt;
                 rt->rtt_sq_sum += (rtt * rtt);
                 rt->recv_count++;
             }

             rt->last_complete_time.tv_sec = (long)GetTickCount();
             rt->state = 2; // Next Probe
        } else {
             // Timeout Check
             DWORD now = GetTickCount();
             if (now - (DWORD)rt->probe_start_time.tv_sec > (DWORD)rt->timeout_ms + 100) { // +100 grace
                 rt->last_complete_time.tv_sec = (long)GetTickCount();
                 rt->state = 2; // Next Probe (Timeout treated as loss)
             }
        }
    }
#else
    if (rt->state != 4) {
        if (rt->callback) {
             ResponseTimeResult res = {0}; // Empty/Fail
             rt->callback(term, session, &res, rt->user_data);
        }
        rt->state = 4;
    }
#endif
}

static void KTerm_Net_ProcessTraceroute(KTerm* term, KTermSession* session) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->traceroute) return;
    KTermTracerouteContext* tr = net->traceroute;

    if (tr->state == 4) return; // DONE

#ifdef __linux__
    // Linux Implementation using IP_RECVERR
    if (tr->state == 1) { // RESOLVE
        // Done in Start (blocking for now)
        tr->state = 2;
    }

    if (tr->state == 2) { // SEND
        if (tr->current_ttl > tr->max_hops) {
             if (tr->continuous) {
                 tr->current_ttl = 1;
                 tr->state = 5; // WAIT_LOOP
                 gettimeofday(&tr->probe_start_time, NULL);
                 return;
             }
             tr->state = 4; // DONE
             // Optional: Callback with hop=0 to signal finish?
             return;
        }

        int ttl = tr->current_ttl;
        if (setsockopt(tr->sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0) {
             // Handle error
        }

        struct sockaddr_in dest = tr->dest_addr;
        dest.sin_port = htons(33434 + tr->current_ttl);

        gettimeofday(&tr->probe_start_time, NULL);
        if (sendto(tr->sockfd, "probe", 5, 0, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
             // Handle error
        }
        tr->state = 3; // WAIT
    }
    else if (tr->state == 3) { // WAIT
        char msg_control[1024];
        struct iovec iov;
        char buf[512];
        struct msghdr msg = {0};
        struct sockaddr_in r_addr;

        iov.iov_base = buf;
        iov.iov_len = sizeof(buf);
        msg.msg_name = &r_addr;
        msg.msg_namelen = sizeof(r_addr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = msg_control;
        msg.msg_controllen = sizeof(msg_control);

        bool got_reply = false;
        bool reached = false;
        char ip_str[INET_ADDRSTRLEN] = {0};

        int n = recvmsg(tr->sockfd, &msg, MSG_ERRQUEUE | MSG_DONTWAIT);
        if (n >= 0) {
            struct cmsghdr *cmsg;
            for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
                if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR) {
                    struct sock_extended_err *ee = (struct sock_extended_err *)CMSG_DATA(cmsg);
                    struct sockaddr_in *offender = (struct sockaddr_in *)SO_EE_OFFENDER(ee);
                    inet_ntop(AF_INET, &offender->sin_addr, ip_str, sizeof(ip_str));

                    if (ee->ee_origin == SO_EE_ORIGIN_ICMP) {
                         if (ee->ee_type == ICMP_TIME_EXCEEDED || ee->ee_type == ICMP_DEST_UNREACH) {
                             got_reply = true;
                             if (ee->ee_type == ICMP_DEST_UNREACH) reached = true;
                         }
                    }
                }
            }
        }

        if (got_reply) {
            struct timeval now;
            gettimeofday(&now, NULL);
            double rtt = (now.tv_sec - tr->probe_start_time.tv_sec) * 1000.0 + (now.tv_usec - tr->probe_start_time.tv_usec) / 1000.0;

            if (tr->callback) {
                tr->callback(term, session, tr->current_ttl, ip_str, rtt, reached, tr->user_data);
            }

            if (reached) {
                if (tr->continuous) {
                    tr->current_ttl = 1;
                    tr->state = 5; // WAIT_LOOP
                    gettimeofday(&tr->probe_start_time, NULL);
                } else {
                    tr->state = 4; // DONE
                }
            } else {
                tr->current_ttl++;
                tr->state = 2; // SEND next
            }
        } else {
             struct timeval now;
             gettimeofday(&now, NULL);
             double elapsed = (now.tv_sec - tr->probe_start_time.tv_sec) * 1000.0 + (now.tv_usec - tr->probe_start_time.tv_usec) / 1000.0;
             if (elapsed > tr->timeout_ms) {
                 if (tr->callback) {
                     tr->callback(term, session, tr->current_ttl, "*", 0, false, tr->user_data);
                 }
                 tr->current_ttl++;
                 tr->state = 2;
             }
        }
    }
    else if (tr->state == 5) { // WAIT_LOOP
        struct timeval now;
        gettimeofday(&now, NULL);
        double elapsed = (now.tv_sec - tr->probe_start_time.tv_sec) * 1000.0 + (now.tv_usec - tr->probe_start_time.tv_usec) / 1000.0;
        if (elapsed > 1000.0) { // 1 second delay
             tr->state = 2;
        }
    }
#elif defined(_WIN32)
    if (tr->state == 2) { // SEND
        if (tr->current_ttl > tr->max_hops) {
             if (tr->continuous) {
                 tr->current_ttl = 1;
                 tr->state = 5; // WAIT_LOOP
                 tr->probe_start_time.tv_sec = GetTickCount();
                 return;
             }
             tr->state = 4; // DONE
             return;
        }

        ResetEvent(tr->icmp_event);
        IP_OPTION_INFORMATION ip_opts = {0};
        ip_opts.Ttl = tr->current_ttl;

        DWORD res = IcmpSendEcho2(
            tr->icmp_handle,
            tr->icmp_event,
            NULL, NULL,
            tr->dest_addr.sin_addr.s_addr,
            "probe", 5,
            &ip_opts,
            tr->reply_buffer, 1024,
            tr->timeout_ms
        );

        if (res == 0 && GetLastError() == ERROR_IO_PENDING) {
            tr->probe_start_time.tv_sec = GetTickCount(); // Store tick count in sec field
            tr->state = 3; // WAIT
        } else if (res > 0) {
            tr->probe_start_time.tv_sec = GetTickCount();
            SetEvent(tr->icmp_event);
            tr->state = 3;
        } else {
             // Error
             if (tr->callback) tr->callback(term, session, tr->current_ttl, "ERR", 0, false, tr->user_data);
             tr->current_ttl++;
        }
    }
    else if (tr->state == 3) { // WAIT
        if (WaitForSingleObject(tr->icmp_event, 0) == WAIT_OBJECT_0) {
             PICMP_ECHO_REPLY reply = (PICMP_ECHO_REPLY)tr->reply_buffer;
             struct in_addr addr;
             addr.s_addr = reply->Address;
             char* ip_str = inet_ntoa(addr);

             double rtt = (double)reply->RoundTripTime;
             bool reached = (reply->Status == IP_SUCCESS);

             if (reply->Status == IP_SUCCESS || reply->Status == IP_TTL_EXPIRED_TRANSIT) {
                 if (tr->callback) tr->callback(term, session, tr->current_ttl, ip_str, rtt, reached, tr->user_data);
             } else {
                 if (tr->callback) tr->callback(term, session, tr->current_ttl, "*", 0, false, tr->user_data);
             }

             if (reached) {
                 if (tr->continuous) {
                     tr->current_ttl = 1;
                     tr->state = 5; // WAIT_LOOP
                     tr->probe_start_time.tv_sec = GetTickCount();
                 } else {
                     tr->state = 4; // DONE
                 }
             } else {
                 tr->current_ttl++;
                 tr->state = 2;
             }
        } else {
             // Timeout Check
             DWORD now = GetTickCount();
             if (now - (DWORD)tr->probe_start_time.tv_sec > (DWORD)tr->timeout_ms) {
                 if (tr->callback) tr->callback(term, session, tr->current_ttl, "*", 0, false, tr->user_data);
                 tr->current_ttl++;
                 tr->state = 2;
             }
        }
    }
    else if (tr->state == 5) { // WAIT_LOOP
        DWORD now = GetTickCount();
        if (now - (DWORD)tr->probe_start_time.tv_sec > 1000) {
             tr->state = 2;
        }
    }
#else
    // Fallback
    if (tr->state != 4) {
        if (tr->callback) tr->callback(term, session, 0, "ERR;UNSUPPORTED_PLATFORM", 0, true, tr->user_data);
        tr->state = 4;
    }
#endif
}

static void KTerm_Net_ProcessPortScan(KTerm* term, KTermSession* session) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->port_scan) return;
    KTermPortScanContext* ps = net->port_scan;

    // NEXT Port Logic
    if (ps->state == 2) {
        // Iterate to next port
        while (ps->ports_ptr && *ps->ports_ptr) {
             // Skip delimiters
             if (*ps->ports_ptr == ',' || *ps->ports_ptr == ' ') {
                 ps->ports_ptr++;
                 continue;
             }

             // Found number
             ps->current_port = atoi(ps->ports_ptr);

             // Move ptr
             char* next = strchr(ps->ports_ptr, ',');
             if (next) {
                 ps->ports_ptr = next + 1;
             } else {
                 ps->ports_ptr = NULL; // End
             }

             if (ps->current_port > 0 && ps->current_port < 65536) {
                 // Start Connect
                 ps->state = 1; // CONNECTING

                 if (IS_VALID_SOCKET(ps->sockfd)) CLOSE_SOCKET(ps->sockfd);
                 ps->sockfd = socket(AF_INET, SOCK_STREAM, 0);
                 if (!IS_VALID_SOCKET(ps->sockfd)) {
                     // Fail this port?
                     if (ps->callback) ps->callback(term, session, ps->host, ps->current_port, 0, ps->user_data);
                     ps->state = 2; // Next
                     return;
                 }

                 // Non-blocking
#ifdef _WIN32
                 u_long mode = 1; ioctlsocket(ps->sockfd, FIONBIO, &mode);
#else
                 fcntl(ps->sockfd, F_SETFL, fcntl(ps->sockfd, F_GETFL, 0) | O_NONBLOCK);
#endif

                 ps->dest_addr.sin_port = htons(ps->current_port);
                 connect(ps->sockfd, (struct sockaddr*)&ps->dest_addr, sizeof(ps->dest_addr));

                 gettimeofday(&ps->start_time, NULL);
                 return; // Wait for next tick
             }
        }

        // Done
        if (ps->user_data) free(ps->user_data);
        free(ps);
        net->port_scan = NULL;
        return;
    }
    else if (ps->state == 1) { // CONNECTING
        fd_set wfds; struct timeval tv = {0, 0};
        FD_ZERO(&wfds);
#ifndef _WIN32
        if (ps->sockfd >= FD_SETSIZE) {
            // Trigger error, fd is too high for select()
            ps->callback(term, session, NULL, -1, 0, ps->user_data); // Report error? Or just skip
            KTerm_Net_FreePortScan(net->port_scan);
            net->port_scan = NULL;
            return;
        }
#endif
        FD_SET(ps->sockfd, &wfds);

        int res = select(ps->sockfd + 1, NULL, &wfds, NULL, &tv);
        if (res > 0) {
            int opt = 0; socklen_t len = sizeof(opt);
            if (getsockopt(ps->sockfd, SOL_SOCKET, SO_ERROR, (char*)&opt, &len) == 0 && opt == 0) {
                // Connected
                if (ps->callback) ps->callback(term, session, ps->host, ps->current_port, 1, ps->user_data);
                CLOSE_SOCKET(ps->sockfd); ps->sockfd = INVALID_SOCKET;
                ps->state = 2; // Next
            } else {
                // Error / Refused
                if (ps->callback) ps->callback(term, session, ps->host, ps->current_port, 0, ps->user_data);
                CLOSE_SOCKET(ps->sockfd); ps->sockfd = INVALID_SOCKET;
                ps->state = 2; // Next
            }
        } else {
            // Check timeout
            struct timeval now;
            gettimeofday(&now, NULL);
            double elapsed = (now.tv_sec - ps->start_time.tv_sec) * 1000.0 + (now.tv_usec - ps->start_time.tv_usec) / 1000.0;
            if (elapsed > ps->timeout_ms) {
                if (ps->callback) ps->callback(term, session, ps->host, ps->current_port, 0, ps->user_data);
                CLOSE_SOCKET(ps->sockfd); ps->sockfd = INVALID_SOCKET;
                ps->state = 2; // Next
            }
        }
    }
}

static void KTerm_Net_ProcessWhois(KTerm* term, KTermSession* session) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->whois) return;
    KTermWhoisContext* ctx = net->whois;

    if (ctx->state == 4) return; // DONE

    if (ctx->state == 1) { // CONNECTING
        fd_set wfds; struct timeval tv = {0, 0};
        FD_ZERO(&wfds);
#ifndef _WIN32
        if (ctx->sockfd >= FD_SETSIZE) {
            ctx->state = 4; // Fail
            return;
        }
#endif
        FD_SET(ctx->sockfd, &wfds);

        int res = select(ctx->sockfd + 1, NULL, &wfds, NULL, &tv);
        if (res > 0) {
            int opt = 0; socklen_t len = sizeof(opt);
            if (getsockopt(ctx->sockfd, SOL_SOCKET, SO_ERROR, (char*)&opt, &len) == 0 && opt == 0) {
                ctx->state = 2; // SENDING
            } else {
                if (ctx->callback) ctx->callback(term, session, "ERR;CONNECT_FAILED", 0, true, ctx->user_data);
                ctx->state = 4;
            }
        } else if (res < 0) {
            if (ctx->callback) ctx->callback(term, session, "ERR;SELECT_FAILED", 0, true, ctx->user_data);
            ctx->state = 4;
        } else {
            // Check timeout
            struct timeval now;
            gettimeofday(&now, NULL);
            double elapsed = (now.tv_sec - ctx->start_time.tv_sec) * 1000.0 + (now.tv_usec - ctx->start_time.tv_usec) / 1000.0;
            if (elapsed > ctx->timeout_ms) {
                if (ctx->callback) ctx->callback(term, session, "ERR;TIMEOUT", 0, true, ctx->user_data);
                ctx->state = 4;
            }
        }
    }
    else if (ctx->state == 2) { // SENDING
        char buf[512];
        snprintf(buf, sizeof(buf), "%s\r\n", ctx->query);
        ssize_t sent = send(ctx->sockfd, buf, strlen(buf), 0);
        if (sent > 0) {
            ctx->state = 3; // RECEIVING
        } else {
             #ifdef _WIN32
             if (WSAGetLastError() != WSAEWOULDBLOCK)
             #else
             if (errno != EAGAIN && errno != EWOULDBLOCK)
             #endif
             {
                 if (ctx->callback) ctx->callback(term, session, "ERR;SEND_FAILED", 0, true, ctx->user_data);
                 ctx->state = 4;
             }
        }
    }
    else if (ctx->state == 3) { // RECEIVING
        char buf[1024];
        ssize_t n = recv(ctx->sockfd, buf, sizeof(buf), 0);
        if (n > 0) {
            if (ctx->callback) ctx->callback(term, session, buf, n, false, ctx->user_data);
        } else if (n == 0) {
            if (ctx->callback) ctx->callback(term, session, NULL, 0, true, ctx->user_data);
            ctx->state = 4;
        } else {
             #ifdef _WIN32
             if (WSAGetLastError() != WSAEWOULDBLOCK)
             #else
             if (errno != EAGAIN && errno != EWOULDBLOCK)
             #endif
             {
                 if (ctx->callback) ctx->callback(term, session, "ERR;RECV_FAILED", 0, true, ctx->user_data);
                 ctx->state = 4;
             }
        }
    }

    if (ctx->state == 4) {
        if (IS_VALID_SOCKET(ctx->sockfd)) CLOSE_SOCKET(ctx->sockfd);
        ctx->sockfd = INVALID_SOCKET;
        if (ctx->user_data) free(ctx->user_data);
        free(ctx);
        net->whois = NULL;
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

    // Process Async Traceroute if active
    if (net && net->traceroute) {
        KTerm_Net_ProcessTraceroute(term, session);
    }

    // Process Async Response Time if active
    if (net && net->response_time) {
        KTerm_Net_ProcessResponseTime(term, session);
    }

    // Process Async Port Scan if active
    if (net && net->port_scan) {
        KTerm_Net_ProcessPortScan(term, session);
    }

    // Process Async Whois if active
    if (net && net->whois) {
        KTerm_Net_ProcessWhois(term, session);
    }

    // Process Async Speedtest if active
    if (net && net->speedtest) {
        void KTerm_Net_ProcessSpeedtest(KTerm* term, KTermSession* session); // Forward
        KTerm_Net_ProcessSpeedtest(term, session);
    }

    // Process Async HttpProbe if active
    if (net && net->http_probe) {
        void KTerm_Net_ProcessHttpProbe(KTerm* term, KTermSession* session); // Forward
        KTerm_Net_ProcessHttpProbe(term, session);
    }

    // Process Async MTU Probe if active
    if (net && net->mtu_probe) {
        void KTerm_Net_ProcessMtuProbe(KTerm* term, KTermSession* session); // Forward
        KTerm_Net_ProcessMtuProbe(term, session);
    }

    // Process Async Frag Test if active
    if (net && net->frag_test) {
        void KTerm_Net_ProcessFragTest(KTerm* term, KTermSession* session); // Forward
        KTerm_Net_ProcessFragTest(term, session);
    }

    // Process Async Ping Ext if active
    if (net && net->ping_ext) {
        void KTerm_Net_ProcessPingExt(KTerm* term, KTermSession* session); // Forward
        KTerm_Net_ProcessPingExt(term, session);
    }

    // Process LiveWire
    if (net && net->livewire) {
        void KTerm_Net_ProcessLiveWire(KTerm* term, KTermSession* session); // Forward
        KTerm_Net_ProcessLiveWire(term, session);
    }

    // Process Voice Capture
#ifndef KTERM_DISABLE_VOICE
    {
        KTermVoiceSendCtx vctx = { term, session };
        KTerm_Voice_ProcessCapture(term, session, _KTerm_Net_VoiceSendCallback, &vctx);
    }
#endif

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

        // --- Authentication Loop ---
        rc = ssh_userauth_none(net->ssh_session, NULL);

        // If "none" fails, try public key
        if (rc == SSH_AUTH_ERROR || rc == SSH_AUTH_DENIED || rc == SSH_AUTH_PARTIAL) {
            rc = ssh_userauth_publickey_auto(net->ssh_session, NULL, NULL);
        }

        // If public key fails, try password
        if (rc == SSH_AUTH_ERROR || rc == SSH_AUTH_DENIED || rc == SSH_AUTH_PARTIAL) {
            if (net->password[0]) {
                rc = ssh_userauth_password(net->ssh_session, NULL, net->password);
            }
        }

        if (rc != SSH_AUTH_SUCCESS) {
            KTerm_Net_TriggerError(term, session, net, "Authentication Failed");
            return;
        }

        // --- Channel Setup ---
        net->ssh_channel = ssh_channel_new(net->ssh_session);
        if (!net->ssh_channel) { KTerm_Net_TriggerError(term, session, net, "Channel alloc failed"); return; }

        if (ssh_channel_open_session(net->ssh_channel) != SSH_OK) {
            KTerm_Net_TriggerError(term, session, net, "Channel open failed"); return;
        }

        // Auto-Terminfo: Request PTY with correct TERM type (fallback to xterm-256color)
        // If we had access to session->term_type, we would use it.
        // For now, hardcode widely compatible default or use "xterm-256color".
        if (ssh_channel_request_pty(net->ssh_channel) != SSH_OK) {
             // Non-fatal, but logged
             KTerm_Net_Log(term, (int)(session - term->sessions), "PTY Request Failed");
        }

        // Send environment variables (e.g., TERM) if supported
        ssh_channel_request_env(net->ssh_channel, "TERM", "xterm-256color");

        if (ssh_channel_request_shell(net->ssh_channel) != SSH_OK) {
            KTerm_Net_TriggerError(term, session, net, "Shell request failed"); return;
        }

        net->state = KTERM_NET_STATE_CONNECTED;
        if (net->callbacks.on_connect) net->callbacks.on_connect(term, session);
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
            if (net->keep_alive_idle > 0) { int idle = net->keep_alive_idle; setsockopt(net->socket_fd, IPPROTO_TCP, TCP_KEEPIDLE, (const char*)&idle, sizeof(idle)); }
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

        // Timeout check
        if (time(NULL) - net->connect_start_time > 10) {
             KTerm_Net_TriggerError(term, session, net, "Connection Timeout");
             CLOSE_SOCKET(net->socket_fd);
             net->socket_fd = INVALID_SOCKET;
             return;
        }

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

#ifndef KTERM_DISABLE_TELNET
            // Negotiate Echo if Telnet
            if (net->protocol == KTERM_NET_PROTO_TELNET) {
                KTerm_Net_SendTelnetCommand(term, session, KTERM_TELNET_WILL, KTERM_TELNET_ECHO);
            }
#endif

            if (net->callbacks.on_auth) {
                // SEC-FIX: Enforce security layer for authentication to prevent cleartext password transmission
                if (!net->security.read || !net->security.write) {
                    KTerm_Net_TriggerError(term, session, net, "Authentication requires security layer (TLS/SSL)");
                    CLOSE_SOCKET(net->socket_fd); net->socket_fd = INVALID_SOCKET;
                    return;
                }
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
#ifndef KTERM_DISABLE_TELNET
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
#endif

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
                                 // SEC-FIX: Secondary check to ensure security layer for password processing
                                 if (!net->security.read || !net->security.write) {
                                     KTerm_Net_TriggerError(term, session, net, "Authentication requires security layer (TLS/SSL)");
                                     CLOSE_SOCKET(net->socket_fd); net->socket_fd = INVALID_SOCKET;
                                     return;
                                 }
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
            }
#ifndef KTERM_DISABLE_TELNET
            else if (net->protocol == KTERM_NET_PROTO_TELNET) {
                bool handled = false;
                if (net->callbacks.on_data) handled = net->callbacks.on_data(term, session, rx, nbytes);
                int target = (net->target_session_index != -1) ? net->target_session_index : session_idx;

                if (!handled) for (int i = 0; i < nbytes; i++) {
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
                                // If buffer full, we just don't write but still increment length to detect truncation if needed
                                // or just clamp it.
                                if (net->sb_len < 2048) net->sb_len++; // Limit growth to avoid wrap-around
                            }
                            break;

                        case TELNET_STATE_SB_IAC:
                            if (c == KTERM_TELNET_SE) {
                                // End of SB
                                if (net->sb_len > 0) {
                                    if (net->callbacks.on_telnet_sb) {
                                        // Pass robust length (clamped to buffer size)
                                        int safe_len = (net->sb_len > 1024) ? 1024 : net->sb_len;
                                        if (safe_len > 1) {
                                            net->callbacks.on_telnet_sb(term, session, net->sb_option, net->sb_buffer + 1, safe_len - 1);
                                        }
                                    }

                                    // Default NEW-ENVIRON handling
                                    if (net->sb_option == 39) { // NEW-ENVIRON
                                        if (net->sb_len > 1 && net->sb_buffer[1] == 1) { // SEND (01)
                                            // Reply IS: IAC SB NEW-ENVIRON IS VAR "USER" VAL "user" IAC SE
                                            // IS = 0, VAR = 0, VAL = 1
                                            unsigned char resp_head[] = { KTERM_TELNET_IAC, KTERM_TELNET_SB, 39, 0 }; // IS
                                            for(int k=0; k<4; k++) { net->tx_buffer[net->tx_head] = resp_head[k]; net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE; if(net->tx_head==net->tx_tail) net->tx_tail=(net->tx_tail+1)%NET_BUFFER_SIZE; }

                                            // VAR "USER"
                                            unsigned char var_user[] = { 0, 'U', 'S', 'E', 'R' };
                                            for(int k=0; k<5; k++) { net->tx_buffer[net->tx_head] = var_user[k]; net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE; if(net->tx_head==net->tx_tail) net->tx_tail=(net->tx_tail+1)%NET_BUFFER_SIZE; }

                                            // VAL "name"
                                            unsigned char val_tag = 1;
                                            net->tx_buffer[net->tx_head] = val_tag; net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE; if(net->tx_head==net->tx_tail) net->tx_tail=(net->tx_tail+1)%NET_BUFFER_SIZE;

                                            const char* u = net->user[0] ? net->user : "guest";
                                            for(int k=0; u[k]; k++) { net->tx_buffer[net->tx_head] = u[k]; net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE; if(net->tx_head==net->tx_tail) net->tx_tail=(net->tx_tail+1)%NET_BUFFER_SIZE; }

                                            // IAC SE
                                            unsigned char resp_tail[] = { KTERM_TELNET_IAC, KTERM_TELNET_SE };
                                            for(int k=0; k<2; k++) { net->tx_buffer[net->tx_head] = resp_tail[k]; net->tx_head = (net->tx_head + 1) % NET_BUFFER_SIZE; if(net->tx_head==net->tx_tail) net->tx_tail=(net->tx_tail+1)%NET_BUFFER_SIZE; }
                                        }
                                    }
                                }
                                net->telnet_state = TELNET_STATE_NORMAL;
                            } else if (c == KTERM_TELNET_IAC) {
                                // Escaped IAC in SB data
                                if (net->sb_len < 1024) net->sb_buffer[net->sb_len] = c;
                                if (net->sb_len < 2048) net->sb_len++;
                                net->telnet_state = TELNET_STATE_SB;
                            } else {
                                // Malformed? Back to SB
                                net->telnet_state = TELNET_STATE_SB;
                            }
                            break;
                    }
                }
            }
#endif
            else {
                bool handled = false;
                if (net->callbacks.on_data) handled = net->callbacks.on_data(term, session, rx, nbytes);
                int target = (net->target_session_index != -1) ? net->target_session_index : session_idx;
                if (!handled && !net->is_server) {
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

// --- Utilities Implementation ---

void KTerm_Net_GetLocalIP(char* buffer, size_t max_len) {
    if (!buffer || max_len == 0) return;
    buffer[0] = '\0';

    socket_t sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (!IS_VALID_SOCKET(sock)) {
         snprintf(buffer, max_len, "ERR;SOCKET");
         return;
    }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8"); // Google DNS (any routed IP works)
    serv.sin_port = htons(53);

    // Try Google DNS first
    int res = connect(sock, (const struct sockaddr*)&serv, sizeof(serv));
    if (res == -1) {
        // Fallback for Air-Gapped/Intranet: Try a private range IP (10.255.255.255)
        // This forces the OS to pick the interface that would route to private LAN.
        serv.sin_addr.s_addr = inet_addr("10.255.255.255");
        res = connect(sock, (const struct sockaddr*)&serv, sizeof(serv));
    }

    if (res != -1) {
         struct sockaddr_in name;
         socklen_t namelen = sizeof(name);
         if (getsockname(sock, (struct sockaddr*)&name, &namelen) != -1) {
             inet_ntop(AF_INET, &name.sin_addr, buffer, max_len);
         } else {
             snprintf(buffer, max_len, "ERR;GETSOCKNAME");
         }
    } else {
         snprintf(buffer, max_len, "ERR;CONNECT");
    }
    CLOSE_SOCKET(sock);
}

bool KTerm_Net_Resolve(const char* host, char* output_ip, size_t max_len) {
    if (!host || !output_ip || max_len == 0) return false;

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4 for now
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, NULL, &hints, &res) == 0) {
        struct sockaddr_in* addr = (struct sockaddr_in*)res->ai_addr;
        inet_ntop(AF_INET, &addr->sin_addr, output_ip, max_len);
        freeaddrinfo(res);
        return true;
    }
    return false;
}

void KTerm_Net_Ping(const char* host, char* output, size_t max_len) {
    if (!host || !output || max_len == 0) return;
    output[0] = '\0';

    // Validation: Allow only alphanumeric, dots, colons, and dashes
    for (int i=0; host[i]; i++) {
        char c = host[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == ':' || c == '-')) {
             snprintf(output, max_len, "ERR;INVALID_HOST");
             return;
        }
    }

    char cmd[512];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "ping -n 1 %s", host);
    #define KT_POPEN _popen
    #define KT_PCLOSE _pclose
#else
    snprintf(cmd, sizeof(cmd), "ping -c 1 %s", host);
    #define KT_POPEN popen
    #define KT_PCLOSE pclose
#endif

    FILE* fp = KT_POPEN(cmd, "r");
    if (fp) {
        size_t n = fread(output, 1, max_len - 1, fp);
        output[n] = '\0';
        KT_PCLOSE(fp);
    } else {
        snprintf(output, max_len, "ERR;POPEN_FAILED");
    }
#undef KT_POPEN
#undef KT_PCLOSE
}

void KTerm_Net_Traceroute(KTerm* term, KTermSession* session, const char* host, int max_hops, int timeout_ms, KTermTracerouteCallback cb, void* user_data) {
    KTerm_Net_TracerouteContinuous(term, session, host, max_hops, timeout_ms, false, cb, user_data);
}

void KTerm_Net_TracerouteContinuous(KTerm* term, KTermSession* session, const char* host, int max_hops, int timeout_ms, bool continuous, KTermTracerouteCallback cb, void* user_data) {
    if (!term || !session || !host) return;

    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (!net) return;

    // Cleanup existing
    if (net->traceroute) {
        if (IS_VALID_SOCKET(net->traceroute->sockfd)) CLOSE_SOCKET(net->traceroute->sockfd);
        free(net->traceroute);
        net->traceroute = NULL;
    }

    net->traceroute = (KTermTracerouteContext*)calloc(1, sizeof(KTermTracerouteContext));
    if (!net->traceroute) return;

    KTermTracerouteContext* tr = net->traceroute;
    strncpy(tr->host, host, sizeof(tr->host)-1);
    tr->max_hops = (max_hops > 0) ? max_hops : 30;
    tr->timeout_ms = (timeout_ms > 0) ? timeout_ms : 2000;
    tr->continuous = continuous;
    tr->callback = cb;
    tr->user_data = user_data;
    tr->current_ttl = 1;

#ifdef __linux__
    // Setup UDP Socket
    if (inet_pton(AF_INET, host, &tr->dest_addr.sin_addr) == 1) {
        tr->dest_addr.sin_family = AF_INET;
    } else {
        struct addrinfo hints = {0}, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        if (getaddrinfo(host, "33434", &hints, &res) != 0) {
            if (cb) cb(term, session, 0, "ERR;DNS_FAILED", 0, true, tr->user_data);
            if (tr->user_data) free(tr->user_data);
            free(tr); net->traceroute = NULL;
            return;
        }
        tr->dest_addr = *(struct sockaddr_in*)res->ai_addr;
        freeaddrinfo(res);
    }

    tr->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (!IS_VALID_SOCKET(tr->sockfd)) {
        if (cb) cb(term, session, 0, "ERR;SOCKET_FAILED", 0, true, tr->user_data);
        if (tr->user_data) free(tr->user_data);
        free(tr); net->traceroute = NULL;
        return;
    }

    int on = 1;
    setsockopt(tr->sockfd, SOL_IP, IP_RECVERR, &on, sizeof(on));

    // Set non-blocking
    fcntl(tr->sockfd, F_SETFL, fcntl(tr->sockfd, F_GETFL, 0) | O_NONBLOCK);

    tr->state = 2; // Ready to SEND
#elif defined(_WIN32)
    // Windows Implementation
    if (inet_pton(AF_INET, host, &tr->dest_addr.sin_addr) == 1) {
        tr->dest_addr.sin_family = AF_INET;
    } else {
        struct addrinfo hints = {0}, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        if (getaddrinfo(host, NULL, &hints, &res) != 0) {
             if (cb) cb(term, session, 0, "ERR;DNS_FAILED", 0, true, tr->user_data);
             if (tr->user_data) free(tr->user_data);
             free(tr); net->traceroute = NULL;
             return;
        }
        tr->dest_addr = *(struct sockaddr_in*)res->ai_addr;
        freeaddrinfo(res);
    }

    tr->icmp_handle = IcmpCreateFile();
    if (tr->icmp_handle == INVALID_HANDLE_VALUE) {
         if (cb) cb(term, session, 0, "ERR;ICMP_CREATE_FAILED", 0, true, tr->user_data);
         if (tr->user_data) free(tr->user_data);
         free(tr); net->traceroute = NULL;
         return;
    }
    tr->icmp_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    tr->state = 2; // SEND
#else
    if (cb) cb(term, session, 0, "ERR;UNSUPPORTED_PLATFORM", 0, true, tr->user_data);
    if (tr->user_data) free(tr->user_data);
    free(tr); net->traceroute = NULL;
#endif
}

bool KTerm_Net_ResponseTime(KTerm* term, KTermSession* session, const char* host, int count, int interval_ms, int timeout_ms, KTermResponseTimeCallback cb, void* user_data) {
    if (!term || !session || !host) return false;

    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (!net) return false;

    // Cleanup existing
    if (net->response_time) {
        if (IS_VALID_SOCKET(net->response_time->sockfd)) CLOSE_SOCKET(net->response_time->sockfd);
#ifdef _WIN32
        if (net->response_time->icmp_handle != INVALID_HANDLE_VALUE) IcmpCloseHandle(net->response_time->icmp_handle);
        if (net->response_time->icmp_event) CloseHandle(net->response_time->icmp_event);
#endif
        if (net->response_time->user_data) free(net->response_time->user_data);
        free(net->response_time);
        net->response_time = NULL;
    }

    net->response_time = (KTermResponseTimeContext*)calloc(1, sizeof(KTermResponseTimeContext));
    if (!net->response_time) return false;

    KTermResponseTimeContext* rt = net->response_time;
    strncpy(rt->host, host, sizeof(rt->host)-1);
    rt->count = (count > 0) ? count : 10;
    rt->interval_ms = (interval_ms > 0) ? interval_ms : 1000;
    rt->timeout_ms = (timeout_ms > 0) ? timeout_ms : 2000;
    rt->callback = cb;
    rt->user_data = user_data;

#ifdef __linux__
    // Setup Socket (Try DGRAM ICMP first, requires ping_group_range)
    if (inet_pton(AF_INET, host, &rt->dest_addr.sin_addr) != 1) {
        struct addrinfo hints = {0}, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM; // Hints DGRAM

        if (getaddrinfo(host, NULL, &hints, &res) != 0) {
            // Failure: Return false, caller handles cleanup/error report
            free(rt); net->response_time = NULL;
            return false;
        }
        rt->dest_addr = *(struct sockaddr_in*)res->ai_addr;
        freeaddrinfo(res);
    }
    rt->dest_addr.sin_family = AF_INET;

    // Try creating PROT_ICMP socket (unprivileged ping)
    rt->sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    rt->is_raw = false;
    if (rt->sockfd < 0) {
         // Fallback to RAW if root?
         rt->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
         rt->is_raw = true;
         if (rt->sockfd < 0) {
             // Failed
             free(rt); net->response_time = NULL;
             return false;
         }
    }

    // Set non-blocking
    fcntl(rt->sockfd, F_SETFL, fcntl(rt->sockfd, F_GETFL, 0) | O_NONBLOCK);

    rt->state = 2; // SEND

#elif defined(_WIN32)
    if (inet_pton(AF_INET, host, &rt->dest_addr.sin_addr) != 1) {
        struct addrinfo hints = {0}, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        if (getaddrinfo(host, NULL, &hints, &res) != 0) {
             free(rt); net->response_time = NULL;
             return false;
        }
        rt->dest_addr = *(struct sockaddr_in*)res->ai_addr;
        freeaddrinfo(res);
    }

    rt->icmp_handle = IcmpCreateFile();
    if (rt->icmp_handle == INVALID_HANDLE_VALUE) {
         free(rt); net->response_time = NULL;
         return false;
    }
    rt->icmp_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    rt->state = 2; // SEND
#else
    free(rt); net->response_time = NULL;
    return false;
#endif
    return true;
}

bool KTerm_Net_LiveWire_Follow(KTerm* term, KTermSession* session, uint32_t flow_id) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->livewire) return false;
    net->livewire->follow_flow_id = flow_id;
    return true;
}

bool KTerm_Net_LiveWire_GetStats(KTerm* term, KTermSession* session, char* out, size_t max) {
    (void)term;
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->livewire) return false;
    KTermLiveWireContext* ctx = net->livewire;

    snprintf(out, max, "PKTS=%llu;BYTES=%llu;TCP=%llu;UDP=%llu;ICMP=%llu;OTHER=%llu",
        (unsigned long long)ctx->stats.total_packets,
        (unsigned long long)ctx->stats.total_bytes,
        (unsigned long long)ctx->stats.tcp_packets,
        (unsigned long long)ctx->stats.udp_packets,
        (unsigned long long)ctx->stats.icmp_packets,
        (unsigned long long)ctx->stats.other_packets);
    return true;
}

bool KTerm_Net_LiveWire_GetFlows(KTerm* term, KTermSession* session, char* out, size_t max) {
    (void)term;
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->livewire) return false;
    KTermLiveWireContext* ctx = net->livewire;

    out[0] = '\0';
    size_t offset = 0;
    int count = 0;

#ifndef _WIN32
    pthread_mutex_lock(&ctx->mutex);
#else
    EnterCriticalSection(&ctx->mutex);
#endif

    for (int i = 0; i < 256; i++) {
        LiveWireFlow* flow = ctx->flow_table[i];
        while (flow) {
            char src[16], dst[16];
            struct in_addr sa = { .s_addr = flow->key.src_ip };
            struct in_addr da = { .s_addr = flow->key.dst_ip };
            inet_ntop(AF_INET, &sa, src, sizeof(src));
            inet_ntop(AF_INET, &da, dst, sizeof(dst));

            int n = snprintf(out + offset, max - offset, "ID=%u;%s:%d->%s:%d;PKTS=%llu|",
                flow->id, src, ntohs(flow->key.src_port), dst, ntohs(flow->key.dst_port),
                (unsigned long long)flow->stats.packets);

            if (n > 0) offset += n;
            if (offset >= max) break;

            count++;
            if (count >= 10) break;

            flow = flow->next;
        }
        if (offset >= max || count >= 10) break;
    }

#ifndef _WIN32
    pthread_mutex_unlock(&ctx->mutex);
#else
    LeaveCriticalSection(&ctx->mutex);
#endif

    return true;
}

bool KTerm_Net_PingExt(KTerm* term, KTermSession* session, const char* host, int count, int interval_ms, int size, bool graph, KTermPingExtCallback cb, void* user_data) {
    if (!term || !session || !host) return false;

    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (!net) return false;

    if (net->ping_ext) {
        if (IS_VALID_SOCKET(net->ping_ext->sockfd)) CLOSE_SOCKET(net->ping_ext->sockfd);
#ifdef _WIN32
        if (net->ping_ext->icmp_handle != INVALID_HANDLE_VALUE) IcmpCloseHandle(net->ping_ext->icmp_handle);
        if (net->ping_ext->icmp_event) CloseHandle(net->ping_ext->icmp_event);
#endif
        if (net->ping_ext->user_data) free(net->ping_ext->user_data);
        free(net->ping_ext);
        net->ping_ext = NULL;
    }

    net->ping_ext = (KTermPingExtContext*)calloc(1, sizeof(KTermPingExtContext));
    if (!net->ping_ext) return false;

    KTermPingExtContext* ctx = net->ping_ext;
    strncpy(ctx->host, host, sizeof(ctx->host)-1);
    ctx->count = (count > 0) ? count : 10;
    ctx->interval_ms = (interval_ms > 0) ? interval_ms : 1000;
    ctx->size = (size > 0) ? size : 64;
    ctx->graph = graph;
    ctx->callback = cb;
    ctx->user_data = user_data;

    // Init stats
    ctx->rtt_min = 999999.0;

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;

    if (getaddrinfo(host, NULL, &hints, &res) != 0) {
        free(ctx); net->ping_ext = NULL;
        return false;
    }
    ctx->dest_addr = *(struct sockaddr_in*)res->ai_addr;
    freeaddrinfo(res);

    ctx->state = 2; // SOCKET
    ctx->sockfd = INVALID_SOCKET;

    return true;
}

static double KTerm_GetTime() {
#ifdef _WIN32
    return (double)GetTickCount() / 1000.0;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#endif
}

static void KTerm_Speedtest_LatencyCB(KTerm* term, KTermSession* session, const ResponseTimeResult* result, void* user_data) {
    (void)term; (void)session;
    KTermSpeedtestContext* st = (KTermSpeedtestContext*)user_data;
    if (st) {
        st->jitter_ms = result->jitter_ms;
        st->latency_done = true;
    }
}

void KTerm_Net_ProcessSpeedtest(KTerm* term, KTermSession* session) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->speedtest) return;
    KTermSpeedtestContext* st = net->speedtest;

    if (st->state == 6) return; // DONE

    // STATE 0: AUTO_SELECT
    if (st->state == 0) {
        if (st->auto_state == 0) { // CONNECT
            if (st->config_fd == INVALID_SOCKET) {
                struct addrinfo hints = {0}, *res;
                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_STREAM;
                // Try c.speedtest.net
                if (getaddrinfo("c.speedtest.net", "80", &hints, &res) != 0) {
                    // Fallback to default immediately
                    strncpy(st->host, "speedtest.tele2.net", sizeof(st->host)-1);
                    st->port = 80;
                    // Need to resolve default
                    if (getaddrinfo(st->host, "80", &hints, &res) == 0) {
                        st->dest_addr = *(struct sockaddr_in*)res->ai_addr;
                        freeaddrinfo(res);
                        st->state = 1; // LATENCY
                        return;
                    }
                    // Fatal if default fails
                    st->state = 6; return;
                }
                st->dest_addr = *(struct sockaddr_in*)res->ai_addr;
                freeaddrinfo(res);

                st->config_fd = socket(AF_INET, SOCK_STREAM, 0);
                if (IS_VALID_SOCKET(st->config_fd)) {
                    #ifdef _WIN32
                    u_long mode = 1; ioctlsocket(st->config_fd, FIONBIO, &mode);
                    #else
                    fcntl(st->config_fd, F_SETFL, fcntl(st->config_fd, F_GETFL, 0) | O_NONBLOCK);
                    #endif
                    connect(st->config_fd, (struct sockaddr*)&st->dest_addr, sizeof(st->dest_addr));
                } else {
                    // Fail
                    st->state = 6; return;
                }
            }

            fd_set wfds; struct timeval tv = {0, 0};
        FD_ZERO(&wfds);
#ifndef _WIN32
        if (st->config_fd >= FD_SETSIZE) {
            // Should probably close and fail
            st->state = 6;
            return;
        }
#endif
        FD_SET(st->config_fd, &wfds);
            if (select(st->config_fd + 1, NULL, &wfds, NULL, &tv) > 0) {
                st->auto_state = 1; // SEND
            }
            // Timeout check
            if (KTerm_GetTime() - st->start_time > 5.0) { st->state = 6; return; }
        }
        else if (st->auto_state == 1) { // SEND
            const char* req = "GET /speedtest-servers-static.php HTTP/1.1\r\nHost: c.speedtest.net\r\nConnection: close\r\n\r\n";
            send(st->config_fd, req, strlen(req), 0);
            st->auto_state = 2; // READ
        }
        else if (st->auto_state == 2) { // READ
            char buf[1024];
            int n = recv(st->config_fd, buf, sizeof(buf), 0);
            if (n > 0) {
                if (st->config_len + n < (int)sizeof(st->config_buffer) - 1) {
                    memcpy(st->config_buffer + st->config_len, buf, n);
                    st->config_len += n;
                    st->config_buffer[st->config_len] = '\0';
                }
            } else if (n == 0) {
                st->auto_state = 3; // PARSE
            }
            if (KTerm_GetTime() - st->start_time > 10.0) st->auto_state = 3; // Force Parse on timeout
        }
        else if (st->auto_state == 3) { // PARSE
            CLOSE_SOCKET(st->config_fd); st->config_fd = INVALID_SOCKET;

            char* start = strstr(st->config_buffer, "<server ");
            bool found = false;
            if (start) {
                char* url = strstr(start, "host=\"");
                if (url) {
                    url += 6;
                    char* end = strchr(url, '"');
                    if (end) {
                        *end = '\0';
                        char* colon = strchr(url, ':');
                        if (colon) {
                            *colon = '\0';
                            strncpy(st->host, url, sizeof(st->host)-1);
                            st->port = atoi(colon + 1);
                        } else {
                            strncpy(st->host, url, sizeof(st->host)-1);
                            st->port = 80;
                        }
                        found = true;

                        // Notify UI
                        if (st->callback) {
                            SpeedtestResult res = {0}; res.phase = 0;
                            st->callback(term, session, &res, st->user_data);
                        }
                    }
                }
            }

            if (!found) {
                strncpy(st->host, "speedtest.tele2.net", sizeof(st->host)-1);
                st->port = 80;
            }

            // Resolve selected host for actual test
            struct addrinfo hints = {0}, *res;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            char port_str[16]; snprintf(port_str, sizeof(port_str), "%d", st->port);

            if (getaddrinfo(st->host, port_str, &hints, &res) == 0) {
                st->dest_addr = *(struct sockaddr_in*)res->ai_addr;
                freeaddrinfo(res);
                st->state = 1; // PROCEED
            } else {
                st->state = 6; // FAIL
            }
        }
        return;
    }

    // STATE 1: LATENCY
    if (st->state == 1) {
        if (!st->latency_started) {
            st->latency_started = true;
            // 4 probes, 200ms interval, 1000ms timeout
            if (!KTerm_Net_ResponseTime(term, session, st->host, 4, 200, 1000, KTerm_Speedtest_LatencyCB, st)) {
                // If failed to start, skip latency
                st->latency_done = true;
            }
        }
        if (st->latency_done) {
            st->state = 2; // CONNECT_DL
            st->start_time = KTerm_GetTime();
        }
        return; // Wait next tick
    }

    if (st->state == 2) { // CONNECT_DL
        // Init Sockets if needed
        if (st->connected_count == 0 && st->streams[0].fd == INVALID_SOCKET) {
             for(int i=0; i<st->num_streams; i++) {
                  st->streams[i].fd = socket(AF_INET, SOCK_STREAM, 0);
                  if (IS_VALID_SOCKET(st->streams[i].fd)) {
#ifdef _WIN32
                      u_long mode = 1; ioctlsocket(st->streams[i].fd, FIONBIO, &mode);
#else
                      fcntl(st->streams[i].fd, F_SETFL, fcntl(st->streams[i].fd, F_GETFL, 0) | O_NONBLOCK);
#endif
                      connect(st->streams[i].fd, (struct sockaddr*)&st->dest_addr, sizeof(st->dest_addr));
                  }
             }
        }

        // Check connection status
        fd_set wfds; struct timeval tv = {0, 0};
        FD_ZERO(&wfds);
        int max_fd = -1;
        for(int i=0; i<st->num_streams; i++) {
            if (IS_VALID_SOCKET(st->streams[i].fd) && !st->streams[i].connected) {
                FD_SET(st->streams[i].fd, &wfds);
                if (st->streams[i].fd > max_fd) max_fd = st->streams[i].fd;
            }
        }

        if (max_fd > -1) {
            int res = select(max_fd + 1, NULL, &wfds, NULL, &tv);
            if (res > 0) {
                 for(int i=0; i<st->num_streams; i++) {
                     if (IS_VALID_SOCKET(st->streams[i].fd) && !st->streams[i].connected) {
                         if (FD_ISSET(st->streams[i].fd, &wfds)) {
                             int opt = 0; socklen_t len = sizeof(opt);
                             if (getsockopt(st->streams[i].fd, SOL_SOCKET, SO_ERROR, (char*)&opt, &len) == 0 && opt == 0) {
                                 st->streams[i].connected = true;
                                 st->connected_count++;
                                 // Send GET
                                 char req[1024];
                                 snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", st->dl_path, st->host);
                                 send(st->streams[i].fd, req, strlen(req), 0);
                             } else {
                                 // Fail this stream
                                 CLOSE_SOCKET(st->streams[i].fd);
                                 st->streams[i].fd = INVALID_SOCKET;
                             }
                         }
                     }
                 }
            }
        }

        // Check timeout (5s for connect phase)
        double now = KTerm_GetTime();
        if (now - st->start_time > 5.0 || st->connected_count == st->num_streams) {
             if (st->connected_count == 0) {
                 // All failed
                 if (st->callback) {
                     SpeedtestResult res = {0}; res.done = true;
                     st->callback(term, session, &res, st->user_data);
                 }
                 st->state = 6;
                 return;
             }
             st->state = 3; // RUN_DL
             st->phase_start_time = KTerm_GetTime();
        }
    }
    else if (st->state == 3) { // RUN_DL
        // Read data
        char buf[16384];
        for(int i=0; i<st->num_streams; i++) {
             if (st->streams[i].connected && IS_VALID_SOCKET(st->streams[i].fd)) {
                 int n = recv(st->streams[i].fd, buf, sizeof(buf), KTERM_MSG_DONTWAIT);
                 if (n > 0) {
                     st->streams[i].bytes += n;
                 } else if (n == 0) {
                     st->streams[i].connected = false;
                     CLOSE_SOCKET(st->streams[i].fd);
                     st->streams[i].fd = INVALID_SOCKET;
                 }
             }
        }

        // Check if any stream is still active
        bool any_connected = false;
        for(int i=0; i<st->num_streams; i++) {
             if (st->streams[i].connected) any_connected = true;
        }

        double now = KTerm_GetTime();
        double elapsed = now - st->phase_start_time;

        // Calc Speed
        uint64_t total = 0;
        for(int i=0; i<st->num_streams; i++) total += st->streams[i].bytes;

        if (elapsed > 0) st->dl_mbps = ((double)total * 8.0) / (elapsed * 1000000.0);

        if (st->callback) {
             SpeedtestResult res = {0};
             res.dl_mbps = st->dl_mbps;
             res.phase = 1;
             res.dl_progress = elapsed / st->duration_sec;
             if (res.dl_progress > 1.0) res.dl_progress = 1.0;
             st->callback(term, session, &res, st->user_data);
        }

        // Finish if duration expired OR all streams finished (e.g. file downloaded)
        if (elapsed >= st->duration_sec || (!any_connected && total > 0)) {
             // Close DL streams
             for(int i=0; i<st->num_streams; i++) {
                 if (IS_VALID_SOCKET(st->streams[i].fd)) { CLOSE_SOCKET(st->streams[i].fd); st->streams[i].fd = INVALID_SOCKET; }
                 st->streams[i].connected = false;
                 st->streams[i].bytes = 0;
             }
             st->connected_count = 0;
             st->state = 4; // CONNECT_UL
             st->start_time = KTerm_GetTime();
        }
    }
    else if (st->state == 4) { // CONNECT_UL
         // Initiate Upload connections
         // Fix: Guard against infinite socket creation loop if connection is pending
         if (st->connected_count == 0 && st->streams[0].fd == INVALID_SOCKET) {
              // Start connections if not already started
              // (Wait, we need to re-open sockets)
              for(int i=0; i<st->num_streams; i++) {
                   st->streams[i].fd = socket(AF_INET, SOCK_STREAM, 0);
                   if (IS_VALID_SOCKET(st->streams[i].fd)) {
#ifdef _WIN32
                       u_long mode = 1; ioctlsocket(st->streams[i].fd, FIONBIO, &mode);
#else
                       fcntl(st->streams[i].fd, F_SETFL, fcntl(st->streams[i].fd, F_GETFL, 0) | O_NONBLOCK);
#endif
                       connect(st->streams[i].fd, (struct sockaddr*)&st->dest_addr, sizeof(st->dest_addr));
                   }
              }
         }

        fd_set wfds; struct timeval tv = {0, 0};
        FD_ZERO(&wfds);
        int max_fd = -1;
        for(int i=0; i<st->num_streams; i++) {
            if (IS_VALID_SOCKET(st->streams[i].fd) && !st->streams[i].connected) {
                FD_SET(st->streams[i].fd, &wfds);
                if (st->streams[i].fd > max_fd) max_fd = st->streams[i].fd;
            }
        }

        if (max_fd > -1) {
            int res = select(max_fd + 1, NULL, &wfds, NULL, &tv);
            if (res > 0) {
                 for(int i=0; i<st->num_streams; i++) {
                     if (IS_VALID_SOCKET(st->streams[i].fd) && !st->streams[i].connected) {
                         if (FD_ISSET(st->streams[i].fd, &wfds)) {
                             int opt = 0; socklen_t len = sizeof(opt);
                             if (getsockopt(st->streams[i].fd, SOL_SOCKET, SO_ERROR, (char*)&opt, &len) == 0 && opt == 0) {
                                 st->streams[i].connected = true;
                                 st->connected_count++;
                                 // Send POST Header
                                 char req[512];
                                 snprintf(req, sizeof(req), "POST /upload.php HTTP/1.1\r\nHost: %s\r\nContent-Length: 104857600\r\n\r\n", st->host);
                                 send(st->streams[i].fd, req, strlen(req), 0);
                             } else {
                                 CLOSE_SOCKET(st->streams[i].fd);
                                 st->streams[i].fd = INVALID_SOCKET;
                             }
                         }
                     }
                 }
            }
        }

        double now = KTerm_GetTime();
        if (now - st->start_time > 5.0 || st->connected_count == st->num_streams) {
             if (st->connected_count == 0) {
                 // Fail
                 st->state = 6;
                 if (st->callback) { SpeedtestResult res={0}; res.done=true; st->callback(term, session, &res, st->user_data); }
                 return;
             }
             st->state = 5; // RUN_UL
             st->phase_start_time = KTerm_GetTime();
        }
    }
    else if (st->state == 5) { // RUN_UL
        // Send data
        char chunk[8192]; // Dummy data
        memset(chunk, 'X', sizeof(chunk));

        for(int i=0; i<st->num_streams; i++) {
             if (st->streams[i].connected && IS_VALID_SOCKET(st->streams[i].fd)) {
                 int sent = send(st->streams[i].fd, chunk, sizeof(chunk), KTERM_MSG_DONTWAIT);
                 if (sent > 0) {
                     st->streams[i].bytes += sent;
                 }
             }
        }

        // Check if any stream is still active
        bool any_connected = false;
        for(int i=0; i<st->num_streams; i++) {
             if (st->streams[i].connected) any_connected = true;
        }

        double now = KTerm_GetTime();
        double elapsed = now - st->phase_start_time;

        uint64_t total = 0;
        for(int i=0; i<st->num_streams; i++) total += st->streams[i].bytes;

        if (elapsed > 0) st->ul_mbps = ((double)total * 8.0) / (elapsed * 1000000.0);

        if (st->callback) {
             SpeedtestResult res = {0};
             res.dl_mbps = st->dl_mbps;
             res.ul_mbps = st->ul_mbps;
             res.phase = 2;
             res.ul_progress = elapsed / st->duration_sec;
             if (res.ul_progress > 1.0) res.ul_progress = 1.0;
             st->callback(term, session, &res, st->user_data);
        }

        if (elapsed >= st->duration_sec || (!any_connected && total > 0)) {
             st->state = 6; // DONE
             // Final callback
             if (st->callback) {
                 SpeedtestResult res = {0};
                 res.dl_mbps = st->dl_mbps;
                 res.ul_mbps = st->ul_mbps;
                 res.phase = 3;
                 res.done = true;
                 st->callback(term, session, &res, st->user_data);
             }
        }
    }

    if (st->state == 6) {
         // Cleanup
         if (IS_VALID_SOCKET(st->config_fd)) { CLOSE_SOCKET(st->config_fd); st->config_fd = INVALID_SOCKET; }
         for(int i=0; i<st->num_streams; i++) {
             if (IS_VALID_SOCKET(st->streams[i].fd)) { CLOSE_SOCKET(st->streams[i].fd); st->streams[i].fd = INVALID_SOCKET; }
         }
    }
}

bool KTerm_Net_Speedtest(KTerm* term, KTermSession* session, const char* host, int port, int streams, const char* path, KTermSpeedtestCallback cb, void* user_data) {
    if (!term || !session) return false;

    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (!net) return false;

    if (net->speedtest) {
        // Cleanup existing
        if (IS_VALID_SOCKET(net->speedtest->config_fd)) CLOSE_SOCKET(net->speedtest->config_fd);
        for(int i=0; i<net->speedtest->num_streams; i++) {
            if (IS_VALID_SOCKET(net->speedtest->streams[i].fd)) CLOSE_SOCKET(net->speedtest->streams[i].fd);
        }
        if (net->speedtest->user_data) free(net->speedtest->user_data);
        free(net->speedtest);
    }

    net->speedtest = (KTermSpeedtestContext*)calloc(1, sizeof(KTermSpeedtestContext));
    if (!net->speedtest) return false;

    KTermSpeedtestContext* st = net->speedtest;
    st->config_fd = INVALID_SOCKET;

    st->num_streams = (streams > 0 && streams <= MAX_ST_STREAMS) ? streams : 4;
    st->callback = cb;
    st->user_data = user_data;
    st->duration_sec = 5.0; // Fixed duration for now

    if (path && path[0]) strncpy(st->dl_path, path, sizeof(st->dl_path)-1);
    else strcpy(st->dl_path, "/100MB.zip");

    // Check for Auto-Select
    if (!host || strcmp(host, "auto") == 0) {
        st->state = 0; // AUTO_SELECT
        st->auto_state = 0; // CONNECT
        st->start_time = KTerm_GetTime(); // Use for timeout
    } else {
        strncpy(st->host, host, sizeof(st->host)-1);
        st->port = (port > 0) ? port : 80;

        // Resolve
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        char port_str[16]; snprintf(port_str, sizeof(port_str), "%d", st->port);

        if (getaddrinfo(host, port_str, &hints, &res) != 0) {
            free(st); net->speedtest = NULL;
            return false;
        }
        st->dest_addr = *(struct sockaddr_in*)res->ai_addr;
        freeaddrinfo(res);
        st->state = 1; // LATENCY
    }

    // Streams init on demand in state CONNECT_DL/CONNECT_UL
    for(int i=0; i<MAX_ST_STREAMS; i++) st->streams[i].fd = INVALID_SOCKET;

    st->latency_started = false;
    st->latency_done = false;

    return true;
}

// --- LiveWire Implementation ---

#ifdef KTERM_ENABLE_LIVEWIRE

// ANSI Colors
#define ANSI_RESET "\x1B[0m"
#define ANSI_GRAY  "\x1B[90m"
#define ANSI_RED   "\x1B[31m"
#define ANSI_GREEN "\x1B[32m"
#define ANSI_YELLOW "\x1B[33m"
#define ANSI_BLUE  "\x1B[34m"
#define ANSI_MAGENTA "\x1B[35m"
#define ANSI_CYAN  "\x1B[36m"

static void LiveWire_ParseRTP(const unsigned char* data, int len, char* out, int max_len) {
    if (len < 12) return;
    int version = (data[0] >> 6) & 0x03;
    if (version != 2) return;
    int pt = data[1] & 0x7F;
    int seq = (data[2] << 8) | data[3];
    uint32_t ts = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    uint32_t ssrc = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
    snprintf(out, max_len, " RTP v2 PT=%d Seq=%d TS=%u SSRC=0x%X", pt, seq, ts, ssrc);
}

static void LiveWire_ParsePTP(const unsigned char* data, int len, char* out, int max_len) {
    if (len < 34) return;
    int msgType = data[0] & 0x0F;
    int ver = data[1] & 0x0F;
    int domain = data[4];
    int seq = (data[30] << 8) | data[31];
    const char* typeStr = "Unknown";
    switch(msgType) {
        case 0: typeStr = "Sync"; break;
        case 1: typeStr = "Delay_Req"; break;
        case 2: typeStr = "Pdelay_Req"; break;
        case 3: typeStr = "Pdelay_Resp"; break;
        case 8: typeStr = "Follow_Up"; break;
        case 9: typeStr = "Delay_Resp"; break;
        case 0xA: typeStr = "Pdelay_Resp_Follow_Up"; break;
        case 0xB: typeStr = "Announce"; break;
        case 0xC: typeStr = "Signaling"; break;
        case 0xD: typeStr = "Management"; break;
    }
    snprintf(out, max_len, " PTPv%d %s Seq=%d Dom=%d", ver, typeStr, seq, domain);
}

static void LiveWire_ParseDNS(const unsigned char* data, int len, char* out, int max_len) {
    if (len < 12) return;
    int qr = (data[2] >> 7) & 0x01;
    int qdcount = (data[4] << 8) | data[5];
    if (qdcount > 0 && len > 12) {
        char name[256];
        int npos = 0;
        int dpos = 12;
        while (dpos < len && npos < 255) {
            int lbl_len = data[dpos++];
            if (lbl_len == 0) break;
            if ((lbl_len & 0xC0) == 0xC0) {
                dpos++;
                break;
            }
            if (npos > 0) name[npos++] = '.';
            if (dpos + lbl_len > len) break;
            for(int i=0; i<lbl_len; i++) {
                char c = data[dpos++];
                if (npos < 255) name[npos++] = c;
            }
        }
        name[npos] = '\0';
        snprintf(out, max_len, " DNS %s %s", qr ? "Resp" : "Query", name);
    } else {
        snprintf(out, max_len, " DNS %s", qr ? "Resp" : "Query");
    }
}

static void LiveWire_ParseHTTP(const unsigned char* data, int len, char* out, int max_len) {
    if (len < 10) return;
    char prefix[16];
    int copy_len = (len < 15) ? len : 15;
    memcpy(prefix, data, copy_len);
    prefix[copy_len] = '\0';

    if (strncmp(prefix, "GET ", 4) == 0 || strncmp(prefix, "POST ", 5) == 0 ||
        strncmp(prefix, "PUT ", 4) == 0 || strncmp(prefix, "HEAD ", 5) == 0) {
        int eol = 0;
        for(int i=0; i<len && i<64; i++) {
            if (data[i] == '\r' || data[i] == '\n') { eol = i; break; }
        }
        if (eol > 0) {
            char line[65];
            memcpy(line, data, eol);
            line[eol] = '\0';
            snprintf(out, max_len, " HTTP %s", line);
        }
        return;
    }

    if (strncmp(prefix, "HTTP/", 5) == 0) {
        int eol = 0;
        for(int i=0; i<len && i<64; i++) {
            if (data[i] == '\r' || data[i] == '\n') { eol = i; break; }
        }
        if (eol > 0) {
            char line[65];
            memcpy(line, data, eol);
            line[eol] = '\0';
            snprintf(out, max_len, " %s", line);
        }
    }
}

static void LiveWire_WriteToBuffer(KTermLiveWireContext* ctx, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

#ifndef _WIN32
    pthread_mutex_lock(&ctx->mutex);
#else
    EnterCriticalSection(&ctx->mutex);
#endif

    for (int i = 0; buf[i]; i++) {
        ctx->out_buf[ctx->buf_head] = buf[i];
        ctx->buf_head = (ctx->buf_head + 1) % 65536;
        if (ctx->buf_head == ctx->buf_tail) ctx->buf_tail = (ctx->buf_tail + 1) % 65536; // Drop oldest
    }

#ifndef _WIN32
    pthread_mutex_unlock(&ctx->mutex);
#else
    LeaveCriticalSection(&ctx->mutex);
#endif
}

static void LiveWire_PacketHandler(u_char *user, const struct pcap_pkthdr *pkthdr, const u_char *pkt) {
    KTermLiveWireContext* ctx = (KTermLiveWireContext*)user;
    if (!ctx || !ctx->running) return;
    if (ctx->paused) return;

    ctx->captured_count++;
    if (ctx->count > 0 && ctx->captured_count > ctx->count) {
        pcap_breakloop(ctx->handle);
        return;
    }

    // Store Packet in Ring (Thread-Safe)
#ifndef _WIN32
    pthread_mutex_lock(&ctx->mutex);
#else
    EnterCriticalSection(&ctx->mutex);
#endif
    int idx = ctx->ring_head;
    ctx->packet_ring[idx].ts = pkthdr->ts;
    int copy_len = pkthdr->caplen;
    if (copy_len > 1500) copy_len = 1500;
    ctx->packet_ring[idx].len = copy_len;
    memcpy(ctx->packet_ring[idx].data, pkt, copy_len);
    ctx->ring_head = (ctx->ring_head + 1) % 128;
    ctx->ring_count++; // Increment total processed count
#ifndef _WIN32
    pthread_mutex_unlock(&ctx->mutex);
#else
    LeaveCriticalSection(&ctx->mutex);
#endif

    // Basic Dissection (Ethernet)
    // Assuming Ethernet for simplicity (DLT_EN10MB)
    // Offset 14 bytes
    if (pkthdr->caplen < 14) return;

    const unsigned char* ip_header = pkt + 14;
    int ip_len = pkthdr->caplen - 14;

    // Check IP version (v4=0x40)
    int version = (ip_header[0] >> 4);
    if (version != 4) return; // Only IPv4 for this demo

    // Frag Check
    uint16_t off = (ip_header[6] << 8) | ip_header[7];
    if ((off & 0x3FFF) != 0) {
        // Detected Fragmentation
#ifndef _WIN32
        pthread_mutex_lock(&ctx->mutex);
#else
        EnterCriticalSection(&ctx->mutex);
#endif
        if (!ctx->trigger_mtu_probe) {
             struct in_addr da;
             memcpy(&da, ip_header + 16, 4);
             inet_ntop(AF_INET, &da, ctx->last_frag_ip, sizeof(ctx->last_frag_ip));
             ctx->trigger_mtu_probe = true;
        }
#ifndef _WIN32
        pthread_mutex_unlock(&ctx->mutex);
#else
        LeaveCriticalSection(&ctx->mutex);
#endif
    }

    int header_len = (ip_header[0] & 0x0F) * 4;
    if (ip_len < header_len) return;

    // Proto
    int proto = ip_header[9];

    // Src/Dst
    char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
    struct in_addr sa, da;
    memcpy(&sa, ip_header + 12, 4);
    memcpy(&da, ip_header + 16, 4);
    inet_ntop(AF_INET, &sa, src, sizeof(src));
    inet_ntop(AF_INET, &da, dst, sizeof(dst));

    // Prepare Output Buffer
    // [Timestamp] Src -> Dst Proto Info

    // Timestamp
    struct tm* tm_info = localtime(&pkthdr->ts.tv_sec);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

    char out[1024];
    int pos = 0;

    // Timestamp (Gray)
    pos += snprintf(out + pos, sizeof(out) - pos, "%s[%s.%06ld]%s ", ANSI_GRAY, time_str, (long)pkthdr->ts.tv_usec, ANSI_RESET);

    // IP Flow (Blue)
    pos += snprintf(out + pos, sizeof(out) - pos, "%s%s \xE2\x86\x92 %s%s ", ANSI_BLUE, src, dst, ANSI_RESET);

    // Protocol Specifics
    uint16_t src_port = 0;
    uint16_t dst_port = 0;
    const unsigned char* flow_payload = NULL;
    int flow_payload_len = 0;

    if (proto == 6) { // TCP
        const unsigned char* tcp = ip_header + header_len;
        int tcp_len = ip_len - header_len;
        if (tcp_len >= 20) {
            int sport = (tcp[0] << 8) | tcp[1];
            int dport = (tcp[2] << 8) | tcp[3];
            src_port = sport; dst_port = dport;
            int flags = tcp[13];

            char flag_str[32] = "";
            if (flags & 0x02) strcat(flag_str, "SYN ");
            if (flags & 0x10) strcat(flag_str, "ACK ");
            if (flags & 0x01) strcat(flag_str, "FIN ");
            if (flags & 0x04) strcat(flag_str, "RST ");
            if (flags & 0x08) strcat(flag_str, "PSH ");

            pos += snprintf(out + pos, sizeof(out) - pos, "%sTCP%s %d\xE2\x86\x92%d %s", ANSI_GREEN, ANSI_RESET, sport, dport, flag_str);

            const unsigned char* payload = tcp + (tcp[12] >> 4) * 4;
            int payload_len = tcp_len - ((tcp[12] >> 4) * 4);
            int cap_remain = pkthdr->caplen - (int)(payload - pkt);
            if (cap_remain < payload_len) payload_len = cap_remain;

            if (payload_len > 0) {
                flow_payload = payload;
                flow_payload_len = payload_len;
            }

            if (sport == 80 || dport == 80 || sport == 8080 || dport == 8080) {
                // Parse HTTP
                if (payload_len > 0) {
                    char http_info[256] = "";
                    LiveWire_ParseHTTP(payload, payload_len, http_info, sizeof(http_info));
                    if (http_info[0]) pos += snprintf(out + pos, sizeof(out) - pos, "%s%s%s", ANSI_YELLOW, http_info, ANSI_RESET);
                }
            }
        }
    } else if (proto == 17) { // UDP
        const unsigned char* udp = ip_header + header_len;
        if (ip_len - header_len >= 8) {
            int sport = (udp[0] << 8) | udp[1];
            int dport = (udp[2] << 8) | udp[3];
            src_port = sport; dst_port = dport;
            int len = (udp[4] << 8) | udp[5];

            // Payload
            const unsigned char* payload = udp + 8;
            int payload_len = len - 8;
            // Safer calculation against captured length
            int cap_remain = pkthdr->caplen - (int)(payload - pkt);
            if (cap_remain < payload_len) payload_len = cap_remain;

            if (payload_len > 0) {
                flow_payload = payload;
                flow_payload_len = payload_len;
            }

            if (dport == 4321 || sport == 4321) {
                pos += snprintf(out + pos, sizeof(out) - pos, "%sUDP%s %d\xE2\x86\x92%d %s[Dante Audio]%s", ANSI_CYAN, ANSI_RESET, sport, dport, ANSI_MAGENTA, ANSI_RESET);
                if (payload_len > 0) {
                    char rtp_info[256] = "";
                    LiveWire_ParseRTP(payload, payload_len, rtp_info, sizeof(rtp_info));
                    if (rtp_info[0]) pos += snprintf(out + pos, sizeof(out) - pos, "%s%s%s", ANSI_MAGENTA, rtp_info, ANSI_RESET);
                }
            }
            else if (dport == 319 || dport == 320 || sport == 319 || sport == 320) {
                pos += snprintf(out + pos, sizeof(out) - pos, "%sUDP%s %d\xE2\x86\x92%d", ANSI_CYAN, ANSI_RESET, sport, dport);
                if (payload_len > 0) {
                    char ptp_info[256] = "";
                    LiveWire_ParsePTP(payload, payload_len, ptp_info, sizeof(ptp_info));
                    if (ptp_info[0]) pos += snprintf(out + pos, sizeof(out) - pos, "%s%s%s", ANSI_CYAN, ptp_info, ANSI_RESET);
                }
            }
            else if (dport == 53 || sport == 53) {
                pos += snprintf(out + pos, sizeof(out) - pos, "%sUDP%s %d\xE2\x86\x92%d", ANSI_CYAN, ANSI_RESET, sport, dport);
                if (payload_len > 0) {
                    char dns_info[256] = "";
                    LiveWire_ParseDNS(payload, payload_len, dns_info, sizeof(dns_info));
                    if (dns_info[0]) pos += snprintf(out + pos, sizeof(out) - pos, "%s%s%s", ANSI_YELLOW, dns_info, ANSI_RESET);
                }
            }
            else {
                pos += snprintf(out + pos, sizeof(out) - pos, "%sUDP%s %d\xE2\x86\x92%d Len=%d", ANSI_CYAN, ANSI_RESET, sport, dport, len);
                // Heuristic RTP check
                if (payload_len > 12 && (payload[0] & 0xC0) == 0x80) { // Version 2
                     // Maybe RTP? Check PT < 128?
                     if ((payload[1] & 0x7F) < 128) {
                         char rtp_info[256] = "";
                         LiveWire_ParseRTP(payload, payload_len, rtp_info, sizeof(rtp_info));
                         // Only print if it looks valid? Just hint it.
                         // Only print if we are sure? Let's just print if it parsed something successfully?
                         // LiveWire_ParseRTP only checks version.
                         // Let's add it if payload type is reasonable.
                         if (rtp_info[0]) pos += snprintf(out + pos, sizeof(out) - pos, "%s%s?%s", ANSI_GRAY, rtp_info, ANSI_RESET);
                     }
                }
            }
        }
    } else if (proto == 1) { // ICMP
        pos += snprintf(out + pos, sizeof(out) - pos, "%sICMP%s", ANSI_MAGENTA, ANSI_RESET);
    } else {
        pos += snprintf(out + pos, sizeof(out) - pos, "Proto=%d", proto);
    }

    // Update Stats & Flows
    LiveWireFlowKey key = {0};
    bool has_key = false;
    char stream_out_buf[512] = {0};

    // Only track IPv4 flows for now (already checked version=4 earlier)
    if (src_port > 0 || dst_port > 0) {
        has_key = true;
        memcpy(&key.src_ip, &sa, 4);
        memcpy(&key.dst_ip, &da, 4);
        key.proto = proto;
        key.src_port = src_port;
        key.dst_port = dst_port;
    }

#ifndef _WIN32
    pthread_mutex_lock(&ctx->mutex);
#else
    EnterCriticalSection(&ctx->mutex);
#endif

    // Update Global Stats
    ctx->stats.total_packets++;
    ctx->stats.total_bytes += pkthdr->len;
    if (proto == 6) ctx->stats.tcp_packets++;
    else if (proto == 17) ctx->stats.udp_packets++;
    else if (proto == 1) ctx->stats.icmp_packets++;
    else ctx->stats.other_packets++;

    // Update Flow
    if (has_key) {
        uint8_t hash = (key.src_ip ^ key.dst_ip ^ key.src_port ^ key.dst_port ^ key.proto) & 0xFF;
        LiveWireFlow* flow = ctx->flow_table[hash];
        while (flow) {
            if (memcmp(&flow->key, &key, sizeof(key)) == 0) break;
            flow = flow->next;
        }
        if (!flow && ctx->next_flow_id < 1024) {
            flow = (LiveWireFlow*)calloc(1, sizeof(LiveWireFlow));
            if (flow) {
                flow->key = key;
                flow->id = ++ctx->next_flow_id;
                flow->next = ctx->flow_table[hash];
                ctx->flow_table[hash] = flow;
            }
        }

        if (flow) {
            flow->stats.packets++;
            flow->stats.bytes += pkthdr->len;

            // Jitter calc (Inter-Arrival Variance)
            double now = (double)pkthdr->ts.tv_sec + (double)pkthdr->ts.tv_usec / 1000000.0;
            if (flow->stats.last_jitter_ts > 0) {
                double delta = now - flow->stats.last_jitter_ts;
                if (delta < 0) delta = 0;

                if (flow->stats.packets > 2) {
                    double diff = delta - flow->stats.prev_delta;
                    if (diff < 0) diff = -diff;
                    flow->stats.jitter += (diff - flow->stats.jitter) / 16.0;
                }
                flow->stats.prev_delta = delta;
            }
            flow->stats.last_jitter_ts = now;

            // Stream Follow
            if (ctx->follow_flow_id == flow->id) {
                if (flow_payload_len > 0) {
                    // Append to stream buffer
                    if (flow->stream.buf_len + flow_payload_len < (int)sizeof(flow->stream.buffer)) {
                        memcpy(flow->stream.buffer + flow->stream.buf_len, flow_payload, flow_payload_len);
                        flow->stream.buf_len += flow_payload_len;
                    }

                    // Format Output (Buffered to avoid deadlock)
                    int so_len = snprintf(stream_out_buf, sizeof(stream_out_buf), "\r\n%s[STREAM] %d bytes:%s ", ANSI_CYAN, flow_payload_len, ANSI_RESET);
                    for(int i=0; i<flow_payload_len && i<32 && so_len < (int)sizeof(stream_out_buf)-10; i++) {
                        unsigned char c = flow_payload[i];
                        if (c >= 32 && c < 127) so_len += snprintf(stream_out_buf + so_len, sizeof(stream_out_buf) - so_len, "%c", c);
                        else so_len += snprintf(stream_out_buf + so_len, sizeof(stream_out_buf) - so_len, ".");
                    }
                    snprintf(stream_out_buf + so_len, sizeof(stream_out_buf) - so_len, "\r\n");
                }
            }
        }
    }

#ifndef _WIN32
    pthread_mutex_unlock(&ctx->mutex);
#else
    LeaveCriticalSection(&ctx->mutex);
#endif

    if (stream_out_buf[0]) {
        LiveWire_WriteToBuffer(ctx, "%s", stream_out_buf);
    }

    pos += snprintf(out + pos, sizeof(out) - pos, "\r\n");

    // Output
    LiveWire_WriteToBuffer(ctx, "%s", out);
}

#ifndef _WIN32
static void* LiveWire_Thread(void* arg) {
#else
static DWORD WINAPI LiveWire_Thread(LPVOID arg) {
#endif
    KTermLiveWireContext* ctx = (KTermLiveWireContext*)arg;
    if (!ctx) return 0;

    pcap_loop(ctx->handle, ctx->count > 0 ? ctx->count : -1, LiveWire_PacketHandler, (u_char*)ctx);

    ctx->running = false;

    // Notify stopped
    LiveWire_WriteToBuffer(ctx, "%s[LiveWire] Stopped.%s\r\n", ANSI_YELLOW, ANSI_RESET);

    return 0;
}

void KTerm_Net_ProcessLiveWire(KTerm* term, KTermSession* session) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->livewire) return;
    KTermLiveWireContext* ctx = net->livewire;

#ifndef _WIN32
    pthread_mutex_lock(&ctx->mutex);
#else
    EnterCriticalSection(&ctx->mutex);
#endif

    // Check MTU Trigger
    bool trigger = ctx->trigger_mtu_probe;
    char target_ip[64];
    if (trigger) {
        strncpy(target_ip, ctx->last_frag_ip, sizeof(target_ip));
        ctx->trigger_mtu_probe = false;
    }

    while (ctx->buf_head != ctx->buf_tail) {
        char c = ctx->out_buf[ctx->buf_tail];
        ctx->buf_tail = (ctx->buf_tail + 1) % 65536;
        KTerm_WriteCharToSession(term, ctx->session_index, c);

        // Limit processing per frame to avoid blocking UI if flood
        // But we are locked, so we should be fast. WriteChar pushes to queue.
    }

#ifndef _WIN32
    pthread_mutex_unlock(&ctx->mutex);
#else
    LeaveCriticalSection(&ctx->mutex);
#endif

    if (trigger) {
        if (!net->mtu_probe) {
             char msg[128];
             snprintf(msg, sizeof(msg), "Auto-Triggering MTU Probe for %s (Frag Detected)", target_ip);
             KTerm_Net_Log(term, ctx->session_index, msg);
             KTerm_Net_MTUProbe(term, session, target_ip, true, 0, 0, NULL, NULL);
        }
    }
}

#endif // KTERM_ENABLE_LIVEWIRE

bool KTerm_Net_LiveWire_Start(KTerm* term, KTermSession* session, const char* params) {
#ifndef KTERM_ENABLE_LIVEWIRE
    KTerm_Net_Log(term, (int)(session - term->sessions), "LiveWire not enabled in build.");
    return false;
#else
    if (!term || !session) return false;

    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net) net = KTerm_Net_CreateContext(session);

    // Stop if already running
    if (net->livewire) KTerm_Net_LiveWire_Stop(term, session);

    net->livewire = (KTermLiveWireContext*)calloc(1, sizeof(KTermLiveWireContext));
    KTermLiveWireContext* ctx = net->livewire;

    ctx->term = term;
    ctx->session_index = (int)(session - term->sessions);
    ctx->snaplen = 65535;
    ctx->promisc = 1;
    ctx->timeout_ms = 1000;

#ifndef _WIN32
    pthread_mutex_init(&ctx->mutex, NULL);
#else
    InitializeCriticalSection(&ctx->mutex);
#endif

    // Defaults
    const char* iface = NULL;

    // Parse params: interface=x;filter=y;snaplen=z;count=c;promisc=p
    // Use a simple parser or strtok (careful with non-reentrant)
    // Note: params is const, need copy
    if (params) {
        char buf[1024];
        strncpy(buf, params, sizeof(buf)-1);
        char* saveptr;
#ifndef _WIN32
        char* token = strtok_r(buf, ";", &saveptr);
#else
        char* token = strtok_s(buf, ";", &saveptr);
#endif
        while (token) {
            if (strncmp(token, "interface=", 10) == 0) {
                strncpy(ctx->dev, token+10, sizeof(ctx->dev)-1);
                iface = ctx->dev;
            } else if (strncmp(token, "filter=", 7) == 0) {
                // Filter might be quoted "..."
                char* val = token + 7;
                if (val[0] == '"') {
                    // Primitive unquote
                    strncpy(ctx->filter_exp, val+1, sizeof(ctx->filter_exp)-1);
                    char* end = strrchr(ctx->filter_exp, '"');
                    if (end) *end = '\0';
                } else {
                    strncpy(ctx->filter_exp, val, sizeof(ctx->filter_exp)-1);
                }
            } else if (strncmp(token, "snaplen=", 8) == 0) {
                ctx->snaplen = atoi(token+8);
            } else if (strncmp(token, "count=", 6) == 0) {
                ctx->count = atoi(token+6);
            } else if (strncmp(token, "promisc=", 8) == 0) {
                ctx->promisc = atoi(token+8);
            }
#ifndef _WIN32
            token = strtok_r(NULL, ";", &saveptr);
#else
            token = strtok_s(NULL, ";", &saveptr);
#endif
        }
    }

    char errbuf[PCAP_ERRBUF_SIZE];

    // Find device if not specified
    if (!iface || !iface[0]) {
        pcap_if_t* alldevs;
        if (pcap_findalldevs(&alldevs, errbuf) == -1) {
            KTerm_Net_Log(term, ctx->session_index, "Failed to find devices");
            free(ctx); net->livewire = NULL;
            return false;
        }
        if (alldevs) {
            strncpy(ctx->dev, alldevs->name, sizeof(ctx->dev)-1);
            iface = ctx->dev;
            pcap_freealldevs(alldevs);
        } else {
            KTerm_Net_Log(term, ctx->session_index, "No devices found");
            free(ctx); net->livewire = NULL;
            return false;
        }
    }

    // Open
    ctx->handle = pcap_open_live(iface, ctx->snaplen, ctx->promisc, ctx->timeout_ms, errbuf);
    if (!ctx->handle) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to open %s: %s", iface, errbuf);
        KTerm_Net_Log(term, ctx->session_index, msg);
#ifndef _WIN32
        pthread_mutex_destroy(&ctx->mutex);
#else
        DeleteCriticalSection(&ctx->mutex);
#endif
        free(ctx); net->livewire = NULL;
        return false;
    }

    // Filter
    if (ctx->filter_exp[0]) {
        struct bpf_program fp;
        if (pcap_compile(ctx->handle, &fp, ctx->filter_exp, 0, 0) == -1) {
            KTerm_Net_Log(term, ctx->session_index, "Bad Filter Expression");
            pcap_close(ctx->handle);
#ifndef _WIN32
            pthread_mutex_destroy(&ctx->mutex);
#else
            DeleteCriticalSection(&ctx->mutex);
#endif
            free(ctx); net->livewire = NULL;
            return false;
        }
        if (pcap_setfilter(ctx->handle, &fp) == -1) {
            KTerm_Net_Log(term, ctx->session_index, "Failed to set filter");
            pcap_close(ctx->handle);
#ifndef _WIN32
            pthread_mutex_destroy(&ctx->mutex);
#else
            DeleteCriticalSection(&ctx->mutex);
#endif
            free(ctx); net->livewire = NULL;
            return false;
        }
    }

    ctx->running = true;

    // Spawn Thread
#ifndef _WIN32
    if (pthread_create(&ctx->thread, NULL, LiveWire_Thread, ctx) != 0) {
        KTerm_Net_Log(term, ctx->session_index, "Failed to create thread");
        pcap_close(ctx->handle);
        pthread_mutex_destroy(&ctx->mutex);
        free(ctx); net->livewire = NULL;
        return false;
    }
#else
    ctx->thread = CreateThread(NULL, 0, LiveWire_Thread, ctx, 0, NULL);
    if (ctx->thread == NULL) {
        KTerm_Net_Log(term, ctx->session_index, "Failed to create thread");
        pcap_close(ctx->handle);
        DeleteCriticalSection(&ctx->mutex);
        free(ctx); net->livewire = NULL;
        return false;
    }
#endif

    LiveWire_WriteToBuffer(ctx, "%s[LiveWire] Started on %s%s\r\n", ANSI_GREEN, iface, ANSI_RESET);
    return true;
#endif
}

void KTerm_Net_LiveWire_Stop(KTerm* term, KTermSession* session) {
#ifdef KTERM_ENABLE_LIVEWIRE
    (void)term;
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (net && net->livewire) {
        KTermLiveWireContext* ctx = net->livewire;
        if (ctx->running && ctx->handle) {
            pcap_breakloop(ctx->handle);
            // Join thread
#ifndef _WIN32
            pthread_join(ctx->thread, NULL);
            pthread_mutex_destroy(&ctx->mutex);
#else
            WaitForSingleObject(ctx->thread, INFINITE);
            CloseHandle(ctx->thread);
            DeleteCriticalSection(&ctx->mutex);
#endif
            ctx->running = false;
        }
        if (ctx->handle) {
            pcap_close(ctx->handle);
            ctx->handle = NULL;
        }
    }
#endif
}

void KTerm_Net_LiveWire_GetStatus(KTerm* term, KTermSession* session, char* buffer, size_t max_len) {
#ifdef KTERM_ENABLE_LIVEWIRE
    (void)term;
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (net && net->livewire) {
        const char* warn = "";
#ifdef _WIN32
        warn = ";WARN=WIN_RESTRICTED";
#endif
        snprintf(buffer, max_len, "RUNNING;CAPTURED=%d%s%s",
                 net->livewire->captured_count,
                 net->livewire->paused ? ";PAUSED" : "",
                 warn);
    } else {
        snprintf(buffer, max_len, "STOPPED");
    }
#else
    snprintf(buffer, max_len, "DISABLED");
#endif
}

void KTerm_Net_LiveWire_Pause(KTerm* term, KTermSession* session) {
    (void)term;
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (net && net->livewire) {
        net->livewire->paused = true;
    }
}

void KTerm_Net_LiveWire_Resume(KTerm* term, KTermSession* session) {
    (void)term;
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (net && net->livewire) {
        net->livewire->paused = false;
    }
}

bool KTerm_Net_LiveWire_SetFilter(KTerm* term, KTermSession* session, const char* filter) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->livewire) return false;

    // Reconstruct params
    KTermLiveWireContext* ctx = net->livewire;
    char params[512];
    snprintf(params, sizeof(params), "interface=%s;filter=%s;snaplen=%d;count=%d;promisc=%d;timeout=%d",
             ctx->dev, filter, ctx->snaplen, ctx->count, ctx->promisc, ctx->timeout_ms);

    // Restart (Start handles stop/restart)
    return KTerm_Net_LiveWire_Start(term, session, params);
}

bool KTerm_Net_LiveWire_GetDetail(KTerm* term, KTermSession* session, int packet_id, char* out, size_t max) {
    (void)term;
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->livewire) return false;
    KTermLiveWireContext* ctx = net->livewire;

    if (packet_id < 0) return false;

#ifndef _WIN32
    pthread_mutex_lock(&ctx->mutex);
#else
    EnterCriticalSection(&ctx->mutex);
#endif

    int total = ctx->ring_count;
    if (packet_id >= total || (total > 128 && packet_id < total - 128)) {
#ifndef _WIN32
        pthread_mutex_unlock(&ctx->mutex);
#else
        LeaveCriticalSection(&ctx->mutex);
#endif
        return false;
    }

    int idx = (ctx->ring_head - total + packet_id);
    while (idx < 0) idx += 128;
    idx %= 128;

    CapturedPacket pkt = ctx->packet_ring[idx]; // Copy under lock

#ifndef _WIN32
    pthread_mutex_unlock(&ctx->mutex);
#else
    LeaveCriticalSection(&ctx->mutex);
#endif

    int pos = 0;
    pos += snprintf(out + pos, max - pos, "PACKET %d (Len=%d)\r\n", packet_id, pkt.len);

    // Hex Dump
    for (int i = 0; i < pkt.len; i += 16) {
        if (pos >= (int)max - 80) break;
        pos += snprintf(out + pos, max - pos, "%04x: ", i);

        for (int j = 0; j < 16; j++) {
            if (i + j < pkt.len) pos += snprintf(out + pos, max - pos, "%02X ", pkt.data[i+j]);
            else pos += snprintf(out + pos, max - pos, "   ");
        }

        pos += snprintf(out + pos, max - pos, " |");
        for (int j = 0; j < 16; j++) {
            if (i + j < pkt.len) {
                unsigned char c = pkt.data[i+j];
                if (c >= 32 && c < 127) pos += snprintf(out + pos, max - pos, "%c", c);
                else pos += snprintf(out + pos, max - pos, ".");
            }
        }
        pos += snprintf(out + pos, max - pos, "|\r\n");
    }

    return true;
}

bool KTerm_Net_Whois(KTerm* term, KTermSession* session, const char* host, const char* query, KTermWhoisCallback cb, void* user_data) {
    if (!term || !session || !host || !query) return false;

    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (!net) return false;

    if (net->whois) {
        if (IS_VALID_SOCKET(net->whois->sockfd)) CLOSE_SOCKET(net->whois->sockfd);
        if (net->whois->user_data) free(net->whois->user_data);
        free(net->whois);
        net->whois = NULL;
    }

    net->whois = (KTermWhoisContext*)calloc(1, sizeof(KTermWhoisContext));
    if (!net->whois) return false;

    KTermWhoisContext* ctx = net->whois;
    strncpy(ctx->host, host, sizeof(ctx->host)-1);
    strncpy(ctx->query, query, sizeof(ctx->query)-1);
    ctx->callback = cb;
    ctx->user_data = user_data;
    ctx->timeout_ms = 5000;
    gettimeofday(&ctx->start_time, NULL);
    
    // Resolve host
    // Optimistic: Check if it's already an IP address to avoid blocking getaddrinfo
    if (inet_pton(AF_INET, host, &ctx->dest_addr.sin_addr) == 1) {
        ctx->dest_addr.sin_family = AF_INET;
        ctx->dest_addr.sin_port = htons(43);
    } else {
        // Fallback to blocking DNS
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        // Default whois port 43
        if (getaddrinfo(host, "43", &hints, &res) != 0) {
            free(ctx); net->whois = NULL;
            return false;
        }
        ctx->dest_addr = *(struct sockaddr_in*)res->ai_addr;
        freeaddrinfo(res);
    }

    ctx->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (!IS_VALID_SOCKET(ctx->sockfd)) {
        free(ctx); net->whois = NULL;
        return false;
    }

#ifdef _WIN32
    u_long mode = 1; ioctlsocket(ctx->sockfd, FIONBIO, &mode);
#else
    fcntl(ctx->sockfd, F_SETFL, fcntl(ctx->sockfd, F_GETFL, 0) | O_NONBLOCK);
#endif

    connect(ctx->sockfd, (struct sockaddr*)&ctx->dest_addr, sizeof(ctx->dest_addr));
    ctx->state = 1; // CONNECTING

    return true;
}

bool KTerm_Net_PortScan(KTerm* term, KTermSession* session, const char* host, const char* ports, int timeout_ms, KTermPortScanCallback cb, void* user_data) {
    if (!term || !session || !host || !ports) return false;

    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (!net) return false;

    // Cleanup existing
    if (net->port_scan) {
        if (IS_VALID_SOCKET(net->port_scan->sockfd)) CLOSE_SOCKET(net->port_scan->sockfd);
        if (net->port_scan->user_data) free(net->port_scan->user_data);
        free(net->port_scan);
        net->port_scan = NULL;
    }

    net->port_scan = (KTermPortScanContext*)calloc(1, sizeof(KTermPortScanContext));
    if (!net->port_scan) return false;

    KTermPortScanContext* ps = net->port_scan;
    strncpy(ps->host, host, sizeof(ps->host)-1);
    strncpy(ps->ports_str, ports, sizeof(ps->ports_str)-1);
    ps->ports_ptr = ps->ports_str; // Start
    ps->timeout_ms = (timeout_ms > 0) ? timeout_ms : 1000;
    ps->callback = cb;
    ps->user_data = user_data;

    // Pre-resolve host (blocking, but just once)
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, NULL, &hints, &res) != 0) {
        free(ps); net->port_scan = NULL;
        return false;
    }
    ps->dest_addr = *(struct sockaddr_in*)res->ai_addr;
    freeaddrinfo(res);

    ps->state = 2; // Ready for Next
    ps->sockfd = INVALID_SOCKET;

    return true;
}

void KTerm_Net_ProcessHttpProbe(KTerm* term, KTermSession* session) {
    KTermNetSession* net = KTerm_Net_GetContext(session);
    if (!net || !net->http_probe) return;
    KTermHttpProbeContext* ctx = net->http_probe;

    if (ctx->state == 6) return; // DONE

    if (ctx->state == 2) { // CONNECT
        fd_set wfds; struct timeval tv = {0, 0};
        FD_ZERO(&wfds);
#ifndef _WIN32
        if (ctx->sockfd >= FD_SETSIZE) {
            ctx->state = 6; // Fail
            return;
        }
#endif
        FD_SET(ctx->sockfd, &wfds);

        int res = select(ctx->sockfd + 1, NULL, &wfds, NULL, &tv);
        if (res > 0) {
            int opt = 0; socklen_t len = sizeof(opt);
            if (getsockopt(ctx->sockfd, SOL_SOCKET, SO_ERROR, (char*)&opt, &len) == 0 && opt == 0) {
                // Connected
                ctx->connect_ms = (KTerm_GetTime() - ctx->connect_start_time) * 1000.0;
                ctx->state = 3; // SEND
            } else {
                // Fail
                if (ctx->callback) {
                    KTermHttpProbeResult r = {0}; r.error = true; snprintf(r.error_msg, sizeof(r.error_msg), "Connect Failed");
                    ctx->callback(term, session, &r, ctx->user_data);
                }
                ctx->state = 6;
            }
        } else {
            // Timeout check (5s fixed for now)
            if (KTerm_GetTime() - ctx->connect_start_time > 5.0) {
                if (ctx->callback) {
                    KTermHttpProbeResult r = {0}; r.error = true; snprintf(r.error_msg, sizeof(r.error_msg), "Connect Timeout");
                    ctx->callback(term, session, &r, ctx->user_data);
                }
                ctx->state = 6;
            }
        }
    }
    else if (ctx->state == 3) { // SEND
        char req[2048];
        snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: KTerm/2.6\r\nConnection: close\r\n\r\n", ctx->path, ctx->host);

        ctx->request_start_time = KTerm_GetTime();
        ssize_t sent = send(ctx->sockfd, req, strlen(req), 0);
        if (sent > 0) {
            ctx->state = 4; // WAIT_HEAD
        } else {
             if (ctx->callback) {
                 KTermHttpProbeResult r = {0}; r.error = true; snprintf(r.error_msg, sizeof(r.error_msg), "Send Failed");
                 ctx->callback(term, session, &r, ctx->user_data);
             }
             ctx->state = 6;
        }
    }
    else if (ctx->state == 4) { // WAIT_HEAD
        // Check for readability
        fd_set rfds; struct timeval tv = {0, 0};
        FD_ZERO(&rfds);
#ifndef _WIN32
        if (ctx->sockfd >= FD_SETSIZE) {
            ctx->state = 6; // Fail
            return;
        }
#endif
        FD_SET(ctx->sockfd, &rfds);

        if (select(ctx->sockfd + 1, &rfds, NULL, NULL, &tv) > 0) {
            ctx->first_byte_time = KTerm_GetTime();
            ctx->ttfb_ms = (ctx->first_byte_time - ctx->request_start_time) * 1000.0;
            ctx->state = 5; // READ_BODY
        } else {
             if (KTerm_GetTime() - ctx->request_start_time > 5.0) {
                 if (ctx->callback) {
                     KTermHttpProbeResult r = {0}; r.error = true; snprintf(r.error_msg, sizeof(r.error_msg), "Response Timeout");
                     ctx->callback(term, session, &r, ctx->user_data);
                 }
                 ctx->state = 6;
             }
        }
    }
    else if (ctx->state == 5) { // READ_BODY
        char buf[4096];
        ssize_t n = recv(ctx->sockfd, buf, sizeof(buf), KTERM_MSG_DONTWAIT);
        if (n > 0) {
            ctx->size_bytes += n;
            // Parse Headers if not fully done or if body separator not found yet
            char* body_start = strstr(ctx->buffer, "\r\n\r\n");
            if (!body_start) {
                // Append to internal small buffer for parsing
                if (ctx->buffer_len < (int)sizeof(ctx->buffer) - 1) {
                    int space = sizeof(ctx->buffer) - ctx->buffer_len - 1;
                    int copy = (n < space) ? n : space;
                    memcpy(ctx->buffer + ctx->buffer_len, buf, copy);
                    ctx->buffer_len += copy;
                    ctx->buffer[ctx->buffer_len] = '\0';

                    if (ctx->status_code == 0 && strncmp(ctx->buffer, "HTTP/", 5) == 0) {
                        char* space1 = strchr(ctx->buffer, ' ');
                        if (space1) {
                            ctx->status_code = atoi(space1 + 1);
                        }
                    }

                    // Look for Content-Length (Case-insensitive approximation: check common cases)
                    if (ctx->content_length == 0) {
                        char* cl = strstr(ctx->buffer, "Content-Length: ");
                        if (!cl) cl = strstr(ctx->buffer, "content-length: ");
                        if (!cl) cl = strstr(ctx->buffer, "Content-length: ");
                        if (cl) {
                            ctx->content_length = strtoull(cl + 16, NULL, 10);
                        }
                    }

                    // Update body_start check after append
                    body_start = strstr(ctx->buffer, "\r\n\r\n");
                }
            }

            // Check if done based on content length (Keep-Alive support)
            if (ctx->content_length > 0 && body_start) {
                uint64_t header_size = (uint64_t)(body_start - ctx->buffer) + 4;
                if (ctx->size_bytes >= header_size + ctx->content_length) {
                    n = 0; // Trigger done
                }
            }

        }

        if (n == 0) {
            // Done
            double end_time = KTerm_GetTime();

            KTermHttpProbeResult r = {0};
            r.status_code = ctx->status_code;
            r.dns_ms = ctx->dns_ms;
            r.connect_ms = ctx->connect_ms;
            r.ttfb_ms = ctx->ttfb_ms;
            r.download_ms = (end_time - ctx->first_byte_time) * 1000.0;
            r.total_ms = (end_time - ctx->start_time) * 1000.0;
            r.size_bytes = ctx->size_bytes;
            r.speed_mbps = 0;
            if (r.download_ms > 0) {
                r.speed_mbps = ((double)ctx->size_bytes * 8.0) / (r.download_ms * 1000.0);
            }

            if (ctx->callback) ctx->callback(term, session, &r, ctx->user_data);
            ctx->state = 6;

        } else {
             #ifdef _WIN32
             if (WSAGetLastError() != WSAEWOULDBLOCK)
             #else
             if (errno != EAGAIN && errno != EWOULDBLOCK)
             #endif
             {
                 // Error
                 if (ctx->callback) {
                     KTermHttpProbeResult r = {0}; r.error = true; snprintf(r.error_msg, sizeof(r.error_msg), "Read Error");
                     ctx->callback(term, session, &r, ctx->user_data);
                 }
                 ctx->state = 6;
             }
        }

        // Read Timeout (10s total from first byte)
        if (ctx->state == 5 && (KTerm_GetTime() - ctx->first_byte_time > 10.0)) {
             if (ctx->callback) {
                 KTermHttpProbeResult r = {0}; r.error = true; snprintf(r.error_msg, sizeof(r.error_msg), "Read Timeout");
                 ctx->callback(term, session, &r, ctx->user_data);
             }
             ctx->state = 6;
        }
    }

    if (ctx->state == 6) {
        if (IS_VALID_SOCKET(ctx->sockfd)) {
             CLOSE_SOCKET(ctx->sockfd);
             ctx->sockfd = INVALID_SOCKET;
        }
    }
}

bool KTerm_Net_HttpProbe(KTerm* term, KTermSession* session, const char* url, KTermHttpProbeCallback cb, void* user_data) {
    if (!term || !session || !url) return false;

    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (!net) return false;

    if (net->http_probe) {
        if (IS_VALID_SOCKET(net->http_probe->sockfd)) CLOSE_SOCKET(net->http_probe->sockfd);
        if (net->http_probe->user_data) free(net->http_probe->user_data);
        free(net->http_probe);
    }

    net->http_probe = (KTermHttpProbeContext*)calloc(1, sizeof(KTermHttpProbeContext));
    if (!net->http_probe) return false;

    KTermHttpProbeContext* ctx = net->http_probe;
    ctx->callback = cb;
    ctx->user_data = user_data;
    ctx->sockfd = INVALID_SOCKET;

    // Parse URL
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    else if (strncmp(p, "https://", 8) == 0) p += 8; // We don't support SSL in this lightweight probe yet? Or assume raw?
    // Note: This probe uses raw sockets, so HTTPS will fail or return garbage unless we use SSL context.
    // Ideally we should reject HTTPS or use KTerm's security layer if configured.
    // For now, assume HTTP or raw TCP.

    const char* slash = strchr(p, '/');
    const char* colon = strchr(p, ':');

    int host_len = 0;
    if (slash) {
        host_len = (int)(slash - p);
        if (colon && colon < slash) host_len = (int)(colon - p);
    } else {
        host_len = strlen(p);
        if (colon) host_len = (int)(colon - p);
    }

    if (host_len >= (int)sizeof(ctx->host)) host_len = sizeof(ctx->host) - 1;
    strncpy(ctx->host, p, host_len);
    ctx->host[host_len] = '\0';

    if (colon && (!slash || colon < slash)) {
        ctx->port = atoi(colon + 1);
    } else {
        ctx->port = 80;
    }

    if (slash) {
        strncpy(ctx->path, slash, sizeof(ctx->path)-1);
    } else {
        strcpy(ctx->path, "/");
    }

    ctx->start_time = KTerm_GetTime();

    // DNS
    ctx->dns_start_time = KTerm_GetTime();
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[16]; snprintf(port_str, sizeof(port_str), "%d", ctx->port);

    if (getaddrinfo(ctx->host, port_str, &hints, &res) != 0) {
        free(ctx); net->http_probe = NULL;
        return false;
    }
    ctx->dest_addr = *(struct sockaddr_in*)res->ai_addr;
    freeaddrinfo(res);

    ctx->dns_ms = (KTerm_GetTime() - ctx->dns_start_time) * 1000.0;

    // Connect
    ctx->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (!IS_VALID_SOCKET(ctx->sockfd)) {
        free(ctx); net->http_probe = NULL;
        return false;
    }

    #ifdef _WIN32
    u_long mode = 1; ioctlsocket(ctx->sockfd, FIONBIO, &mode);
    #else
    fcntl(ctx->sockfd, F_SETFL, fcntl(ctx->sockfd, F_GETFL, 0) | O_NONBLOCK);
    #endif

    ctx->connect_start_time = KTerm_GetTime();
    connect(ctx->sockfd, (struct sockaddr*)&ctx->dest_addr, sizeof(ctx->dest_addr));

    ctx->state = 2; // CONNECT

    return true;
}

bool KTerm_Net_MTUProbe(KTerm* term, KTermSession* session, const char* host, bool df, int start_size, int max_size, KTermMtuProbeCallback cb, void* user_data) {
    if (!term || !session || !host) return false;

    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (!net) return false;

    if (net->mtu_probe) {
        if (IS_VALID_SOCKET(net->mtu_probe->sockfd)) CLOSE_SOCKET(net->mtu_probe->sockfd);
#ifdef _WIN32
        if (net->mtu_probe->icmp_handle != INVALID_HANDLE_VALUE) IcmpCloseHandle(net->mtu_probe->icmp_handle);
        if (net->mtu_probe->icmp_event) CloseHandle(net->mtu_probe->icmp_event);
#endif
        if (net->mtu_probe->user_data) free(net->mtu_probe->user_data);
        free(net->mtu_probe);
        net->mtu_probe = NULL;
    }

    net->mtu_probe = (KTermMtuProbeContext*)calloc(1, sizeof(KTermMtuProbeContext));
    if (!net->mtu_probe) return false;

    KTermMtuProbeContext* ctx = net->mtu_probe;
    strncpy(ctx->host, host, sizeof(ctx->host)-1);
    ctx->df = df;
    ctx->min_size = (start_size > 0) ? start_size : 64;
    ctx->max_size = (max_size > 0) ? max_size : 1500;
    ctx->callback = cb;
    ctx->user_data = user_data;

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;

    if (getaddrinfo(host, NULL, &hints, &res) != 0) {
        free(ctx); net->mtu_probe = NULL;
        return false;
    }
    ctx->dest_addr = *(struct sockaddr_in*)res->ai_addr;
    freeaddrinfo(res);

    ctx->state = 2; // SOCKET
    ctx->sockfd = INVALID_SOCKET;
    ctx->known_good_size = 0;

    return true;
}

bool KTerm_Net_FragTest(KTerm* term, KTermSession* session, const char* host, int size, int fragments, KTermFragTestCallback cb, void* user_data) {
    if (!term || !session || !host) return false;

    KTermNetSession* net = KTerm_Net_CreateContext(session);
    if (!net) return false;

    if (net->frag_test) {
        if (IS_VALID_SOCKET(net->frag_test->sockfd)) CLOSE_SOCKET(net->frag_test->sockfd);
#ifdef _WIN32
        if (net->frag_test->icmp_handle != INVALID_HANDLE_VALUE) IcmpCloseHandle(net->frag_test->icmp_handle);
        if (net->frag_test->icmp_event) CloseHandle(net->frag_test->icmp_event);
#endif
        if (net->frag_test->user_data) free(net->frag_test->user_data);
        free(net->frag_test);
        net->frag_test = NULL;
    }

    net->frag_test = (KTermFragTestContext*)calloc(1, sizeof(KTermFragTestContext));
    if (!net->frag_test) return false;

    KTermFragTestContext* ctx = net->frag_test;
    strncpy(ctx->host, host, sizeof(ctx->host)-1);
    ctx->size = (size > 0) ? size : 2000;
    ctx->fragments = (fragments > 0) ? fragments : 2;
    ctx->callback = cb;
    ctx->user_data = user_data;

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;

    if (getaddrinfo(host, NULL, &hints, &res) != 0) {
        free(ctx); net->frag_test = NULL;
        return false;
    }
    ctx->dest_addr = *(struct sockaddr_in*)res->ai_addr;
    freeaddrinfo(res);

    ctx->state = 2; // SOCKET
    ctx->sockfd = INVALID_SOCKET;

    return true;
}

#endif // KTERM_DISABLE_NET

#endif // KTERM_NET_IMPLEMENTATION_GUARD
#endif // KTERM_NET_IMPLEMENTATION
