unsigned char *
memory_page_alloc(
    struct main_context *main_context,
    int page_count
    );

unsigned char *
memory_page_realloc(
    struct main_context *main_context,
    unsigned char *offset,
    int old_page_count,
    int page_count
    );

int
memory_page_free(
    struct main_context *main_context,
    unsigned char *region,
    int page_count
    );

unsigned char *
memory_alloc(
    int amount
    );

void
memory_free(
    void *record
    );

unsigned char *
memory_realloc(
    void *region,
    int old_amount,
    int new_amount
    );
