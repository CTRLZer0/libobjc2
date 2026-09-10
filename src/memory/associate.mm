#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "objc/runtime.h"
#include "objc/objc-arc.h"
#include "nsobject.h"
#include "spinlock.h"
#include "class.h"
#include "dtable.h"
#include "selector.h"
#include "lock.h"
#include "gc_ops.h"
#include "helpers.hh"

/**
 * A single associative reference.  Contains the key, value, and association
 * policy.
 */
struct reference
{
	/**
	 * Even while stable, odd while a writer is publishing a new tuple.
	 */
	uintptr_t generation;
	/**
	 * Key, value, and policy are accessed atomically so lock-free readers never
	 * race a writer. The generation field makes the three values one snapshot.
	 */
	const void *key;
	void *object;
	uintptr_t policy;
};

#define REFERENCE_LIST_SIZE 10

/**
 * Linked list of references associated with an object.  We assume that there
 * won't be very many, so we don't bother with a proper hash table, and just
 * iterate over a list.
 */
struct reference_list
{
	/**
	 * Next group of references.  This is only ever used if we have more than
	 * 10 references associated with an object, which seems highly unlikely.
	 */
	struct reference_list *next;
	/**
	 * Mutex.  Only set for the first reference list in a chain.  It serializes
	 * writers and also backs @synchronize().
	 */
	mutex_t lock;
	BOOL removing;
	BOOL deallocating;
	/**
	 * Array of references.
	 */
	struct reference list[REFERENCE_LIST_SIZE];
};
static inline const void *loadReferenceKey(const struct reference *r)
{
	return __atomic_load_n(&r->key, __ATOMIC_ACQUIRE);
}

static inline struct reference_list *loadNextReferenceList(
	const struct reference_list *list)
{
	return __atomic_load_n(&list->next, __ATOMIC_ACQUIRE);
}

static struct reference* findReferenceLocked(struct reference_list *list,
                                              const void *key)
{
	while (list != NULL)
	{
		for (int i = 0; i < REFERENCE_LIST_SIZE; ++i)
		{
			if (__atomic_load_n(&list->list[i].key, __ATOMIC_RELAXED) == key)
			{
				return &list->list[i];
			}
		}
		list = __atomic_load_n(&list->next, __ATOMIC_RELAXED);
	}
	return NULL;
}

static void publishReference(struct reference *r, const void *key,
                             void *object, uintptr_t policy)
{
	uintptr_t generation =
		__atomic_fetch_add(&r->generation, 1, __ATOMIC_ACQ_REL);
	assert((generation & 1) == 0);
	__atomic_store_n(&r->object, object, __ATOMIC_RELAXED);
	__atomic_store_n(&r->policy, policy, __ATOMIC_RELAXED);
	__atomic_store_n(&r->key, key, __ATOMIC_RELAXED);
	__atomic_fetch_add(&r->generation, 1, __ATOMIC_RELEASE);
}

static BOOL snapshotReference(struct reference *r, const void *key,
                              void **object, uintptr_t *policy)
{
	for (;;)
	{
		uintptr_t before = __atomic_load_n(&r->generation, __ATOMIC_ACQUIRE);
		if (before & 1) { continue; }
		const void *observedKey = __atomic_load_n(&r->key, __ATOMIC_RELAXED);
		void *observedObject = __atomic_load_n(&r->object, __ATOMIC_RELAXED);
		uintptr_t observedPolicy = __atomic_load_n(&r->policy, __ATOMIC_RELAXED);
		uintptr_t after = __atomic_load_n(&r->generation, __ATOMIC_ACQUIRE);
		if (before != after) { continue; }
		if (observedKey != key) { return NO; }
		*object = observedObject;
		*policy = observedPolicy;
		return YES;
	}
}

static BOOL findReferenceSnapshot(struct reference_list *list, const void *key,
                                  void **object, uintptr_t *policy)
{
	while (list != NULL)
	{
		for (int i = 0; i < REFERENCE_LIST_SIZE; ++i)
		{
			struct reference *r = &list->list[i];
			if (loadReferenceKey(r) != key) { continue; }
			if (snapshotReference(r, key, object, policy)) { return YES; }
		}
		list = loadNextReferenceList(list);
	}
	return NO;
}
static void cleanupReferenceList(struct reference_list *list)
{
	for (struct reference_list *node = list; node != NULL;
	     node = loadNextReferenceList(node))
	{
		for (int i = 0; i < REFERENCE_LIST_SIZE; ++i)
		{
			struct reference *r = &node->list[i];
			if (__atomic_load_n(&r->key, __ATOMIC_RELAXED) == NULL) { continue; }
			void *object = __atomic_load_n(&r->object, __ATOMIC_RELAXED);
			uintptr_t policy = __atomic_load_n(&r->policy, __ATOMIC_RELAXED);
			publishReference(r, NULL, NULL, OBJC_ASSOCIATION_ASSIGN);
			if ((object != NULL) && (policy != OBJC_ASSOCIATION_ASSIGN))
			{
				objc_release((id)object);
			}
		}
	}
}

static void freeReferenceList(struct reference_list *list)
{
	while (list != NULL)
	{
		struct reference_list *next =
			__atomic_load_n(&list->next, __ATOMIC_RELAXED);
		free(list);
		list = next;
	}
}

static BOOL prepareReferenceValue(void **obj, uintptr_t policy)
{
	switch (policy)
	{
		default: return NO;
		case OBJC_ASSOCIATION_COPY_NONATOMIC:
		case OBJC_ASSOCIATION_COPY:
			*obj = [(id)*obj copy];
			return YES;
		case OBJC_ASSOCIATION_RETAIN_NONATOMIC:
		case OBJC_ASSOCIATION_RETAIN:
			*obj = objc_retain((id)*obj);
			return YES;
		case OBJC_ASSOCIATION_ASSIGN:
			return YES;
	}
}

static void setReference(struct reference_list *list,
                         const void *key,
                         void *obj,
                         uintptr_t policy)
{
	if ((list == NULL) || !prepareReferenceValue(&obj, policy)) { return; }

	void *oldObject = NULL;
	uintptr_t oldPolicy = OBJC_ASSOCIATION_ASSIGN;
	BOOL installed = NO;
	LOCK(&list->lock);
	if (!list->deallocating && !list->removing)
	{
		struct reference *r = findReferenceLocked(list, key);
		if ((r == NULL) && (obj != NULL))
		{
			r = findReferenceLocked(list, NULL);
			if (r == NULL)
			{
				struct reference_list *tail = list;
				struct reference_list *next =
					__atomic_load_n(&tail->next, __ATOMIC_RELAXED);
				while (next != NULL)
				{
					tail = next;
					next = __atomic_load_n(&tail->next, __ATOMIC_RELAXED);
				}
				next = allocate_zeroed<struct reference_list>();
				if (next != NULL)
				{
					__atomic_store_n(&tail->next, next, __ATOMIC_RELEASE);
					r = &next->list[0];
				}
			}
		}
		if (r != NULL)
		{
			oldObject = __atomic_load_n(&r->object, __ATOMIC_RELAXED);
			oldPolicy = __atomic_load_n(&r->policy, __ATOMIC_RELAXED);
			publishReference(r,
				obj == NULL ? NULL : key,
				obj,
				obj == NULL ? OBJC_ASSOCIATION_ASSIGN : policy);
			installed = obj != NULL;
		}
	}
	UNLOCK(&list->lock);

	if (!installed && (obj != NULL) && (policy != OBJC_ASSOCIATION_ASSIGN))
	{
		objc_release((id)obj);
	}
	if ((oldObject != NULL) && (oldPolicy != OBJC_ASSOCIATION_ASSIGN))
	{
		objc_release((id)oldObject);
	}
}

static void deallocHiddenClass(id obj, SEL _cmd);

static inline Class findHiddenClass(id obj)
{
	Class cls = obj->isa;
	while (Nil != cls && 
	       !objc_test_class_flag(cls, objc_class_flag_assoc_class))
	{
		cls = class_getSuperclass(cls);
	}
	return cls;
}

static Class allocateHiddenClass(Class superclass)
{
	checkARCAccessorsSlow(superclass);
	const unsigned long lifetimeFlags = superclass->info &
		(objc_class_flag_fast_arc | objc_class_flag_permanent_instances |
		 objc_class_flag_is_block);
	struct objc_class *newClass =
		allocate_zeroed<struct objc_class>(sizeof(struct reference_list));

	if (Nil == newClass) { return Nil; }

	// Set up the new class
	newClass->isa = superclass->isa;
	newClass->name = superclass->name;
	// Uncomment this for debugging: it makes it easier to track which hidden
	// class is which
	// static int count;
	//asprintf(&newClass->name, "%s%d", superclass->name, count++);
	newClass->info = objc_class_flag_resolved | objc_class_flag_user_created |
		objc_class_flag_hidden_class | objc_class_flag_assoc_class | lifetimeFlags;
	newClass->super_class = superclass;
	newClass->dtable = uninstalled_dtable;
	newClass->instance_size = superclass->instance_size;

	return (Class)newClass;
}

static inline Class initHiddenClassForObject(id obj)
{
	if ((obj == nil) || class_isMetaClass(obj->isa)) { return Nil; }
	Class superclass = obj->isa;
	Class hiddenClass = allocateHiddenClass(superclass);
	if (hiddenClass == Nil) { return Nil; }
	static SEL cxx_destruct = sel_registerName(".cxx_destruct");
	const char *types = sizeof(void*) == 4 ? "v8@0:4" : "v16@0:8";
	if ((cxx_destruct == NULL) || !class_addMethod(hiddenClass, cxx_destruct,
		(IMP)deallocHiddenClass, types))
	{
		freeMethodLists(hiddenClass);
		freeIvarLists(hiddenClass);
		free(hiddenClass);
		return Nil;
	}
	{
		LOCK_RUNTIME_FOR_SCOPE();
		hiddenClass->sibling_class = superclass->subclass_list;
		superclass->subclass_list = hiddenClass;
	}
	obj->isa = hiddenClass;
	return hiddenClass;
}

static void deallocHiddenClass(id obj, SEL _cmd)
{
	LOCK_RUNTIME_FOR_SCOPE();
	Class hiddenClass = findHiddenClass(obj);
	// After calling [super dealloc], the object will no longer exist.
	// Free the hidden class.
	struct reference_list *list = static_cast<struct reference_list *>(object_getIndexedIvars(hiddenClass));
	LOCK(&list->lock);
	list->deallocating = YES;
	cleanupReferenceList(list);
	UNLOCK(&list->lock);
	DESTROY_LOCK(list->lock);
	freeReferenceList(__atomic_load_n(&list->next, __ATOMIC_RELAXED));
	//fprintf(stderr, "Deallocating dtable %p\n", hiddenClass->dtable);
	if (hiddenClass->dtable != uninstalled_dtable) { free_dtable(hiddenClass->dtable); }
	// We shouldn't have any subclasses left at this point
	assert(hiddenClass->subclass_list == 0);
	// Remove the class from the subclass list of its superclass
	Class sub = hiddenClass->super_class->subclass_list;
	if (sub == hiddenClass)
	{
		hiddenClass->super_class->subclass_list = hiddenClass->sibling_class;
	}
	else
	{
		while (sub != NULL)
		{
			if ((Class)sub->sibling_class == hiddenClass)
			{
				sub->sibling_class = hiddenClass->sibling_class;
				break;
			}
			sub = sub->sibling_class;
		}
	}
	obj->isa = hiddenClass->super_class;
	// Free the introspection structures:
	freeMethodLists(hiddenClass);
	freeIvarLists(hiddenClass);
	// Free the class
	free(hiddenClass);
}

static struct reference_list* referenceListForObject(id object, BOOL create)
{
	if (class_isMetaClass(object->isa))
	{
		Class cls = (Class)object;
		if ((NULL == cls->extra_data) && create)
		{
			struct reference_list *list = allocate_zeroed<struct reference_list>();
			if (list == NULL) { return NULL; }
			auto guard = acquire_locks_for_pointers(cls);
			if (NULL == cls->extra_data)
			{
				INIT_LOCK(list->lock);
				cls->extra_data = list;
			}
			else
			{
				free(list);
			}
		}
		return cls->extra_data;
	}
	Class hiddenClass = findHiddenClass(object);
	if ((NULL == hiddenClass) && create)
	{
		auto guard = acquire_locks_for_pointers(object);
		hiddenClass = findHiddenClass(object);
		if (NULL == hiddenClass)
		{
			hiddenClass = initHiddenClassForObject(object);
			if (hiddenClass == Nil) { return NULL; }
			struct reference_list *list = static_cast<struct reference_list *>(object_getIndexedIvars(hiddenClass));
			INIT_LOCK(list->lock);
		}
	}
	return hiddenClass ? static_cast<struct reference_list*>(object_getIndexedIvars(hiddenClass)) : nullptr;
}

void objc_setAssociatedObject(id object,
                              const void *key,
                              id value,
                              objc_AssociationPolicy policy)
{
	if (isSmallObject(object)) { return; }
	struct reference_list *list = referenceListForObject(object, YES);
	if (list != NULL) { setReference(list, key, value, policy); }
}

static id getReferenceRetained(struct reference_list *list, const void *key)
{
	LOCK(&list->lock);
	struct reference *r = findReferenceLocked(list, key);
	id value = nil;
	if (r != NULL)
	{
		value = (id)__atomic_load_n(&r->object, __ATOMIC_RELAXED);
		uintptr_t policy = __atomic_load_n(&r->policy, __ATOMIC_RELAXED);
		if ((value != nil) && (policy & OBJC_ASSOCIATION_RETAIN_NONATOMIC))
		{
			objc_retainAutorelease(value);
		}
	}
	UNLOCK(&list->lock);
	return value;
}

static id getReference(struct reference_list *list, const void *key)
{
	if (list == NULL) { return nil; }
	void *object = NULL;
	uintptr_t policy = OBJC_ASSOCIATION_ASSIGN;
	if (!findReferenceSnapshot(list, key, &object, &policy)) { return nil; }
	if ((object != NULL) && (policy & OBJC_ASSOCIATION_RETAIN_NONATOMIC))
	{
		return getReferenceRetained(list, key);
	}
	return (id)object;
}

id objc_getAssociatedObject(id object, const void *key)
{
	if (isSmallObject(object)) { return nil; }
	struct reference_list *list = referenceListForObject(object, NO);
	if (NULL == list) { return nil; }
	id value = getReference(list, key);
	if (value != nil) { return value; }
	if (class_isMetaClass(object->isa))
	{
		return nil;
	}
	Class cls = object->isa;
	while (Nil != cls)
	{
		while (Nil != cls && 
			   !objc_test_class_flag(cls, objc_class_flag_assoc_class))
		{
			cls = class_getSuperclass(cls);
		}
		if (Nil != cls)
		{
			struct reference_list *next_list = static_cast<struct reference_list *>(object_getIndexedIvars(cls));
			if (list != next_list)
			{
				list = next_list;
				id inherited = getReference(list, key);
				if (inherited != nil) { return inherited; }
			}
			cls = class_getSuperclass(cls);
		}
	}
	return nil;
}


void objc_removeAssociatedObjects(id object)
{
	if (isSmallObject(object)) { return; }
	struct reference_list *list = referenceListForObject(object, NO);
	if (list == NULL) { return; }
	LOCK(&list->lock);
	list->removing = YES;
	cleanupReferenceList(list);
	list->removing = NO;
	UNLOCK(&list->lock);
}

OBJC_PUBLIC
int objc_sync_enter(id object)
{
	if ((object == 0) || isSmallObject(object)) { return 0; }
	struct reference_list *list = referenceListForObject(object, YES);
	if (list == NULL) { return 1; }
	LOCK(&list->lock);
	return 0;
}

OBJC_PUBLIC
int objc_sync_exit(id object)
{
	if ((object == 0) || isSmallObject(object)) { return 0; }
	struct reference_list *list = referenceListForObject(object, NO);
	if (NULL != list)
	{
		UNLOCK(&list->lock);
		return 0;
	}
	return 1;
}

static Class hiddenClassForObject(id object)
{
	if (isSmallObject(object)) { return nil; }
	if (class_isMetaClass(object->isa))
	{
		return object->isa;
	}
	Class hiddenClass = findHiddenClass(object);
	if (NULL == hiddenClass)
	{
		auto guard = acquire_locks_for_pointers(object);
		hiddenClass = findHiddenClass(object);
		if (NULL == hiddenClass)
		{
			hiddenClass = initHiddenClassForObject(object);
			if (hiddenClass == Nil) { return Nil; }
			struct reference_list *list = static_cast<struct reference_list*>(object_getIndexedIvars(hiddenClass));
			INIT_LOCK(list->lock);
		}
	}
	return hiddenClass;
}

BOOL object_addMethod_np(id object, SEL name, IMP imp, const char *types)
{
	return class_addMethod(hiddenClassForObject(object), name, imp, types);
}

IMP object_replaceMethod_np(id object, SEL name, IMP imp, const char *types)
{
	return class_replaceMethod(hiddenClassForObject(object), name, imp, types);
}
static char prototypeKey;

id object_clone_np(id object)
{
	if (isSmallObject(object)) { return object; }
	// Make sure that the prototype has a hidden class, so that methods added
	// to it will appear in the clone.
	if (referenceListForObject(object, YES) == NULL) { return nil; }
	id newInstance = class_createInstance(object->isa, 0);
	if (newInstance == nil) { return nil; }
	Class hiddenClass = initHiddenClassForObject(newInstance);
	if (hiddenClass == Nil) { object_dispose(newInstance); return nil; }
	struct reference_list *list = static_cast<struct reference_list*>(object_getIndexedIvars(hiddenClass));
	INIT_LOCK(list->lock);
	objc_setAssociatedObject(newInstance, &prototypeKey, object,
			OBJC_ASSOCIATION_RETAIN_NONATOMIC);
	return newInstance;
}

id object_getPrototype_np(id object)
{
	return objc_getAssociatedObject(object, &prototypeKey);
}
