#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define MAX_SESSIONS 4
#define KTERM_VERSION_STRING "Test"

typedef struct KTerm_T KTerm;
typedef struct KTermSession_T KTermSession;

struct KTermSession_T { void* user_data; int rows; int cols; };
struct KTerm_T { KTermSession sessions[MAX_SESSIONS]; void (*response_callback)(KTerm* term, const char* data, int len); int width; int height; };

void KTerm_WriteCharToSession(KTerm* term, int session_idx, int c) { (void)term; (void)session_idx; (void)c; }
void KTerm_WriteString(KTerm* term, const char* s) { (void)term; (void)s; }
void KTerm_Resize(KTerm* term, int w, int h) { (void)term; (void)w; (void)h; }
void KTerm_SetOutputSink(KTerm* term, void* sink, void* user) { (void)term; (void)sink; (void)user; }

#define KTERM_NET_IMPLEMENTATION
#define KTERM_DISABLE_VOICE
#define KTERM_DISABLE_TELNET
#define KTERM_DISABLE_WIREDIAG

#include "../kt_net.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    // SSH
    const KTermProtocolDef* ssh = KTerm_Net_IdentifyProtocol(22, false);
    assert(ssh != NULL);
    assert(strcmp(ssh->short_name, "SSH") == 0);
    assert(ssh->supports_auth == true);
    printf("SSH OK.\n");

    // RTMP
    const KTermProtocolDef* rtmp = KTerm_Net_IdentifyProtocol(1935, false);
    assert(rtmp != NULL);
    assert(strcmp(rtmp->short_name, "RTMP") == 0);
    assert(strcmp(rtmp->category, "Streaming") == 0);
    assert(rtmp->supports_auth == true);
    assert(rtmp->plaintext_auth == true);
    printf("RTMP OK.\n");

    // NDI
    const KTermProtocolDef* ndi = KTerm_Net_IdentifyProtocol(5960, false);
    assert(ndi != NULL);
    assert(strcmp(ndi->short_name, "NDI") == 0);
    assert(strcmp(ndi->category, "Streaming") == 0);
    printf("NDI OK.\n");

    printf("All tests passed.\n");
    return 0;
}
