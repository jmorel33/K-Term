#include "kterm.h"
#include <stdio.h>

int main() {
    KTermConfig config = {0};
    config.width = 80;
    config.height = 24;
    printf("KTermConfig size: %zu\n", sizeof(config));
    // We can't access KTerm internals here, e.g. sizeof(KTerm) should fail or be incomplete
    // KTerm* t; // t->width should fail
    return 0;
}
