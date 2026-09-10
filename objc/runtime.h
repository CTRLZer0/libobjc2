#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif
#include "runtime-types.h"

#ifndef __LIBOBJC_RUNTIME_H_INCLUDED__
#define __LIBOBJC_RUNTIME_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

#include "slot.h"
#include "message.h"
#include "runtime-class.h"
#include "runtime-object.h"
#include "runtime-ivar.h"
#include "runtime-method.h"


#include "runtime-protocol.h"

#include "runtime-property.h"

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

#include "runtime-selector.h"

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

/**
 * Registers a class for small objects.  Small objects are stored inside a
 * pointer.  If the class can be registered, then this returns YES.  The second
 * argument specifies the bit pattern to use to identify the small object.
 */
OBJC_PUBLIC
BOOL objc_registerSmallObjectClass_np(Class cls, uintptr_t classId);

/**
 * The mask identifying the bits that can be used in an object pointer to
 * identify a small object.  On 32-bit systems, we use the low bit.  On 64-bit
 * systems, we use the low 3 bits.  In both cases, the lowest bit must be 1.
 * This restriction may be relaxed in the future on 64-bit systems.
 */
#ifndef UINTPTR_MAX
#	define OBJC_SMALL_OBJECT_MASK ((sizeof(void*) == 4) ? 1 : 7)
#elif UINTPTR_MAX < UINT64_MAX
#	define OBJC_SMALL_OBJECT_MASK 1
#else
#	define OBJC_SMALL_OBJECT_MASK 7
#endif
/**
 * The number of bits reserved for the class identifier in a small object.
 */
#ifndef UINTPTR_MAX
#	define OBJC_SMALL_OBJECT_SHIFT ((sizeof(void*) == 4) ? 1 : 3)
#elif UINTPTR_MAX < UINT64_MAX
#	define OBJC_SMALL_OBJECT_SHIFT 1
#else
#	define OBJC_SMALL_OBJECT_SHIFT 3
#endif


#include "runtime-association.h"

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

#define _C_ID       '@'
#define _C_CLASS    '#'
#define _C_SEL      ':'
#define _C_BOOL     'B'

#define _C_CHR      'c'
#define _C_UCHR     'C'
#define _C_SHT      's'
#define _C_USHT     'S'
#define _C_INT      'i'
#define _C_UINT     'I'
#define _C_LNG      'l'
#define _C_ULNG     'L'
#define _C_LNG_LNG  'q'
#define _C_ULNG_LNG 'Q'

#define _C_FLT      'f'
#define _C_DBL      'd'

#define _C_BFLD     'b'
#define _C_VOID     'v'
#define _C_UNDEF    '?'
#define _C_PTR      '^'

#define _C_CHARPTR  '*'
#define _C_ATOM     '%'

#define _C_ARY_B    '['
#define _C_ARY_E    ']'
#define _C_UNION_B  '('
#define _C_UNION_E  ')'
#define _C_STRUCT_B '{'
#define _C_STRUCT_E '}'
#define _C_VECTOR   '!'

#define _C_COMPLEX  'j'
#define _C_CONST    'r'
#define _C_IN       'n'
#define _C_INOUT    'N'
#define _C_OUT      'o'
#define _C_BYCOPY   'O'
#define _C_ONEWAY   'V'

#include "runtime-deprecated.h"

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_H_INCLUDED__
