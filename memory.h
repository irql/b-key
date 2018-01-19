unsigned char
*memory_alloc(
    struct main_context *main_context,
    int page_count
    );

int
memory_free(
    struct main_context *main_context,
    unsigned char *region,
    int page_count
);
