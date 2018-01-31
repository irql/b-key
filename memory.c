#define _GNU_SOURCE

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "os.h"
#include "debug.h"
#include "context.h"

#ifndef DEBUG_MEMORY
    #undef DEBUG_PRINT
    #define DEBUG_PRINT(...)
#endif

unsigned char *
memory_page_alloc(
    struct main_context *main_context,
    int page_count
) {
    DEBUG_PRINT("memory_page_alloc(page_count = %d);\n", page_count);
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
    void *offset,
    int old_page_count,
    int page_count
) {
    DEBUG_PRINT("memory_page_realloc(offset = %p, old_page_count = %d, new_page_count = %d);\n", offset, old_page_count, page_count);
    if(old_page_count > 0 && page_count > 0) {
#ifdef __MACOSX__
        // There is no "mremap()" in MAC OS, and thus it will never have performant page reallocation :(
        munmap(offset, main_context->system_page_size * old_page_count);
        unsigned char *region =
            mmap(NULL,
                    main_context->system_page_size * page_count,
                    PROT_READ | PROT_WRITE,
                    MAP_ANONYMOUS | MAP_PRIVATE,
                    -1,
                    0);
#else
        unsigned char *region =
            mremap(offset,
                    main_context->system_page_size * old_page_count,
                    main_context->system_page_size * page_count,
                    MREMAP_MAYMOVE
                  );
#endif
        if(region == MAP_FAILED) {
            DEBUG_PRINT("memory_page_realloc() failed: %s\n", strerror(errno));
            return 0;
        }
        else {
            return region;
        }
    }

    DEBUG_PRINT("memory_page_realloc(): Both old and new page count must be >0\n");

    return 0;
}

int memory_page_free(
    struct main_context *main_context,
    unsigned char *region,
    int page_count
) {
    DEBUG_PRINT("memory_page_free(region = %p, page_count = %d);\n", region, page_count);
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
    DEBUG_PRINT("memory_alloc(%d);\n", amount);
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
    DEBUG_PRINT("memory_free(record = %p);\n", record);
    free(record);
}

unsigned char *
memory_realloc(
    void *region,
    int old_amount,
    int new_amount
) {
    DEBUG_PRINT("memory_realloc(old_region = %p, old_amount = %d, new_amount = %d);\n",
            region, old_amount, new_amount);
    void *new_region = realloc(region, new_amount);
    if(new_region && new_amount > old_amount) {
        memset(new_region + old_amount, 0, new_amount - old_amount);
    }
    return new_region;
}
