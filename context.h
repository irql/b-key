/** @file context.h
 *  @brief Data structures that provide contexts to methods
 */

/** @brief The main (system) context */
typedef struct main_context {
    unsigned long system_page_size;       ///< The result of a call made to sysconf(_SC_PAGE_SIZE)
    unsigned long system_phys_page_count; ///< The result of a call made to sysconf(_SC_PHYS_PAGES)
} Context_main;
