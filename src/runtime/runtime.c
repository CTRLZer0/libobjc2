#include "objc/runtime.h"
#include "selector.h"
#include "class.h"
#include "protocol.h"
#include "ivar.h"
#include "method.h"
#include "lock.h"
#include "dtable.h"
#include "gc_ops.h"
#include "crt_compat.h"
#include "allocation.h"

/* Make glibc export objc2_strdup() */

#if defined __GLIBC__
	#define __USE_BSD 1
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#define CHECK_ARG(arg) if (0 == arg) { return 0; }

static inline void safe_remove_from_subclass_list(Class cls);
PRIVATE BOOL objc_resolve_class(Class);
void objc_send_initialize(id object);

/**
 * Calls C++ destructors in the correct order.
 */
PRIVATE void call_cxx_destruct(id obj)
{
	static SEL cxx_destruct_storage;
	SEL cxx_destruct = objc2_get_or_register_selector(
		&cxx_destruct_storage, ".cxx_destruct");
	// Don't call object_getClass(), because we want to get hidden classes too
	Class cls = classForObject(obj);

	while (cls)
	{
		// If we're deallocating a class with a hidden class, then the
		// `.cxx_destruct` method may deallocate the class.
		Class currentClass = cls;
		cls = cls->super_class;
		if (currentClass->cxx_destruct)
		{
			currentClass->cxx_destruct(obj, cxx_destruct);
		}
	}
}

static void call_cxx_construct_for_class(Class cls, id obj)
{
	static SEL cxx_construct_storage;
	SEL cxx_construct = objc2_get_or_register_selector(
		&cxx_construct_storage, ".cxx_construct");

	if (cls->super_class)
	{
		call_cxx_construct_for_class(cls->super_class, obj);
	}
	if (cls->cxx_construct)
	{
		cls->cxx_construct(obj, cxx_construct);
	}
}

PRIVATE void call_cxx_construct(id obj)
{
	call_cxx_construct_for_class(classForObject(obj), obj);
}

/**
 * Looks up the instance method in a specific class, without recursing into
 * superclasses.
 */
static Method class_getInstanceMethodNonrecursive(Class aClass, SEL aSelector)
{
	for (struct objc_method_list *methods = aClass->methods;
		methods != NULL ; methods = methods->next)
	{
		for (int i=0 ; i<methods->count ; i++)
		{
			Method method = method_at_index(methods, i);
			if (sel_isEqual(method->selector, aSelector))
			{
				return method;
			}
		}
	}
	return NULL;
}

BOOL class_addIvar(Class cls, const char *name, size_t size, uint8_t alignment,
		const char *types)
{
	CHECK_ARG(cls);
	CHECK_ARG(name);
	CHECK_ARG(types);
	if (objc_test_class_flag(cls, objc_class_flag_initialized)) { return NO; }
	if (class_getInstanceVariable(cls, name) != NULL) { return NO; }
	if ((size > UINT32_MAX) || (cls->instance_size < 0)) { return NO; }
	if (alignment >= (sizeof(size_t) * CHAR_BIT)) { return NO; }

	size_t byteAlignment = ((size_t)1) << alignment;
	size_t currentSize = (size_t)cls->instance_size;
	if (currentSize > SIZE_MAX - (byteAlignment - 1)) { return NO; }
	size_t offset = (currentSize + byteAlignment - 1) & ~(byteAlignment - 1);
	if ((size > SIZE_MAX - offset) || (offset + size > LONG_MAX)) { return NO; }

	char *nameCopy = objc2_strdup(name);
	char *typeCopy = objc2_strdup(types);
	if ((NULL == nameCopy) || (NULL == typeCopy))
	{
		free(nameCopy);
		free(typeCopy);
		return NO;
	}

	struct objc_ivar_list *oldList = cls->ivars;
	if ((oldList != NULL) && ((oldList->count < 0) ||
	    (oldList->size < sizeof(struct objc_ivar))))
	{
		free(nameCopy);
		free(typeCopy);
		return NO;
	}
	size_t oldCount = oldList ? (size_t)oldList->count : 0;
	if (oldCount >= INT_MAX)
	{
		free(nameCopy);
		free(typeCopy);
		return NO;
	}
	size_t stride = oldList ? oldList->size : sizeof(struct objc_ivar);
	size_t allocationSize;
	if (!objc2_flexible_array_size(sizeof(struct objc_ivar_list), oldCount + 1,
	                               stride, &allocationSize))
	{
		free(nameCopy);
		free(typeCopy);
		return NO;
	}
	struct objc_ivar_list *updated = realloc(oldList, allocationSize);
	if (NULL == updated)
	{
		free(nameCopy);
		free(typeCopy);
		return NO;
	}
	if (0 == oldCount) { updated->size = sizeof(struct objc_ivar); }
	updated->count = (int)(oldCount + 1);
	cls->ivars = updated;

	Ivar ivar = ivar_at_index(updated, (int)oldCount);
	memset(ivar, 0, stride);
	ivar->name = nameCopy;
	ivar->type = typeCopy;
	ivar->size = (uint32_t)size;
	ivarSetAlign(ivar, byteAlignment);
	ivar->offset = (int*)(uintptr_t)offset;
	cls->instance_size = (long)(offset + size);
	return YES;
}

static void update_cxx_method_cache(Class cls, const char *name, IMP imp)
{
	if (strcmp(name, ".cxx_construct") == 0) { cls->cxx_construct = imp; }
	else if (strcmp(name, ".cxx_destruct") == 0) { cls->cxx_destruct = imp; }
}

BOOL class_addMethod(Class cls, SEL name, IMP imp, const char *types)
{
	CHECK_ARG(cls);
	CHECK_ARG(name);
	CHECK_ARG(imp);
	CHECK_ARG(types);
	const char *methodName = sel_getName(name);
	struct objc_method_list *methods;
	for (methods=cls->methods; methods!=NULL ; methods=methods->next)
	{
		for (int i=0 ; i<methods->count ; i++)
		{
			Method method = method_at_index(methods, i);
			if (strcmp(sel_getName(method->selector), methodName) == 0)
			{
				return NO;
			}
		}
	}

	SEL typedSelector = sel_registerTypedName_np(methodName, types);
	char *typeCopy = objc2_strdup(types);
	if ((NULL == typedSelector) || (NULL == typeCopy))
	{
		free(typeCopy);
		return NO;
	}
	methods = calloc(1, sizeof(struct objc_method_list) + sizeof(struct objc_method));
	if (NULL == methods)
	{
		free(typeCopy);
		return NO;
	}
	methods->next = cls->methods;
	methods->size = sizeof(struct objc_method);
	methods->count = 1;
	struct objc_method *m0 = method_at_index(methods, 0);
	m0->selector = typedSelector;
	m0->types = typeCopy;
	m0->imp = imp;
	cls->methods = methods;
	update_cxx_method_cache(cls, methodName, imp);

	if (classHasDtable(cls))
	{
		add_method_list_to_class(cls, methods);
	}

	return YES;
}

BOOL class_addProtocol(Class cls, Protocol *protocol)
{
	CHECK_ARG(cls);
	CHECK_ARG(protocol);
	if (class_conformsToProtocol(cls, protocol)) { return NO; }
	struct objc_protocol_list *protocols =
		malloc(sizeof(struct objc_protocol_list) + sizeof(Protocol*));
	if (protocols == NULL) { return NO; }
	protocols->next = cls->protocols;
	protocols->count = 1;
	protocols->list[0] = protocol;
	cls->protocols = protocols;

	return YES;
}

Ivar * class_copyIvarList(Class cls, unsigned int *outCount)
{
	if (outCount != NULL) { *outCount = 0; }
	CHECK_ARG(cls);
	struct objc_ivar_list *ivarlist = cls->ivars;
	if ((ivarlist == NULL) || (ivarlist->count <= 0)) { return NULL; }

	size_t count = (size_t)ivarlist->count;
	if (count > UINT_MAX) { return NULL; }
	size_t allocationSize;
	if (!objc2_flexible_array_size(0, count + 1, sizeof(Ivar), &allocationSize))
	{
		return NULL;
	}
	Ivar *list = malloc(allocationSize);
	if (list == NULL) { return NULL; }
	for (size_t index = 0; index < count; index++)
	{
		list[index] = ivar_at_index(ivarlist, (int)index);
	}
	list[count] = NULL;
	if (outCount != NULL) { *outCount = (unsigned int)count; }
	return list;
}

Method * class_copyMethodList(Class cls, unsigned int *outCount)
{
	if (outCount != NULL) { *outCount = 0; }
	CHECK_ARG(cls);

	size_t count = 0;
	for (struct objc_method_list *methods = cls->methods; methods != NULL; methods = methods->next)
	{
		if (methods->count < 0) { return NULL; }
		size_t nodeCount = (size_t)methods->count;
		if (nodeCount > UINT_MAX - count) { return NULL; }
		count += nodeCount;
	}
	if (count == 0) { return NULL; }

	size_t allocationSize;
	if ((count == SIZE_MAX) ||
	    !objc2_flexible_array_size(0, count + 1, sizeof(Method), &allocationSize))
	{
		return NULL;
	}
	Method *list = malloc(allocationSize);
	if (list == NULL) { return NULL; }
	size_t out = 0;
	for (struct objc_method_list *methods = cls->methods; methods != NULL; methods = methods->next)
	{
		for (int index = 0; index < methods->count; index++)
		{
			list[out++] = method_at_index(methods, index);
		}
	}
	list[out] = NULL;
	if (outCount != NULL) { *outCount = (unsigned int)out; }
	return list;
}

Protocol*__unsafe_unretained* class_copyProtocolList(Class cls, unsigned int *outCount)
{
	if (outCount != NULL) { *outCount = 0; }
	CHECK_ARG(cls);

	size_t count = 0;
	for (struct objc_protocol_list *node = cls->protocols; node != NULL; node = node->next)
	{
		if ((node->count > UINT_MAX) || (node->count > UINT_MAX - count))
		{
			return NULL;
		}
		count += node->count;
	}
	if (count == 0) { return NULL; }

	size_t allocationSize;
	if ((count == SIZE_MAX) ||
	    !objc2_flexible_array_size(0, count + 1, sizeof(Protocol *), &allocationSize))
	{
		return NULL;
	}
	Protocol **protocols = malloc(allocationSize);
	if (protocols == NULL) { return NULL; }
	size_t out = 0;
	for (struct objc_protocol_list *node = cls->protocols; node != NULL; node = node->next)
	{
		if (node->count != 0)
		{
			memcpy(&protocols[out], node->list, node->count * sizeof(Protocol *));
			out += node->count;
		}
	}
	protocols[out] = NULL;
	if (outCount != NULL) { *outCount = (unsigned int)out; }
	return protocols;
}

id class_createInstance(Class cls, size_t extraBytes)
{
	CHECK_ARG(cls);
	if (sizeof(id) == 4)
	{
		if (cls == SmallObjectClasses[0])
		{
			return (id)1;
		}
	}
	else
	{
		for (int i=0 ; i<4 ; i++)
		{
			if (cls == SmallObjectClasses[i])
			{
				return (id)(uintptr_t)((i<<1)+1);
			}
		}
	}

	if (Nil == cls)	{ return nil; }
	// Don't try to allocate an object of size 0, because there's no space for
	// its isa pointer!
	if (cls->instance_size < sizeof(Class)) { return nil; }
	id obj = gc->allocate_class(cls, extraBytes);
	if (obj == nil) { return nil; }
	obj->isa = cls;
	checkARCAccessorsSlow(cls);
	call_cxx_construct(obj);
	return obj;
}

id objc_constructInstance(Class cls, void *bytes)
{
	if ((cls == Nil) || (bytes == NULL)) { return nil; }
	if (cls->instance_size < sizeof(Class)) { return nil; }

	id obj = (id)bytes;
	obj->isa = cls;
	call_cxx_construct(obj);
	return obj;
}

void *objc_destructInstance(id obj)
{
	if (obj == nil) { return NULL; }
	call_cxx_destruct(obj);
	return obj;
}

id object_copy(id obj, size_t size)
{
	if (obj == nil) { return nil; }
	Class cls = object_getClass(obj);
	if (cls == Nil) { return nil; }
	size_t instanceSize = class_getInstanceSize(cls);
	if ((size < instanceSize) || (size < sizeof(id))) { return nil; }
	id cpy = class_createInstance(cls, size - instanceSize);
	if (cpy == nil) { return nil; }
	if (size > sizeof(id))
	{
		memcpy(((char*)cpy + sizeof(id)), ((char*)obj + sizeof(id)),
		       size - sizeof(id));
	}
	return cpy;
}

id object_dispose(id obj)
{
	objc_destructInstance(obj);
	gc->free_object(obj);
	return nil;
}

Method class_getInstanceMethod(Class aClass, SEL aSelector)
{
	CHECK_ARG(aClass);
	CHECK_ARG(aSelector);
	// If the class has a dtable installed, then we can use the fast path
	if (classHasInstalledDtable(aClass))
	{
		// Do a dtable lookup to find out which class the method comes from.
		struct objc_slot2 *slot = objc_get_slot2(aClass, aSelector, NULL);
		if (NULL == slot)
		{
			slot = objc_get_slot2(aClass, sel_registerName(sel_getName(aSelector)), NULL);
			if (NULL == slot)
			{
				return NULL;
			}
		}
		// Slots are the same as methods.
		return (struct objc_method*)slot;
	}
	Method m = class_getInstanceMethodNonrecursive(aClass, aSelector);
	if (NULL != m)
	{
		return m;
	}
	return class_getInstanceMethod(class_getSuperclass(aClass), aSelector);
}

Method class_getClassMethod(Class aClass, SEL aSelector)
{
	return class_getInstanceMethod(object_getClass((id)aClass), aSelector);
}

Ivar class_getClassVariable(Class cls, const char* name)
{
	// Note: We don't have compiler support for cvars in ObjC
	return class_getInstanceVariable(object_getClass((id)cls), name);
}

size_t class_getInstanceSize(Class cls)
{
	if (Nil == cls) { return 0; }
	return cls->instance_size;
}

Ivar class_getInstanceVariable(Class cls, const char *name)
{
	if (name != NULL)
	{
		while (cls != Nil)
		{
			struct objc_ivar_list *ivarlist = cls->ivars;

			if (ivarlist != NULL)
			{
				for (int i = 0; i < ivarlist->count; i++)
				{
					Ivar ivar = ivar_at_index(ivarlist, i);
					if (strcmp(ivar->name, name) == 0)
					{
						return ivar;
					}
				}
			}
			cls = class_getSuperclass(cls);
		}
	}
	return NULL;
}

// The format of the char* is undocumented.  This function is only ever used in
// conjunction with class_setIvarLayout().
const uint8_t *class_getIvarLayout(Class cls)
{
	CHECK_ARG(cls);
	return (uint8_t*)cls->ivars;
}


const char * class_getName(Class cls)
{
	if (Nil == cls) { return "nil"; }
	return cls->name;
}

int class_getVersion(Class theClass)
{
	CHECK_ARG(theClass);
	return theClass->version;
}

const uint8_t *class_getWeakIvarLayout(Class cls)
{
	assert(0 && "Weak ivars not supported");
	return NULL;
}

BOOL class_isMetaClass(Class cls)
{
	CHECK_ARG(cls);
	return objc_test_class_flag(cls, objc_class_flag_meta);
}

IMP class_replaceMethod(Class cls, SEL name, IMP imp, const char *types)
{
	if (Nil == cls) { return (IMP)0; }
	SEL sel = sel_registerTypedName_np(sel_getName(name), types);
	Method method = class_getInstanceMethodNonrecursive(cls, sel);
	if (method == NULL)
	{
		class_addMethod(cls, sel, imp, types);
		return NULL;
	}
	IMP old = (IMP)method->imp;
	method->imp = imp;
	update_cxx_method_cache(cls, sel_getName(sel), imp);
	return old;
}


void class_setIvarLayout(Class cls, const uint8_t *layout)
{
	if ((Nil == cls) || (NULL == layout)) { return; }
	struct objc_ivar_list *list = (struct objc_ivar_list*)layout;
	size_t listsize = sizeof(struct objc_ivar_list) +
			sizeof(struct objc_ivar) * (list->count);
	cls->ivars = malloc(listsize);
	memcpy(cls->ivars, list, listsize);
}

__attribute__((deprecated))
Class class_setSuperclass(Class cls, Class newSuper)
{
	CHECK_ARG(cls);
	CHECK_ARG(newSuper);
	Class oldSuper;
	if (Nil == cls) { return Nil; }

	{
		LOCK_RUNTIME_FOR_SCOPE();

		oldSuper = cls->super_class;

		if (oldSuper == newSuper) { return newSuper; }

		safe_remove_from_subclass_list(cls);
		objc_resolve_class(newSuper);

		cls->super_class = newSuper;

		// The super class's subclass list is used in certain method resolution scenarios.
		cls->sibling_class = cls->super_class->subclass_list;
		cls->super_class->subclass_list = cls;

		if (UNLIKELY(class_isMetaClass(cls)))
		{
			// newSuper is presumably a metaclass. Its isa will therefore be the appropriate root metaclass.
			cls->isa = newSuper->isa;
		}
		else
		{
			Class meta = cls->isa, newSuperMeta = newSuper->isa;
			// Update the metaclass's superclass.
			safe_remove_from_subclass_list(meta);
			objc_resolve_class(newSuperMeta);

			meta->super_class = newSuperMeta;
			meta->isa = newSuperMeta->isa;

			// The super class's subclass list is used in certain method resolution scenarios.
			meta->sibling_class = newSuperMeta->subclass_list;
			newSuperMeta->subclass_list = meta;
		}

		LOCK_FOR_SCOPE(&initialize_lock);
		if (!objc_test_class_flag(cls, objc_class_flag_initialized))
		{
			// Uninitialized classes don't have dtables to update
			// and don't need their superclasses initialized.
			return oldSuper;
		}
	}

	objc_send_initialize((id)newSuper); // also initializes the metaclass
	objc_update_dtable_for_new_superclass(cls->isa, newSuper->isa);
	objc_update_dtable_for_new_superclass(cls, newSuper);

	return oldSuper;
}

void class_setVersion(Class theClass, int version)
{
	if (Nil == theClass) { return; }
	theClass->version = version;
}

void class_setWeakIvarLayout(Class cls, const uint8_t *layout)
{
	assert(0 && "Not implemented");
}

const char * ivar_getName(Ivar ivar)
{
	CHECK_ARG(ivar);
	return ivar->name;
}

ptrdiff_t ivar_getOffset(Ivar ivar)
{
	CHECK_ARG(ivar);
	return *ivar->offset;
}

const char * ivar_getTypeEncoding(Ivar ivar)
{
	CHECK_ARG(ivar);
	return ivar->type;
}


void method_exchangeImplementations(Method m1, Method m2)
{
	if (NULL == m1 || NULL == m2) { return; }
	IMP tmp = (IMP)m1->imp;
	m1->imp = m2->imp;
	m2->imp = tmp;
}

IMP method_getImplementation(Method method)
{
	if (NULL == method) { return (IMP)NULL; }
	return (IMP)method->imp;
}

SEL method_getName(Method method)
{
	if (NULL == method) { return (SEL)NULL; }
	return (SEL)method->selector;
}


IMP method_setImplementation(Method method, IMP imp)
{
	if (NULL == method) { return (IMP)NULL; }
	IMP old = (IMP)method->imp;
	method->imp = imp;
	return old;
}

Class objc_getRequiredClass(const char *name)
{
	CHECK_ARG(name);
	Class cls = (Class)objc_getClass(name);
	if (nil == cls)
	{
		abort();
	}
	return cls;
}

PRIVATE void freeMethodLists(Class aClass)
{
	struct objc_method_list *methods = aClass->methods;
	while(methods != NULL)
	{
		for (int i=0 ; i<methods->count ; i++)
		{
			free((void*)method_at_index(methods, i)->types);
		}
		struct objc_method_list *current = methods;
	   	methods = methods->next;
		free(current);
	}
}

PRIVATE void freeIvarLists(Class aClass)
{
	struct objc_ivar_list *ivarlist = aClass->ivars;
	if (NULL == ivarlist) { return; }

	if ((ivarlist->count > 0) &&
	    objc_test_class_flag(aClass, objc_class_flag_owned_ivar_offsets))
	{
		// Registered dynamically-created classes own one contiguous offset array.
		free(ivar_at_index(ivarlist, 0)->offset);
	}

	for (int i=0 ; i<ivarlist->count ; i++)
	{
		Ivar ivar = ivar_at_index(ivarlist, i);
		free((void*)ivar->type);
		free((void*)ivar->name);
	}
	free(ivarlist);
}

/*
 * Removes a class from the subclass list found on its super class.
 * Must be called with the objc runtime mutex locked.
 */
static inline void safe_remove_from_subclass_list(Class cls)
{
	// If this class hasn't been added to the class hierarchy, then this is easy
	if (!objc_test_class_flag(cls, objc_class_flag_resolved)) { return; }
	if (cls->super_class == Nil) { return; }
	Class sub = cls->super_class->subclass_list;
	if (sub == cls)
	{
		cls->super_class->subclass_list = cls->sibling_class;
	}
	else
	{
		while (sub != NULL)
		{
			if (sub->sibling_class == cls)
			{
				sub->sibling_class = cls->sibling_class;
				break;
			}
			sub = sub->sibling_class;
		}
	}
}

void objc_disposeClassPair(Class cls)
{
	if (0 == cls) { return; }
	Class meta = ((id)cls)->isa;
	// Remove from the runtime system so nothing tries updating the dtable
	// while we are freeing the class.
	{
		LOCK_RUNTIME_FOR_SCOPE();
		safe_remove_from_subclass_list(meta);
		safe_remove_from_subclass_list(cls);
		if (objc_lookUpClass(cls->name) == cls)
		{
			class_table_remove(cls);
		}
	}

	// Free the method and ivar lists.
	freeMethodLists(cls);
	freeMethodLists(meta);
	freeIvarLists(cls);
	if (cls->dtable != uninstalled_dtable)
	{
		free_dtable(cls->dtable);
	}
	if (meta->dtable != uninstalled_dtable)
	{
		free_dtable(meta->dtable);
	}

	// User-created class and metaclass share one runtime-owned name copy.
	free((void*)cls->name);
	// Free the class and metaclass
	gc->free(meta);
	gc->free(cls);
}

Class objc_allocateClassPair(Class superclass, const char *name, size_t extraBytes)
{
	if (name == NULL) { return Nil; }
	// Check the class doesn't already exist.
	if (nil != objc_lookUpClass(name)) { return Nil; }

	size_t classSize;
	if (!objc2_size_add(sizeof(struct objc_class), extraBytes, &classSize) ||
	    (classSize > (size_t)PTRDIFF_MAX))
	{
		return Nil;
	}

	Class newClass = gc->malloc(classSize);
	if (Nil == newClass) { return Nil; }

	Class metaClass = gc->malloc(sizeof(struct objc_class));
	if (Nil == metaClass)
	{
		gc->free(newClass);
		return Nil;
	}

	char *nameCopy = objc2_strdup(name);
	if (nameCopy == NULL)
	{
		gc->free(metaClass);
		gc->free(newClass);
		return Nil;
	}

	if (Nil == superclass)
	{
		metaClass->isa = metaClass;
		metaClass->super_class = newClass;
	}
	else
	{
		metaClass->isa = superclass->isa;
		metaClass->super_class = superclass->isa;
	}
	metaClass->name = nameCopy;
	metaClass->info = objc_class_flag_meta | objc_class_flag_user_created;
	metaClass->dtable = uninstalled_dtable;
	metaClass->instance_size = sizeof(struct objc_class);

	newClass->isa = metaClass;
	newClass->super_class = superclass;
	newClass->name = nameCopy;
	newClass->info = objc_class_flag_user_created;
	newClass->dtable = uninstalled_dtable;
	newClass->abi_version = 2;
	metaClass->abi_version = 2;
	newClass->instance_size = (Nil == superclass)
		? sizeof(struct objc_class*) : superclass->instance_size;

	return newClass;
}


void *object_getIndexedIvars(id obj)
{
	CHECK_ARG(obj);
	size_t size = classForObject(obj)->instance_size;
	if ((0 == size) && class_isMetaClass(classForObject(obj)))
	{
		size = sizeof(struct objc_class);
	}
	return ((char*)obj) + size;
}

Class object_getClass(id obj)
{
	CHECK_ARG(obj);
	Class isa = classForObject(obj);
	while ((Nil != isa) && objc_test_class_flag(isa, objc_class_flag_hidden_class))
	{
		isa = isa->super_class;
	}
	return isa;
}

Class object_setClass(id obj, Class cls)
{
	CHECK_ARG(obj);
	// If this is a small object, then don't set its class.
	if (isSmallObject(obj)) { return classForObject(obj); }
	Class oldClass =  obj->isa;
	obj->isa = cls;
	return oldClass;
}

const char *object_getClassName(id obj)
{
	CHECK_ARG(obj);
	return class_getName(object_getClass(obj));
}

void objc_registerClassPair(Class cls)
{
	if (cls == Nil) { return; }
	if ((cls->ivars != NULL) &&
	    !objc_test_class_flag(cls, objc_class_flag_owned_ivar_offsets))
	{
		if (cls->ivars->count < 0) { return; }
		size_t count = (size_t)cls->ivars->count;
		size_t bytes;
		if (!objc2_size_multiply(count, sizeof(int), &bytes)) { return; }
		int *ptrs = count == 0 ? NULL : calloc(1, bytes);
		if ((count != 0) && (ptrs == NULL)) { return; }
		for (size_t i = 0; i < count; i++)
		{
			ptrs[i] = (int)(intptr_t)ivar_at_index(cls->ivars, (int)i)->offset;
		}
		for (size_t i = 0; i < count; i++)
		{
			ivar_at_index(cls->ivars, (int)i)->offset = &ptrs[i];
		}
		objc_set_class_flag(cls, objc_class_flag_owned_ivar_offsets);
	}
	LOCK_RUNTIME_FOR_SCOPE();
	class_table_insert(cls);
	objc_resolve_class(cls);
}

