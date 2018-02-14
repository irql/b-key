B-Key DB    {#mainpage}
========
B-Key database is currently nothing more than a garbage-collector, written in C.
This project is hosted [here](https://github.com/irql/b-key) on github.
[TOC]

Data Structures     {#data-structures}
===============

## Memory Management {#ds-memory}

![Relationships between database memory-management data-structures](https://irql.github.io/b-key/images/data_structures_1.png)
- database_record
  + ptbl_record
    - *page_usage* - pointer to an array of characters, each bit of which represents a boolean value determining whether the corresponding value slot is used (1) or not used (0)
    - *m_offset* - pointer to the start of a bucket's data (i.e. the data for values stored in a bucket)
    - *key* - Positive integer value which identifies the bucket numerically. This value is *six bits* in size.
  + kv_record
    - *bucket* - The key of the bucket (i.e. ptbl_record) that the value corresponding to a key/value pair resides at
    - *index* - Used to determine the location of the value data in the extent of the bucket's data.
      + value_ptr = ptbl_record.m_offset + index * PTBL_CALC_BUCKET_WORD_SIZE(bucket)

See records.h.
