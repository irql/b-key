/** @file database.h
 *  @brief Method definitions for working with the database and associated records
 */

/** @brief Returns the ptbl_record given a corresponding bucket, or 0 if no such record exists yet.
 *  @returns Pointer to a ptbl_record on success, 0 on failure
 */
Record_ptbl *
database_ptbl_search(
    Context_main *ctx_main,        ///< [in] The main context
    Record_database *rec_database, ///< [in] The database record
    int bucket                     ///< [in] Bucket number
    );

/** @brief Returns a pointer to the newly allocated region, of size main_context.system_page_size *
 *         \a bucket, on success, or 0 on failure.
 * 
 * Optionally, a ptbl_record (\a rec_ptbl) pointer to a *pointer* can be specified, which a pointer to the
 * ptbl_record for the corresponding \a bucket of the database_record can be written.
 *
 * @returns Pointer to a region of allocated memory on success, 0 on failure.
 */
unsigned char *
database_pages_alloc(
    Context_main *ctx_main,        ///<[in]  The main context
    Record_database *rec_database, ///<[in]  The database record
    Record_ptbl **rec_ptbl,        ///<[out] The ptbl_record in \a rec_database for the corresponding \a bucket
    int page_count,                ///<[in]  The number of pages to allocate
    int bucket                     ///<[in]  The bucket to allocate in
    );

/** @brief Frees all the structures nested within \a rec_database and it's sub-structures */
void
database_pages_free(
    Context_main *ctx_main,       //<[in] The main context
    Record_database *rec_database //<[in] The database record
    );


/** @brief Frees a single key \a k in \a rec_database
 *  @returns 1 on success, 0 on failure
 */
int
database_kv_free(
    Context_main *ctx_main,
    Record_database *rec_database,
    unsigned long k
    );

/** @brief returns the key of a newly allocated record in rec_database database_record.kv_record_tbl 
 *         that has been initialized with \a size bytes from \a buffer on success, or 0 on failure.
 *  @returns The key of a new record in rec_database.kv_record_tbl on success, or 0 on failure.
 */
unsigned long
database_kv_alloc(
    Context_main *ctx_main,        ///<[in] The main context
    Record_database *rec_database, ///<[in] The database record

    unsigned char flags,           ///<[in] Flags to set kv_record.flags_and_size
                                   ///<     @see KV_RECORD_GET_FLAGS()
                                   ///<     @see KV_RECORD_SET_FLAGS()

    unsigned long size,            ///<[in] The number of bytes to read from buffer, in addition to the
                                   ///<     size to set in kv_record.flags_and_size
                                   ///<     @see KV_RECORD_GET_SIZE()
                                   ///<     @see KV_RECORD_SET_SIZE()

    unsigned char *buffer          ///<[in] The buffer to read \a size bytes into the newly allocated
                                   ///<     value in database_record.kv_record_tbl from
    );

/** @brief Given the \a length of a value in bytes, returns the corresponding bucket for that value
 *  @returns A bucket that contains values of an equivalent size
 */
int database_calc_bucket(
    unsigned long length ///<[in] The length of a value
    );
