#define _GNU_SOURCE

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "context.h"

unsigned char *
memory_page_alloc(
    struct main_context *main_context,
    int page_count
) {
    fprintf(stderr, "memory_page_alloc(..., %d);\n", page_count);
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

unsigned char *
memory_page_realloc(
    struct main_context *main_context,
    unsigned char *offset,
    int old_page_count,
    int page_count
) {
    if(page_count > 0 && page_count > 0) {
        unsigned char *region =
            mremap(offset,
                    main_context->system_page_size * old_page_count,
                    main_context->system_page_size * page_count,
                    MREMAP_MAYMOVE
                  );
        if(region == MAP_FAILED) {
            fprintf(stderr, "memory_page_realloc(): %s\n", strerror(errno));
            return 0;
        }
        else {
            return region;
        }
    }

    fprintf(stderr, "memory_page_realloc(): Both old and new page count must be >0\n");

    return 0;
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

unsigned char *
memory_alloc(
    int amount
) {
    if(amount > 0) {
        unsigned char *region = malloc(amount);
        memset(region, 0, amount);
        return region;
    }
    else {
        return 0;
    }
}

void
memory_free(
    void *record
) {
    free(record);
}

unsigned char *
memory_realloc(
    void *region,
    int old_amount,
    int new_amount
) {
    void *new_region = realloc(region, new_amount);
    if(new_region && new_amount > old_amount) {
        memset(region + old_amount, 0, new_amount - old_amount);
    }
    return new_region;
}
