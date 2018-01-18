// Page Table Record
struct ptbl_record {
    /*
     * 0 -  2 ( 3b): key_high / key_low
     * 3 - 31 (31b): page_count / offset
     */
    unsigned int key_high_and_page_count;
    unsigned int key_low_and_offset;
    unsigned long int *page_usage;
};

#define PTBL_KEY_BITMASK 0xE0000000
#define PTBL_KEY_HIGH_BITMASK 0x38
#define PTBL_KEY_LOW_BITMASK 0x7
#define PTBL_KEY_HIGH_SHIFT 26
#define PTBL_KEY_LOW_SHIFT 29

// Helpers to extract the page_count and offset sans bits from key

#define PTBL_RECORD_GET_PAGE_COUNT(x) (x.key_high_and_page_count & ~PTBL_KEY_BITMASK)
#define PTBL_RECORD_SET_PAGE_COUNT(x,y) \
    x.key_high_and_page_count &= PTBL_KEY_BITMASK; \
    x.key_high_and_page_count |= (y & ~PTBL_KEY_BITMASK);

#define PTBL_RECORD_GET_OFFSET(x) (x.key_high_and_page_count & ~PTBL_KEY_BITMASK)
#define PTBL_RECORD_SET_OFFSET(x,y) \
    x.key_low_and_offset &= PTBL_KEY_BITMASK; \
    x.key_low_and_offset |= (y & ~PTBL_KEY_BITMASK);

// The 6-bit key is sharded out to the upper three bits of the page_count and offset fields
#define PTBL_RECORD_GET_KEY(x) (((x.key_high_and_page_count & PTBL_KEY_BITMASK) >> PTBL_KEY_HIGH_SHIFT) | ((x.key_low_and_offset & PTBL_KEY_BITMASK) >> PTBL_KEY_LOW_SHIFT))

// Only to be called once, upon initialization of the ptbl.
// Once set, the key is expected to _NEVER_ change.
#define PTBL_RECORD_SET_KEY(x,y) \
    x.key_high_and_page_count &= ~PTBL_KEY_BITMASK; \
    x.key_low_and_offset &= ~PTBL_KEY_BITMASK; \
    x.key_high_and_page_count |= ((y & PTBL_KEY_HIGH_BITMASK) << PTBL_KEY_HIGH_SHIFT); \
    x.key_low_and_offset |= ((y & PTBL_KEY_LOW_BITMASK) << PTBL_KEY_LOW_SHIFT);

// K/V Record
struct kv_record {
    /*
     * 0 -  7 ( 8b): flags
     * 8 - 63 (56b): size
     */
    unsigned long int flags_and_size;

    unsigned long int offset;
};

#define KV_RECORD_FLAGS_BITMASK 0xFF00000000000000
#define KV_RECORD_FLAGS_SHIFT 56

#define KV_RECORD_GET_SIZE(x) (x.flags_and_size & ~KV_RECORD_FLAGS_BITMASK)
#define KV_RECORD_SET_SIZE(x,y) \
    x.flags_and_size &= KV_RECORD_FLAGS_BITMASK; \
    x.flags_and_size |= (y & ~KV_RECORD_FLAGS_BITMASK);

#define KV_RECORD_GET_FLAGS(x) ((x.flags_and_size & KV_RECORD_FLAGS_BITMASK) >> KV_RECORD_FLAGS_SHIFT)
#define KV_RECORD_SET_FLAGS(x,y) \
    x.flags_and_size &= ~KV_RECORD_FLAGS_BITMASK; \
    x.flags_and_size |= ((unsigned long)(y & (KV_RECORD_FLAGS_BITMASK >> KV_RECORD_FLAGS_SHIFT)) << KV_RECORD_FLAGS_SHIFT);

// Master Record
struct master_record {
    // blocks of 4096 allocated
    unsigned long int page_count;
    unsigned long int kv_record_count;
    unsigned long int ptbl_record_count;
    struct ptbl_record *ptbl_record_tbl;
    struct kv_record *kv_record_tbl;
};
