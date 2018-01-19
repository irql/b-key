#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "context.h"
#include "records.h"
#include "memory.h"

int test_memory_alloc(struct main_context * main_context, int pages) {
    fprintf(stderr, "Mapping %d pages (%.2fGB)...", pages, (((float)pages * main_context->system_page_size) / 1000000000));
    unsigned char * region = memory_alloc(main_context, pages);
    if(region) {
        memset(region, 0, pages * main_context->system_page_size);
        fprintf(stderr, "free(%p)'d %s\n", region, memory_free(main_context, region, pages) == -1 ? strerror(errno) : "OK");
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

    unsigned char i = 0;
    for(; (1 << i) < main_context->system_phys_page_count; i++) {
        test_memory_alloc(main_context, (1 << i));
    }
    fprintf(stderr, "Attempting to allocate all but 65536 pyshical pages in the system\n");
    test_memory_alloc(main_context, main_context->system_phys_page_count - 0x10000);

    return 1;
}
