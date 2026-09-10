#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __LIBOBJC_RUNTIME_CLASS_H_INCLUDED__
#define __LIBOBJC_RUNTIME_CLASS_H_INCLUDED__

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Adds an instance variable to the named class.  The class must not have been
 * registered by the runtime.  The alignment must be the base-2 logarithm of
 * the alignment requirement and the types should be an Objective-C type encoding.
 */
OBJC_PUBLIC
BOOL class_addIvar(Class cls,
                   const char *name,
                   size_t size,
                   uint8_t alignment,
                   const char *types);

/**
 * Adds a method to the class.
 */
OBJC_PUBLIC
BOOL class_addMethod(Class cls, SEL name, IMP imp, const char *types);

/**
 * Adds a protocol to the class.
 */
OBJC_PUBLIC
BOOL class_addProtocol(Class cls, Protocol *protocol);

/**
 * Tests for protocol conformance.  Note: Currently, protocols with the same
 * name are regarded as equivalent, even if they have different methods.  This
 * behaviour will change in a future version.
 */
OBJC_PUBLIC
BOOL class_conformsToProtocol(Class cls, Protocol *protocol);

/**
 * Copies the instance variable list for this class.  The integer pointed to by
 * the outCount argument is set to the number of instance variables returned.
 * The caller is responsible for freeing the returned buffer.
 */
OBJC_PUBLIC
Ivar* class_copyIvarList(Class cls, unsigned int *outCount);

/**
 * Copies the method list for this class.  The integer pointed to by the
 * outCount argument is set to the number of methods returned.  The caller is
 * responsible for freeing the returned buffer.
 */
OBJC_PUBLIC
Method * class_copyMethodList(Class cls, unsigned int *outCount);

/**
 * Copies the declared property list for this class.  The integer pointed to by
 * the outCount argument is set to the number of declared properties returned.
 * The caller is responsible for freeing the returned buffer.
 */
OBJC_PUBLIC
objc_property_t* class_copyPropertyList(Class cls, unsigned int *outCount);

/**
 * Copies the protocol list for this class.  The integer pointed to by the
 * outCount argument is set to the number of protocols returned.  The caller is
 * responsible for freeing the returned buffer.
 */
OBJC_PUBLIC
Protocol *__unsafe_unretained* class_copyProtocolList(Class cls, unsigned int *outCount);

/**
 * Creates an instance of this class, allocating memory using malloc.
 */
OBJC_PUBLIC OBJC_RETURNS_RETAINED
id class_createInstance(Class cls, size_t extraBytes);

/** Compiler-facing allocation and initialization fast paths. */
OBJC_PUBLIC OBJC_RETURNS_RETAINED
id objc_alloc(Class cls) OBJC_NONPORTABLE;
OBJC_PUBLIC OBJC_RETURNS_RETAINED
id objc_allocWithZone(Class cls) OBJC_NONPORTABLE;
OBJC_PUBLIC OBJC_RETURNS_RETAINED
id objc_alloc_init(Class cls) OBJC_NONPORTABLE;

/**
 * Returns a pointer to the method metadata for the specified method in this
 * class.  This is an opaque data type and must be accessed with the method_*()
 * family of functions.
 */
OBJC_PUBLIC
Method class_getClassMethod(Class aClass, SEL aSelector);

/**
 * Returns a pointer to the  metadata for the specified class variable in
 * this class.  This is an opaque data type and must be accessed with the
 * ivar_*() family of functions.
 */
OBJC_PUBLIC
Ivar class_getClassVariable(Class cls, const char* name);

/**
 * Returns a pointer to the method metadata for the specified instance method
 * in this class.  This is an opaque data type and must be accessed with the
 * method_*() family of functions.
 */
OBJC_PUBLIC
Method class_getInstanceMethod(Class aClass, SEL aSelector);

/**
 * Returns the size of an instance of the named class, in bytes.  All of the
 * class's superclasses must be loaded before this call, or the result is
 * undefined with the non-fragile ABI.
 */
OBJC_PUBLIC
size_t class_getInstanceSize(Class cls);

/**
 * Look up the named instance variable in the class (and its superclasses)
 * returning a pointer to the instance variable definition or a null
 * pointer if no instance variable of that name was found.
 */
OBJC_PUBLIC
Ivar class_getInstanceVariable(Class cls, const char* name);

/**
 * Returns a pointer to the function used to handle the specified message.  If
 * the receiver does not have a method corresponding to this message then this
 * function may return a runtime function that performs forwarding.
 */
OBJC_PUBLIC
IMP class_getMethodImplementation(Class cls, SEL name);

/**
 * Identical to class_getMethodImplementation().
 */
OBJC_PUBLIC
IMP class_getMethodImplementation_stret(Class cls, SEL name);

/**
 * Returns the name of the class.  This string is owned by the runtime and is
 * valid for (at least) as long as the class remains loaded.
 */
OBJC_PUBLIC
const char * class_getName(Class cls);

/**
 * Retrieves metadata about the property with the specified name.
 */
OBJC_PUBLIC
objc_property_t class_getProperty(Class cls, const char *name);

/**
 * Returns the superclass of the specified class.
 */
OBJC_PUBLIC
Class class_getSuperclass(Class cls);

/**
 * Returns the version of the class.  Currently, the class version is not used
 * inside the runtime at all, however it may be used for the developer-mode ABI.
 */
OBJC_PUBLIC
int class_getVersion(Class theClass);

/**
 * Sets the version for this class.
 */
OBJC_PUBLIC
void class_setVersion(Class theClass, int version);

OBJC_PUBLIC OBJC_GNUSTEP_RUNTIME_UNSUPPORTED("Weak instance variables")
const uint8_t *class_getWeakIvarLayout(Class cls);

/**
 * Returns whether the class is a metaclass.  This can be used in conjunction
 * with object_getClass() for differentiating between objects and classes.
 */
OBJC_PUBLIC
BOOL class_isMetaClass(Class cls);

/**
 * Registers an alias for the class. Returns YES if the alias could be
 * registered successfully.
 */
OBJC_PUBLIC OBJC_NONPORTABLE
BOOL class_registerAlias_np(Class cls, const char *alias);

/**
 * Replaces the named method with a new implementation.  Note: the GNUstep
 * Objective-C runtime uses typed selectors, however the types of the selector
 * will be ignored and a new selector registered with the specified types.
 */
OBJC_PUBLIC
IMP class_replaceMethod(Class cls, SEL name, IMP imp, const char *types);

/**
 * Returns YES if instances of this class has a method that implements the
 * specified message, NO otherwise.  If the class handles this message via one
 * or more of the various forwarding mechanisms, then this will still return
 * NO.
 */
OBJC_PUBLIC
BOOL class_respondsToSelector(Class cls, SEL sel);

/**
 * Returns the instance variable layout of this class as an opaque list that
 * can be applied to other classes.
 */
OBJC_PUBLIC
const uint8_t *class_getIvarLayout(Class cls);
/**
 * Sets the class's instance variable layout.  The layout argument must be a
 * value returned by class_getIvarLayout().
 */
OBJC_PUBLIC
void class_setIvarLayout(Class cls, const uint8_t *layout);

/**
 * Sets the superclass of the specified class.  This function is deprecated,
 * because modifying the superclass of a class at run time is a very complex
 * operation and this function is almost always used incorrectly.
 */
OBJC_PUBLIC __attribute__((deprecated))
Class class_setSuperclass(Class cls, Class newSuper);

OBJC_PUBLIC OBJC_GNUSTEP_RUNTIME_UNSUPPORTED("Weak instance variables")
void class_setWeakIvarLayout(Class cls, const uint8_t *layout);

/**
 * Allocates a new class and metaclass inheriting from the specified class,
 * with some space after the class for storing extra data.  This space can be
 * used for class variables by adding instance variables to the returned
 * metaclass.
 */
OBJC_PUBLIC
Class objc_allocateClassPair(Class superclass, const char *name, size_t extraBytes);

/**
 * Frees a class and metaclass allocated with objc_allocateClassPair().  Any
 * attempts to send messages to instances of this class or its subclasses
 * result in undefined behaviour.
 */
OBJC_PUBLIC
void objc_disposeClassPair(Class cls);

/**
 * Returns the class with the specified name, if one has been registered with
 * the runtime, or nil if one does not exist.  If no class of this name is
 * loaded, it calls the _objc_lookup_class() callback to allow an external
 * library to load the module providing this class.
 */
OBJC_PUBLIC
id objc_getClass(const char *name);

/**
 * Copies all of the classes currently registered with the runtime into the
 * buffer specified as the first argument.  If the buffer is NULL or its length
 * is 0, it returns the total number of classes registered with the runtime.
 * Otherwise, it copies classes and returns the number copied.
 */
OBJC_PUBLIC
int objc_getClassList(Class *buffer, int bufferLen);
/**
 * Returns a copy of the list of all classes in the system.  The caller is
 * responsible for freeing this list.  The number of classes is returned in the
 * parameter.
 */
OBJC_PUBLIC
Class *objc_copyClassList(unsigned int *outCount);

/**
 * Returns the metaclass with the specified name.  This is equivalent to
 * calling object_getClass() on the result of objc_getClass().
 */
OBJC_PUBLIC
id objc_getMetaClass(const char *name);

/**
 * Returns the class with the specified name, aborting if none is found.  This
 * function should generally only be called early on in a program, to ensure
 * that all required libraries are loaded.
 */
OBJC_PUBLIC
Class objc_getRequiredClass(const char *name);

/**
 * Looks up the class with the specified name, but does not invoke any
 * external lazy loading mechanisms.
 */
OBJC_PUBLIC
Class objc_lookUpClass(const char *name);

/**
 * Registers a new class and its metaclass with the runtime.  This function
 * should be called after allocating a class with objc_allocateClassPair() and
 * adding instance variables and methods to it.  A class can not have instance
 * variables added to it after objc_registerClassPair() has been called.
 */
OBJC_PUBLIC
void objc_registerClassPair(Class cls);

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_CLASS_H_INCLUDED__
