#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main() {
    printf("Checking version...\n");
    printf("KTERM_VERSION_STRING: %s\n", KTERM_VERSION_STRING);

    if (strstr(KTERM_VERSION_STRING, "2.4.19") != NULL && strstr(KTERM_VERSION_STRING, "Unified Event Pipeline") != NULL) {
        printf("PASS: Version is 2.4.19 (Unified Event Pipeline)\n");
        return 0;
    } else {
        printf("FAIL: Version mismatch\n");
        return 1;
    }
}
