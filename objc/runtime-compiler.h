#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __LIBOBJC_RUNTIME_COMPILER_H_INCLUDED__
#define __LIBOBJC_RUNTIME_COMPILER_H_INCLUDED__

#include "runtime-types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * This function is inserted by the compiler when a mutation is detected during
 * a foreach iteration. It is exported as a weak symbol to enable GNUstep or
 * some other framework to replace it trivially.
 */
OBJC_PUBLIC
void __attribute__((weak)) objc_enumerationMutation(id obj);

/**
 * Ensure that `+initialize` has been sent to the class of the argument (or the
 * argument, if it is a class).  This will not call `+initialize` if it has
 * been called already, either via an explicit call to this function or by
 * being sent some other message.
 */
OBJC_PUBLIC
void objc_send_initialize(id object) OBJC_NONPORTABLE;

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_COMPILER_H_INCLUDED__
