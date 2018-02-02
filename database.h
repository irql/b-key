Record_ptbl *
database_ptbl_search(
    Context_main *ctx_main,
    Record_database *rec_database,
    int bucket
    );

unsigned char *
database_pages_alloc(
    Context_main *ctx_main,
    Record_database *rec_database,
    Record_ptbl **rec_ptbl,
    int page_count,
    int bucket
    );

void
database_pages_free(
    Context_main *ctx_main,
    Record_database *rec_database
    );


int
database_kv_free(
    Context_main *ctx_main,
    Record_database *rec_database,
    unsigned long k
    );

unsigned long
database_alloc_kv(
    Context_main *ctx_main,
    Record_database *rec_database,
    char data_type,
    unsigned long size,
    unsigned char *buffer
    );

int database_ptbl_calc_page_usage_length(
    int bucket,
    int page_count
    );

int database_calc_bucket(
    unsigned long length
    );
