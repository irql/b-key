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

Record_ptbl *
database_ptbl_get(
    Context_main *ctx_main,
    Record_database *rec_database,
    int bucket
) {
    Record_ptbl *record = 0;
    int i = 0;
    for(; i < rec_database->ptbl_record_count; i++) {
        if(PTBL_RECORD_GET_KEY(rec_database->ptbl_record_tbl[i]) == bucket)
            record = &rec_database->ptbl_record_tbl[i];
    }
    return record;
}

void database_ptbl_init(
    Context_main *ctx_main,
    Record_ptbl *ptbl_entry,
    int page_count,
    int bucket
) {
    ptbl_entry->m_offset = memory_page_alloc(ctx_main, ((bucket <= 8) ? page_count : (page_count << (bucket - 8))));

    ptbl_entry->page_usage_length = PTBL_CALC_PAGE_USAGE_LENGTH(bucket, page_count);

    // Leave page_usage bits zero, they will be set/unset upon the storage or deletion of individual k/v pairs
    ptbl_entry->page_usage = memory_alloc(sizeof(unsigned char) * ptbl_entry->page_usage_length);

    PTBL_RECORD_SET_PAGE_COUNT(ptbl_entry[0], page_count);
    PTBL_RECORD_SET_KEY(ptbl_entry[0], bucket);
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
    Record_ptbl **rec_ptbl,
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
        database_ptbl_init(ctx_main, &rec_database->ptbl_record_tbl[0], page_count, bucket);
        if(rec_ptbl) rec_ptbl[0] = rec_database->ptbl_record_tbl;
        rec_database->ptbl_record_count = 1;

        return rec_database->ptbl_record_tbl[0].m_offset;
    }

    // Database ptbl_record_tbl exists

    Record_ptbl *ptbl =
        database_ptbl_get(ctx_main, rec_database, bucket);

    if(!ptbl) {
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
        rec_database->ptbl_record_count = new_ptbl_record_count;
        if(!new_ptbl) {
            DEBUG_PRINT("\tERR Failed to realloc database->ptbl_record_tbl\n");
            return 0;
        }
        rec_database->ptbl_record_tbl = new_ptbl;

        Record_ptbl *ptbl_entry = &rec_database->ptbl_record_tbl[rec_database->ptbl_record_count - 1];

        database_ptbl_init(ctx_main, ptbl_entry, page_count, bucket);

        if(rec_ptbl) rec_ptbl[0] = ptbl_entry;

        return ptbl_entry->m_offset;
    }

    if(!ptbl->m_offset) {
        DEBUG_PRINT("\tERR Ptbl record, bucket %d, corrupt (m_offset is null)\n", bucket);
        return 0;
    }

    /*DEBUG_PRINT("\ndatabase_ptbl_alloc(.., %d, %d);\n", page_count, bucket);
    DEBUG_PRINT("\tpage_usage_length=%d\n", ptbl->page_usage_length);
    DEBUG_PRINT("\tpage_count=%d\n", PTBL_RECORD_GET_PAGE_COUNT(ptbl[0]));*/

    unsigned char *offset = 0;
    int i = 0,
        free_pages = 0,
        bits = PTBL_CALC_PAGE_USAGE_BITS(bucket),
        bytes = PTBL_CALC_PAGE_USAGE_BYTES(bucket),
        last_free_page = -1;

    for(i = 0; i < ptbl->page_usage_length / bytes; i++) {
        int j = 0, free = 0;

        // Multiple bytes per page
        if(bits >= 64) {
            unsigned long *subset = (unsigned long *)(&ptbl->page_usage[i * bytes]);
            for(j = 0; j < bytes / sizeof(unsigned long); j++) {
                //DEBUG_PRINT("%d(%d)[%d]: %016lx\n", i, j, (i * bytes + ((j + 1) * 8)), subset[j]);
                if(subset[j] == 0) {
                    free++;
                }
            }
        }
        else if(bits == 32) {
            unsigned usage = ((unsigned *)ptbl->page_usage)[i];
            //DEBUG_PRINT("%d: %08x\n", i, usage);
            if(usage == 0) {
                free = j = 1;
            }
        }
        else if(bits == 16) {
            unsigned short usage = ((unsigned short *)ptbl->page_usage)[i];
            //DEBUG_PRINT("%d: %x\n", i, usage);
            if(usage == 0) {
                free = j = 1;
            }
        }
        else if(bits == 8) {
            //DEBUG_PRINT("%d: %x\n", i, ptbl->page_usage[i]);
            if(ptbl->page_usage[i] == 0) {
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
            int max = (i == ((ptbl->page_usage_length / bytes) - 1)) ?
                (
                 (PTBL_RECORD_GET_PAGE_COUNT(ptbl[0]) % ppb) > 0 ?
                    (PTBL_RECORD_GET_PAGE_COUNT(ptbl[0]) % ppb) :
                    ppb // return ppb if there is no remainder (all bit groups in use)
                ) : ppb;

            unsigned char mask =
                (bits == 4) ? 0xF :
                (bits == 2) ? 0x3 :
                (bits == 1) ? 1 : 0;

            for(k = 0; k < max; k++) {
                unsigned char usage =
                    (
                     (ptbl->page_usage[i] & (mask << (bits * k)))
                     >> (bits * k)
                    );
                //DEBUG_PRINT("%d: %d\n", (i * ppb) + k, usage);
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

            //DEBUG_PRINT("Free pages: %d, last_free_page: %d\n", free_pages, last_free_page);
        }

        if(bits > 4) {
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
            offset = ptbl->m_offset
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
            ////DEBUG_PRINT("Offset decided = %p (%ldB, page bucket starts %p)\n", offset, offset - ptbl->m_offset, ptbl->m_offset);
            break;
        }
    }

    if(!offset || last_free_page != -1) {
        // Realloc (add) more pages
        int new_page_count = PTBL_RECORD_GET_PAGE_COUNT(ptbl[0]) + page_count - free_pages;

        offset = memory_page_realloc(
                ctx_main,
                ptbl->m_offset,
                PTBL_RECORD_GET_PAGE_COUNT(ptbl[0]),
                ((bucket <= 8) ? new_page_count : (new_page_count << (bucket - 8)))
                );

        if(!offset) {
            return 0;
        }

        unsigned int new_page_usage_length = PTBL_CALC_PAGE_USAGE_LENGTH(bucket, new_page_count);

        // Current method doesn't garbage collect the page-tables (yet),
        // however we should avoid calling the realloc() function on
        // page_usage if the new length is exactly the same as the old
        // length (i.e. on any bucket >8 where multiple pages are
        // represented in a single byte)
        if(new_page_usage_length > ptbl->page_usage_length) {
            ptbl->page_usage = memory_realloc(ptbl->page_usage, sizeof(unsigned char) * ptbl->page_usage_length, sizeof(unsigned char) * new_page_usage_length);
            if(!ptbl->page_usage) {
                return 0;
            }
            ptbl->page_usage_length = new_page_usage_length;                
            DEBUG_PRINT("\tIncreased size of page_usage: %d\n", ptbl->page_usage_length);
        }

        // If the OS assigns us a new virtual address, we need to record that
        ptbl->m_offset = offset;
        PTBL_RECORD_SET_PAGE_COUNT(ptbl[0], new_page_count);

        // This needs to be done AFTER setting ptbl->m_offset to the right page base
        // (for obvious reasons)
        if(last_free_page != -1) {
            offset += (new_page_count - page_count) * ctx_main->system_page_size * ((bucket <= 8) ? 1 : (1 << (bucket - 8)));
        }
    }

    if(rec_ptbl) rec_ptbl[0] = ptbl;

    return offset;
}

void database_ptbl_free(
    Context_main *ctx_main,
    Record_database *rec_database
) {
    if(rec_database->ptbl_record_tbl) {
        int i = 0;
        for(; i < rec_database->ptbl_record_count; i++) {
            if(rec_database->ptbl_record_tbl[i].page_usage) {
                Record_ptbl *ptbl_rec = &rec_database->ptbl_record_tbl[i];

                memory_free(ptbl_rec->page_usage);
                ptbl_rec->page_usage = 0;
                rec_database->ptbl_record_count = 0;

                unsigned page_count = PTBL_RECORD_GET_PAGE_COUNT(ptbl_rec[0]);

                if(ptbl_rec->m_offset) {
                    memory_page_free(
                            ctx_main,
                            ptbl_rec->m_offset,
                            page_count
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
    if(k > rec_database->kv_record_count) {
        DEBUG_PRINT("database_kv_free(k = %d) Tried to free invalid key\n", k);
        return 0;
    }

    Record_kv *kv_rec = &rec_database->kv_record_tbl[k];
    if(!KV_RECORD_GET_SIZE(kv_rec[0])) {
        DEBUG_PRINT("database_kv_free(k = %d) kv_rec already free'd\n", k);
        return 1;
    }

    Record_ptbl *ptbl_entry = database_ptbl_get(ctx_main, rec_database, KV_RECORD_GET_BUCKET(kv_rec[0]));
    if(!ptbl_entry) {
        DEBUG_PRINT("database_kv_free(k = %d) No ptbl entry found for bucket - corrupt kv record\n", k);
        return 0;
    }

    unsigned char bucket = KV_RECORD_GET_BUCKET(kv_rec[0]);
    unsigned long bucket_wsz = PTBL_CALC_BUCKET_WORD_SIZE(bucket);
    unsigned long kv_index = KV_RECORD_GET_INDEX(kv_rec[0]);

    // Set record size to 0 to disable lookup
    KV_RECORD_SET_SIZE(kv_rec[0], 0);

    // Zero-out value
    unsigned char *region = (unsigned char *)(ptbl_entry->m_offset + kv_index * bucket_wsz);
    memset(region, 0, bucket_wsz);

    // Mark value as freed in page_usage
    PTBL_RECORD_PAGE_USAGE_FREE(ptbl_entry, kv_index);

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
    Record_ptbl *ptbl_entry = database_ptbl_get(ctx_main, rec_database, bucket);
    if(!ptbl_entry) {
        if(!database_ptbl_alloc(ctx_main, rec_database, &ptbl_entry, 1, bucket)) {
            DEBUG_PRINT("database_value_free(): Failed call to database_ptbl_alloc()\n");
            return 0;
        }
    }
    if(!ptbl_entry->page_usage || ptbl_entry->page_usage_length != PTBL_CALC_PAGE_USAGE_LENGTH(bucket, PTBL_RECORD_GET_PAGE_COUNT(ptbl_entry[0]))) {
        DEBUG_PRINT("database_value_free(): page_usage_length does not match page count\n");
        return 0;
    }

    PTBL_RECORD_PAGE_USAGE_FREE(ptbl_entry, index);

    return 1;
}

unsigned long
_database_value_alloc(
    Context_main *ctx_main,
    Record_database *rec_database,
    Record_ptbl **ptbl_out,
    char bucket
) {
    DEBUG_PRINT("_database_value_alloc(bucket = %d)\n", bucket);

    Record_ptbl *ptbl_entry = database_ptbl_get(ctx_main, rec_database, bucket);
    if(!ptbl_entry) {
        if(!database_ptbl_alloc(ctx_main, rec_database, &ptbl_entry, 1, bucket)) {
            DEBUG_PRINT("_database_value_alloc(): Failed call to database_ptbl_alloc()\n");
            return -1;
        }
    }

    if(ptbl_out) ptbl_out[0] = ptbl_entry;

    unsigned long free_index = -1;

    unsigned int page_count = PTBL_RECORD_GET_PAGE_COUNT(ptbl_entry[0]),
        page_usage_bytes = PTBL_CALC_PAGE_USAGE_BYTES(bucket),
        page_usage_bits = PTBL_CALC_PAGE_USAGE_BITS(bucket);

    // Identify the first unused "slot" that can hold a value of the appropriate size
    for(int i = 0; i < ptbl_entry->page_usage_length && free_index == -1; i++) {
        unsigned char bits = ptbl_entry->page_usage[i];
        int max = 8;
        if(page_usage_bits < 8) {
            // Ensure that we don't try to read more bits than there are total
            if(!(max = ((page_count * page_usage_bits) % 8))) {
                max = 8;
            }
        }
        for(int j = 0; j < max && free_index == -1; j++) {
            // Mark value slot as used since we will occupy the empty slot
            ptbl_entry->page_usage[i] |= (1 << j);
            free_index = i * 8 + j;
        }
    }

    if(free_index == -1) {
        // No free slots exist in any of the pages, so we need to allocate a new page
        if(!database_ptbl_alloc(ctx_main, rec_database, &ptbl_entry, 1, bucket)) {
            return -1;
        }
        free_index = (page_count - 1) * page_usage_bytes * 8;
        ptbl_entry->page_usage[free_index / 8] |= 1;
    }

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

    Record_ptbl *ptbl_entry = 0;
    unsigned long free_index = _database_value_alloc(ctx_main, rec_database, &ptbl_entry, bucket);
    if(free_index == -1) {
        DEBUG_PRINT("\tERR failed to allocate new value in bucket %d\n", bucket);
        return -1;
    }
    if(ptbl_entry == 0) {
        DEBUG_PRINT("\tERR ptbl_entry is null\n");
        return -1;
    }

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

Record_kv *
database_kv_get(
    Context_main *ctx_main,
    Record_database *rec_database,
    unsigned long k
) {
    DEBUG_PRINT("database_kv_get(k = %d);\n", k);

    if(k >= rec_database->kv_record_count) {
        DEBUG_PRINT("\tERR k is greater than kv_record_count\n");
        return 0;
    }

    Record_kv *rec_kv = &rec_database->kv_record_tbl[k];

    if(0 == KV_RECORD_GET_SIZE(rec_kv[0])) {
        DEBUG_PRINT("\tERR size of record is 0\n");
        return 0;
    }

    return rec_kv;
}

unsigned char *
database_kv_get_value(
    Context_main *ctx_main,
    Record_database *rec_database,
    Record_ptbl **ptbl_out,
    Record_kv **kv_out,
    unsigned long k
) {
    DEBUG_PRINT("database_kv_get_value(k = %d);\n", k);

    Record_kv *rec_kv = database_kv_get(ctx_main, rec_database, k);
    if(!rec_kv) {
        DEBUG_PRINT("\tERR failed to get kv_record\n");
        return 0;
    }

    if(kv_out) kv_out[0] = rec_kv;

    int bucket = KV_RECORD_GET_BUCKET(rec_kv[0]);
    Record_ptbl *rec_ptbl = database_ptbl_get(ctx_main, rec_database, bucket);
    if(!rec_ptbl) {
        DEBUG_PRINT("\tERR rec_database is corrupt- found orphaned kv_record in non-existant bucket");
        return 0;
    }

    if(ptbl_out) ptbl_out[0] = rec_ptbl;

    return PTBL_RECORD_VALUE_PTR(rec_ptbl, rec_kv);
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

    Record_ptbl *old_ptbl = 0;
    Record_kv *kv_rec = 0;
    unsigned char *region = database_kv_get_value(ctx_main, rec_database, &old_ptbl, &kv_rec, k);
    if(!region) {
        DEBUG_PRINT("\tERR failed to get region\n");
        return 0;
    }
    if(!old_ptbl) {
        DEBUG_PRINT("\tERR failed to get ptbl_record\n");
        return 0;
    }
    if(!kv_rec) {
        DEBUG_PRINT("\tERR failed to get kv_record\n");
        return 0;
    }

    /* We allocate a new value, then swap the old value that the kv_record has with the new one */

    Record_ptbl *new_ptbl = 0;
    char bucket = database_calc_bucket(length);
    unsigned long new_index = _database_value_alloc(ctx_main, rec_database, &new_ptbl, bucket);
    if(new_index == -1) {
        DEBUG_PRINT("\tERR failed to realloc value\n");
        return 0;
    }

    // "Disable" kv_rec by setting size to 0
    KV_RECORD_SET_SIZE(kv_rec[0], 0);

    // Free the value in the bucket
    DEBUG_PRINT("\tkv_rec = %p, page_usage = %p\n", kv_rec, old_ptbl->page_usage);
    PTBL_RECORD_PAGE_USAGE_FREE(old_ptbl, KV_RECORD_GET_INDEX(kv_rec[0]));

    // Set a new bucket/index for kv_rec
    KV_RECORD_SET_BUCKET(kv_rec[0], bucket);
    KV_RECORD_SET_INDEX(kv_rec[0], new_index);

    // Copy over buffer to the new value
    unsigned char *new_region = PTBL_RECORD_VALUE_PTR(new_ptbl, kv_rec);
    memcpy(new_region, buffer, length);

    // "Enable" record by setting size to new_index value length
    KV_RECORD_SET_SIZE(kv_rec[0], length);

    return 1;
}
