#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#define KTERM_ENABLE_GATEWAY
#include "../kterm.h"
#include "test_utilities.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static bool gateway_callback_called = false;
static char gateway_cmd[64] = {0};

void my_gateway_callback(KTerm* term, const char* class_id, const char* id, const char* command, const char* params) {
    (void)term; (void)class_id; (void)id; (void)params;
    gateway_callback_called = true;
    strncpy(gateway_cmd, command, sizeof(gateway_cmd) - 1);
}

int test_gateway_case_sensitivity(KTerm* term, KTermSession* session) {
    int errors = 0;
    // Register callback
    KTerm_SetGatewayCallback(term, my_gateway_callback);

    printf("Testing Uppercase 'PING'...\n");
    gateway_callback_called = false;
    KTerm_GatewayProcess(term, session, "KTERM", "0", "PING", "host");
    if (gateway_callback_called) {
        printf("FAIL: Uppercase 'PING' fell through to callback\n");
        errors++;
    } else {
        printf("PASS: Uppercase 'PING' handled internally\n");
    }

    printf("Testing Lowercase 'ping'...\n");
    gateway_callback_called = false;
    KTerm_GatewayProcess(term, session, "KTERM", "0", "ping", "host");
    if (gateway_callback_called) {
        printf("FAIL (Expected if broken): Lowercase 'ping' fell through to callback\n");
        errors++;
    } else {
        printf("PASS: Lowercase 'ping' handled internally\n");
    }

    printf("Testing Mixed Case 'Ping'...\n");
    gateway_callback_called = false;
    KTerm_GatewayProcess(term, session, "KTERM", "0", "Ping", "host");
    if (gateway_callback_called) {
        printf("FAIL (Expected if broken): Mixed case 'Ping' fell through to callback\n");
        errors++;
    } else {
        printf("PASS: Mixed case 'Ping' handled internally\n");
    }

    printf("Testing 'HELP' command...\n");
    gateway_callback_called = false;
    // Clear response buffer first (not easy via API, but we can consume)
    // Actually we just check if handled internally
    KTerm_GatewayProcess(term, session, "KTERM", "0", "help", "");
    if (gateway_callback_called) {
        printf("FAIL: 'help' fell through to callback\n");
        errors++;
    } else {
        printf("PASS: 'help' handled internally\n");
    }
    return errors;
}

int main() {
    KTerm* term = create_test_term(80, 25);
    if (!term) return 1;
    KTermSession* session = GET_SESSION(term);
    int ret = test_gateway_case_sensitivity(term, session);
    destroy_test_term(term);

    if (ret > 0) {
        printf("\nTests FAILED with %d errors.\n", ret);
        return 1;
    }
    printf("\nAll Tests PASSED.\n");
    return 0;
}
