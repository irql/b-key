/** @file debug.h
 *  @brief Various macros and defines for debugging
 */

//#define DEBUG_DATABASE
//#define DEBUG_MEMORY
//#define DEBUG_TESTS

/** @brief A simple wrapper for fprintf(stderr, ...) */
#define DEBUG_PRINT(...) \
    fprintf(stderr, __VA_ARGS__)
