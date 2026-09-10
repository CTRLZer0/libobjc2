#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif
#include "objc-visibility.h"

#ifndef __OBJC_EXCEPTION_INCLUDED__
#define __OBJC_EXCEPTION_INCLUDED__

#include "runtime-types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*objc_uncaught_exception_handler)(id exception);

/** 
 * Throw a runtime exception. Inserted by the compiler in place of @throw.
 */
OBJC_PUBLIC
void objc_exception_throw(id object);

/**
 * Installs handler for uncaught Objective-C exceptions.  If the unwind library
 * reaches the end of the stack without finding a handler then the handler is
 * called. Returns the previous handler.
 */
OBJC_PUBLIC
objc_uncaught_exception_handler objc_setUncaughtExceptionHandler(objc_uncaught_exception_handler handler);

/**
 * Toggles whether Objective-C objects caught in C++ exception handlers in
 * Objective-C++ mode should follow Objective-C or C++ semantics.  The obvious
 * choice is for them to follow C++ semantics, because people using a C++
 * language construct would intuitively expect them to have C++ semantics,
 * where the catch behaviour depends on the static type of the thrown object,
 * not its run-time type.
 *
 * Apple, therefore, chose the other option.
 *
 * We default to Apple-compatible mode, but can enable the sane behaviour if
 * the user opts in.  Note that doing this when linking against third-party
 * frameworks written in Objective-C++ 2 may cause weird problems if the expect
 * the other behaviour.
 *
 * This currently sets a global value.  In the future, it may be configurable
 * on a per-thread basis.
 */
OBJC_PUBLIC
int objc_set_apple_compatible_objcxx_exceptions(int newValue) OBJC_NONPORTABLE;

#ifdef __cplusplus
}
#endif

#endif // __OBJC_EXCEPTION_INCLUDED__
