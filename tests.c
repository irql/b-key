#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "context.h"
#include "records.h"
#include "memory.h"
#include "database.h"
#include "debug.h"

#undef DEBUG_PRINT
#define DEBUG_PRINT(...)

enum {
    TEST_FAILED,
    TEST_SUCCESS
};

typedef struct test_context {
    int count;
    int status;
    char *reason;
    struct ptbl_record ptbl_rec;
} Test_context;

int test_page_alloc(struct main_context * main_context, int pages) {
    DEBUG_PRINT("Mapping %d pages (%.2fGB)...", pages, (((float)pages * main_context->system_page_size) / 1000000000));
    unsigned char * region = memory_page_alloc(main_context, pages);
    if(region) {
        DEBUG_PRINT("free(%p)'d %s\n", region, memory_page_free(main_context, region, pages) == -1 ? strerror(errno) : "OK");
        return 1;
    }
    else {
        DEBUG_PRINT("Failed to allocate\n");
        return 0;
    }
}

void test_start(Test_context *ctx, char *desc) {
    DEBUG_PRINT("%d. %s... ", ++ctx->count, desc);
    ctx->status = TEST_SUCCESS;
}

void test_stop(Test_context *ctx) {
    if(ctx->status == TEST_FAILED) {
        // Don't ignore a test failure
        fprintf(stderr, "FAIL: %s\n", ctx->reason);
        exit(1);
    }
    else if(ctx->status == TEST_SUCCESS) {
        DEBUG_PRINT("OK\n");
    }
}

int run_tests(struct main_context * main_context) {
    RECORD_CREATE(Test_context, ctx);

    /* Test macros */

    test_start(ctx, "PTBL_RECORD_SET_PAGE_COUNT()");
    PTBL_RECORD_SET_PAGE_COUNT(ctx->ptbl_rec, 0xffffffff);
    if(0x1fffffff != ctx->ptbl_rec.key_high_and_page_count) {
        ctx->reason = "Page count set past boundary";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    test_start(ctx, "PTBL_RECORD_GET_PAGE_COUNT()");
    ctx->ptbl_rec.key_high_and_page_count = 0xffffffff;
    if(0x1fffffff != PTBL_RECORD_GET_PAGE_COUNT(ctx->ptbl_rec)) {
        ctx->reason = "Page count get past boundary";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    test_start(ctx, "PTBL_RECORD_GET_KEY()");
    if(0x38 != PTBL_RECORD_GET_KEY(ctx->ptbl_rec)) {
        ctx->reason = "Key get past boundary";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    test_start(ctx, "PTBL_RECORD_SET_OFFSET()");
    PTBL_RECORD_SET_OFFSET(ctx->ptbl_rec, 0xffffffff);
    if(0x1fffffff != ctx->ptbl_rec.key_low_and_offset) {
        ctx->reason = "Offset get past boundary";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    test_start(ctx, "PTBL_RECORD_GET_OFFSET()");
    ctx->ptbl_rec.key_low_and_offset = 0xffffffff;
    if(0x1fffffff != PTBL_RECORD_GET_OFFSET(ctx->ptbl_rec)) {
        ctx->reason = "Page offset get past boundary";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    test_start(ctx, "PTBL_RECORD_GET_KEY()");
    if(0x3f != PTBL_RECORD_GET_KEY(ctx->ptbl_rec)) {
        ctx->reason = "Key get past boundary";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    ctx->ptbl_rec.key_high_and_page_count = 0;
    ctx->ptbl_rec.key_low_and_offset = 0;
    PTBL_RECORD_SET_KEY(ctx->ptbl_rec, -1);
    test_start(ctx, "PTBL_RECORD_SET_KEY()");
    if(0xE0000000 != ctx->ptbl_rec.key_high_and_page_count || 0xE0000000 != ctx->ptbl_rec.key_low_and_offset) {
        ctx->reason = "Key set past boundary";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    struct kv_record kv_record;
    memset(&kv_record, 0, sizeof(kv_record));

    KV_RECORD_SET_FLAGS(kv_record, 0xffff);
    test_start(ctx, "KV_RECORD_SET_FLAGS()");
    if(0xFF00000000000000 != kv_record.flags_and_size) {
        ctx->reason = "Flags set past boundary";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    test_start(ctx, "KV_RECORD_GET_FLAGS()");
    if(0xFF != KV_RECORD_GET_FLAGS(kv_record)) {
        ctx->reason = "Flags get past boundary";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    kv_record.flags_and_size = 0;
    KV_RECORD_SET_SIZE(kv_record, 0xffffffffffffffff);
    test_start(ctx, "KV_RECORD_SET_SIZE()");
    if(0x00FFFFFFFFFFFFFF != kv_record.flags_and_size) {
        ctx->reason = "Size set past boundary";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    test_start(ctx, "KV_RECORD_GET_SIZE()");
    kv_record.flags_and_size <<= 8;
    if(0x00FFFFFFFFFFFF00 != KV_RECORD_GET_SIZE(kv_record)) {
        ctx->reason = "Size get past boundary";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    /* Test system parameters */
    // TODO: Migrate to server initialization
    test_start(ctx, "Standard system page size");
    if(0x1000 != main_context->system_page_size) {
        ctx->reason = "System page size non-standard";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    test_start(ctx, "System physical memory >=512MB");
    if(0x80 > main_context->system_phys_page_count) {
        ctx->reason = "System phys memory < 512MB";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    int i = 0;
    /*for(; (1 << i) < main_context->system_phys_page_count; i++) {
        test_page_alloc(main_context, (1 << i));
    }

    * Don't do this unless you're masochistic

    DEBUG_PRINT("Attempting to allocate all but 65536 pyshical pages in the system\n");
    if(!test_memory_alloc(main_context, main_context->system_phys_page_count - 0x10000))
        return 0;

    */

    RECORD_CREATE(Record_database, database);

    /* Testing database.c functionality */

    RECORD_ALLOC(Record_ptbl, database->ptbl_record_tbl);
    database->ptbl_record_count = 1;

    PTBL_RECORD_SET_KEY(database->ptbl_record_tbl[0], 3);
    PTBL_RECORD_SET_PAGE_COUNT(database->ptbl_record_tbl[0], 1);

    Record_ptbl *ptbl = database_ptbl_search(main_context, database, 3);
    test_start(ctx, "database_ptbl_search() finds record");
    if(ptbl != database->ptbl_record_tbl) {
        ctx->reason = "Failed to find ptbl entry";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    test_start(ctx, "database_ptbl_search() doesn't find record");
    if(0 != database_ptbl_search(main_context, database, -1)) {
        ctx->reason = "Ptbl lookup should have failed";
        ctx->status = TEST_FAILED;
    }
    test_stop(ctx);

    // The following tests to be run on buckets 0 through 8
    database_pages_free(main_context, database);
    for(i = 0; i <= 8; i++) {
        Record_ptbl *ptbl_entry;
        // Alloc a new bucket
        test_start(ctx, "Allocate a new bucket");
        unsigned char *page_base = database_pages_alloc(main_context, database, &ptbl_entry, 10, i);
        if(!page_base) {
            ctx->reason = "Failed to allocate 10 pages for bucket";
            ctx->status = TEST_FAILED;
        }
        test_stop(ctx);

        test_start(ctx, "Correct ptbl_entry");
        if(!ptbl_entry || PTBL_RECORD_GET_KEY(ptbl_entry[0]) != i) {
            ctx->reason = "Incorrect entry";
            ctx->status = TEST_FAILED;
            DEBUG_PRINT("(%p && %d == %d)\n", ptbl_entry, PTBL_RECORD_GET_KEY(ptbl_entry[0]), i);
        }
        test_stop(ctx);

        test_start(ctx, "Correct ptbl_record_count");
        if(database->ptbl_record_count != i + 1) {
            ctx->reason = "Incorrect ptbl record count";
            ctx->status = TEST_FAILED;
            DEBUG_PRINT("(%d != %d + 1)\n", database->ptbl_record_count, i);
        }
        test_stop(ctx);

        unsigned int count, count2;

        test_start(ctx, "Correct page_count");
        if((count = PTBL_RECORD_GET_PAGE_COUNT(database->ptbl_record_tbl[i])) != 10) {
            ctx->reason = "Page count incorrect";
            ctx->status = TEST_FAILED;
        }
        test_stop(ctx);

        test_start(ctx, "Correct page_usage_length");
        if((count = database->ptbl_record_tbl[i].page_usage_length) != (count2 = database_ptbl_calc_page_usage_length(i, 10))) {
            ctx->reason = "Bucket %d failed to alloc correct amount for page_usage (%d != %d)";
            ctx->status = TEST_FAILED;
        }
        test_stop(ctx);

        // Alloc a page in the same bucket (should be same result as first time because bucket will be empty)
        test_start(ctx, "Allocate new page in empty space");
        unsigned char *new_page_base = database_pages_alloc(main_context, database, 0, 1, i);
        if(!new_page_base) {
            ctx->reason = "Bucket %d failed realloc: %p => %p";
            ctx->status = TEST_FAILED;
        }
        test_stop(ctx);

        test_start(ctx, "New page base == old page base");
        if(new_page_base != page_base) {
            ctx->reason = "mmap()d new page when it shouldn't have";
            ctx->status = TEST_FAILED;
        }
        test_stop(ctx);

        // Test that we can allocate j free pages in a bucket correctly when page (j - 1) is in use
        for(int j = 1; j <= 10; j++) {
            int bits = (i < 8) ? (256 >> i) : 1;

            unsigned int old_page_count = PTBL_RECORD_GET_PAGE_COUNT(database->ptbl_record_tbl[i]);
            unsigned int old_page_usage_length = database->ptbl_record_tbl[i].page_usage_length;
            unsigned char *old_page_base = new_page_base;

            int k;
            if(i <= 5) {
                for(k = 0; k < old_page_count; k++)
                    database->ptbl_record_tbl[i].page_usage[(32 >> i) * k] = 0;
                database->ptbl_record_tbl[i].page_usage[(32 >> i) * (j - 1)] = 1;
            }
            else {
                int slice = 8 / bits;
                for(k = 0; k < old_page_count; k++) {
                    database->ptbl_record_tbl[i].page_usage[k / slice] = 0;
                }
                database->ptbl_record_tbl[i].page_usage[(j - 1) / slice] |= (1 << (((j - 1) % slice) * bits));
            }

            // Since we allocated 10 pages in the beginning, it makes sense for new allocations
            // of a length < 5 to not need to expand the page table persay, because they will
            // be able to fit into the free space between pages.
            unsigned int expected_new_page_count = (j > 5) ? 2 + old_page_count : old_page_count;
            unsigned int expected_new_page_usage_length = (j > 5) ? database_ptbl_calc_page_usage_length(i, expected_new_page_count) : old_page_usage_length;

            // Because we expect new_page_base to change entirely when it needs to remap the
            // pages because of MREMAP_MAYMOVE, we ignore this check (using -1) if j > 5
            unsigned char *expected_new_page_base = (j > 5) ? (unsigned char *)-1 : old_page_base + main_context->system_page_size;

            new_page_base = database_pages_alloc(main_context, database, 0, j, i);

            unsigned int new_page_count = PTBL_RECORD_GET_PAGE_COUNT(database->ptbl_record_tbl[i]);
            unsigned int new_page_usage_length = database->ptbl_record_tbl[i].page_usage_length;

            test_start(ctx, "Correct new_page_base");
            if(!new_page_base) {
                ctx->reason = "New page base is null";
                ctx->status = TEST_FAILED;
            }
            if(expected_new_page_base != (unsigned char *)-1 && new_page_base != expected_new_page_base) {
                ctx->reason = "New page base is incorrect";
                ctx->status = TEST_FAILED;
            }
            test_stop(ctx);

            test_start(ctx, "Correct new_page_usage_length");
            if(new_page_usage_length != expected_new_page_usage_length) {
                ctx->reason = "New page usage length %d != %d";
                ctx->status = TEST_FAILED;
            }
            if(new_page_count != expected_new_page_count) {
                ctx->reason = "New page count %d != %d";
                ctx->status = TEST_FAILED;
            }
            test_stop(ctx);
        }
    }

    //database_pages_free(main_context, database);

    // Test that database_calc_bucket() calculates the correct bucket number
    // for varying buffer (value) lengths

#define ASSERT(x,y) \
    test_start(ctx, y); \
    if(! (x) ) { \
        ctx->reason = "Failed"; \
        ctx->status = TEST_FAILED; \
    } \
    test_stop(ctx);

    // 4 pages = (16 << 10) bytes
    unsigned char *buffer = memory_page_alloc(main_context, 4);
    ASSERT(buffer != 0, "allocate 4 pages");

    int fd = open("/dev/urandom", O_RDONLY);
    ASSERT(fd != -1, "Opening /dev/urandom");

    unsigned int buffer_length = 4 * main_context->system_page_size;
    ASSERT(read(fd, buffer, buffer_length) == buffer_length, "Read random data into buffer");
    close(fd);

    for(i = 0; i < 10; i++) {
        unsigned long length = 16 << i;
        unsigned int bucket = database_calc_bucket(length);

        test_start(ctx, "database_calc_bucket()");
        if(bucket != i) {
            ctx->reason = "Wrong bucket";
            ctx->status = TEST_FAILED;
        }
        test_stop(ctx);

        database_alloc_kv(main_context, database, bucket, length, buffer);
    }
    memory_page_free(main_context, buffer, 4);

    database->ptbl_record_tbl[0].page_usage[0] = 3;
    database_alloc_kv(main_context, database, 1, 12, "this is a test");

    for(i = 0; i < 32; i++)
    database->ptbl_record_tbl[0].page_usage[i] = 0xff;

    database_pages_free(main_context, database);
    memory_free(database);
    memory_free(ctx);

    return 1;
}
