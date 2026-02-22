#ifndef PCAP_H_MOCK
#define PCAP_H_MOCK

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PCAP_ERRBUF_SIZE 256

typedef struct pcap pcap_t;
typedef struct pcap_dumper pcap_dumper_t;
typedef struct pcap_if pcap_if_t;
typedef struct pcap_addr pcap_addr_t;

struct pcap_pkthdr {
    struct timeval ts;
    uint32_t caplen;
    uint32_t len;
};

struct pcap_addr {
    struct pcap_addr *next;
    struct sockaddr *addr;
    struct sockaddr *netmask;
    struct sockaddr *broadaddr;
    struct sockaddr *dstaddr;
};

struct pcap_if {
    struct pcap_if *next;
    char *name;
    char *description;
    struct pcap_addr *addresses;
    uint32_t flags;
};

struct bpf_program {
    unsigned int bf_len;
    struct bpf_insn *bf_insns;
};

typedef void (*pcap_handler)(unsigned char *user, const struct pcap_pkthdr *pkthdr, const unsigned char *bytes);

pcap_t *pcap_open_live(const char *device, int snaplen, int promisc, int to_ms, char *errbuf);
int pcap_findalldevs(pcap_if_t **alldevsp, char *errbuf);
void pcap_freealldevs(pcap_if_t *alldevs);
int pcap_compile(pcap_t *p, struct bpf_program *fp, const char *str, int optimize, uint32_t netmask);
int pcap_setfilter(pcap_t *p, struct bpf_program *fp);
int pcap_loop(pcap_t *p, int cnt, pcap_handler callback, unsigned char *user);
void pcap_breakloop(pcap_t *p);
void pcap_close(pcap_t *p);
char *pcap_geterr(pcap_t *p);
int pcap_datalink(pcap_t *p);

#define DLT_EN10MB 1

#ifdef __cplusplus
}
#endif

#endif
