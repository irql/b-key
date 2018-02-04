/** @file os.h
 *  @brief Defines used to identify which OS should be compiled for
 */

#if defined(__MACOSX__) || defined(__APPLE__) || defined(__OpenBSD__)
#define __MACOSX__
#endif
