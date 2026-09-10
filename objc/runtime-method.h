#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __LIBOBJC_RUNTIME_METHOD_H_INCLUDED__
#define __LIBOBJC_RUNTIME_METHOD_H_INCLUDED__

#include "runtime-types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Copies the type encoding of an argument of this method.  The caller is
 * responsible for freeing the returned C string.  Arguments 0 and 1 of any
 * Objective-C method will be the self and _cmd parameters, so the returned
 * value will be "@" and ":" respectively.
 */
OBJC_PUBLIC
char* method_copyArgumentType(Method method, unsigned int index);

/**
 * Copies the type encoding of an argument of this method.  The caller is
 * responsible for freeing the returned C string.
 */
OBJC_PUBLIC
char* method_copyReturnType(Method method);

/**
 * Exchanges the implementations of the two methods.  Note: this call is very
 * expensive on the GNUstep runtime and its use is discouraged.  It is
 * recommended that users call class_replaceMethod() instead.
 */
OBJC_PUBLIC
void method_exchangeImplementations(Method m1, Method m2);

/**
 * Copies the Objective-C type encoding of a specified method parameter into a
 * buffer provided by the caller.  This method does not provide any means for
 * the caller to easily detect truncation, and will only NULL-terminate the
 * output string if there is enough space for the argument type and the NULL
 * terminator.  Its use is therefore discouraged.
 */
OBJC_PUBLIC
void method_getArgumentType(Method method, unsigned int index, char *dst, size_t dst_len);

/**
 * Returns a pointer to the function used to implement this method.
 */
OBJC_PUBLIC
IMP method_getImplementation(Method method);

/**
 * Returns the selector used to identify this method.  Note that, unlike the
 * Apple runtimes, the GNUstep runtime uses typed selectors, so the return
 * value for this also identifies the type of the method, not just its name,
 * although calling method_getTypeEncoding() is faster if you just require the
 * types.
 */
OBJC_PUBLIC
SEL method_getName(Method method);

/** Returns the typed selector stored by the GNUstep runtime. */
OBJC_PUBLIC
SEL method_getTypedSelector_np(Method method) OBJC_NONPORTABLE;

/**
 * Returns the number of arguments (including self and _cmd) that this method
 * expects.
 */
OBJC_PUBLIC
unsigned method_getNumberOfArguments(Method method);

/** Historical spelling retained for source and binary compatibility. */
OBJC_PUBLIC
unsigned method_get_number_of_arguments(struct objc_method *method);

/**
 * Copies the Objective-C type encoding of a method's return value into a
 * buffer provided by the caller.  This method does not provide any means for
 * the caller to easily detect truncation, and will only NULL-terminate the
 * output string if there is enough space for the argument type and the NULL
 * terminator.  Its use is therefore discouraged.
 */
OBJC_PUBLIC
void method_getReturnType(Method method, char *dst, size_t dst_len);

/**
 * Returns the type encoding for the method.  This string is owned by the
 * runtime and will persist for (at least) as long as the class owning the
 * method is loaded.
 */
OBJC_PUBLIC
const char * method_getTypeEncoding(Method method);

/**
 * Sets the function used to implement this method.  This function is very
 * expensive with the GNUstep runtime and its use is discouraged.  It is
 * recommended that you call class_replaceMethod() instead.
 */
OBJC_PUBLIC
IMP method_setImplementation(Method method, IMP imp);

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_METHOD_H_INCLUDED__
