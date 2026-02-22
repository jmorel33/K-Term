#define KTERM_NET_IMPLEMENTATION
#define KTERM_ENABLE_WIREDIAG
#define KTERM_USE_BUNDLED_PCAP
#define KTERM_DISABLE_VOICE
#define KTERM_VERSION_STRING "2.6.42"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>

#define MAX_SESSIONS 4

// Forward decl
struct KTerm_T;

typedef struct KTermSession_T {
    void* user_data;
} KTermSession;

typedef struct KTerm_T {
    KTermSession sessions[MAX_SESSIONS];
    void (*response_callback)(struct KTerm_T*, const char*, int);
    int width, height;
} KTerm;

void KTerm_WriteCharToSession(KTerm* term, int session_index, char c) {}
void KTerm_Resize(KTerm* term, int w, int h) { term->width = w; term->height = h; }
void KTerm_WriteString(KTerm* term, const char* s) {}
void KTerm_SetOutputSink(KTerm* term, void* sink, void* user_data) {}

// Include pcap header
#include "../deps/pcap.h"

// Stubs
pcap_t* pcap_open_live(const char* device, int snaplen, int promisc, int to_ms, char* errbuf) { return (pcap_t*)1; }
int pcap_compile(pcap_t* p, struct bpf_program* fp, const char* str, int optimize, uint32_t netmask) { return 0; }
int pcap_setfilter(pcap_t* p, struct bpf_program* fp) { return 0; }
int pcap_loop(pcap_t* p, int cnt, pcap_handler callback, u_char* user) { return 0; }
void pcap_breakloop(pcap_t* p) {}
void pcap_close(pcap_t* p) {}
int pcap_findalldevs(pcap_if_t** alldevs, char* errbuf) { *alldevs = NULL; return 0; }
void pcap_freealldevs(pcap_if_t* alldevs) {}
char* pcap_geterr(pcap_t* p) { return "Stub Error"; }

#include "../kt_net.h"

int main() {
    printf("Verifying Advanced Auth Detection...\n");

    // 1. Verify KTerm_Net_QueryProtocol
    {
        const KTermProtocolDef* def = KTerm_Net_QueryProtocol(22, false); // SSH TCP
        assert(def != NULL);
        printf("SSH Short Name: %s\n", def->short_name);
        assert(strcmp(def->short_name, "SSH") == 0);
        assert(def->supports_auth == true);
        assert(def->high_bruteforce_risk == true);
        printf("PASS: QueryProtocol SSH\n");

        def = KTerm_Net_QueryProtocol(21, false); // FTP
        assert(def != NULL);
        assert(def->plaintext_auth == true);
        printf("PASS: QueryProtocol FTP\n");
    }

    // 2. Verify Heuristic Detection (WireDiag)
    {
        KTermWireDiagContext ctx = {0};
        ctx.running = true;
        // ctx.handle = ...;
        // Need to set up session context because ScanAuthFlows uses GetContext(session)

        KTerm term = {0};
        KTermSession* session = &term.sessions[0];

        // Mock session user_data to point to a KTermNetSession containing our ctx
        KTermNetSession net = {0};
        net.wirediag = &ctx;
        session->user_data = &net;

#ifndef _WIN32
        pthread_mutex_init(&ctx.mutex, NULL);
#else
        InitializeCriticalSection(&ctx.mutex);
#endif

        unsigned char pkt[100];
        memset(pkt, 0, sizeof(pkt));
        struct pcap_pkthdr hdr = {0};
        hdr.caplen = 80;
        hdr.len = 80;

        // Construct TCP Packet on Non-Standard Port (2222)
        pkt[14] = 0x45; // v4
        pkt[23] = 6;    // TCP

        // Src 10.0.0.1 (0A 00 00 01)
        pkt[26] = 10; pkt[27] = 0; pkt[28] = 0; pkt[29] = 1;
        // Dst 10.0.0.2 (0A 00 00 02)
        pkt[30] = 10; pkt[31] = 0; pkt[32] = 0; pkt[33] = 2;

        // TCP Offset 34
        // Src Port 12345
        pkt[34] = 0x30; pkt[35] = 0x39;
        // Dst Port 2222 (0x08AE)
        pkt[36] = 0x08; pkt[37] = 0xAE;

        // Data Offset
        pkt[46] = 0x50; // 20 bytes

        // Payload at 54
        // Inject SSH Signature
        sprintf((char*)&pkt[54], "SSH-2.0-OpenSSH_8.9p1\r\n");

        WireDiag_PacketHandler((u_char*)&ctx, &hdr, pkt);

        // Verify Flow Table has Auth Detected
        char buf[4096];
        bool res = KTerm_Net_ScanAuthFlows(&term, session, buf, sizeof(buf));
        assert(res == true);
        printf("Scan Result: %s\n", buf);

        assert(strstr(buf, "PROTO=SSH") != NULL);
        assert(strstr(buf, ":12345->") != NULL);
        assert(strstr(buf, ":2222") != NULL);

        printf("PASS: Heuristic Auth Detection\n");
    }

    printf("All tests passed!\n");
    return 0;
}
