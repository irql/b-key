#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "context.h"

unsigned char *memory_page_alloc(
    struct main_context *main_context,
    int page_count
) {
    if(page_count > 0) {
        unsigned char *region =
            mmap(NULL,
                page_count * main_context->system_page_size,
                PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_PRIVATE,
                -1,
                0);
        if(region == MAP_FAILED) {
            printf("%s\n", strerror(errno));
            return NULL;
        }
        else {
            memset(region, 0, page_count * main_context->system_page_size);
            return region;
        }
    }
    
    return NULL;
}

int memory_page_free(
    struct main_context *main_context,
    unsigned char *region,
    int page_count
) {
    if(page_count > 0) {
        return munmap(region, page_count * main_context->system_page_size);
    }
    else {
        return 0;
    }
}

unsigned char *memory_alloc(int amount) {
    if(amount > 0) {
        unsigned char *region = malloc(amount);
        memset(region, 0, amount);
        return region;
    }
    else {
        return 0;
    }
}

void memory_free(void *record) {
    free(record);
}

unsigned char *memory_realloc(void *region, int new_amount) {
    return realloc(region, new_amount);
}
