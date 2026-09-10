#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __LIBOBJC_RUNTIME_DISPATCH_H_INCLUDED__
#define __LIBOBJC_RUNTIME_DISPATCH_H_INCLUDED__

#include "types.h"
#include <objc/slot.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The message lookup function used by the GCC ABI.  This returns a pointer to
 * the function (either a method or a forwarding hook) that should be called in
 * response to a given message.
 */
OBJC_PUBLIC
IMP objc_msg_lookup(id, SEL) OBJC_NONPORTABLE;
/**
 * The message lookup function used for messages sent to super in the GCC ABI.
 * This specifies both the class and the 
 */
OBJC_PUBLIC
IMP objc_msg_lookup_super(struct objc_super*, SEL) OBJC_NONPORTABLE;

/**
 * New ABI lookup function.  Receiver may be modified during lookup or proxy
 * forwarding and the sender may affect how lookup occurs.
 */
OBJC_PUBLIC
extern struct objc_slot *objc_msg_lookup_sender(id *receiver, SEL selector, id sender)
	OBJC_NONPORTABLE OBJC_DEPRECATED;

/**
 * Deprecated function for accessing a slot without going via any forwarding
 * mechanisms.
 */
OBJC_PUBLIC
extern struct objc_slot *objc_get_slot(Class, SEL)
	OBJC_NONPORTABLE OBJC_DEPRECATED;

/**
 * Look up a slot, without invoking any forwarding mechanisms.  The third
 * parameter is used to return a pointer to the current value of the version
 * counter.  If this value is equal to `objc_method_cache_version` then the
 * slot is safe to reuse without performing another lookup.
 */
OBJC_PUBLIC
extern struct objc_slot2 *objc_get_slot2(Class, SEL, uint64_t*)
	OBJC_NONPORTABLE;

/**
 * Look up a slot, invoking any required forwarding mechanisms.  The third
 * parameter is used to return a pointer to the current value of the version
 * counter.  If this value is equal to `objc_method_cache_version` then the
 * slot is safe to reuse without performing another lookup.
 */
OBJC_PUBLIC
extern struct objc_slot2 *objc_slot_lookup_version(id *receiver, SEL selector, uint64_t*)
	OBJC_NONPORTABLE;

/**
 * Look up a slot, invoking any required forwarding mechanisms.
 */
OBJC_PUBLIC
extern IMP objc_msg_lookup2(id *receiver, SEL selector) OBJC_NONPORTABLE;

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_DISPATCH_H_INCLUDED__
