#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "records.h"

int main(void) {
    struct ptbl_record rec;
    memset(&rec, 0, sizeof(rec));

    PTBL_RECORD_SET_PAGE_COUNT(rec, 0xffffffff);
    if(0x1fffffff != rec.key_high_and_page_count) {
        fprintf(stderr, "Page count set past boundary\n");
        return 1;
    }

    rec.key_high_and_page_count = 0xffffffff;
    if(0x1fffffff != PTBL_RECORD_GET_PAGE_COUNT(rec)) {
        fprintf(stderr, "Page count get past boundary\n");
        return 1;
    }

    if(0x38 != PTBL_RECORD_GET_KEY(rec)) {
        fprintf(stderr, "Key get past boundary\n");
        return 1;
    }

    PTBL_RECORD_SET_OFFSET(rec, 0xffffffff);
    if(0x1fffffff != rec.key_low_and_offset) {
        fprintf(stderr, "Offset get past boundary\n");
        return 1;
    }

    rec.key_low_and_offset = 0xffffffff;
    if(0x1fffffff != PTBL_RECORD_GET_OFFSET(rec)) {
        fprintf(stderr, "Offset get past boundary\n");
        return 1;
    }

    if(0x3f != PTBL_RECORD_GET_KEY(rec)) {
        fprintf(stderr, "Key get past boundary\n");
        return 1;
    }

    rec.key_high_and_page_count = 0;
    rec.key_low_and_offset = 0;
    PTBL_RECORD_SET_KEY(rec, -1);
    if(0xE0000000 != rec.key_high_and_page_count || 0xE0000000 != rec.key_low_and_offset) {
        fprintf(stderr, "Key set past boundary\n");
        return 1;
    }

    struct kv_record kv_record;
    memset(&kv_record, 0, sizeof(kv_record));

    KV_RECORD_SET_FLAGS(kv_record, 0xffff);
    if(0xFF00000000000000 != kv_record.flags_and_size) {
        fprintf(stderr, "Flags set past boundary: %p\n", kv_record.flags_and_size);
        return 1;
    }

    if(0xFF != KV_RECORD_GET_FLAGS(kv_record)) {
        fprintf(stderr, "Flags get past boundary\n");
        return 1;
    }

    kv_record.flags_and_size = 0;
    KV_RECORD_SET_SIZE(kv_record, 0xffffffffffffffff);
    if(0x00FFFFFFFFFFFFFF != kv_record.flags_and_size) {
        fprintf(stderr, "Size set past boundary\n");
        return 1;
    }

    kv_record.flags_and_size <<= 8;
    if(0x00FFFFFFFFFFFF00 != KV_RECORD_GET_SIZE(kv_record)) {
        fprintf(stderr, "Size get past boundary\n");
        return 1;
    }

    return 0;
};
