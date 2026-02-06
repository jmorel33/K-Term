#define KTERM_IMPLEMENTATION
#define KTERM_TESTING
#include "../kterm.h"
#include <stdio.h>
#include "mock_situation.h"

int main() {
    printf("KTerm Version: %d.%d.%d-%s\n",
        KTERM_VERSION_MAJOR,
        KTERM_VERSION_MINOR,
        KTERM_VERSION_PATCH,
        KTERM_VERSION_REVISION);
#ifdef KTERM_VERSION_STRING
    printf("KTerm Version String: %s\n", KTERM_VERSION_STRING);
#endif

    if (KTERM_VERSION_MAJOR == 2 && KTERM_VERSION_MINOR == 4 && KTERM_VERSION_PATCH == 25) {
        return 0;
    }
    return 1;
}
