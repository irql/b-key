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
database_ptbl_search(
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

int
database_ptbl_calc_page_usage_length(
    int bucket,
    int page_count
) {
    unsigned int length = 0;

    if(bucket < 6) {
        length = ((bucket < 5) ? (32 >> bucket) : 1) * page_count;
    }
    else {
        int bits = (bucket < 8) ? (256 >> bucket) : 1;
        length = ((page_count * bits) / 8) + (((page_count * bits) % 8) > 0 ? 1 : 0);
    }

    return length;
}

void database_ptbl_init(
    Context_main *ctx_main,
    Record_ptbl *ptbl_entry,
    int page_count,
    int bucket
) {
    ptbl_entry->page_usage_length =
        database_ptbl_calc_page_usage_length(bucket, page_count);

    // Leave page_usage bits zero, they will be set/unset
    // upon the storage or deletion of individual k/v pairs
    ptbl_entry->page_usage =
        memory_alloc(sizeof(unsigned char) * ptbl_entry->page_usage_length);

    PTBL_RECORD_SET_KEY(ptbl_entry[0], bucket);

    ptbl_entry->m_offset = memory_page_alloc(ctx_main, page_count);
    PTBL_RECORD_SET_PAGE_COUNT(ptbl_entry[0], page_count);
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
database_pages_alloc(
    Context_main *ctx_main,
    Record_database *rec_database,
    Record_ptbl **rec_ptbl,
    int page_count,
    int bucket
) {
    if(!rec_database) {
        return 0;
    }

    if(rec_database->ptbl_record_tbl) {
        // Database ptbl_record_tbl exists

        Record_ptbl *ptbl =
            database_ptbl_search(ctx_main, rec_database, bucket);

        if(ptbl) {
            if(!ptbl->m_offset) {
                DEBUG_PRINT("Ptbl record, bucket %d, corrupt (m_offset is null)\n", bucket);
                return 0;
            }

            DEBUG_PRINT("\ndatabase_pages_alloc(.., %d, %d);\n", page_count, bucket);
            DEBUG_PRINT("\tpage_usage_length=%d\n", ptbl->page_usage_length);
            DEBUG_PRINT("\tpage_count=%d\n", PTBL_RECORD_GET_PAGE_COUNT(ptbl[0]));

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
                        DEBUG_PRINT("%d(%d)[%d]: %016lx\n", i, j, (i * bytes + ((j + 1) * 8)), subset[j]);
                        if(subset[j] == 0) {
                            free++;
                        }
                    }
                }
                else if(bits == 32) {
                    unsigned usage = ((unsigned *)ptbl->page_usage)[i];
                    DEBUG_PRINT("%d: %08x\n", i, usage);
                    if(usage == 0) {
                        free = j = 1;
                    }
                }
                else if(bits == 16) {
                    unsigned short usage = ((unsigned short *)ptbl->page_usage)[i];
                    DEBUG_PRINT("%d: %x\n", i, usage);
                    if(usage == 0) {
                        free = j = 1;
                    }
                }
                else if(bits == 8) {
                    DEBUG_PRINT("%d: %x\n", i, ptbl->page_usage[i]);
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
                        DEBUG_PRINT("%d: %d\n", (i * (8 / bits)) + k, usage);
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
                    DEBUG_PRINT("Offset decided = %p (%ldB, page bucket starts %p)\n", offset, offset - ptbl->m_offset, ptbl->m_offset);
                    break;
                }
            }

            if(!offset || last_free_page != -1) {
                // Realloc (add) more pages
                // TODO: Decide if we want to support buckets > 8 (i.e. bucket x after 8 holds 4096 * (1 << (x - 8)) )
                int new_page_count = PTBL_RECORD_GET_PAGE_COUNT(ptbl[0]) + page_count - free_pages;

                offset = memory_page_realloc(
                        ctx_main,
                        ptbl->m_offset,
                        PTBL_RECORD_GET_PAGE_COUNT(ptbl[0]),
                        new_page_count * ((bucket <= 8) ? 1 : (1 << (bucket - 8)))
                        );

                if(!offset) {
                    return 0;
                }

                unsigned int new_page_usage_length = database_ptbl_calc_page_usage_length(bucket, new_page_count);

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
        else {
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
                DEBUG_PRINT("database_alloc_pages(bucket = %d): Failed to realloc database->ptbl_record_tbl\n", bucket);
                return 0;
            }
            rec_database->ptbl_record_tbl = new_ptbl;

            Record_ptbl *ptbl_entry = &rec_database->ptbl_record_tbl[rec_database->ptbl_record_count - 1];

            database_ptbl_init(ctx_main, ptbl_entry, page_count, bucket);

            if(rec_ptbl) rec_ptbl[0] = ptbl_entry;
            return ptbl_entry->m_offset;
        }
    }
    else {
        // Initialize the first database ptbl_record_tbl
        RECORD_ALLOC(Record_ptbl, rec_database->ptbl_record_tbl);
        database_ptbl_init(ctx_main, &rec_database->ptbl_record_tbl[0], page_count, bucket);
        if(rec_ptbl) rec_ptbl[0] = rec_database->ptbl_record_tbl;
        rec_database->ptbl_record_count = 1;
        return rec_database->ptbl_record_tbl[0].m_offset;
    }
}

void database_pages_free(
    Context_main *ctx_main,
    Record_database *rec_database
) {
    if(rec_database->ptbl_record_tbl) {
        int i = 0;
        for(; i < rec_database->ptbl_record_count; i++) {
            if(rec_database->ptbl_record_tbl[i].page_usage) {
                memory_free(rec_database->ptbl_record_tbl[i].page_usage);
                memory_page_free(
                        ctx_main,
                        rec_database->ptbl_record_tbl[i].m_offset,
                        PTBL_RECORD_GET_PAGE_COUNT(rec_database->ptbl_record_tbl[i])
                        );
            }
        }
        memory_free(rec_database->ptbl_record_tbl);
        rec_database->ptbl_record_count = 0;
        rec_database->ptbl_record_tbl = 0;
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

unsigned long
database_alloc_kv(
    Context_main *ctx_main,
    Record_database *rec_database,
    char data_type,
    unsigned long size,
    unsigned char *buffer
) {
    DEBUG_PRINT("database_alloc_kv(data_type = %d, size = %d, buffer = %p);\n", data_type, size, buffer);

    // Allocate based on page-table mappings
    // If no page table exists for records of
    // a given size, create one.

    unsigned char bucket = database_calc_bucket(size);
    DEBUG_PRINT("\tbucket = %d\n", bucket);

    Record_ptbl *ptbl_entry = database_ptbl_search(ctx_main, rec_database, bucket);

    if(!ptbl_entry) {
        if(!database_pages_alloc(ctx_main, rec_database, &ptbl_entry, 1, bucket)) {
            DEBUG_PRINT("database_alloc_kv(): Failed call to database_pages_alloc()\n");
            return -1;
        }
    }

    if(!ptbl_entry->page_usage || ptbl_entry->page_usage_length != database_ptbl_calc_page_usage_length(bucket, PTBL_RECORD_GET_PAGE_COUNT(ptbl_entry[0]))) {
        DEBUG_PRINT("database_alloc_kv(): page_usage_length does not match page count\n");
        return -1;
    }

    // Identify the first unused "slot" that can hold a value of the appropriate size
    int free_index = -1;
    for(int i = 0; i < PTBL_RECORD_GET_PAGE_COUNT(ptbl_entry[0]) && free_index == -1; i++) {

        //DEBUG_PRINT("%d:\t", i);

        for(int j = 0; j < PTBL_CALC_PAGE_USAGE_BYTES(bucket) && free_index == -1; j++) {
            int index = (i * PTBL_CALC_PAGE_USAGE_BYTES(bucket)) + j; // page-level granularity
            unsigned char bits = ptbl_entry->page_usage[index];

            //DEBUG_PRINT("%02x ", bits);

            for(int k = 0; k < 8 && free_index == -1; k++) {
                if( !(bits & (1 << k)) ) {
                    // Mark value slot as used since we will occupy the empty slot
                    ptbl_entry->page_usage[index] |= (1 << k);
                    free_index = (index * 8) + k; // value-level granularity //(index * 8 * (1 << (4 + bucket))) + (k * (1 << (4 + bucket)));
                }
            }
        }

        //DEBUG_PRINT("\n");
    }

    if(free_index == -1) {
        // No free slots exist in any of the pages, so we need to allocate a new page
        if(!database_pages_alloc(ctx_main, rec_database, &ptbl_entry, 1, bucket)) {
            return 0;
        }
        free_index = 0;
    }

    unsigned long value_offset = free_index * (1 << (4 + bucket));

    DEBUG_PRINT("database_alloc_kv() %d + %p = %p\n", free_index, ptbl_entry->m_offset, ptbl_entry->m_offset + value_offset);

    // We need to find a free spot in the kv_record table and occupy it
    unsigned long free_kv = 0;
    if(!rec_database->kv_record_tbl) {
        rec_database->kv_record_tbl = (Record_kv *)memory_alloc(sizeof(Record_kv));
        rec_database->kv_record_count = 0;
    }
    else {
        for(int i = 0; i < rec_database->kv_record_count; i++) {
        }
    }

    Record_kv *kv_rec = &rec_database->kv_record_tbl[free_kv];

    KV_RECORD_SET_FLAGS(kv_rec[0], 0);
    KV_RECORD_SET_SIZE(kv_rec[0], size);
    KV_RECORD_SET_BUCKET(kv_rec[0], bucket);
    KV_RECORD_SET_INDEX(kv_rec[0], free_index);

    memcpy((unsigned char *)(ptbl_entry->m_offset + value_offset), buffer, size);

    return 1;
}
