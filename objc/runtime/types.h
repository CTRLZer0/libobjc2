#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __LIBOBJC_RUNTIME_TYPES_H_INCLUDED__
#define __LIBOBJC_RUNTIME_TYPES_H_INCLUDED__

#include <objc/support/visibility.h>
#include <objc/objc-config.h>
#include <objc/support/availability.h>

#ifndef __GNUSTEP_RUNTIME__
#	define __GNUSTEP_RUNTIME__
#endif

#ifndef __has_feature
#	define __has_feature(x) 0
#endif
#ifndef __has_attribute
#	define __has_attribute(x) 0
#endif

#ifndef __unsafe_unretained
#	if !__has_feature(objc_arc)
#		define __unsafe_unretained
#	endif
#endif

#if defined(__OBJC__) && __has_attribute(ns_returns_retained)
#	define OBJC_RETURNS_RETAINED __attribute__((ns_returns_retained))
#else
#	define OBJC_RETURNS_RETAINED
#endif

#ifndef __STDC_LIMIT_MACROS
#	define __STDC_LIMIT_MACROS 1
#endif

#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef class_setVersion
#	undef class_setVersion
#endif
#ifdef class_getClassMethod
#	undef class_getClassMethod
#endif
#ifdef objc_getClass
#	undef objc_getClass
#endif
#ifdef objc_lookUpClass
#	undef objc_lookUpClass
#endif

/**
 * Opaque type for Objective-C instance variable metadata.
 */
typedef struct objc_ivar* Ivar;

// Don't redefine these types if the old GCC header was included first.
#ifndef __objc_INCLUDE_GNU
// Define the macro so that including the old GCC header does nothing.
#	define __objc_INCLUDE_GNU
#	define __objc_api_INCLUDE_GNU


/**
 * Opaque type used for selectors.
 */
#if !defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
typedef const struct objc_selector *SEL;
#else
typedef struct objc_selector *SEL;
#endif

/**
 * Opaque type for Objective-C classes.
 */
typedef struct objc_class *Class;

/**
 * Type for Objective-C objects.
 */
typedef struct objc_object
{
	/**
	 * Pointer to this object's class.  Accessing this directly is STRONGLY
	 * discouraged.  You are recommended to use object_getClass() instead.
	 */
#ifndef __OBJC_RUNTIME_INTERNAL__
	__attribute__((deprecated))
#endif
	Class isa;
} *id;

/**
 * Structure used for calling superclass methods.
 */
struct objc_super
{
	/** The receiver of the message. */
	__unsafe_unretained id receiver;
	/** The class containing the method to call. */
#	if !defined(__cplusplus)  &&  !__OBJC2__
	Class class;
#	else
	Class super_class;
#	endif
};

/**
 * Instance Method Pointer type.  Note: Since the calling convention for
 * variadic functions sometimes differs from the calling convention for
 * non-variadic functions, you must cast an IMP to the correct type before
 * calling.
 */
typedef id (*IMP)(id, SEL, ...);
/**
 * Opaque type for Objective-C method metadata.
 */
typedef struct objc_method *Method;

/**
 * Objective-C boolean type.
 */
#	ifdef STRICT_APPLE_COMPATIBILITY
typedef signed char BOOL;
#	else
#		if defined(__vxworks) || defined(_WIN32)
typedef  int BOOL;
#		else
typedef unsigned char BOOL;
#		endif
#	endif

#else
// Method in the GCC runtime is a struct, Method_t is the pointer
#	define Method Method_t
#endif // __objc_INCLUDE_GNU


/**
 * Opaque type for Objective-C property metadata.
 */
typedef struct objc_property* objc_property_t;
/**
 * Opaque type for Objective-C protocols.  Note that, although protocols are
 * objects, sending messages to them is deprecated in Objective-C 2 and may not
 * work in the future.
 */
#ifdef __OBJC__
@class Protocol;
#else
typedef struct objc_protocol Protocol;
#endif

/**
 * Objective-C method description.
 */
struct objc_method_description
{
	/**
	 * The name of this method.
	 */
	SEL   name;
	/**
	 * The types of this method.
	 */
	const char *types;
};

/**
 * The objc_property_attribute_t type is used to store attributes for
 * properties.  This is used to store a decomposed version of the property
 * encoding, with each flag stored in the name and each value in the value.
 *
 * All of the strings that these refer to are internal to the runtime and
 * should not be freed.
 */
typedef struct
{
	/**
	 * The flag that this attribute describes.  All current flags are single characters,
	 */
	const char *name;
	/**
	 */
	const char *value;
} objc_property_attribute_t;



#ifndef YES
#	if __has_feature(objc_bool)
#		define YES __objc_yes
#	else
#		define YES ((BOOL)1)
#	endif
#endif
#ifndef NO
#	if __has_feature(objc_bool)
#		define NO __objc_no
#	else
#		define NO ((BOOL)0)
#	endif
#endif

#if __has_feature(cxx_nullptr)
#	define _OBJC_NULL_PTR nullptr
#else
#	define _OBJC_NULL_PTR NULL
#endif

#ifndef nil
#	define nil _OBJC_NULL_PTR
#endif

#ifndef Nil
#	define Nil _OBJC_NULL_PTR
#endif

#endif // __LIBOBJC_RUNTIME_TYPES_H_INCLUDED__
