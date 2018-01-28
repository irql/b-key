Record_ptbl *
database_ptbl_search(
    Context_main *ctx_main,
    Record_database *rec_database,
    int bucket
    );

unsigned char *database_pages_alloc(
    Context_main *ctx_main,
    Record_database *rec_database,
    int page_count,
    int bucket
    );

void
database_pages_free(
    Context_main *ctx_main,
    Record_database *rec_database
    );


int database_alloc_kv(
    Context_main *ctx_main,
    Record_database *rec_database,
    int data_type,
    unsigned long size,
    unsigned long *buffer
    );

int database_ptbl_calc_page_usage_length(
    int bucket,
    int page_count
    );

int database_calc_bucket(
    unsigned long length
    );
