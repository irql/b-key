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

#ifndef DEBUG_TESTS
    #undef DEBUG_PRINT
    #define DEBUG_PRINT(...)
#endif

#define TEST_MAX_BUCKET 23

enum {
    TEST_FAILED,
    TEST_SUCCESS
};

typedef struct test_context {
    int count;
    int status;
    char *reason;
    struct ptbl_record ptbl_rec;
    struct kv_record kv_rec;
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

#define ASSERT(x,y) \
    test_start(ctx, y); \
    if(! (x) ) { \
        ctx->reason = "Failed"; \
        ctx->status = TEST_FAILED; \
    } \
    test_stop(ctx);

    /* Test our macros */

    // PTBL_*
    PTBL_RECORD_SET_PAGE_COUNT(ctx->ptbl_rec, 0xffffffff);
    ASSERT(0x1fffffff == ctx->ptbl_rec.key_high_and_page_count, "PTBL_RECORD_SET_PAGE_COUNT()");

    ctx->ptbl_rec.key_high_and_page_count = 0xffffffff;
    ASSERT(0x1fffffff == PTBL_RECORD_GET_PAGE_COUNT(ctx->ptbl_rec), "PTBL_RECORD_GET_PAGE_COUNT()");

    ASSERT(0x38 == PTBL_RECORD_GET_KEY(ctx->ptbl_rec), "PTBL_RECORD_GET_KEY()");

    ctx->ptbl_rec.key_high_and_page_count = 0;

    PTBL_RECORD_SET_OFFSET(ctx->ptbl_rec, 0xffffffff);
    ASSERT(0x1fffffff == ctx->ptbl_rec.key_low_and_offset, "PTBL_RECORD_SET_OFFSET()");

    ctx->ptbl_rec.key_low_and_offset = 0xffffffff;
    ASSERT(0x1fffffff == PTBL_RECORD_GET_OFFSET(ctx->ptbl_rec), "PTBL_RECORD_GET_OFFSET()");

    // Because only the lower 3 bits should be flipped
    ASSERT(0x07 == PTBL_RECORD_GET_KEY(ctx->ptbl_rec), "PTBL_RECORD_GET_KEY()");

    ctx->ptbl_rec.key_high_and_page_count = 0;
    ctx->ptbl_rec.key_low_and_offset = 0;
    PTBL_RECORD_SET_KEY(ctx->ptbl_rec, -1);
    ASSERT(
            0xE0000000 == ctx->ptbl_rec.key_high_and_page_count &&
            0xE0000000 == ctx->ptbl_rec.key_low_and_offset,
            "PTBL_RECORD_SET_KEY()");

    // KV_*
    // flags_and_size
    KV_RECORD_SET_FLAGS(ctx->kv_rec, 0xffff);
    ASSERT(0xFF00000000000000 == ctx->kv_rec.flags_and_size, "KV_RECORD_SET_FLAGS()");

    ASSERT(0xFF == KV_RECORD_GET_FLAGS(ctx->kv_rec), "KV_RECORD_GET_FLAGS()");

    ctx->kv_rec.flags_and_size = 0;
    KV_RECORD_SET_SIZE(ctx->kv_rec, 0xffffffffffffffff);
    ASSERT(0x00FFFFFFFFFFFFFF == ctx->kv_rec.flags_and_size, "KV_RECORD_SET_SIZE()");

    ctx->kv_rec.flags_and_size <<= 8;
    ASSERT(0x00FFFFFFFFFFFF00 == KV_RECORD_GET_SIZE(ctx->kv_rec), "KV_RECORD_GET_SIZE()");

    // bucket_and_index
    KV_RECORD_SET_BUCKET(ctx->kv_rec, 0xff);
    DEBUG_PRINT("%lx\n", ctx->kv_rec.bucket_and_index);
    ASSERT(0xFC00000000000000 == ctx->kv_rec.bucket_and_index, "KV_RECORD_SET_BUCKET()");

    ASSERT(0x3F == KV_RECORD_GET_BUCKET(ctx->kv_rec), "KV_RECORD_GET_BUCKET()");

    ctx->kv_rec.bucket_and_index = 0;
    KV_RECORD_SET_INDEX(ctx->kv_rec, 0xffffffffffffffff);
    ASSERT(0x03FFFFFFFFFFFFFF == ctx->kv_rec.bucket_and_index, "KV_RECORD_SET_INDEX()");

    ctx->kv_rec.bucket_and_index <<= 8;
    ASSERT(0x03FFFFFFFFFFFF00 == KV_RECORD_GET_INDEX(ctx->kv_rec), "KV_RECORD_GET_INDEX()");

    /* Test system parameters */

    // TODO: Migrate to server initialization
    ASSERT(0x1000 == main_context->system_page_size, "Standard system page size");
    ASSERT(0x80 <= main_context->system_phys_page_count, "System physical memory >=512MB");

    int i = 0;
    /*for(; (1 << i) < main_context->system_phys_page_count; i++) {
        test_page_alloc(main_context, (1 << i));
    }

    * Don't do this unless you're masochistic

    DEBUG_PRINT("Attempting to allocate all but 65536 pyshical pages in the system\n");
    if(!test_memory_alloc(main_context, main_context->system_phys_page_count - 0x10000))
        return 0;

    */

    /* Testing database.c functionality */

    RECORD_CREATE(Record_database, database);

    RECORD_ALLOC(Record_ptbl, database->ptbl_record_tbl);
    database->ptbl_record_count = 1;

    PTBL_RECORD_SET_KEY(database->ptbl_record_tbl[0], 3);
    PTBL_RECORD_SET_PAGE_COUNT(database->ptbl_record_tbl[0], 1);

    Record_ptbl *ptbl = database_ptbl_get(main_context, database, 3);
    ASSERT(ptbl == database->ptbl_record_tbl, "database_ptbl_get() finds record");

    ASSERT(0 == database_ptbl_get(main_context, database, -1), "database_ptbl_get() doesn't find record");

    /* The following tests to be run on every bucket
     *
     * NOTE: DO NOT run on buckets > 25, depending on memory requirements.
     *
     * The tests will try to mmap() 20 pages for each bucket.
     * The page size is 4096 for buckets <=8.
     * The page size is 4096 * 2^(x - 8) for buckets (x) >8.
     *
     * e.g. bucket 25 will end up trying to allocate ((4096 * 2^(25 - 8)) * 20)
     * bytes in total (10.7 GB !). If your system only has 8GB of memory, the
     * max bucket you can test will probably be 24 (only 5.3 GB allocated).
     *
     * In addition, each bit in page_usage records whether or not a particular
     * value within a page has been used. In the case of buckets whose maximum
     * value length is >=4096 (buckets >=8), each bit represents one page,
     * regardless of how many multiples of the system page size that one page is.
     *
     *
     * Bucket | Max length of values
     * -------+---------------------
	 * 0      | 16B
	 * 1      | 32B
	 * 2      | 64B
	 * 3      | 128B
	 * 4      | 256B
	 * 5      | 512B
	 * 6      | 1KB
	 * 7      | 2KB
	 * 8      | 4KB
	 * 9      | 8KB
	 * 10     | 16KB
	 * 11     | 32KB
	 * 12     | 65KB
	 * 13     | 131KB
	 * 14     | 262KB
	 * 15     | 524KB
	 * 16     | 1MB
	 * 17     | 2MB
	 * 18     | 4MB
	 * 19     | 8MB
	 * 20     | 16MB
	 * 21     | 33MB
	 * 22     | 67MB
	 * 23     | 134MB
	 * 24     | 268MB
	 * 25     | 536MB
	 * 26     | 1GB
	 * 27     | 2GB
	 * 28     | 4GB
	 * 29     | 8GB
	 * 30     | 17GB
	 * 31     | 34GB
	 * 32     | 68GB
	 * 33     | 137GB
	 * 34     | 274GB
	 * 35     | 549GB
	 * 36     | 1TB
	 * 37     | 2TB
	 * 38     | 4TB
	 * 39     | 8TB
	 * 40     | 17TB
	 * 41     | 35TB
	 * 42     | 70TB
	 * 43     | 140TB
	 * 44     | 281TB
	 * 45     | 562TB
	 * 46     | 1PB
	 * 47     | 2PB
	 * 48     | 4PB
	 * 49     | 9PB
	 * 50     | 18PB
	 * 51     | 36PB
	 * 52     | 72PB
	 * 53     | 144PB
	 * 54     | 288PB
	 * 55     | 576PB
	 * 56     | 1EB
	 * 57     | 2EB
	 * 58     | 4EB
	 * 59     | 9EB
	 * 60     | 18EB
	 * 61     | 36EB
	 * 62     | 73EB
	 * 63     | 147EB
     */
    database_ptbl_free(main_context, database);
    for(i = 0; i <= TEST_MAX_BUCKET; i++) {
        Record_ptbl *ptbl_entry;
        // Alloc a new bucket
        unsigned char *page_base = database_ptbl_alloc(main_context, database, &ptbl_entry, 10, i);
        ASSERT(0 != page_base, "Allocate a new bucket");

        ASSERT(ptbl_entry, "ptbl_entry not null");
        ASSERT(PTBL_RECORD_GET_KEY(ptbl_entry[0]) == i, "Correct ptbl_entry");

        ASSERT(database->ptbl_record_count == i + 1, "Correct ptbl_record_count");

        unsigned int count, count2;

        ASSERT((count = PTBL_RECORD_GET_PAGE_COUNT(database->ptbl_record_tbl[i])) == 10, "Correct page_count");

        ASSERT((count = database->ptbl_record_tbl[i].page_usage_length) == (count2 = PTBL_CALC_PAGE_USAGE_LENGTH(i, 10)), "Correct page_usage_length");

        // Alloc a page in the same bucket (should be same result as first time because bucket will be empty)
        unsigned char *new_page_base = database_ptbl_alloc(main_context, database, 0, 1, i);
        ASSERT(0 != new_page_base, "Allocate new page in empty space");

        ASSERT(new_page_base == page_base, "New page base == old page base");

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
            unsigned int expected_new_page_usage_length = (j > 5) ? PTBL_CALC_PAGE_USAGE_LENGTH(i, expected_new_page_count) : old_page_usage_length;

            // Because we expect new_page_base to change entirely when it needs to remap the
            // pages because of MREMAP_MAYMOVE, we ignore this check (using -1) if j > 5
            unsigned char *expected_new_page_base = (j > 5) ? (unsigned char *)-1 : old_page_base + main_context->system_page_size * ((i <= 8) ? 1 : (1 << (i - 8)));
            new_page_base = database_ptbl_alloc(main_context, database, 0, j, i);

            ASSERT(new_page_base, "Correct new_page_base");
            ASSERT(expected_new_page_base == (unsigned char *)-1 || new_page_base == expected_new_page_base, "Correct new_page_base");

            unsigned int new_page_count = PTBL_RECORD_GET_PAGE_COUNT(database->ptbl_record_tbl[i]);
            unsigned int new_page_usage_length = database->ptbl_record_tbl[i].page_usage_length;

            ASSERT(new_page_usage_length == expected_new_page_usage_length, "Correct new_page_usage_length");
            ASSERT(new_page_count == expected_new_page_count, "Correct new_page_usage_length");
        }
    }

    database_ptbl_free(main_context, database);

    unsigned int buffer_length = 16 << TEST_MAX_BUCKET;
    unsigned int pages_count = ((buffer_length > main_context->system_page_size) ? buffer_length / main_context->system_page_size : 1);
    unsigned char *buffer = memory_page_alloc(main_context, pages_count);
    ASSERT(buffer != 0, "allocate pages");

    int fd = open("/dev/zero", O_RDONLY);
    ASSERT(fd != -1, "Opening /dev/zero");

    ssize_t count = 0;
    ASSERT((count = read(fd, buffer, buffer_length)) == buffer_length, "Read random data into buffer");
    close(fd);

    for(i = 0; i < TEST_MAX_BUCKET; i++) {
        unsigned long length = 16 << i;
        unsigned int bucket = database_calc_bucket(length);
        unsigned long max_j = // Test as many allocs as we can, but don't go over 65MB
            (0x10 * main_context->system_page_size) / length;

        // Test that database_calc_bucket() calculates the correct bucket number
        // for varying buffer (value) lengths
        ASSERT(bucket == i, "database_calc_bucket()");

#define TEST_ALLOC_KV(x) \
        for(int j = 0; j < max_j; j++) { \
            unsigned long k = database_kv_alloc(main_context, database, 1, length, buffer); \
            ASSERT(-1 != k, "database_kv_alloc() succeeds"); \
            Record_kv *rec_kv = database_kv_get(main_context, database, k); \
            ASSERT(0 != rec_kv, "database_kv_get() succeeds"); \
            ASSERT(KV_RECORD_GET_SIZE(rec_kv[0]) == length, "Record size equals what was alloc'd"); \
            ASSERT(KV_RECORD_GET_BUCKET(rec_kv[0]) == database_calc_bucket(length), "Record bucket correct");\
            ASSERT(KV_RECORD_GET_FLAGS(rec_kv[0]) == 1, "Record flags correct"); \
            unsigned char *found_buffer = database_kv_get_value(main_context, database, k); \
            ASSERT(0 != found_buffer, "database_kv_get_value() succeeds"); \
            ASSERT(0 == memcmp(found_buffer, buffer, length), "found_buffer equals buffer"); \
            if(x > 0) { \
                ASSERT(((j + (x - 1)) / x) == k, "database_kv_alloc() returns correct k"); \
                if(j % x) { \
                    ASSERT(database_kv_free(main_context, database, k), "database_kv_free()"); \
                } \
            } \
            else { \
                ASSERT(j == k, "database_kv_alloc() Returns correct k"); \
            } \
        }\
        for(int k = database->kv_record_count - 1; k >= 0; k--) { \
            ASSERT(database_kv_free(main_context, database, k), "database_kv_free()"); \
        }

        /* Phase 1: Contiguous allocation and contiguous free */

        TEST_ALLOC_KV(0);

        /* Phase 2: Every l'th KV free'd */

        for(int l = 2; l < 10; l++) {
            TEST_ALLOC_KV(l);

            //TODO: Create test to verify ptbl_entry->page_usage
        }
    }

    memory_page_free(main_context, buffer, pages_count);

    database_ptbl_free(main_context, database);
    memory_free(database);
    memory_free(ctx);

    return 1;
}
