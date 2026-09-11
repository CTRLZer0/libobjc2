#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __LIBOBJC_RUNTIME_OBJECT_H_INCLUDED__
#define __LIBOBJC_RUNTIME_OBJECT_H_INCLUDED__

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sets the object value of a specified instance variable.
 */
OBJC_PUBLIC
void object_setIvar(id object, Ivar ivar, id value);
/**
 * Sets a named instance variable to the value specified by *value.  Note that
 * the instance variable must be a pointer-sized quantity.
 */
OBJC_PUBLIC
Ivar object_setInstanceVariable(id obj, const char *name, void *value);

/**
 * Returns the value of the named instance variable.  This should not be used
 * with instance variables that are not pointers.
 */
OBJC_PUBLIC
id object_getIvar(id object, Ivar ivar);

/**
 * Returns a named instance variable via the final parameter.  Note that
 * calling object_getIvar() on the value returned from this function is faster.
 *
 * Note that the instance variable must be a pointer-sized quantity.
 */
OBJC_PUBLIC
Ivar object_getInstanceVariable(id obj, const char *name, void **outValue);

/**
 * Returns a pointer immediately after the instance variables declared in an
 * object.  This is a pointer to the storage specified with the extraBytes
 * parameter given when allocating an object.
 */
OBJC_PUBLIC
void *object_getIndexedIvars(id obj);

// FIXME: The GNU runtime has a version of this which omits the size parameter
//id object_copy(id obj, size_t size);

/**
 * Constructs an instance of cls in caller-provided, suitably aligned,
 * zero-filled storage of at least class_getInstanceSize(cls) bytes.
 */
OBJC_PUBLIC
id objc_constructInstance(Class cls, void *bytes);

/**
 * Runs instance destruction without freeing the caller-provided storage.
 * Associated references are removed as part of the normal destruction path.
 */
OBJC_PUBLIC
void *objc_destructInstance(id obj);

/**
 * Free an object created with class_createInstance().
 */
OBJC_PUBLIC
id object_dispose(id obj);

/**
 * Returns the class of the object.  Note: the isa pointer should not be
 * accessed directly with the GNUstep runtime.
 */
OBJC_PUBLIC
Class object_getClass(id obj);

/**
 * Sets the class of the object.  Note: the isa pointer should not be
 * accessed directly with the GNUstep runtime.
 */
OBJC_PUBLIC
Class object_setClass(id obj, Class cls);

/**
 * Returns the name of the class of the object.  This is equivalent to calling
 * class_getName() on the result of object_getClass().
 */
OBJC_PUBLIC
const char *object_getClassName(id obj);


/**
 * Adds a method to a specific object,  This method will not be added to any
 * other instances of the same class.
 */
OBJC_PUBLIC
BOOL object_addMethod_np(id object, SEL name, IMP imp, const char *types);

/**
 * Replaces a method on a specific object,  This method will not be added to
 * any other instances of the same class.
 */
OBJC_PUBLIC
IMP object_replaceMethod_np(id object, SEL name, IMP imp, const char *types);

/**
 * Creates a clone, in the JavaScript sense - an object which inherits both
 * associated references and methods from the original object.
 */
OBJC_PUBLIC
id object_clone_np(id object);

/**
 * Returns the prototype of the object if it was created with
 * object_clone_np(), or nil otherwise.
 */
OBJC_PUBLIC
id object_getPrototype_np(id object);

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_OBJECT_H_INCLUDED__
