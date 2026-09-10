#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __LIBOBJC_RUNTIME_BLOCK_H_INCLUDED__
#define __LIBOBJC_RUNTIME_BLOCK_H_INCLUDED__

#include "runtime-types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Converts a block into an IMP that can be used as a method.  The block should
 * take an object pointer (self) as its first argument, and then the same
 * arguments as the method.
 */
OBJC_PUBLIC
IMP imp_implementationWithBlock(id block);
/**
 * Returns the type encoding of an IMP that would be returned by passing the
 * block to imp_implementationWithBlock().  Returns NULL if this is not a valid
 * block encoding for transforming to an IMP (it must take id as its first
 * argument).  The caller is responsible for freeing the returned value.
 */
OBJC_PUBLIC
char *block_copyIMPTypeEncoding_np(id block);
/**
 * Returns the block that was used in an IMP created by
 * imp_implementationWithBlock().  The result of calling this function with any
 * other IMP is undefined.
 */
OBJC_PUBLIC
id imp_getBlock(IMP anImp);
/**
 * Removes a block that was converted to an IMP with
 * imp_implementationWithBlock().  The result of calling this function with any
 * other IMP is undefined.  Returns YES on success, NO on failure.
 */
OBJC_PUBLIC
BOOL imp_removeBlock(IMP anImp);

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_BLOCK_H_INCLUDED__
