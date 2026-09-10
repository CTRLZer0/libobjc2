#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __LIBOBJC_RUNTIME_ASSOCIATION_H_INCLUDED__
#define __LIBOBJC_RUNTIME_ASSOCIATION_H_INCLUDED__

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Valid values for objc_AssociationPolicy.  This is really a bitfield, but
 * only specific combinations of flags are permitted.
 */
enum
{
	/**
	 * Perform straight assignment, no message sends.
	 */
	OBJC_ASSOCIATION_ASSIGN = 0,
	/**
	 * Retain the associated object.
	 */
	OBJC_ASSOCIATION_RETAIN_NONATOMIC = 1,
	/**
	 * Copy the associated object, by sending it a -copy message.
	 */
	OBJC_ASSOCIATION_COPY_NONATOMIC = 3,
	/**
	 * Atomic retain.
	 */
	OBJC_ASSOCIATION_RETAIN = 0x301,
	/**
	 * Atomic copy.
	 */
	OBJC_ASSOCIATION_COPY = 0x303
};
/**
 * Association policy, used when setting associated objects.
 */
typedef uintptr_t objc_AssociationPolicy;

/**
 * Returns an object previously stored by calling objc_setAssociatedObject()
 * with the same arguments, or nil if none exists.
 */
OBJC_PUBLIC
id objc_getAssociatedObject(id object, const void *key);
/**
 * Associates an object with another.  This provides a mechanism for storing
 * extra state with an object, beyond its declared instance variables.  The
 * pointer used as a key is treated as an opaque value.  The best way of
 * ensuring this is to pass the pointer to a static variable as the key.  The
 * value may be any object, but must respond to -copy or -retain, and -release,
 * if an association policy of copy or retain is passed as the final argument.
 */
OBJC_PUBLIC
void objc_setAssociatedObject(id object, const void *key, id value, objc_AssociationPolicy policy);
/**
 * Removes all associations from an object.
 */
OBJC_PUBLIC
void objc_removeAssociatedObjects(id object);

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_ASSOCIATION_H_INCLUDED__
