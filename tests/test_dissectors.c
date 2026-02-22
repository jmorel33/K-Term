#define KTERM_NET_IMPLEMENTATION
#define KTERM_ENABLE_LIVEWIRE
#define KTERM_USE_BUNDLED_PCAP
#define KTERM_DISABLE_VOICE

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
    // Mock Resize
    int width, height;
} KTerm;

// Mock KTerm_WriteCharToSession
void KTerm_WriteCharToSession(KTerm* term, int session_index, char c) {
    // No-op for dissector verify test, we inspect ring buffer directly
}

void KTerm_Resize(KTerm* term, int w, int h) {
    term->width = w; term->height = h;
}

void KTerm_WriteString(KTerm* term, const char* s) {}

void KTerm_SetOutputSink(KTerm* term, void* sink, void* user_data) {}

// Include pcap header so we have types
#include "../deps/pcap.h"

// Stubs for pcap functions to satisfy linker
pcap_t* pcap_open_live(const char* device, int snaplen, int promisc, int to_ms, char* errbuf) { return (pcap_t*)1; }
int pcap_compile(pcap_t* p, struct bpf_program* fp, const char* str, int optimize, uint32_t netmask) { return 0; }
int pcap_setfilter(pcap_t* p, struct bpf_program* fp) { return 0; }
int pcap_loop(pcap_t* p, int cnt, pcap_handler callback, u_char* user) { return 0; }
void pcap_breakloop(pcap_t* p) {}
void pcap_close(pcap_t* p) {}
int pcap_findalldevs(pcap_if_t** alldevs, char* errbuf) { *alldevs = NULL; return 0; }
void pcap_freealldevs(pcap_if_t* alldevs) {}
char* pcap_geterr(pcap_t* p) { return "Stub Error"; }

// Include the header
#include "../kt_net.h"

// Helper to inspect buffer
static void CheckBufferContains(KTermLiveWireContext* ctx, const char* expected) {
    // Reconstruct string from ring buffer
    char buf[65536];
    int len = 0;
    int head = ctx->buf_head;
    int tail = ctx->buf_tail;

    while (tail != head) {
        buf[len++] = ctx->out_buf[tail];
        tail = (tail + 1) % 65536;
    }
    buf[len] = '\0';

    if (strstr(buf, expected) == NULL) {
        printf("FAIL: Expected '%s' in buffer.\nBuffer content: %s\n", expected, buf);
        exit(1);
    } else {
        printf("PASS: Found '%s'\n", expected);
    }
}

static void ClearBuffer(KTermLiveWireContext* ctx) {
    ctx->buf_tail = ctx->buf_head;
}

int main() {
    printf("Verifying Dissectors...\n");

    KTermLiveWireContext ctx = {0};
    ctx.running = true;
    ctx.count = 100;
#ifndef _WIN32
    pthread_mutex_init(&ctx.mutex, NULL);
#else
    InitializeCriticalSection(&ctx.mutex);
#endif

    // 1. Test Dante (UDP 4321)
    {
        printf("Test 1: Dante (UDP 4321)...\n");
        unsigned char pkt[100];
        memset(pkt, 0, sizeof(pkt));
        struct pcap_pkthdr hdr = {0};
        hdr.caplen = 60;
        hdr.len = 60;

        // Eth (14) + IP (20) + UDP (8) + Payload
        // IP offset 14
        pkt[14] = 0x45; // v4, len 5
        pkt[23] = 17;   // UDP

        // UDP offset 34 (14+20)
        // Src Port 1234
        pkt[34] = 0x04; pkt[35] = 0xD2;
        // Dst Port 4321 (0x10E1)
        pkt[36] = 0x10; pkt[37] = 0xE1;
        // Len
        pkt[38] = 0x00; pkt[39] = 26; // 8 header + 18 payload

        // RTP Payload (Offset 42)
        // V=2 (0x80)
        pkt[42] = 0x80;
        // PT=96 (dynamic)
        pkt[43] = 96;
        // Seq=123
        pkt[44] = 0x00; pkt[45] = 123;
        // TS
        pkt[46] = 0; pkt[47] = 0; pkt[48] = 0; pkt[49] = 100;
        // SSRC
        pkt[50] = 0; pkt[51] = 0; pkt[52] = 0; pkt[53] = 5;

        LiveWire_PacketHandler((u_char*)&ctx, &hdr, pkt);

        CheckBufferContains(&ctx, "Dante Audio");
        CheckBufferContains(&ctx, "RTP v2");
        CheckBufferContains(&ctx, "Seq=123");
        ClearBuffer(&ctx);
    }

    // 2. Test PTP (UDP 319)
    {
        printf("Test 2: PTP (UDP 319)...\n");
        unsigned char pkt[100];
        memset(pkt, 0, sizeof(pkt));
        struct pcap_pkthdr hdr = {0};
        hdr.caplen = 80;
        hdr.len = 80;

        pkt[14] = 0x45;
        pkt[23] = 17;   // UDP

        // Dst Port 319 (0x013F)
        pkt[36] = 0x01; pkt[37] = 0x3F;
        // Len
        pkt[38] = 0; pkt[39] = 8 + 38; // 46

        // PTP Payload (Offset 42)
        // MsgType=0 (Sync), Ver=2 -> 0x02
        pkt[42] = 0x00; // Type 0
        pkt[43] = 0x02; // Ver 2
        // Domain=1
        pkt[46] = 1;
        // Seq=55
        pkt[72] = 0; pkt[73] = 55; // Offset 30 in PTP -> 42+30 = 72

        LiveWire_PacketHandler((u_char*)&ctx, &hdr, pkt);

        CheckBufferContains(&ctx, "PTPv2 Sync");
        CheckBufferContains(&ctx, "Seq=55");
        CheckBufferContains(&ctx, "Dom=1");
        ClearBuffer(&ctx);
    }

    // 3. Test DNS (UDP 53)
    {
        printf("Test 3: DNS (UDP 53)...\n");
        unsigned char pkt[100];
        memset(pkt, 0, sizeof(pkt));
        struct pcap_pkthdr hdr = {0};
        hdr.caplen = 70;
        hdr.len = 70;

        pkt[14] = 0x45;
        pkt[23] = 17;   // UDP

        // Dst Port 53
        pkt[36] = 0x00; pkt[37] = 53;
        // Len
        pkt[38] = 0; pkt[39] = 8 + 28; // 36

        // DNS Payload (Offset 42)
        // ID=1
        pkt[42] = 0; pkt[43] = 1;
        // Flags=0 (Query)
        pkt[44] = 0; pkt[45] = 0;
        // QDCOUNT=1
        pkt[46] = 0; pkt[47] = 1;

        // QNAME: 3www6google3com0
        unsigned char* q = &pkt[54];
        *q++ = 3; memcpy(q, "www", 3); q += 3;
        *q++ = 6; memcpy(q, "google", 6); q += 6;
        *q++ = 3; memcpy(q, "com", 3); q += 3;
        *q++ = 0;

        LiveWire_PacketHandler((u_char*)&ctx, &hdr, pkt);

        CheckBufferContains(&ctx, "DNS Query");
        CheckBufferContains(&ctx, "www.google.com");
        ClearBuffer(&ctx);
    }

    // 4. Test HTTP (TCP 80)
    {
        printf("Test 4: HTTP (TCP 80)...\n");
        unsigned char pkt[200];
        memset(pkt, 0, sizeof(pkt));
        struct pcap_pkthdr hdr = {0};
        hdr.caplen = 100;
        hdr.len = 100;

        pkt[14] = 0x45;
        pkt[23] = 6;   // TCP

        // TCP Header Offset 34
        // Dst Port 80
        pkt[36] = 0x00; pkt[37] = 80;
        // Data Offset = 5 (20 bytes) -> 0x50 at byte 46 (12th byte of TCP)
        pkt[46] = 0x50;

        // Payload at 34 + 20 = 54
        sprintf((char*)&pkt[54], "GET /index.html HTTP/1.1\r\nHost: example.com\r\n\r\n");

        LiveWire_PacketHandler((u_char*)&ctx, &hdr, pkt);

        CheckBufferContains(&ctx, "TCP");
        CheckBufferContains(&ctx, "HTTP GET /index.html HTTP/1.1");
        ClearBuffer(&ctx);
    }

    printf("All tests passed!\n");
    return 0;
}
