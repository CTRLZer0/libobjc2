#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __LIBOBJC_RUNTIME_SELECTOR_H_INCLUDED__
#define __LIBOBJC_RUNTIME_SELECTOR_H_INCLUDED__

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the name of the specified selector.
 */
OBJC_PUBLIC
const char *sel_getName(SEL sel);

/**
 * Registers a selector with the runtime.  This is equivalent to sel_registerName().
 */
OBJC_PUBLIC
SEL sel_getUid(const char *selName);

/**
 * Returns whether two selectors are equal.  For the purpose of comparison,
 * selectors with the same name and type are regarded as equal.  Selectors with
 * the same name and different types are regarded as different.  If one
 * selector is typed and the other is untyped, but the names are the same, then
 * they are regarded as equal.  This means that sel_isEqual(a, b) and
 * sel_isEqual(a, c) does not imply sel_isEqual(b, c) - if a is untyped but
 * both b and c are typed selectors with different types, then then the first
 * two will return YES, but the third case will return NO.
 */
OBJC_PUBLIC
BOOL sel_isEqual(SEL sel1, SEL sel2);

/**
 * Registers an untyped selector with the runtime.
 */
OBJC_PUBLIC
SEL sel_registerName(const char *selName);

/**
 * Register a typed selector.
 */
OBJC_PUBLIC
SEL sel_registerTypedName_np(const char *selName, const char *types) OBJC_NONPORTABLE;

/**
 * Returns the type encoding associated with a selector, or the empty string is
 * there is no such type.
 */
OBJC_PUBLIC
const char *sel_getType_np(SEL aSel) OBJC_NONPORTABLE;

/**
 * Enumerates all of the type encodings associated with a given selector name
 * (up to a specified limit).  This function returns the number of types that
 * exist for a specific selector, but only copies up to count of them into the
 * array passed as the types argument.  This allows you to call the function
 * once with a relatively small on-stack buffer and then only call it again
 * with a heap-allocated buffer if there is not enough space.
 */
OBJC_PUBLIC
unsigned sel_copyTypes_np(const char *selName, const char **types, unsigned count) OBJC_NONPORTABLE;

/**
 * Enumerates all of the type encodings associated with a given selector name
 * (up to a specified limit).  This function returns the number of types that
 * exist for a specific selector, but only copies up to count of them into the
 * array passed as the types argument.  This allows you to call the function
 * once with a relatively small on-stack buffer and then only call it again
 * with a heap-allocated buffer if there is not enough space.
 */
OBJC_PUBLIC
unsigned sel_copyTypedSelectors_np(const char *selName, SEL *const sels, unsigned count) OBJC_NONPORTABLE;

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_SELECTOR_H_INCLUDED__
