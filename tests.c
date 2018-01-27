#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "context.h"
#include "records.h"
#include "memory.h"
#include "database.h"

int test_page_alloc(struct main_context * main_context, int pages) {
    fprintf(stderr, "Mapping %d pages (%.2fGB)...", pages, (((float)pages * main_context->system_page_size) / 1000000000));
    unsigned char * region = memory_page_alloc(main_context, pages);
    if(region) {
        fprintf(stderr, "free(%p)'d %s\n", region, memory_page_free(main_context, region, pages) == -1 ? strerror(errno) : "OK");
        return 1;
    }
    else {
        fprintf(stderr, "Failed to allocate\n");
        return 0;
    }
}

int run_tests(struct main_context * main_context) {
    struct ptbl_record rec;
    memset(&rec, 0, sizeof(rec));

    PTBL_RECORD_SET_PAGE_COUNT(rec, 0xffffffff);
    if(0x1fffffff != rec.key_high_and_page_count) {
        fprintf(stderr, "Page count set past boundary\n");
        return 0;
    }

    rec.key_high_and_page_count = 0xffffffff;
    if(0x1fffffff != PTBL_RECORD_GET_PAGE_COUNT(rec)) {
        fprintf(stderr, "Page count get past boundary\n");
        return 0;
    }

    if(0x38 != PTBL_RECORD_GET_KEY(rec)) {
        fprintf(stderr, "Key get past boundary\n");
        return 0;
    }

    PTBL_RECORD_SET_OFFSET(rec, 0xffffffff);
    if(0x1fffffff != rec.key_low_and_offset) {
        fprintf(stderr, "Offset get past boundary\n");
        return 0;
    }

    rec.key_low_and_offset = 0xffffffff;
    if(0x1fffffff != PTBL_RECORD_GET_OFFSET(rec)) {
        fprintf(stderr, "Offset get past boundary\n");
        return 0;
    }

    if(0x3f != PTBL_RECORD_GET_KEY(rec)) {
        fprintf(stderr, "Key get past boundary\n");
        return 0;
    }

    rec.key_high_and_page_count = 0;
    rec.key_low_and_offset = 0;
    PTBL_RECORD_SET_KEY(rec, -1);
    if(0xE0000000 != rec.key_high_and_page_count || 0xE0000000 != rec.key_low_and_offset) {
        fprintf(stderr, "Key set past boundary\n");
        return 0;
    }

    struct kv_record kv_record;
    memset(&kv_record, 0, sizeof(kv_record));

    KV_RECORD_SET_FLAGS(kv_record, 0xffff);
    if(0xFF00000000000000 != kv_record.flags_and_size) {
        fprintf(stderr, "Flags set past boundary: %lx\n", kv_record.flags_and_size);
        return 0;
    }

    if(0xFF != KV_RECORD_GET_FLAGS(kv_record)) {
        fprintf(stderr, "Flags get past boundary\n");
        return 0;
    }

    kv_record.flags_and_size = 0;
    KV_RECORD_SET_SIZE(kv_record, 0xffffffffffffffff);

    if(0x00FFFFFFFFFFFFFF != kv_record.flags_and_size) {
        fprintf(stderr, "Size set past boundary\n");
        return 0;
    }

    kv_record.flags_and_size <<= 8;
    if(0x00FFFFFFFFFFFF00 != KV_RECORD_GET_SIZE(kv_record)) {
        fprintf(stderr, "Size get past boundary\n");
        return 0;
    }

    if(0x1000 != main_context->system_page_size) {
        fprintf(stderr, "System page size non-standard\n");
        return 0;
    }

    if(0x80 > main_context->system_phys_page_count) {
        fprintf(stderr, "System phys memory < 512MB\n");
        return 0;
    }

    int i = 0;
    /*for(; (1 << i) < main_context->system_phys_page_count; i++) {
        test_page_alloc(main_context, (1 << i));
    }

    * Don't do this unless you're masochistic

    fprintf(stderr, "Attempting to allocate all but 65536 pyshical pages in the system\n");
    if(!test_memory_alloc(main_context, main_context->system_phys_page_count - 0x10000))
        return 0;

    */

    RECORD_CREATE(Record_database, database);

    RECORD_ALLOC(Record_ptbl, database->ptbl_record_tbl);
    database->ptbl_record_count = 1;
    PTBL_RECORD_SET_KEY(database->ptbl_record_tbl[0], 3);
    PTBL_RECORD_SET_PAGE_COUNT(database->ptbl_record_tbl[0], 1);

    Record_ptbl *ptbl = database_ptbl_search(main_context, database, 3);
    if(ptbl != database->ptbl_record_tbl) {
        fprintf(stderr, "Failed to find ptbl entry\n");
        return 0;
    }

    if(0 != database_ptbl_search(main_context, database, -1)) {
        fprintf(stderr, "Ptbl lookup should have failed\n");
        return 0;
    }

    database_pages_free(main_context, database);

    for(i = 0; i <= 8; i++) {
        // Alloc a new bucket
        unsigned char *page_base = database_pages_alloc(main_context, database, 10, i);
        if(!page_base) {
            fprintf(stderr, "Failed to allocate 10 pages for bucket %d\n", i);
            return 0;
        }
        if(database->ptbl_record_count != i + 1) {
            fprintf(stderr, "Incorrect ptbl record count (bucket %d)\n", i);
            return 0;
        }
        unsigned int count, count2;
        if((count = PTBL_RECORD_GET_PAGE_COUNT(database->ptbl_record_tbl[i])) != 10) {
            fprintf(stderr, "Bucket %d page count incorrect (%d != 10)\n", i, count);
            return 0;
        }
        if((count = database->ptbl_record_tbl[i].page_usage_length) != (count2 = database_ptbl_calc_page_usage_length(i, 10))) {
            fprintf(stderr, "Bucket %d failed to alloc correct amount for page_usage (%d != %d)\n", i, count, count2);
            return 0;
        }

        // Alloc a page in the same bucket (should be same result as first time because bucket will be empty)
        unsigned char *new_page_base = database_pages_alloc(main_context, database, 1, i);
        if(!new_page_base) {
            fprintf(stderr, "Bucket %d failed realloc: %p => %p\n", i, page_base, new_page_base);
            return 0;
        }
        if(new_page_base != page_base) {
            fprintf(stderr, "Bucket %d mmap()d new page when it shouldn't have", i);
            return 0;
        }

        // Test that we can allocate j free pages in a bucket correctly when page (j - 1) is in use
        int j = 1;
        for(; j <= 10; j++) {
            int k = 0;
            int bits = (i < 8) ? (256 >> i) : 1;

            unsigned int old_page_count = PTBL_RECORD_GET_PAGE_COUNT(database->ptbl_record_tbl[i]);
            unsigned int old_page_usage_length = database->ptbl_record_tbl[i].page_usage_length;
            unsigned char *old_page_base = new_page_base;

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

            new_page_base = database_pages_alloc(main_context, database, j, i);

            unsigned int new_page_count = PTBL_RECORD_GET_PAGE_COUNT(database->ptbl_record_tbl[i]);
            unsigned int new_page_usage_length = database->ptbl_record_tbl[i].page_usage_length;

            if(!new_page_base) {
                fprintf(stderr, "Bucket %d failed realloc(%d): New page base is null\n", i, j);
                return 0;
            }
            if(expected_new_page_base != (unsigned char *)-1 && new_page_base != expected_new_page_base) {
                fprintf(stderr, "Bucket %d failed realloc(%d): New page base %p != %p\n", i, j, new_page_base, expected_new_page_base);
                return 0;
            }
            if(new_page_usage_length != expected_new_page_usage_length) {
                fprintf(stderr, "Bucket %d failed realloc(%d): New page usage length %d != %d\n", i, j, new_page_usage_length, expected_new_page_usage_length);
                return 0;
            }
            if(new_page_count != expected_new_page_count) {
                fprintf(stderr, "Bucket %d failed realloc(%d): New page count %d != %d\n", i, j, new_page_count, expected_new_page_count);
                return 0;
            }
        }
    }

    database_pages_free(main_context, database);

    // TODO: Add test cases for K/V pair storing
    memory_free(database);

    return 1;
}
