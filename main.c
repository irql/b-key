#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "records.h"
#include "context.h"
#include "memory.h"

int run_tests(struct main_context *);

struct main_context *main_create_context(void) {
    RECORD_CREATE(struct main_context, main_context);
    if(main_context) {
        main_context->system_page_size = sysconf(_SC_PAGE_SIZE);
        main_context->system_phys_page_count = sysconf(_SC_PHYS_PAGES);
        return main_context;
    }
    else {
        return 0;
    }
}

int main(void) {
    struct main_context *main_context = main_create_context();
    if(!run_tests(main_context))
        return 1;
    free(main_context);
    return 0;
};
