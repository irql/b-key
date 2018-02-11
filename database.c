#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "records.h"
#include "context.h"
#include "memory.h"

#ifndef DEBUG_DATABASE
    #undef DEBUG_PRINT
    #define DEBUG_PRINT(...)
#endif

char
database_ptbl_get(
    Context_main *ctx_main,
    Record_database *rec_database,
    int bucket
) {
    DEBUG_PRINT("database_ptbl_get(bucket = %d);\n", bucket);

    Record_ptbl *record = 0;
    char i = 0, ret = -1;
    for(; i < rec_database->ptbl_record_count; i++) {
        if(PTBL_RECORD_GET_KEY(rec_database->ptbl_record_tbl[i]) == bucket) {
            ret = i;
            break;
        }
    }

    DEBUG_PRINT("\treturn %d\n", ret);

    return ret;
}

int database_ptbl_init(
    Context_main *ctx_main,
    Record_ptbl *ptbl_entry,
    int page_count,
    int bucket
) {
    DEBUG_PRINT("database_ptbl_init(ptbl_entry = %p, page_count = %d, bucket = %d);\n", ptbl_entry, page_count, bucket);

    if(ptbl_entry->m_offset || ptbl_entry->page_usage || ptbl_entry->page_usage_length > 0) {
        DEBUG_PRINT("\tERR ptbl_entry already initialized\n");
        return 0;
    }

    ptbl_entry->m_offset = memory_page_alloc(ctx_main, ((bucket <= 8) ? page_count : (page_count << (bucket - 8))));
    if(!ptbl_entry->m_offset) {
        DEBUG_PRINT("\tERR Failed to allocate pages for bucket\n");
        return 0;
    }

    ptbl_entry->page_usage_length = PTBL_CALC_PAGE_USAGE_LENGTH(bucket, page_count);

    // Leave page_usage bits zero, they will be set/unset upon the storage or deletion of individual k/v pairs
    ptbl_entry->page_usage = memory_alloc(sizeof(unsigned char) * ptbl_entry->page_usage_length);
    if(!ptbl_entry->page_usage) {
        DEBUG_PRINT("\tERR Failed to allocate page_usage\n");
        return 0;
    }

    PTBL_RECORD_SET_PAGE_COUNT(ptbl_entry[0], page_count);
    PTBL_RECORD_SET_KEY(ptbl_entry[0], bucket);

    return 1;
}

// x = bucket
// max_value_len_inside_page = 2^(4+x)
// i.e., bucket #0 is for <=16-byte values
//       bucket #1 is for <=32-byte values
//       bucket #2 is for <=64-byte values
//       bucket #3 is for <=128-byte values
//       bucket #4 is for <=256-byte values
//       bucket #5 is for <=512-byte values
//       bucket #6 is for <=1024-byte values
//       bucket #7 is for <=2048-byte values
//       bucket #8 is for <=4096-byte values
//       ...
//       bucket #20 is for <=64MB values
//
// Returns:
//  ptr64 on success
//  0 on failure
unsigned char *
database_ptbl_alloc(
    Context_main *ctx_main,
    Record_database *rec_database,
    char *ptbl_index,
    int page_count,
    int bucket
) {
    DEBUG_PRINT("database_ptbl_alloc(page_count = %d, bucket = %d);\n", page_count, bucket);

    if(!rec_database) {
        return 0;
    }

    if(!rec_database->ptbl_record_tbl) {
        DEBUG_PRINT("\tInitializing database ptbl_record table\n");

        // Initialize the first database ptbl_record_tbl for given bucket

        RECORD_ALLOC(Record_ptbl, rec_database->ptbl_record_tbl);
        if(!database_ptbl_init(ctx_main, &rec_database->ptbl_record_tbl[0], page_count, bucket)) {
            DEBUG_PRINT("\tERR Failed to initialize ptbl record\n");
            return 0;
        }

        if(ptbl_index) ptbl_index[0] = 0;

        rec_database->ptbl_record_count = 1;

        return rec_database->ptbl_record_tbl[0].m_offset;
    }

    // Database ptbl_record_tbl exists

    char new_ptbl_index = database_ptbl_get(ctx_main, rec_database, bucket);

    if(new_ptbl_index == -1) {
        DEBUG_PRINT("\tInitializing ptbl_record\n");

        // Create new ptbl record to init bucket
        unsigned int new_ptbl_record_count = rec_database->ptbl_record_count + 1;
        Record_ptbl *new_ptbl =
            (Record_ptbl *)
            memory_realloc(
                rec_database->ptbl_record_tbl,
                rec_database->ptbl_record_count * sizeof(Record_ptbl),
                new_ptbl_record_count * sizeof(Record_ptbl)
                );
        if(!new_ptbl) {
            DEBUG_PRINT("\tERR Failed to realloc database->ptbl_record_tbl\n");
            return 0;
        }
        rec_database->ptbl_record_tbl = new_ptbl;
        rec_database->ptbl_record_count = new_ptbl_record_count;

#define _NEW_PTBL rec_database->ptbl_record_tbl[rec_database->ptbl_record_count - 1]

        if(!database_ptbl_init(ctx_main, &_NEW_PTBL, page_count, bucket)) {
            DEBUG_PRINT("\tERR Failed to initialize ptbl record\n");
            return 0;
        }

        if(ptbl_index) ptbl_index[0] = rec_database->ptbl_record_count - 1;

        return _NEW_PTBL.m_offset;
    }

#define _PTBL rec_database->ptbl_record_tbl[new_ptbl_index]

    if(!_PTBL.m_offset) {
        DEBUG_PRINT("\tERR Ptbl record, bucket %d, corrupt (m_offset is null)\n", bucket);
        return 0;
    }

    DEBUG_PRINT("\tpage_usage_length=%d\n", _PTBL.page_usage_length);
    DEBUG_PRINT("\tpage_count=%d\n", PTBL_RECORD_GET_PAGE_COUNT(_PTBL));

    unsigned char *offset = 0;
    int free_pages = 0,
        bits = PTBL_CALC_PAGE_USAGE_BITS(bucket),
        bytes = PTBL_CALC_PAGE_USAGE_BYTES(bucket),
        last_free_page = -1;

    for(int i = 0; i < _PTBL.page_usage_length / bytes; i++) {
        int j = 0, free = 0;

        // Multiple bytes per page
        if(bits >= 64) {

#define _SUBSET ((unsigned long *)(&_PTBL.page_usage[i * bytes]))

            for(j = 0; j < bytes / sizeof(unsigned long); j++) {
                DEBUG_PRINT("%d(%d)[%d]: %016lx\n", i, j, (i * bytes + ((j + 1) * 8)), _SUBSET[j]);
                if(_SUBSET[j] == 0) {
                    free++;
                }
            }
        }
        else if(bits == 32) {
            unsigned usage = ((unsigned *)_PTBL.page_usage)[i];
            DEBUG_PRINT("%d: %08x\n", i, usage);
            if(usage == 0) {
                free = j = 1;
            }
        }
        else if(bits == 16) {
            unsigned short usage = ((unsigned short *)_PTBL.page_usage)[i];
            DEBUG_PRINT("%d: %x\n", i, usage);
            if(usage == 0) {
                free = j = 1;
            }
        }
        else if(bits == 8) {
            DEBUG_PRINT("%d: %x\n", i, _PTBL.page_usage[i]);
            if(_PTBL.page_usage[i] == 0) {
                free = j = 1;
            }
        }

        // Multiple pages per byte
        else {
            // Bucket 6 (1024-byte values) = 4 bits per page (2 pages per byte)
            // Bucket 7 (2048-byte values) = 2 bits per page (4 pages per byte)
            // Bucket >= 8 (>= 4096-byte values) = 1 bit per page (8 pages per byte)

            // ppb = pages per byte
            int k, ppb = 8 / bits;

            // max - all the bits in a byte aren't always in use if page_count
            // divided by pages per byte has a remainder (the remainder is the
            // number of bit-groups to continue processing).
            int max = (i == ((_PTBL.page_usage_length / bytes) - 1)) ?
                (
                 (PTBL_RECORD_GET_PAGE_COUNT(_PTBL) % ppb) > 0 ?
                    (PTBL_RECORD_GET_PAGE_COUNT(_PTBL) % ppb) :
                    ppb // return ppb if there is no remainder (all bit groups in use)
                ) : ppb;

            unsigned char mask =
                (bits == 4) ? 0xF :
                (bits == 2) ? 0x3 :
                (bits == 1) ? 1 : 0;

            for(k = 0; k < max; k++) {
                unsigned char usage =
                    (
                     (_PTBL.page_usage[i] & (mask << (bits * k)))
                     >> (bits * k)
                    );
                DEBUG_PRINT("%d: %d\n", (i * ppb) + k, usage);
                if(usage == 0) {
                    free++;
                    j = k + 1;
                }
                else {
                    free = free_pages = 0;
                    last_free_page = -1;
                }
            }

            if(free > 0) {
                free_pages += free;
                if(last_free_page == -1)
                    last_free_page = i * ppb + (j - free);
            }

            DEBUG_PRINT("Free pages: %d, last_free_page: %d\n", free_pages, last_free_page);
        }

        if(bits >= 8) {
            if(free > 0 && j > 0 && free == j) {
                free_pages++;
                last_free_page = i;
            }
            else {
                // free_pages counts contiguous free pages
                free_pages = 0;
                last_free_page = -1;
            }
        }

        if(free_pages >= page_count) {
            offset = _PTBL.m_offset
                + (
                    (
                     (bits < 8 ?
                      ((i * (8 / bits)) + j - 1 - (free_pages - page_count)) :
                      i
                     )
                     - (page_count - 1)
                    )
                    * ctx_main->system_page_size
                    * ((bucket <= 8) ? 1 : (1 << (bucket - 8)))
                );
            last_free_page = -1;
            break;
        }
    }

    if(!offset || last_free_page != -1) {
        // Realloc (add) more pages
        int new_page_count = PTBL_RECORD_GET_PAGE_COUNT(_PTBL) + page_count - free_pages;

        offset = memory_page_realloc(
                ctx_main,
                _PTBL.m_offset,
                PTBL_RECORD_GET_PAGE_COUNT(_PTBL),
                ((bucket <= 8) ? new_page_count : (new_page_count << (bucket - 8)))
                );

        if(!offset) {
            return 0;
        }

        // If the OS assigns us a new virtual address, we need to record that
        _PTBL.m_offset = offset;

        unsigned int new_page_usage_length = PTBL_CALC_PAGE_USAGE_LENGTH(bucket, new_page_count);

        // Current method doesn't garbage collect the page-tables (yet),
        // however we should avoid calling the realloc() function on
        // page_usage if the new length is exactly the same as the old
        // length (i.e. on any bucket >8 where multiple pages are
        // represented in a single byte)
        if(new_page_usage_length > _PTBL.page_usage_length) {
            unsigned char *new_page_usage = memory_realloc(_PTBL.page_usage, sizeof(unsigned char) * _PTBL.page_usage_length, sizeof(unsigned char) * new_page_usage_length);
            if(!new_page_usage) {
                DEBUG_PRINT("\tERR failed to increase the size of page_usage\n");
                return 0;
            }
            _PTBL.page_usage = new_page_usage;
            _PTBL.page_usage_length = new_page_usage_length;
            DEBUG_PRINT("\tIncreased size of page_usage: %d\n", _PTBL.page_usage_length);
        }

        PTBL_RECORD_SET_PAGE_COUNT(_PTBL, new_page_count);

        // This needs to be done AFTER setting _PTBL.m_offset to the right page base
        // (for obvious reasons)
        if(last_free_page != -1) {
            offset += (new_page_count - page_count) * ctx_main->system_page_size * ((bucket <= 8) ? 1 : (1 << (bucket - 8)));
        }
    }

    if(ptbl_index) ptbl_index[0] = new_ptbl_index;

    DEBUG_PRINT("\tOffset decided = %p (%ldB, page bucket starts %p)\n", offset, offset - _PTBL.m_offset, _PTBL.m_offset);

    return offset;
}

void database_ptbl_free(
    Context_main *ctx_main,
    Record_database *rec_database
) {
    DEBUG_PRINT("database_ptbl_free();\n");

    if(rec_database->ptbl_record_tbl) {
        for(int i = 0; i < rec_database->ptbl_record_count; i++) {

#undef _PTBL
#define _PTBL rec_database->ptbl_record_tbl[i]

            if(_PTBL.page_usage) {

                memory_free(_PTBL.page_usage);
                _PTBL.page_usage = 0;
                _PTBL.page_usage_length = 0;

                if(_PTBL.m_offset) {
                    memory_page_free(
                            ctx_main,
                            _PTBL.m_offset,
                            PTBL_RECORD_GET_PAGE_COUNT(_PTBL)
                            );
                }
            }
        }

        memory_free(rec_database->ptbl_record_tbl);
        rec_database->ptbl_record_count = 0;
        rec_database->ptbl_record_tbl = 0;

        if(rec_database->kv_record_tbl) {
            memory_free(rec_database->kv_record_tbl);
        }
        rec_database->kv_record_count = 0;
        rec_database->kv_record_tbl = 0;
    }
}

int database_calc_bucket(
    unsigned long length
) {
    int i = -3;
    length--;
    while(length >>= 1)
        i++;
    return ((i > 0) ? i : 0);
}

int
database_kv_free(
    Context_main *ctx_main,
    Record_database *rec_database,
    unsigned long k
) {
    if(k >= rec_database->kv_record_count) {
        DEBUG_PRINT("\tERR k is greater than kv_record_count\n");
        return 0;
    }

#define _REC_KV rec_database->kv_record_tbl[k]

    if(0 == KV_RECORD_GET_SIZE(_REC_KV)) {
        DEBUG_PRINT("\tRecord already freed\n");
        return 1;
    }


    char ptbl_index = database_ptbl_get(ctx_main, rec_database, KV_RECORD_GET_BUCKET(_REC_KV));
    if(ptbl_index == -1) {
        DEBUG_PRINT("database_kv_free(k = %d) No ptbl entry found for bucket - corrupt kv record\n", k);
        return 0;
    }

    unsigned char bucket = KV_RECORD_GET_BUCKET(_REC_KV);
    unsigned long bucket_wsz = PTBL_CALC_BUCKET_WORD_SIZE(bucket);
    unsigned long kv_index = KV_RECORD_GET_INDEX(_REC_KV);

    // Set record size to 0 to disable lookup
    KV_RECORD_SET_SIZE(_REC_KV, 0);

    // Zero-out value
    memset(PTBL_RECORD_VALUE_PTR(rec_database, ptbl_index, _REC_KV), 0, bucket_wsz);

    // Mark value as freed in page_usage
    PTBL_RECORD_PAGE_USAGE_FREE(rec_database, ptbl_index, kv_index);

    // Only decrement kv_record_count if the kv_record being free()d is the one at the very end of rec_database->kv_record_tbl
    // This is because we don't want to lose a record at the end of kv_record_tbl if a record is free()d in the middle.
    if(k == rec_database->kv_record_count - 1) rec_database->kv_record_count--;

    // If this was the last record in the table, free() it to make sure it gets reinitialized
    if(rec_database->kv_record_count == 0) {
        memory_free(rec_database->kv_record_tbl);
        rec_database->kv_record_tbl = 0;
    }

    return 1;
}

int
database_value_free(
    Context_main *ctx_main,
    Record_database *rec_database,
    char bucket,
    unsigned long index
) {
    char ptbl_index = database_ptbl_get(ctx_main, rec_database, bucket);
    if(-1 == ptbl_index) {
        char new_ptbl_index;
        if(!database_ptbl_alloc(ctx_main, rec_database, &new_ptbl_index, 1, bucket)) {
            DEBUG_PRINT("database_value_free(): Failed call to database_ptbl_alloc()\n");
            return 0;
        }

        ptbl_index = new_ptbl_index;
    }

#undef _PTBL
#define _PTBL rec_database->ptbl_record_tbl[ptbl_index]

    if(!_PTBL.page_usage || _PTBL.page_usage_length != PTBL_CALC_PAGE_USAGE_LENGTH(bucket, PTBL_RECORD_GET_PAGE_COUNT(_PTBL))) {
        DEBUG_PRINT("database_value_free(): page_usage_length does not match page count\n");
        return 0;
    }

    PTBL_RECORD_PAGE_USAGE_FREE(rec_database, ptbl_index, index);

    return 1;
}

unsigned long
_database_value_alloc(
    Context_main *ctx_main,
    Record_database *rec_database,
    char *ptbl_index,
    char bucket
) {
    DEBUG_PRINT("_database_value_alloc(bucket = %d)\n", bucket);

    char new_ptbl_index = database_ptbl_get(ctx_main, rec_database, bucket);
    if(-1 == new_ptbl_index) {
        char new_new_ptbl_index;

        if(!database_ptbl_alloc(ctx_main, rec_database, &new_new_ptbl_index, 1, bucket)) {
            DEBUG_PRINT("_database_value_alloc(): Failed call to database_ptbl_alloc()\n");
            return -1;
        }

        new_ptbl_index = new_new_ptbl_index;
    }

    unsigned long free_index = -1;

#undef _PTBL
#define _PTBL rec_database->ptbl_record_tbl[new_ptbl_index]

    unsigned int page_count = PTBL_RECORD_GET_PAGE_COUNT(_PTBL),
        page_usage_bytes = PTBL_CALC_PAGE_USAGE_BYTES(bucket),
        page_usage_bits = PTBL_CALC_PAGE_USAGE_BITS(bucket);

    // Identify the first unused "slot" that can hold a value of the appropriate size
    for(int i = 0; i < _PTBL.page_usage_length && free_index == -1; i++) {
        unsigned char bits = _PTBL.page_usage[i];
        int max = 8;
        if(page_usage_bits < 8) {
            // Ensure that we don't try to read more bits than there are total
            if(!(max = ((page_count * page_usage_bits) % 8))) {
                max = 8;
            }
        }
        for(int j = 0; j < max && free_index == -1; j++) {
            // Mark value slot as used since we will occupy the empty slot
            _PTBL.page_usage[i] |= (1 << j);
            free_index = i * 8 + j;
        }
    }

    if(free_index == -1) {
        char new_new_ptbl_index;

        // No free slots exist in any of the pages, so we need to allocate a new page
        if(!database_ptbl_alloc(ctx_main, rec_database, &new_new_ptbl_index, 1, bucket)) {
            return -1;
        }

        // Mark the first free value slot as not-free
        free_index = (page_count - 1) * page_usage_bytes * 8;
        rec_database->ptbl_record_tbl[new_new_ptbl_index].page_usage[free_index / 8] |= 1;

        new_ptbl_index = new_new_ptbl_index;
    }

    if(ptbl_index) ptbl_index[0] = new_ptbl_index;

    return free_index;
}

unsigned long
database_kv_alloc(
    Context_main *ctx_main,
    Record_database *rec_database,
    unsigned char flags,
    unsigned long size,
    unsigned char *buffer
) {
    DEBUG_PRINT("database_alloc_kv(flags = %02x, size = %d, buffer = %p);\n", flags, size, buffer);

    // Allocate based on page-table mappings
    // If no page table exists for records of
    // a given size, create one.

    unsigned char bucket = database_calc_bucket(size);
    DEBUG_PRINT("\tbucket = %d\n", bucket);

    char ptbl_index;
    unsigned long free_index = _database_value_alloc(ctx_main, rec_database, &ptbl_index, bucket);
    if(free_index == -1) {
        DEBUG_PRINT("\tERR failed to allocate new value in bucket %d\n", bucket);
        return -1;
    }
    Record_ptbl *ptbl_entry = &rec_database->ptbl_record_tbl[ptbl_index];

    unsigned long value_offset = free_index * PTBL_CALC_BUCKET_WORD_SIZE(bucket);

    // We need to find a free spot in the kv_record table and occupy it
    unsigned long free_kv = 0;
    if(!rec_database->kv_record_tbl) {
        rec_database->kv_record_tbl = (Record_kv *)memory_alloc(sizeof(Record_kv));
        if(!rec_database->kv_record_tbl) {
            DEBUG_PRINT("database_alloc_kv() Failed to allocate kv_record_tbl\n");
            return -1;
        }
        rec_database->kv_record_count = 1;
    }
    else {
        free_kv = -1;
        for(int i = 0; i < rec_database->kv_record_count; i++) {
            if(!KV_RECORD_GET_SIZE(rec_database->kv_record_tbl[i])) {
                free_kv = i;
            }
        }

        // If we can't find a free record to annex, we should
        // try to reallocate the record table
        if(free_kv == -1) {
            Record_kv *new_kv_tbl = (Record_kv *)
                memory_realloc(
                    rec_database->kv_record_tbl,
                    rec_database->kv_record_count * sizeof(Record_kv),
                    (rec_database->kv_record_count + 1) * sizeof(Record_kv)
                    );
            if(!new_kv_tbl) {
                DEBUG_PRINT("database_alloc_kv(): Failed to increase the size of kv_record_tbl\n");
                return -1;
            }

            free_kv = rec_database->kv_record_count;

            rec_database->kv_record_tbl = new_kv_tbl;
            rec_database->kv_record_count++;
        }
    }

    Record_kv *kv_rec = &rec_database->kv_record_tbl[free_kv];
    DEBUG_PRINT("KV_REC: %d, %p, %p\n", free_kv, kv_rec, rec_database->kv_record_tbl);

    KV_RECORD_SET_FLAGS(kv_rec[0], flags);
    KV_RECORD_SET_SIZE(kv_rec[0], size);
    KV_RECORD_SET_BUCKET(kv_rec[0], bucket);
    KV_RECORD_SET_INDEX(kv_rec[0], free_index);

    memcpy((unsigned char *)(ptbl_entry->m_offset + value_offset), buffer, size);

    return free_kv;
}

unsigned char *
database_kv_get_value(
    Context_main *ctx_main,
    Record_database *rec_database,
    char *ptbl_index,
    unsigned long k
) {
    DEBUG_PRINT("database_kv_get_value(k = %d);\n", k);

    if(k >= rec_database->kv_record_count) {
        DEBUG_PRINT("\tERR k is greater than kv_record_count\n");
        return 0;
    }

#undef _REC_KV
#define _REC_KV rec_database->kv_record_tbl[k]

    if(0 == KV_RECORD_GET_SIZE(_REC_KV)) {
        DEBUG_PRINT("\tERR size of record is 0\n");
        return 0;
    }

    int bucket = KV_RECORD_GET_BUCKET(_REC_KV);
    char rec_ptbl_index = database_ptbl_get(ctx_main, rec_database, bucket);
    if(rec_ptbl_index == -1) {
        DEBUG_PRINT("\tERR rec_database is corrupt- found orphaned kv_record in non-existant bucket");
        return 0;
    }

    if(ptbl_index) ptbl_index[0] = rec_ptbl_index;

    return PTBL_RECORD_VALUE_PTR(rec_database, rec_ptbl_index, _REC_KV);
}

int
database_kv_set_value(
    Context_main *ctx_main,
    Record_database *rec_database,
    unsigned long k,
    unsigned long length,
    unsigned char *buffer
) {
    DEBUG_PRINT("database_kv_set_value(k = %d, length = %d, buffer = %p);\n", k, length, buffer);

    if(k >= rec_database->kv_record_count) {
        DEBUG_PRINT("\tERR k is greater than kv_record_count\n");
        return 0;
    }

#undef _REC_KV
#define _REC_KV rec_database->kv_record_tbl[k]

    if(0 == KV_RECORD_GET_SIZE(_REC_KV)) {
        DEBUG_PRINT("\tERR size of record is 0\n");
        return 0;
    }

    char old_ptbl_index;
    unsigned char *region = database_kv_get_value(ctx_main, rec_database, &old_ptbl_index, k);
    if(!region) {
        DEBUG_PRINT("\tERR failed to get region\n");
        return 0;
    }

    /* We allocate a new value, then swap the old value that the kv_record has with the new one */

    char new_ptbl_index;
    char bucket = database_calc_bucket(length);
    unsigned long new_index = _database_value_alloc(ctx_main, rec_database, &new_ptbl_index, bucket);
    if(new_index == -1) {
        DEBUG_PRINT("\tERR failed to realloc value\n");
        return 0;
    }
    Record_ptbl *new_ptbl = &rec_database->ptbl_record_tbl[new_ptbl_index];

    // "Disable" kv_rec by setting size to 0
    KV_RECORD_SET_SIZE(_REC_KV, 0);

    // Free the value in the bucket
    DEBUG_PRINT("\tkv_rec = %p, page_usage = %p\n", &_REC_KV, rec_database->ptbl_record_tbl[old_ptbl_index]);
    PTBL_RECORD_PAGE_USAGE_FREE(rec_database, old_ptbl_index, KV_RECORD_GET_INDEX(_REC_KV));

    // Set a new bucket/index for kv_rec
    KV_RECORD_SET_BUCKET(_REC_KV, bucket);
    KV_RECORD_SET_INDEX(_REC_KV, new_index);

    // Copy over buffer to the new value
    unsigned char *new_region = PTBL_RECORD_VALUE_PTR(rec_database, new_ptbl_index, _REC_KV);
    memcpy(new_region, buffer, length);

    // "Enable" record by setting size to new_index value length
    KV_RECORD_SET_SIZE(_REC_KV, length);

    return 1;
}
