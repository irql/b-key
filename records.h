/** @file records.h
 *  @brief Data structures (and macros) that comprise the database index
 */

/** @brief Holds information relating to a bucket (\a key), including
 *         the number of pages allocated, as well as a pointer to
 *         those pages in memory.
 *
 * This record has three composed values, which are encoded using the
 * bits of \a key_high_and_page_count and \a key_low_and_offset and
 * require a macro to either get or set them.
 *
 * The composed values are:
 * - \a key - bucket
 * - \a page_count - Number of pages currently allocated
 * - \a offset - (\b disk-only) An offset to the start of the page region
 *
 */
typedef struct ptbl_record {
    /** @brief Holds the uppermost three bits of \a key and all
     *         bits of \a page_count
     *
     * | Range in bits | Size in bits | Description |
     * | ------------- | -----------: | ----------- |
     * | 0 - 2         | 3            | \a key (high)  |
     * | 3 - 31        | 31           | \a page_count  |
     *
     * @see PTBL_RECORD_SET_PAGE_COUNT()
     * @see PTBL_RECORD_GET_PAGE_COUNT()
     * @see PTBL_RECORD_SET_KEY()
     * @see PTBL_RECORD_GET_KEY()
     */
    unsigned int key_high_and_page_count;

    /** @brief Holds the lowermost three bits of \a key and all
     *         bits of \a offset
     *
     * | Range in bits | Size in bits | Description |
     * | ------------- | -----------: | ----------- |
     * | 0 - 2         | 3            | \a key (low )  |
     * | 3 - 31        | 31           | \a offset      |
     *
     * @see PTBL_RECORD_SET_OFFSET()
     * @see PTBL_RECORD_GET_OFFSET()
     * @see PTBL_RECORD_SET_KEY()
     * @see PTBL_RECORD_GET_KEY()
     */
    unsigned int key_low_and_offset;
    
    unsigned char *m_offset; ///< A pointer to the start of the allocated pages in memory

    unsigned int page_usage_length; ///< The length of \a page_usage in bytes

    /** @brief Bookkeeping for usage status of all values across
     *         all pages managed by this \a ptbl_record
     *
     * @see PTBL_CALC_PAGE_USAGE_BYTES()
     * @see PTBL_CALC_PAGE_USAGE_BITS()
     *
     */
    unsigned char *page_usage;
} Record_ptbl;

/** @brief Calculate bytes used by one pages bookkeeping
 *
 * Computes the number of bytes it would take to represent the
 * usage status of every value inside a page of bucket \a x
 *
 * | Bucket | bytes |
 * | -----: | ----: |
 * | 0      | 32    |
 * | 1      | 16    |
 * | 2      | 8     |
 * | \a x   | (32 >> \a x)|
 * | 5      | 1     |
 * | 6      | ^     |
 * | \a x   | ^     |
 *
 * @param x bucket \f$0 \leq x \leq 63\f$
 * @returns number of bytes
 * @see PTBL_CALC_PAGE_USAGE_BITS()
 * @see ptbl_record
 *
 */
#define PTBL_CALC_PAGE_USAGE_BYTES(x) ((x < 5) ? (32 >> x) : 1)

/** @brief Calculate bits used by one page's bookkeeping
 *
 * Computes the number of bits it would take to represent the
 * usage status of every value inside a page of bucket * \a x
 *
 * | Bucket | bits |
 * | -----: | ----: |
 * | 0      | 256   |
 * | 1      | 128   |
 * | 2      | 64    |
 * | \a x   | (256 >> \a x)|
 * | 8      | 1     |
 * | 9      | ^     |
 * | \a x   | ^     |
 *
 * @param x bucket \f$0 \leq x \leq 63\f$
 * @returns number of bits
 * @see PTBL_CALC_PAGE_USAGE_BYTES()
 * @see ptbl_record
 *
 */
#define PTBL_CALC_PAGE_USAGE_BITS(x) ((x < 8) ? (256 >> x) : 1)

/** @brief Calculate max value size for bucket \a x
 *
 * Computes the maximum number of bytes that a value in
 * a page in bucket \a x can occupy
 *
 * | Bucket | word size |
 * | -----: | --------: |
 * | 0      | 16B       |
 * | 1      | 32B       |
 * | 2      | 64B       |
 * | 3      | 128B      |
 * | \a x   | (1 << (4 + \a x)) |
 * | 63     | 147EB     |
 *
 * @param x bucket \f$0 \leq x \leq 63\f$
 * @returns max value length in bytes
 * @see ptbl_record
 *
 */
#define PTBL_CALC_BUCKET_WORD_SIZE(x) (1 << (4 + x))

#define PTBL_KEY_BITMASK (0xE0 << 24)
#define PTBL_KEY_HIGH_BITMASK 0x38
#define PTBL_KEY_LOW_BITMASK 0x7
#define PTBL_KEY_HIGH_SHIFT 26
#define PTBL_KEY_LOW_SHIFT 29

// Helpers to extract the page_count and offset sans bits from key

#define PTBL_RECORD_GET_PAGE_COUNT(x) (x.key_high_and_page_count & ~PTBL_KEY_BITMASK)
#define PTBL_RECORD_SET_PAGE_COUNT(x,y) \
    x.key_high_and_page_count &= PTBL_KEY_BITMASK; \
    x.key_high_and_page_count |= ((unsigned long)y & ~PTBL_KEY_BITMASK);

#define PTBL_RECORD_GET_OFFSET(x) (x.key_high_and_page_count & ~PTBL_KEY_BITMASK)
#define PTBL_RECORD_SET_OFFSET(x,y) \
    x.key_low_and_offset &= PTBL_KEY_BITMASK; \
    x.key_low_and_offset |= ((unsigned long)y & ~PTBL_KEY_BITMASK);

// The 6-bit key is sharded out to the upper three bits of the page_count and offset fields
#define PTBL_RECORD_GET_KEY(x) (((x.key_high_and_page_count & PTBL_KEY_BITMASK) >> PTBL_KEY_HIGH_SHIFT) | ((x.key_low_and_offset & PTBL_KEY_BITMASK) >> PTBL_KEY_LOW_SHIFT))

// Only to be called once, upon initialization of the record.
// Once set, the key is expected to _NEVER_ change.
#define PTBL_RECORD_SET_KEY(x,y) \
    x.key_high_and_page_count &= ~PTBL_KEY_BITMASK; \
    x.key_low_and_offset &= ~PTBL_KEY_BITMASK; \
    x.key_high_and_page_count |= ((y & PTBL_KEY_HIGH_BITMASK) << PTBL_KEY_HIGH_SHIFT); \
    x.key_low_and_offset |= ((y & PTBL_KEY_LOW_BITMASK) << PTBL_KEY_LOW_SHIFT);

// K/V Record
typedef struct kv_record {
    /*
     * 0 -  7 ( 8b): flags
     * 8 - 63 (56b): size
     */
    unsigned long flags_and_size;

    /*
     * 0 -  5 ( 6b): bucket
     * 6 - 63 (58b): index (page base + index * max bucket value length)
     */
    unsigned long bucket_and_index;
} Record_kv;

#define KV_RECORD_BUCKET_SHIFT 58
#define KV_RECORD_BUCKET_BITMASK ((unsigned long)0x3F << KV_RECORD_BUCKET_SHIFT)

#define KV_RECORD_GET_BUCKET(x) ((x.bucket_and_index & KV_RECORD_BUCKET_BITMASK) >> KV_RECORD_BUCKET_SHIFT)
#define KV_RECORD_SET_BUCKET(x,y) \
    x.bucket_and_index &= ~KV_RECORD_BUCKET_BITMASK; \
    x.bucket_and_index |= ((unsigned long)(y & (KV_RECORD_BUCKET_BITMASK >> KV_RECORD_BUCKET_SHIFT)) << KV_RECORD_BUCKET_SHIFT);

#define KV_RECORD_GET_INDEX(x) (x.bucket_and_index & ~KV_RECORD_BUCKET_BITMASK)
#define KV_RECORD_SET_INDEX(x,y) \
    x.bucket_and_index &= KV_RECORD_BUCKET_BITMASK; \
    x.bucket_and_index |= (y & ~KV_RECORD_BUCKET_BITMASK);

#define KV_RECORD_FLAGS_SHIFT 56
#define KV_RECORD_FLAGS_BITMASK ((unsigned long)0xFF << KV_RECORD_FLAGS_SHIFT)

#define KV_RECORD_GET_SIZE(x) (x.flags_and_size & ~KV_RECORD_FLAGS_BITMASK)
#define KV_RECORD_SET_SIZE(x,y) \
    x.flags_and_size &= KV_RECORD_FLAGS_BITMASK; \
    x.flags_and_size |= (y & ~KV_RECORD_FLAGS_BITMASK);

#define KV_RECORD_GET_FLAGS(x) ((x.flags_and_size & KV_RECORD_FLAGS_BITMASK) >> KV_RECORD_FLAGS_SHIFT)
#define KV_RECORD_SET_FLAGS(x,y) \
    x.flags_and_size &= ~KV_RECORD_FLAGS_BITMASK; \
    x.flags_and_size |= ((unsigned long)(y & (KV_RECORD_FLAGS_BITMASK >> KV_RECORD_FLAGS_SHIFT)) << KV_RECORD_FLAGS_SHIFT);

// Master Record
typedef struct database_record {
    // blocks of 4096 allocated
    unsigned long int page_count;
    unsigned long int kv_record_count;
    unsigned long int ptbl_record_count;
    struct ptbl_record *ptbl_record_tbl;
    struct kv_record *kv_record_tbl;
} Record_database;

#define RECORD_CREATE(x,y) \
    x *y = (x *)memory_alloc(sizeof(x));

#define RECORD_ALLOC(x, y) \
    y = (x *)memory_alloc(sizeof(x));
