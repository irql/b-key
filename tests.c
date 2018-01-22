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
        fprintf(stderr, "Flags set past boundary: %p\n", kv_record.flags_and_size);
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

    /*unsigned char i = 0;
    for(; (1 << i) < main_context->system_phys_page_count; i++) {
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

    memory_free(database->ptbl_record_tbl);
    database->ptbl_record_tbl = 0;

    /*unsigned int count;
    unsigned char *page = database_pages_alloc(main_context, database, 1, 0);
    if(!page) {
        fprintf(stderr, "Failed to crate a page\n");
        return 0;
    }
    if(database->ptbl_record_count != 1) {
        fprintf(stderr, "ptbl_record_count != 1\n");
        return 0;
    }
    if((count = PTBL_RECORD_GET_PAGE_COUNT(database->ptbl_record_tbl[0])) != 1) {
        fprintf(stderr, "Bucket 0 page count incorrect (%d != 1)\n", count);
        return 0;
    }
    if((count = database->ptbl_record_tbl[0].page_usage_length) != 32) {
        fprintf(stderr, "Failed to alloc correct amount for page_usage (%d != 32)\n", count);
        return 0;
    }

    // Realloc a NEW bucket
    unsigned char *newpage = database_pages_alloc(main_context, database, 4, 1);
    if(!newpage) {
        fprintf(stderr, "Failed to realloc a new page bucket\n");
        return 0;
    }
    if(database->ptbl_record_count != 2) {
        fprintf(stderr, "ptbl_record_count != 2\n");
        return 0;
    }
    if((count = PTBL_RECORD_GET_PAGE_COUNT(database->ptbl_record_tbl[1])) != 4) {
        fprintf(stderr, "Bucket 1 page count incorrect (%d != 4)\n", count);
        return 0;
    }
    if((count = database->ptbl_record_tbl[1].page_usage_length) != 16 * 4) {
        fprintf(stderr, "Failed to alloc correct amount for page_usage (%d != 16 * 4)\n", count);
        return 0;
    }

    // Realloc the SAME bucket with same size
    // NOTE: should return the exact same "page" since it is unused
    //database->ptbl_record_tbl[1].page_usage[(8 * 2) - 1] = 0xFF;
    unsigned char *newpage2 = database_pages_alloc(main_context, database, 3, 1);
    if(!newpage2) {
        fprintf(stderr, "Failed to realloc the same page bucket\n");
        return 0;
    }
    if(newpage != newpage2) {
        fprintf(stderr, "New page has a new address %p=>%p\n", newpage, newpage2);
        return 0;
    }*/

    int i = 0;
    for(; i <= 5; i++) {
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
        if((count = database->ptbl_record_tbl[i].page_usage_length) != (count2 = PTBL_CALC_PAGE_USAGE_LENGTH(i) * 10)) {
            fprintf(stderr, "Bucket %d failed to alloc correct amount for page_usage (%d != %d)\n", i, count, count2);
            return 0;
        }
        unsigned char *new_page_base = database_pages_alloc(main_context, database, 1, i);
        if(!new_page_base) {
            fprintf(stderr, "Bucket %d failed realloc: %p => %p\n", page_base, new_page_base);
            return 0;
        }
        if(new_page_base != page_base) {
            fprintf(stderr, "Bucket %d mmap()d new page when it shouldn't have", i);
            return 0;
        }

#define SET_USED(p,x,v) \
        if(p < 5) \
            database->ptbl_record_tbl[i].page_usage[(32 >> p) * x] = v;

        SET_USED(i, 0, 1)
        new_page_base = database_pages_alloc(main_context, database, 1, i);
        if(new_page_base != page_base + main_context->system_page_size) {
            fprintf(stderr, "Bucket %d failed realloc(1): %p != %p\n", i, new_page_base, page_base + main_context->system_page_size);
            return 0;
        }

        SET_USED(i, 0, 0)
        SET_USED(i, 1, 1)
        new_page_base = database_pages_alloc(main_context, database, 2, i);
        if(new_page_base != page_base + main_context->system_page_size * 2) {
            fprintf(stderr, "Bucket %d failed realloc(2): %p != %p\n", i, new_page_base, page_base + (i * main_context->system_page_size));
            return 0;
        }
        //fprintf(stderr, "Bucket %d, alloc(2) %p => %p\n", i, page_base, new_page_base);
    }

    database_pages_free(main_context, database);
    memory_free(database);

    return 1;
}
