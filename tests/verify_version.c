#include <stdio.h>
#include "kterm.h"

int main() {
    printf("KTerm Version: %d.%d.%d-%s\n",
        KTERM_VERSION_MAJOR,
        KTERM_VERSION_MINOR,
        KTERM_VERSION_PATCH,
        KTERM_VERSION_REVISION);

    if (KTERM_VERSION_PATCH == 44) {
        return 0;
    }
    return 1;
}
