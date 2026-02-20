#ifndef KT_VOIP_H
#define KT_VOIP_H

#ifdef __cplusplus
extern "C" {
#endif

// VoIP API
void KTerm_VoIP_Register(const char* user, const char* pass, const char* domain);
void KTerm_VoIP_Dial(const char* target);
void KTerm_VoIP_DTMF(const char* digits);
void KTerm_VoIP_Hangup(void);

// Future API (Planned)
// void KTerm_VoIP_Init(void);
// void KTerm_VoIP_Cleanup(void);
// void KTerm_VoIP_Answer(void);

#ifdef __cplusplus
}
#endif

#endif // KT_VOIP_H

#ifdef KTERM_VOIP_IMPLEMENTATION
#ifndef KTERM_VOIP_IMPLEMENTATION_GUARD
#define KTERM_VOIP_IMPLEMENTATION_GUARD

#include <stdio.h>

#ifdef KTERM_USE_PJSIP
// Phase 1: Foundation & Lifecycle
// #include <pjsua-lib/pjsua.h>

// Context for PJSIP
/*
typedef struct {
    pj_pool_t* pool;
    pjsua_acc_id acc_id;
    pjsua_call_id call_id;
    // ... custom media port ...
} KTermVoIPContext;
static KTermVoIPContext g_voip_ctx = {0};
*/

#endif

void KTerm_VoIP_Register(const char* user, const char* pass, const char* domain) {
#ifdef KTERM_USE_PJSIP
    // Phase 2: Signaling Core
    // 1. Configure account config (pjsua_acc_config)
    // 2. Set ID (sip:user@domain) and creds
    // 3. Call pjsua_acc_add()
    fprintf(stderr, "[VoIP] PJSIP Register (TODO)\n");
#else
    fprintf(stderr, "[VoIP] Registering user=%s domain=%s\n", user ? user : "(null)", domain ? domain : "(null)");
#endif
}

void KTerm_VoIP_Dial(const char* target) {
#ifdef KTERM_USE_PJSIP
    // Phase 2: Signaling Core
    // 1. Validate target URI
    // 2. Call pjsua_call_make_call()
    // Phase 3: Media Bridge (Voice Reactor)
    // 3. Ensure audio is routed via Custom Media Port to kt_voice.h buffer
    fprintf(stderr, "[VoIP] PJSIP Dial (TODO)\n");
#else
    fprintf(stderr, "[VoIP] Dialing target=%s\n", target ? target : "(null)");
#endif
}

void KTerm_VoIP_DTMF(const char* digits) {
#ifdef KTERM_USE_PJSIP
    // Phase 4: Data & Control
    // 1. Call pjsua_call_dial_dtmf()
    fprintf(stderr, "[VoIP] PJSIP DTMF (TODO)\n");
#else
    fprintf(stderr, "[VoIP] Sending DTMF: %s\n", digits ? digits : "(null)");
#endif
}

void KTerm_VoIP_Hangup(void) {
#ifdef KTERM_USE_PJSIP
    // Phase 2: Signaling Core
    // 1. Call pjsua_call_hangup_all()
    fprintf(stderr, "[VoIP] PJSIP Hangup (TODO)\n");
#else
    fprintf(stderr, "[VoIP] Hanging up\n");
#endif
}

#endif // KTERM_VOIP_IMPLEMENTATION_GUARD
#endif // KTERM_VOIP_IMPLEMENTATION
