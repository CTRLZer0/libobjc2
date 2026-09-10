#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __LIBOBJC_RUNTIME_PROPERTY_H_INCLUDED__
#define __LIBOBJC_RUNTIME_PROPERTY_H_INCLUDED__

#include "runtime-types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the name of a specified property.
 */
OBJC_PUBLIC
const char *property_getName(objc_property_t property);

/**
 * Returns the attributes for the specified property.  This is similar to an
 * Objective-C type encoding, but contains some extra information.  A full
 * description of the format for this string may be found in Apple's
 * Objective-C Runtime Programming Guide.
 */
OBJC_PUBLIC
const char *property_getAttributes(objc_property_t property);

/**
 * Returns an array of attributes for this property.
 */
OBJC_PUBLIC
objc_property_attribute_t *property_copyAttributeList(objc_property_t property,
                                                      unsigned int *outCount);
/**
 * Adds a property to the class, given a specified set of attributes.  Note
 * that this only sets the property metadata.  The property accessor methods
 * must already be created.
 */
OBJC_PUBLIC
BOOL class_addProperty(Class cls,
                       const char *name,
                       const objc_property_attribute_t *attributes,
                       unsigned int attributeCount);

/**
 * Replaces property metadata.  If the property does not exist, then this is
 * equivalent to calling class_addProperty().
 */
OBJC_PUBLIC
void class_replaceProperty(Class cls,
                           const char *name,
                           const objc_property_attribute_t *attributes,
                           unsigned int attributeCount);

/**
 * Returns a copy of a single attribute.
 */
OBJC_PUBLIC
char *property_copyAttributeValue(objc_property_t property,
                                  const char *attributeName);

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_PROPERTY_H_INCLUDED__
