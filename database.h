/** @file  database.h
 *  @brief Method definitions for working with the database and associated records
 */

/** @brief   Returns the ptbl_record for the corresponding \a bucket, or 0 if no such record exists yet.
 *  @returns Pointer to a ptbl_record on success, 0 on failure
 *  @see     ptbl_record
 */
Record_ptbl *
database_ptbl_get(
    Context_main *ctx_main,        ///< [in] main context
    Record_database *rec_database, ///< [in] database record
    int bucket                     ///< [in] Bucket number
    );

/** @brief Returns a pointer to the newly allocated region, of size main_context.system_page_size *
 *         \a bucket, on success, or 0 on failure.
 * 
 * Optionally, a ptbl_record (\a rec_ptbl) pointer to a *pointer* can be specified, to which a pointer to the
 * ptbl_record for the corresponding \a bucket of the database_record will be written.
 *
 * @returns Pointer to a region of allocated memory on success, 0 on failure.
 * @see     database_ptbl_free()
 * @see     ptbl_record
 */
unsigned char *
database_ptbl_alloc(
    Context_main *ctx_main,        ///<[in]  main context
    Record_database *rec_database, ///<[in]  database record
    Record_ptbl **rec_ptbl,        ///<[out] Where a pointer to the found ptbl_record should be written
    int page_count,                ///<[in]  number of pages to allocate
    int bucket                     ///<[in]  bucket to allocate in
    );

/** @brief Frees all the structures nested within \a rec_database and it's sub-structures
 *  @see   database_ptbl_alloc()
 *  @see   ptbl_record
 */
void
database_ptbl_free(
    Context_main *ctx_main,       ///<[in] main context
    Record_database *rec_database ///<[in] database record
    );


/** @brief   Frees a single key \a k in \a rec_database
 *  @returns 1 on success, 0 on failure
 *  @see     database_kv_alloc()
 *  @see     kv_record
 */
int
database_kv_free(
    Context_main *ctx_main,        ///<[in] main context
    Record_database *rec_database, ///<[in] database record
    unsigned long k                ///<[in] key of the kv_record to free
    );

/** @brief   returns the key of a newly allocated record in rec_database database_record.kv_record_tbl 
 *           that has been initialized with \a size bytes from \a buffer on success, or 0 on failure.
 *  @returns The key of a new record in rec_database.kv_record_tbl on success, or -1 on failure.
 *  @see     database_kv_free()
 *  @see     kv_record.bucket_and_index
 */
unsigned long
database_kv_alloc(
    Context_main *ctx_main,        ///<[in] main context
    Record_database *rec_database, ///<[in] database record

    unsigned char flags,           ///<[in] Flags to set kv_record.flags_and_size
                                   ///<     @see KV_RECORD_GET_FLAGS()
                                   ///<     @see KV_RECORD_SET_FLAGS()

    unsigned long size,            ///<[in] number of bytes to read from buffer, in addition to the
                                   ///<     size to set in kv_record.flags_and_size
                                   ///<     @see KV_RECORD_GET_SIZE()
                                   ///<     @see KV_RECORD_SET_SIZE()

    unsigned char *buffer          ///<[in] buffer to read \a size bytes into the newly allocated
                                   ///<     value in database_record.kv_record_tbl from
    );

/** @brief Internal method used to allocate a single value within a \a bucket
 *
 *  This is an \b internal method, meaning it should \b not be used by any methods outside of database.h
 *
 *  Optionally, a ptbl_out (\a ptbl_record) pointer to a *pointer* can be specified, to which a pointer to the
 *  ptbl_record for the corresponding \a bucket of the database_record.ptbl_record_tbl will be written.
 *
 *  @returns The index into the \a bucket (ptbl_record) where the value is found on success, or a -1 on failure
 *  @see kv_record.bucket_and_index
 */
unsigned long
_database_value_alloc(
    Context_main *ctx_main,        ///<[in]  main context
    Record_database *rec_database, ///<[in]  database record
    Record_ptbl **ptbl_out,        ///<[out] Where a pointer to the found ptbl_record should be written
    char bucket                    ///<[in]  bucket to allocate in
    );

/** @brief   Given an existing key \a k, returns a pointer to the kv_record from database_record.kv_record_tbl
 *           at key \a k on success, or a 0 on failure.
 *  @returns A pointer to a kv_record on success, or 0 on failure
 *  @see     database_kv_alloc()
 */
Record_kv *
database_kv_get(
    Context_main *ctx_main,        ///<[in] main context
    Record_database *rec_database, ///<[in] database record
    unsigned long k                ///<[in] key to return the kv_record for
    );

/** @brief   Given an existing key \a k, sets the value of said key to a value of \a length bytes taken
 *           from buffer.
 *  @returns 1 on success, 0 on failure
 *  @see     database_kv_get_value()
 */
int
database_kv_set_value(
    Context_main *ctx_main,        ///<[in] main context
    Record_database *rec_database, ///<[in] database record
    unsigned long k,               ///<[in] key to return the kv_record for
    unsigned long length,          ///<[in] length of buffer in bytes
    unsigned char *buffer          ///<[in] New data to set the value to
    );

/** @brief Attempts to resolve the index of the kv_record specified by \a k to the region which the value
 *         component resides at in memory.
 *
 *  Optionally, a kv_out (\a kv_record) pointer to a *pointer* can be specified, to which a pointer to the
 *  kv_record for the corresponding \a k of the database_record.kv_record_tbl will be written. The same thing
 *  applies to the \a ptbl_out parameter for the found ptbl_record.
 *
 *  \b NOTE: Do \b not directly write to the region of memory that is returned by this function. Use
 *  database_kv_set_value() instead. How would you feel if you were trying to read some data, when some jackass
 *  comes along and overwrites it, leaving you with partially-written data?
 *
 *  @returns A pointer to the value corresponding to the kv_record identified by \a k on success, or a 0
 *         on failure
 */
unsigned char *
database_kv_get_value(
    Context_main *ctx_main,        ///<[in]  main context
    Record_database *rec_database, ///<[in]  database record
    Record_ptbl **ptbl_out,        ///<[out] Where a pointer to the found ptbl_record should be written
    Record_kv **kv_out,            ///<[out] Where a pointer to the found kv_record should be written
    unsigned long k                ///<[in]  key to resolve the value's offset in memory for
    );

/** @brief   Given the \a length of a value in bytes, returns the corresponding bucket for that value
 *  @returns A bucket that contains values of an equivalent size
 *  @see     PTBL_CALC_BUCKET_WORD_SIZE()
 */
int database_calc_bucket(
    unsigned long length ///<[in] length of a value in bytes
    );
