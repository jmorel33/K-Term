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

#ifdef __cplusplus
}
#endif

#endif // KT_VOIP_H

#ifdef KTERM_VOIP_IMPLEMENTATION
#ifndef KTERM_VOIP_IMPLEMENTATION_GUARD
#define KTERM_VOIP_IMPLEMENTATION_GUARD

#include <stdio.h>

void KTerm_VoIP_Register(const char* user, const char* pass, const char* domain) {
#ifdef KTERM_USE_PJSIP
    // PJSIP implementation would go here
#else
    fprintf(stderr, "[VoIP] Registering user=%s domain=%s\n", user ? user : "(null)", domain ? domain : "(null)");
#endif
}

void KTerm_VoIP_Dial(const char* target) {
#ifdef KTERM_USE_PJSIP
    // PJSIP implementation
#else
    fprintf(stderr, "[VoIP] Dialing target=%s\n", target ? target : "(null)");
#endif
}

void KTerm_VoIP_DTMF(const char* digits) {
#ifdef KTERM_USE_PJSIP
    // PJSIP implementation
#else
    fprintf(stderr, "[VoIP] Sending DTMF: %s\n", digits ? digits : "(null)");
#endif
}

void KTerm_VoIP_Hangup(void) {
#ifdef KTERM_USE_PJSIP
    // PJSIP implementation
#else
    fprintf(stderr, "[VoIP] Hanging up\n");
#endif
}

#endif // KTERM_VOIP_IMPLEMENTATION_GUARD
#endif // KTERM_VOIP_IMPLEMENTATION
