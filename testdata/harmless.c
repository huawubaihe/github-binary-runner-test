#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *marker = getenv("RUNNER_TEST_MARKER");
    if (marker) {
        FILE *file = fopen(marker, "w");
        if (!file) return 1;
        fputs("ok", file);
        fclose(file);
    }
    return 0;
}
