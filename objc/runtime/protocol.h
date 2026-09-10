#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __LIBOBJC_RUNTIME_PROTOCOL_H_INCLUDED__
#define __LIBOBJC_RUNTIME_PROTOCOL_H_INCLUDED__

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the protocol with the specified name.
 */
OBJC_PUBLIC
Protocol *objc_getProtocol(const char *name);
/**
 * Allocates a new protocol.  This returns NULL if a protocol with the same
 * name already exists in the system.
 *
 * Protocols are immutable after they have been registered, so may only be
 * modified between calling this function and calling objc_registerProtocol().
 */
OBJC_PUBLIC
Protocol *objc_allocateProtocol(const char *name);
/**
 * Registers a protocol with the runtime.  After this point, the protocol may
 * not be modified.
 */
OBJC_PUBLIC
void objc_registerProtocol(Protocol *proto);
/**
 * Adds a method to the protocol.
 */
OBJC_PUBLIC
void protocol_addMethodDescription(Protocol *aProtocol,
                                   SEL name,
                                   const char *types,
                                   BOOL isRequiredMethod,
                                   BOOL isInstanceMethod);
/**
 * Adds a protocol to the protocol.
 */
OBJC_PUBLIC
void protocol_addProtocol(Protocol *aProtocol, Protocol *addition);
/**
 * Adds a property to the protocol.
 */
OBJC_PUBLIC
void protocol_addProperty(Protocol *aProtocol,
                          const char *name,
                          const objc_property_attribute_t *attributes,
                          unsigned int attributeCount,
                          BOOL isRequiredProperty,
                          BOOL isInstanceProperty);

/**
 * Testswhether a protocol conforms to another protocol.
 */
OBJC_PUBLIC
BOOL protocol_conformsToProtocol(Protocol *p, Protocol *other);

/**
 * Returns an array of method descriptions.  Stores the number of elements in
 * the array in the variable pointed to by the last parameter.  The caller is
 * responsible for freeing this array.
 */
OBJC_PUBLIC
struct objc_method_description *protocol_copyMethodDescriptionList(Protocol *p,
	BOOL isRequiredMethod, BOOL isInstanceMethod, unsigned int *count);

/**
 * Returns an array of required instance properties, with the number being
 * stored in the variable pointed to by the last argument.  The caller is
 * responsible for freeing the returned array.
 */
OBJC_PUBLIC
objc_property_t *protocol_copyPropertyList(Protocol *p, unsigned int *count);

/**
 * Returns an array of properties specified by this class, with the number
 * being stored in the variable pointed to by the last argument.  The caller is
 * responsible for freeing the returned array.
 */
OBJC_PUBLIC
objc_property_t *protocol_copyPropertyList2(Protocol *p, unsigned int *count,
	BOOL isRequiredProperty, BOOL isInstanceProperty);

/**
 * Returns an array of protocols that this protocol conforms to, with the
 * number of protocols in the array being returned via the last argument.  The
 * caller is responsible for freeing this array.
 */
OBJC_PUBLIC
Protocol *__unsafe_unretained*protocol_copyProtocolList(Protocol *p, unsigned int *count);

/**
 * Returns all of the protocols that the runtime is aware of.  Note that
 * protocols compiled by GCC and not attacked to classes may not have been
 * registered with the runtime.  The number of protocols returned is stored at
 * the address indicated by the pointer argument.
 *
 * The caller is responsible for freeing the returned array.
 */
OBJC_PUBLIC
Protocol *__unsafe_unretained*objc_copyProtocolList(unsigned int *outCount);
/**
 * Returns the method description for the specified method within a given
 * protocol.
 */
OBJC_PUBLIC
struct objc_method_description protocol_getMethodDescription(Protocol *p,
	SEL aSel, BOOL isRequiredMethod, BOOL isInstanceMethod);

/**
 * Returns the extended type encoding of the specified method.
 *
 * Note: This function is used by JavaScriptCore but is not public in Apple's
 * implementation and so its semantics may change in the future and this
 * runtime may diverge from Apple's.
 */
OBJC_PUBLIC
const char *_protocol_getMethodTypeEncoding(Protocol *p, SEL aSel,
	BOOL isRequiredMethod, BOOL isInstanceMethod);

/**
 * Returns the name of the specified protocol.
 */
OBJC_PUBLIC
const char* protocol_getName(Protocol *p);

/**
 * Returns the property metadata for the property with the specified name.
 */
OBJC_PUBLIC
objc_property_t protocol_getProperty(Protocol *p, const char *name,
	BOOL isRequiredProperty, BOOL isInstanceProperty);

/**
 * Compares two protocols.  Currently, protocols are assumed to be equal if
 * their names match.  This is required for compatibility with the GCC ABI,
 * which made not attempt to unique protocols (or even register them with the
 * runtime).
 */
OBJC_PUBLIC
BOOL protocol_isEqual(Protocol *p, Protocol *other);

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_PROTOCOL_H_INCLUDED__
