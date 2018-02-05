/** @file memory.h
 *  @brief Methods and wrappers for allocating or freeing system memory
 */

/** @brief Allocate a number of system pages using mmap()
 *
 * If \a page_count is <= 0, this method will fail.
 *
 * The size of the allocated region, in bytes, will be main_context.system_page_size * \a page_count
 *
 * @returns A pointer to the allocted region on success, or 0 on failure
 */
unsigned char *
memory_page_alloc(
    struct main_context *main_context, ///<[in] The main context
    int page_count                     ///<[in] The number of pages to allocate
    );

/** @brief Reallocate a region allocated by memory_page_alloc()
 *
 * Uses mremap() on non-BSD/Apple systems, otherwise just munmap() and mmap().
 *
 * \b NOTE: Expect the re-allocated region to start at a different address in memory than \a offset
 *
 * @returns A pointer to the re-allocated region on success, or 0 on failure
 */
unsigned char *
memory_page_realloc(
    struct main_context *main_context, ///<[in] The main context
    unsigned char *offset,             ///<[in] A pointer to the start of the region to reallocate
    int old_page_count,                ///<[in] The old page count
    int page_count                     ///<[in] The new page count
    );

/** @brief Free a region allocated by memory_page_alloc()
 *
 * Uses munmap()
 *
 * @returns 1 on success, or 0 on failure
 */
int
memory_page_free(
    struct main_context *main_context, ///<[in] The main context
    unsigned char *region,             ///<[in] A pointer to the start of the region to free
    int page_count                     ///<[in] The number of pages that were allocated to the region
    );

/** @brief   Allocate using stdlib
 *  @returns A pointer to the region on success, or 0 on failure
 */
unsigned char *
memory_alloc(
    int amount ///<[in] The amount to allocate in bytes
    );

/** @brief Free a region allocated using memory_alloc() */
void
memory_free(
    void *record ///<[in] A pointer to the region that was allocated using memory_alloc()
    );

/** @brief Reallocate a region allocated using memory_alloc()
 *
 * If \a new_amount is greater than \a old_amount, this method will initialize the bytes that are
 * added to 0 by using memset().
 *
 * @returns A pointer to the re-allocated region on success, or 0 on failure
 */
unsigned char *
memory_realloc(
    void *region,   ///<[in] A pointer to the region that was allocated using memory_alloc()
    int old_amount, ///<[in] The current size of the region in bytes
    int new_amount  ///<[in] The size that the region should change to in bytes
    );
