#include "../deps/pcap.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Mock pcap structure
struct pcap {
    int fd;
    char errbuf[PCAP_ERRBUF_SIZE];
    int break_loop;
};

static pcap_if_t mock_if = {
    .next = NULL,
    .name = "eth0",
    .description = "Mock Interface",
    .addresses = NULL,
    .flags = 0
};

pcap_t *pcap_open_live(const char *device, int snaplen, int promisc, int to_ms, char *errbuf) {
    (void)device; (void)snaplen; (void)promisc; (void)to_ms;
    pcap_t *p = (pcap_t *)malloc(sizeof(pcap_t));
    if (p) {
        p->break_loop = 0;
        p->fd = 1; // Fake fd
    }
    return p;
}

int pcap_findalldevs(pcap_if_t **alldevsp, char *errbuf) {
    (void)errbuf;
    // Return a mock interface list
    // We need to allocate it because caller calls freealldevs
    pcap_if_t *dev = (pcap_if_t *)malloc(sizeof(pcap_if_t));
    if (!dev) return -1;
    memcpy(dev, &mock_if, sizeof(pcap_if_t));
    // Allocate name/desc
    dev->name = strdup(mock_if.name);
    dev->description = strdup(mock_if.description);
    dev->next = NULL;
    *alldevsp = dev;
    return 0;
}

void pcap_freealldevs(pcap_if_t *alldevs) {
    pcap_if_t *cur = alldevs;
    while (cur) {
        pcap_if_t *next = cur->next;
        if (cur->name) free(cur->name);
        if (cur->description) free(cur->description);
        free(cur);
        cur = next;
    }
}

int pcap_compile(pcap_t *p, struct bpf_program *fp, const char *str, int optimize, uint32_t netmask) {
    (void)p; (void)fp; (void)str; (void)optimize; (void)netmask;
    return 0; // Success
}

int pcap_setfilter(pcap_t *p, struct bpf_program *fp) {
    (void)p; (void)fp;
    return 0; // Success
}

int pcap_loop(pcap_t *p, int cnt, pcap_handler callback, unsigned char *user) {
    // Simulate some packets
    if (cnt <= 0) cnt = 5; // Default for mock loop

    for (int i = 0; i < cnt; i++) {
        if (p->break_loop) break;

        struct pcap_pkthdr hdr;
        gettimeofday(&hdr.ts, NULL);
        hdr.caplen = 64;
        hdr.len = 64;

        unsigned char pkt[64];
        memset(pkt, 0, sizeof(pkt));
        // Mock Ethernet
        // Dst
        pkt[0] = 0xFF; pkt[1] = 0xFF; pkt[2] = 0xFF; pkt[3] = 0xFF; pkt[4] = 0xFF; pkt[5] = 0xFF;
        // Src
        pkt[6] = 0x00; pkt[7] = 0x11; pkt[8] = 0x22; pkt[9] = 0x33; pkt[10] = 0x44; pkt[11] = 0x55;
        // Type: IP (0x0800)
        pkt[12] = 0x08; pkt[13] = 0x00;

        // Mock IP
        pkt[14] = 0x45; // Ver/IHL
        pkt[23] = 6;    // Protocol: TCP
        // Src IP: 1.2.3.4 (0x01020304)
        pkt[26] = 1; pkt[27] = 2; pkt[28] = 3; pkt[29] = 4;
        // Dst IP: 5.6.7.8
        pkt[30] = 5; pkt[31] = 6; pkt[32] = 7; pkt[33] = 8;

        // Mock TCP
        // Header starts at 14 + 20 = 34
        // Src Port: 80 (0x0050)
        pkt[34] = 0x00; pkt[35] = 0x50;
        // Dst Port: 12345
        pkt[36] = 0x30; pkt[37] = 0x39;
        // Flags (offset 13 from tcp start) = 34 + 13 = 47
        pkt[47] = 0x02; // SYN

        callback(user, &hdr, pkt);
        usleep(10000); // 10ms delay
    }
    return 0;
}

void pcap_breakloop(pcap_t *p) {
    if (p) p->break_loop = 1;
}

void pcap_close(pcap_t *p) {
    if (p) free(p);
}

char *pcap_geterr(pcap_t *p) {
    (void)p;
    return "Mock Error";
}

int pcap_datalink(pcap_t *p) {
    (void)p;
    return DLT_EN10MB;
}
