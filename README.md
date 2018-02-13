B-Key DB {#mainpage} 
========
B-Key database is currently nothing more than a garbage-collector, written in C.
[TOC]

Data Structures {#data-structures}
===============
![Relationships between database data-structures](https://irql.github.io/b-key/images/data_structures_1.png)
- ptbl_record
  + \a page_usage - pointer to an array of characters, each bit of which represents a boolean value determining whether the corresponding value slot is used (1) or not used (0)
  + \a m_offset - pointer to the start of a bucket's data (i.e. the data for values stored in a bucket)
  + \a key - Positive integer value which identifies the bucket numerically. This value is *six bits* in size.
- kv_record
  + \a bucket - The key of the bucket (i.e. Record_ptbl) that the value corresponding to a key/value pair resides at
  + \a index - Used to determine the location of the value data in the extent of the bucket's data.
    - value_ptr = Record_ptbl.m_offset + index * PTBL_CALC_BUCKET_WORD_SIZE(bucket)

See records.h.
