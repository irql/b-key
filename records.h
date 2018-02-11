/** @file records.h
 *  @brief Data structures (and macros) that comprise the database index
 */

/** @brief Holds information relating to a bucket (\a key), including the number of pages allocated, as well as a pointer to
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
 */
typedef struct ptbl_record {
    /** @brief Holds the uppermost three bits of \a key and all bits of \a page_count
     *
     * | Range in bits | Size in bits | Description   |
     * | ------------- | -----------: | -----------   |
     * |  0 - 28       | 29           | \a page_count |
     * | 29 - 31       | 3            | \a key (high) |
     *
     * @see PTBL_RECORD_SET_PAGE_COUNT()
     * @see PTBL_RECORD_GET_PAGE_COUNT()
     * @see PTBL_RECORD_SET_KEY()
     * @see PTBL_RECORD_GET_KEY()
     */
    unsigned int key_high_and_page_count;

    /** @brief Holds the lowermost three bits of \a key and all bits of \a offset
     *
     * | Range in bits | Size in bits | Description |
     * | ------------- | -----------: | ----------- |
     * |  0 - 28       | 29           | \a offset   |
     * | 29 - 31       | 3            | \a key (low)|
     *
     * @see PTBL_RECORD_SET_OFFSET()
     * @see PTBL_RECORD_GET_OFFSET()
     * @see PTBL_RECORD_SET_KEY()
     * @see PTBL_RECORD_GET_KEY()
     */
    unsigned int key_low_and_offset;
    
    /** @brief A pointer to the start of the allocated pages in memory
     *  @see   kv_record.bucket_and_index
     */
    unsigned char *m_offset;

    unsigned int page_usage_length; ///< The length of \a page_usage in bytes

    /** @brief Bookkeeping for usage status of all values across all pages managed by this \a ptbl_record
     *  @see   PTBL_CALC_PAGE_USAGE_BYTES()
     *  @see   PTBL_CALC_PAGE_USAGE_BITS()
     *  @see   PTBL_RECORD_PAGE_USAGE_FREE()
     */
    unsigned char *page_usage;
} Record_ptbl;

/** @brief Calculate bytes used by multiple pages bookkeeping
 *
 * Computes the number of bytes it would take to represent the usage status of every value inside \a y pages of bucket \a x
 *
 * @param   x bucket
 * @param   y number of pages
 * @returns   number of bytes
 * @see       PTBL_CALC_PAGE_USAGE_BYTES()
 * @see       PTBL_CALC_PAGE_USAGE_BITS()
 * @see       ptbl_record
 */
#define PTBL_CALC_PAGE_USAGE_LENGTH(x,y) ((x <= 5) ?\
        (PTBL_CALC_PAGE_USAGE_BYTES(x) * y) :\
        (((PTBL_CALC_PAGE_USAGE_BITS(x) * y) / 8) + (((PTBL_CALC_PAGE_USAGE_BITS(x) * y) % 8) > 0 ? 1 : 0)))

/** @brief Calculate bytes used by one pages bookkeeping
 *
 * Computes the number of bytes it would take to represent the usage status of every value inside a page of bucket \a x
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
 * @param   x bucket \f$0 \leq x \leq 63\f$
 * @returns   number of bytes
 * @see       PTBL_CALC_PAGE_USAGE_BITS()
 * @see       ptbl_record
 */
#define PTBL_CALC_PAGE_USAGE_BYTES(x) ((x < 5) ? (32 >> x) : 1)

/** @brief Calculate bits used by one page's bookkeeping
 *
 * Computes the number of bits it would take to represent the usage status of every value inside a page of bucket \a x
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
 * @param   x bucket \f$0 \leq x \leq 63\f$
 * @returns number of bits
 * @see     PTBL_CALC_PAGE_USAGE_BYTES()
 * @see     ptbl_record
 */
#define PTBL_CALC_PAGE_USAGE_BITS(x) ((x < 8) ? (256 >> x) : 1)

/** @brief Calculate max value size for bucket \a x
 *
 * Computes the maximum number of bytes that a value in a page in bucket \a x can occupy
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
 * @param   x bucket \f$0 \leq x \leq 63\f$
 * @returns   max value length in bytes
 * @see       ptbl_record
 * @see       kv_record.bucket_and_index
 */
#define PTBL_CALC_BUCKET_WORD_SIZE(x) (1 << (4 + x))

#define PTBL_KEY_BITMASK (0xE0 << 24) ///< Used for selecting the uppermost three bits of a 32-bit integer
#define PTBL_KEY_HIGH_BITMASK 0x38 ///< Upper three bits
#define PTBL_KEY_LOW_BITMASK 0x7 ///< Lower three bits

/** @brief Amount to shift the bits in ptbl_record.key_high_and_page_count right by to get the uppermost three bits of \a key */
#define PTBL_KEY_HIGH_SHIFT 26

/** @brief Amount to shift the bits in ptbl_record.key_low_and_offset right by to get the lowermost three bits of \a key */
#define PTBL_KEY_LOW_SHIFT 29

/** @brief   Get \a page_count from a ptbl_record
 *  @param x ptbl_record (\b not a pointer)
 *  @see     ptbl_record.key_high_and_page_count
 */
#define PTBL_RECORD_GET_PAGE_COUNT(x) (x.key_high_and_page_count & ~PTBL_KEY_BITMASK)

/** @brief   Set \a page_count in a ptbl_record
 *  @param x ptbl_record (\b not a pointer)
 *  @param y new \a page_count (unsigned long)
 *  @see     ptbl_record.key_high_and_page_count
 */
#define PTBL_RECORD_SET_PAGE_COUNT(x,y) \
    x.key_high_and_page_count &= PTBL_KEY_BITMASK; \
    x.key_high_and_page_count |= ((unsigned int)y & ~PTBL_KEY_BITMASK);

/** @brief   Get \a offset from a ptbl_record
 *  @param x ptbl_record (\b not a pointer)
 *  @see     ptbl_record.key_low_and_offset
 */
#define PTBL_RECORD_GET_OFFSET(x) (x.key_low_and_offset & ~PTBL_KEY_BITMASK)

/** @brief   Set \a offset in a ptbl_record
 *  @param x ptbl_record (\b not a pointer)
 *  @param y new \a offset (unsigned int)
 *  @see     ptbl_record.key_low_and_offset
 */
#define PTBL_RECORD_SET_OFFSET(x,y) \
    x.key_low_and_offset &= PTBL_KEY_BITMASK; \
    x.key_low_and_offset |= ((unsigned int)y & ~PTBL_KEY_BITMASK);

/** @brief Get \a key from a ptbl_record
 *
 * Note that \a key is the bucket number.
 *
 * @param x ptbl_record (\b not a pointer)
 * @see     ptbl_record.key_high_and_page_count
 * @see     ptbl_record.key_low_and_offset
 */
#define PTBL_RECORD_GET_KEY(x) (((x.key_high_and_page_count & PTBL_KEY_BITMASK) >> PTBL_KEY_HIGH_SHIFT) | ((x.key_low_and_offset & PTBL_KEY_BITMASK) >> PTBL_KEY_LOW_SHIFT))

/** @brief Set \a key in a ptbl_record
 *
 * Note that \a key is the bucket number.
 *
 * This macro is only to be called once, upon initialization of the record.
 * Once set, the \a key (bucket) is expected to \b never change.
 *
 * @param x ptbl_record (\b not a pointer)
 * @param y bucket \f$0 \leq y \leq 63\f$
 * @see     ptbl_record.key_high_and_page_count
 * @see     ptbl_record.key_low_and_offset
 */
#define PTBL_RECORD_SET_KEY(x,y) \
    x.key_high_and_page_count &= ~PTBL_KEY_BITMASK; \
    x.key_low_and_offset &= ~PTBL_KEY_BITMASK; \
    x.key_high_and_page_count |= ((y & PTBL_KEY_HIGH_BITMASK) << PTBL_KEY_HIGH_SHIFT); \
    x.key_low_and_offset |= ((y & PTBL_KEY_LOW_BITMASK) << PTBL_KEY_LOW_SHIFT);

/** @brief Mark page number \a z as freed in ptbl_record at index \a y of database_record \a x
 *  @param x database_record (pointer)
 *  @param y Index into the ptbl_record_tbl corresponding to the page's bucket
 *  @param z page number (kv_record.bucket_and_index)
 */
#define PTBL_RECORD_PAGE_USAGE_FREE(x,y,z) \
    (x)->ptbl_record_tbl[y].page_usage[z / 8] &= ~((unsigned char)1 << (z % 8));

/** @brief Calculate the address in memory that a given kv_record value resides at
 *  @param x database_record (pointer)
 *  @param y The ptbl_record for the bucket that kv_record \a y lives within (\b is a pointer)
 *  @param z The kv_record
 */
#define PTBL_RECORD_VALUE_PTR(x,y,z) \
    (unsigned char *)(x->ptbl_record_tbl[y].m_offset + KV_RECORD_GET_INDEX(z) * PTBL_CALC_BUCKET_WORD_SIZE(KV_RECORD_GET_BUCKET(z)))

/** @brief Holds information for a key/value pair, including the bucket the value resides in, it's \a index (offset) into the
 *         bucket (ptbl_record.m_offset + \a index), in addition to the size of the value in bytes
 *
 * This record has four composed values, which are encoded using the bits of \a flags_and_size and \a bucket_and_index and require a
 * macro to either get or set them.
 *
 * The composed values are:
 * - \a flags - The data type of the KV
 * - \a size - The size of the value in bytes
 * - \a bucket - Which bucket (ptbl_record) the page that holds this value resides in
 * - \a index - Used to determine the offset into the pages of bucket that this
 *              value starts at (value = ptbl_record.m_offset + index * PTBL_CALC_BUCKET_WORD_SIZE(bucket))
 */
typedef struct kv_record {
    /** @brief Holds the bits of both \a flags and \a size
     *
     * | Range in bits | Size in bits | Description |
     * | ------------- | -----------: | ----------- |
     * |  0 - 55       | 56           | \a size     |
     * | 56 - 63       | 8            | \a flags    |
     *
     * @see KV_RECORD_GET_SIZE()
     * @see KV_RECORD_SET_SIZE()
     * @see KV_RECORD_GET_FLAGS()
     * @see KV_RECORD_SET_FLAGS()
     */
    unsigned long flags_and_size;

    /** @brief Holds the bits of both \a bucket and \a index
     *
     * \a index is used to determine the offset into the pages of \a bucket (ptbl_record in a database_record)
     * that this value starts at. This offset is used to determine the pointer to the actual kv_record
     * value data in both the in-memory database_record as well as in the serialized copy that lives on-disk.
     *
     * value = ptbl_record.m_offset + \a index * PTBL_CALC_BUCKET_WORD_SIZE(bucket)
     *
     * | Range in bits | Size in bits | Description |
     * | ------------- | -----------: | ----------- |
     * |  0 - 57       | 58           | \a index    |
     * | 58 - 63       | 6            | \a bucket   |
     *
     * @see KV_RECORD_GET_BUCKET()
     * @see KV_RECORD_SET_BUCKET()
     * @see KV_RECORD_GET_INDEX()
     * @see KV_RECORD_SET_INDEX()
     */
    unsigned long bucket_and_index;
} Record_kv;

#define KV_RECORD_BUCKET_SHIFT 58 ///< Amount to shift \a bucket_and_index right by to extract \a bucket
#define KV_RECORD_BUCKET_BITMASK ((unsigned long)0x3F << KV_RECORD_BUCKET_SHIFT) ///< To select the upper six bits

/** @brief   Get \a bucket from a kv_record pointer
 *  @param x kv_record
 *  @see     kv_record.bucket_and_index
 */
#define KV_RECORD_GET_BUCKET(x) (((x).bucket_and_index & KV_RECORD_BUCKET_BITMASK) >> KV_RECORD_BUCKET_SHIFT)

/** @brief   Set \a bucket in a kv_record
 *  @param x kv_record (\b not a pointer)
 *  @param y bucket \f$0 \leq y \leq 63\f$
 *  @see     kv_record.bucket_and_index
 */
#define KV_RECORD_SET_BUCKET(x,y) \
    x.bucket_and_index &= ~KV_RECORD_BUCKET_BITMASK; \
    x.bucket_and_index |= ((unsigned long)(y & (KV_RECORD_BUCKET_BITMASK >> KV_RECORD_BUCKET_SHIFT)) << KV_RECORD_BUCKET_SHIFT);

/** @brief   Get \a index from a kv_record pointer
 *  @param x kv_record
 *  @see     kv_record.bucket_and_index
 */
#define KV_RECORD_GET_INDEX(x) ((x).bucket_and_index & ~KV_RECORD_BUCKET_BITMASK)

/** @brief   Set \a index in a kv_record
 *  @param x kv_record (\b not a pointer)
 *  @param y index
 *  @see     kv_record.bucket_and_index
 */
#define KV_RECORD_SET_INDEX(x,y) \
    x.bucket_and_index &= KV_RECORD_BUCKET_BITMASK; \
    x.bucket_and_index |= (y & ~KV_RECORD_BUCKET_BITMASK);

#define KV_RECORD_FLAGS_SHIFT 56 ///< Amount to shift \a flags_and_size right by to extract \a flags
#define KV_RECORD_FLAGS_BITMASK ((unsigned long)0xFF << KV_RECORD_FLAGS_SHIFT) ///< To select the upper 8 bits

/** @brief     Get \a size from a kv_record
 *  @param   x kv_record (\b not a pointer)
 *  @returns   Size of the value in bytes
 *  @see       kv_record.flags_and_size
 */
#define KV_RECORD_GET_SIZE(x) (x.flags_and_size & ~KV_RECORD_FLAGS_BITMASK)

/** @brief   Set \a size in a kv_record
 *  @param x kv_record (\b not a pointer)
 *  @param y size
 *  @see     kv_record.flags_and_size
 */
#define KV_RECORD_SET_SIZE(x,y) \
    x.flags_and_size &= KV_RECORD_FLAGS_BITMASK; \
    x.flags_and_size |= (y & ~KV_RECORD_FLAGS_BITMASK);

/** @brief   Get \a flags in a kv_record
 *  @param x kv_record (\b not a pointer)
 *  @see     kv_record.flags_and_size
 */
#define KV_RECORD_GET_FLAGS(x) ((x.flags_and_size & KV_RECORD_FLAGS_BITMASK) >> KV_RECORD_FLAGS_SHIFT)

/** @brief   Set \a flags in a kv_record
 *  @param x kv_record (\b not a pointer)
 *  @param y flags
 *  @see     kv_record.flags_and_size
 */
#define KV_RECORD_SET_FLAGS(x,y) \
    x.flags_and_size &= ~KV_RECORD_FLAGS_BITMASK; \
    x.flags_and_size |= ((unsigned long)(y & (KV_RECORD_FLAGS_BITMASK >> KV_RECORD_FLAGS_SHIFT)) << KV_RECORD_FLAGS_SHIFT);

/** @brief Holds the global state of the database */
typedef struct database_record {
    unsigned long int ptbl_record_count; ///< Total number of records in \a ptbl_record_tbl
    struct ptbl_record *ptbl_record_tbl; ///< All records for this database
    unsigned long int kv_record_count; ///< Total number of records in \a kv_record_tbl
    struct kv_record *kv_record_tbl; ///< All records for this database
} Record_database;

/** @brief Helper to instantiate a new record type
 *
 * This will create a new variable, in addition to allocating space for it.
 *
 * @param x Record type
 * @param y Variable name
 */
#define RECORD_CREATE(x,y) \
    x *y = (x *)memory_alloc(sizeof(x))

/** @brief Helper to allocate a new record type
 *
 * This will only allocate \a sizeof(x) bytes, and assign it to \a y. It does not create a new variable named \a y.
 *
 * @param x Record type
 * @param y Variable name
 */
#define RECORD_ALLOC(x, y) \
    y = (x *)memory_alloc(sizeof(x))
