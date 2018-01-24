#include <stdio.h>

#include "debug.h"
#include "records.h"
#include "context.h"
#include "memory.h"

#ifdef DEBUG_DATABASE
    #define DEBUG_PRINT(...) \
        fprintf(stderr, __VA_ARGS__)
#else
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

void database_ptbl_init(
    Context_main *ctx_main,
    Record_ptbl *ptbl_entry,
    int page_count,
    int bucket
) {
    int bytes = PTBL_CALC_PAGE_USAGE_LENGTH(bucket);
    ptbl_entry->page_usage_length = bytes * page_count;

    DEBUG_PRINT("database_ptbl_init(): Bucket %d: Bytes %d\n", bucket, ptbl_entry->page_usage_length);

    // Leave page_usage bits zero, they will be set/unset
    // upon the storage or deletion of individual k/v pairs
    ptbl_entry->page_usage = memory_alloc(sizeof(unsigned char) * ptbl_entry->page_usage_length);

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
unsigned char *database_pages_alloc(
    Context_main *ctx_main,
    Record_database *rec_database,
    int page_count,
    int bucket
) {
    if(!rec_database) {
        return 0;
    }

    if(rec_database->ptbl_record_tbl) {
        // Database ptbl_record_tbl exists

        Record_ptbl *ptbl = database_ptbl_search(ctx_main, rec_database, bucket);

        if(ptbl) {
            if(!ptbl->m_offset) {
                DEBUG_PRINT("Ptbl record, bucket %d, corrupt (m_offset is null)\n", bucket);
                return 0;
            }

            DEBUG_PRINT("\ndatabase_pages_alloc(.., %d, %d);\n", page_count, bucket);
            DEBUG_PRINT("\tpage_usage_length=%d\n", ptbl->page_usage_length);
            // TODO: MUST CHECK page_count REQUESTED AND NOT JUST RETURN THE FIRST PAGE
            unsigned char *offset = 0;
            int i = 0,
                free_pages = 0,
                bits = (256 >> bucket),
                bytes = PTBL_CALC_PAGE_USAGE_LENGTH(bucket),
                last_free_page = 0;

            for(; i < ptbl->page_usage_length / bytes; i++) {
                int j = 0, free = 0;

                if(bits >= 64) {
                    unsigned long *subset = (unsigned long *)(&ptbl->page_usage[i * bytes]);
                    for(; j < bytes / 8; j++) {
                        DEBUG_PRINT("%d(%d)[%d]: %016lx\n", i, j, (i * bytes + ((j + 1) * 8)), subset[j]);
                        if(subset[j] == 0) {
                            free++;
                        }
                    }
                }
                // little locks and keys :)
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
                else {
                    DEBUG_PRINT("Bits %d\n", bits);
                    // TODO: Bitwise needed for bucket >= 6
                    // Bucket 6 (1024-byte values) = 4 bits per page
                    // Bucket 7 (2048-byte values) = 2 bits per page
                    // Bucket >= 8 (>= 4096-byte values) = 1 bit per page
                }

                if(free > 0 && j > 0 && free == j) {
                    free_pages++;
                    last_free_page = i;
                }
                else {
                    free_pages = last_free_page = 0;
                }

                if(free_pages == page_count) {
                    //offset = ptbl->m_offset + (i - (page_count - 1)) * ctx_main->system_page_size;
                    offset = ptbl->m_offset + ((i - (page_count - 1)) << 12);
                    last_free_page = 0;
                    DEBUG_PRINT("Offset decided = %p (%dB, page bucket starts %p)\n", offset, offset - ptbl->m_offset, ptbl->m_offset);
                    break;
                }
            }

            // TODO: Reclaim empty pages at the very end of alloc'd region
            if(!offset || last_free_page) {
                // Realloc (add) more pages
                int new_page_count = PTBL_RECORD_GET_PAGE_COUNT(ptbl[0]) + page_count - (last_free_page ? free_pages : 0);
                offset = memory_page_realloc(
                        ctx_main,
                        ptbl->m_offset,
                        PTBL_RECORD_GET_PAGE_COUNT(ptbl[0]),
                        new_page_count
                        );
                if(!offset) {
                    return 0;
                }

                unsigned int new_page_usage_length = PTBL_CALC_PAGE_USAGE_LENGTH(bucket) * new_page_count;
                ptbl->page_usage = memory_realloc(ptbl->page_usage, sizeof(unsigned char) * ptbl->page_usage_length, sizeof(unsigned char) * new_page_usage_length);
                ptbl->page_usage_length = new_page_usage_length;
                DEBUG_PRINT("\tIncreased size of page_usage: %d\n", ptbl->page_usage_length);
                if(!ptbl->page_usage) {
                    return 0;
                }

                ptbl->m_offset = offset;
                PTBL_RECORD_SET_PAGE_COUNT(ptbl[0], new_page_count);

                // This needs to be done AFTER setting ptbl->m_offset to the right page base
                if(last_free_page) {
                    offset += (new_page_count - page_count) * ctx_main->system_page_size;
                }
            }

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
                DEBUG_PRINT("database_alloc_pages(..%d..): Failed to realloc database->ptbl_record_tbl\n", bucket);
                return 0;
            }
            rec_database->ptbl_record_tbl = new_ptbl;

            Record_ptbl *ptbl_entry = &rec_database->ptbl_record_tbl[rec_database->ptbl_record_count - 1];

            database_ptbl_init(ctx_main, ptbl_entry, page_count, bucket);
            return ptbl_entry->m_offset;
        }
    }
    else {
        // Initialize the first database ptbl_record_tbl
        RECORD_ALLOC(Record_ptbl, rec_database->ptbl_record_tbl);
        database_ptbl_init(ctx_main, &rec_database->ptbl_record_tbl[0], page_count, bucket);
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
            }
        }
        memory_free(rec_database->ptbl_record_tbl);
        rec_database->ptbl_record_count = 0;
    }
}

int database_alloc_kv(
    Context_main *ctx_main,
    Record_database *rec_database,
    int data_type,
    unsigned long size,
    unsigned long *buffer
) {
    // Allocate based on page-table mappings
    // If no page table exists for records of
    // a given size, create one.
}
