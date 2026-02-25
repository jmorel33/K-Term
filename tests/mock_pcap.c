#include "deps/pcap.h"
#include <stdlib.h>
#include <string.h>

pcap_t *pcap_open_live(const char *device, int snaplen, int promisc, int to_ms, char *errbuf) {
    return (pcap_t*)malloc(1);
}

int pcap_findalldevs(pcap_if_t **alldevsp, char *errbuf) {
    pcap_if_t* dev = (pcap_if_t*)calloc(1, sizeof(pcap_if_t));
    dev->name = strdup("eth0");
    *alldevsp = dev;
    return 0;
}

void pcap_freealldevs(pcap_if_t *alldevs) {
    if (alldevs) {
        if (alldevs->name) free(alldevs->name);
        free(alldevs);
    }
}

int pcap_compile(pcap_t *p, struct bpf_program *fp, const char *str, int optimize, uint32_t netmask) {
    return 0;
}

int pcap_setfilter(pcap_t *p, struct bpf_program *fp) {
    return 0;
}

int pcap_loop(pcap_t *p, int cnt, pcap_handler callback, unsigned char *user) {
    #ifdef _WIN32
    Sleep(100);
    #else
    usleep(100000);
    #endif
    return 0;
}

void pcap_breakloop(pcap_t *p) {
}

void pcap_close(pcap_t *p) {
    free(p);
}

char *pcap_geterr(pcap_t *p) {
    return "Mock Error";
}

int pcap_datalink(pcap_t *p) {
    return DLT_EN10MB;
}
