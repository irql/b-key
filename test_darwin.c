#include <stdio.h>

int main(void) {
#if defined(__MACOSX__) || defined(__APPLE__)
    puts("Running on mac");
#endif
}
