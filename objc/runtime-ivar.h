#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __LIBOBJC_RUNTIME_IVAR_H_INCLUDED__
#define __LIBOBJC_RUNTIME_IVAR_H_INCLUDED__

#include "runtime-types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the name of an instance variable.
 */
OBJC_PUBLIC
const char* ivar_getName(Ivar ivar);

/**
 * Returns the offset of an instance variable.  This value can be added to the
 * object pointer to get the address of the instance variable.
 */
OBJC_PUBLIC
ptrdiff_t ivar_getOffset(Ivar ivar);

/**
 * Returns the Objective-C type encoding of the instance variable.
 */
OBJC_PUBLIC
const char* ivar_getTypeEncoding(Ivar ivar);

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_IVAR_H_INCLUDED__
