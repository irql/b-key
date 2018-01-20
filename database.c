#include <stdio.h>

#include "records.h"
#include "context.h"
#include "memory.h"

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
    // 32 bytes (4w) for bucket #0 (16 byte values) (4096 / 16 = 256bits. 256/8 = 32 bytes)
    // 16 bytes (2w) for bucket #1 (32 byte vlaues)
    int words = (4 >> bucket) > 0 ? (4 >> bucket) : 1;
    ptbl_entry->page_usage = (unsigned long *)memory_alloc(sizeof(unsigned long) * words);
    ptbl_entry->page_usage_length = words;

    PTBL_RECORD_SET_KEY(ptbl_entry[0], bucket);

    ptbl_entry->m_offset = memory_page_alloc(ctx_main, page_count);
    PTBL_RECORD_SET_PAGE_COUNT(ptbl_entry[0], page_count);
}

// x = bucket
// max_value_len_inside_page = 2^(4+x)
// i.e., bucket #0 is for <=16-byte values
//       bucket #1 is for <=32-byte values
//       bucket #2 is for <=64-byte values
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
    if(rec_database->ptbl_record_tbl) {
        Record_ptbl *ptbl = database_ptbl_search(ctx_main, rec_database, bucket);
        if(ptbl) {
            // Try to find free contiguous pages matching page_count requirements
            // Realloc (add) more pages if necessary
            return -1;
        }
        else {
            // Create new ptbl record for bucket
            Record_ptbl *new_ptbl = (Record_ptbl *)memory_realloc(
                    rec_database->ptbl_record_tbl,
                    (++rec_database->ptbl_record_count * sizeof(Record_ptbl))
                    );
            if(!new_ptbl) {
                fprintf(stderr, "database_alloc_pages(..%d..): Failed to realloc database->ptbl_record_tbl\n", bucket);
                return 0;
            }
            rec_database->ptbl_record_tbl = new_ptbl;

            Record_ptbl *ptbl_entry = &rec_database->ptbl_record_tbl[rec_database->ptbl_record_count - 1];

            database_ptbl_init(ctx_main, ptbl_entry, page_count, bucket);
            return ptbl_entry->m_offset;
        }
    }
    else {
        // Initialize the database ptbl_record_tbl
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
