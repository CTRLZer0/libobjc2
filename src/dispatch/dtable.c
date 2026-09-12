#define __BSD_VISIBLE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "objc/runtime.h"
#include "objc/support/hooks.h"
#include "sarray2.h"
#include "selector.h"
#include "class.h"
#include "lock.h"
#include "method.h"
#include "dtable.h"
#include "visibility.h"
#include "asmconstants.h"
#include "tracing.h"
#include "observability.h"

_Static_assert(__builtin_offsetof(struct objc_class, dtable) == DTABLE_OFFSET,
		"Incorrect dtable offset for assembly");
_Static_assert(__builtin_offsetof(SparseArray, shift) == SHIFT_OFFSET,
		"Incorrect shift offset for assembly");
_Static_assert(__builtin_offsetof(SparseArray, data) == DATA_OFFSET,
		"Incorrect data offset for assembly");
// Slots are now a public interface to part of the method structure, so make
// sure that it's safe to use method and slot structures interchangeably.
_Static_assert(__builtin_offsetof(struct objc_slot2, method) == SLOT_OFFSET,
		"Incorrect slot offset for assembly");
_Static_assert(__builtin_offsetof(struct objc_method, imp) == SLOT_OFFSET,
		"Incorrect slot offset for assembly");

static inline SparseArray *new_sparse_array_or_abort(uint32_t depth)
{
	SparseArray *array = SparseArrayNewWithDepth(depth);
	if (array == NULL) { abort(); }
	return array;
}

static inline SparseArray *copy_sparse_array_or_abort(SparseArray *array)
{
	SparseArray *copy = SparseArrayCopy(array);
	if (copy == NULL) { abort(); }
	return copy;
}

static inline void sparse_array_insert_or_abort(SparseArray *array, uint32_t index, void *value)
{
	if (!SparseArrayInsert(array, index, value)) { abort(); }
}
PRIVATE dtable_t uninstalled_dtable;
#ifdef OBJC2_TRACING_SUPPORTED
PRIVATE dtable_t tracing_dtable;
#endif
#ifndef ENOTSUP
#	define ENOTSUP -1
#endif

/** Head of the list of temporary dtables.  Protected by initialize_lock. */
PRIVATE InitializingDtable *temporary_dtables;
/** Lock used to protect the temporary dtables list. */
PRIVATE mutex_t initialize_lock;
/** The size of the largest dtable.  This is a sparse array shift value, so is
 * 2^x in increments of 8. */
static uint32_t dtable_depth = 8;

#ifndef NO_SAFE_CACHING
_Atomic(uint64_t) objc_method_cache_version;
#endif

/**
 * Starting at `cls`, finds the class that provides the implementation of the
 * method identified by `sel`.
 */
static Class ownerForMethod(Class cls, SEL sel)
{
	struct objc_slot2 *slot = objc_get_slot2(cls, sel, NULL);
	if (slot == NULL)
	{
		return Nil;
	}
	if (cls->super_class == NULL)
	{
		return cls;
	}
	if (objc_get_slot2(cls->super_class, sel, NULL) == slot)
	{
		return ownerForMethod(cls->super_class, sel);
	}
	return cls;
}

/**
 * Returns YES if the class implements a method for the specified selector, NO
 * otherwise.
 */
static BOOL ownsMethod(Class cls, SEL sel)
{
	return ownerForMethod(cls, sel) == cls;
}


#ifdef DEBUG_ARC_COMPAT
#define ARC_DEBUG_LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define ARC_DEBUG_LOG(...) do {} while(0)
#endif

/**
 * Check whether this class pair implement or override `+alloc`,
 * `+allocWithZone`, or `-init` in a way that requires the methods to be
 * called.
 */
static void checkFastAllocInit(Class cls)
{
	// This needs to be called on the class, not the metaclass
	if (class_isMetaClass(cls))
	{
		return;
	}
	static SEL allocStorage, allocWithZoneStorage, initStorage, trivialStorage;
	SEL alloc = objc2_get_or_register_selector(&allocStorage, "alloc");
	SEL allocWithZone = objc2_get_or_register_selector(
		&allocWithZoneStorage, "allocWithZone:");
	SEL init = objc2_get_or_register_selector(&initStorage, "init");
	SEL isTrivialAllocInit = objc2_get_or_register_selector(
		&trivialStorage, "_TrivialAllocInit");
	Class metaclass = cls->isa;
	Class isTrivialOwner = ownerForMethod(metaclass, isTrivialAllocInit);
	// If nothing in this hierarchy opts in to trivial alloc / init behaviour, give up.
	if (isTrivialOwner == nil)
	{
		objc_clear_class_flag(cls, objc_class_flag_fast_alloc_init);
		objc_clear_class_flag(metaclass, objc_class_flag_fast_alloc_init);
		return;
	}
	// Check for overrides of alloc or allocWithZone:.
	// This check has some false negatives.  If you override only one of alloc
	// or allocWithZone, both will hit the slow path.  That's fine because the
	// fast path is an optimisation, not a guarantee.
	Class allocOwner = ownerForMethod(metaclass, alloc);
	Class allocWithZoneOwner = ownerForMethod(metaclass, allocWithZone);
	if (((allocOwner == nil) || (allocOwner == isTrivialOwner)) &&
	    ((allocWithZoneOwner == nil) || (allocWithZoneOwner == isTrivialOwner)))
	{
		objc_set_class_flag(metaclass, objc_class_flag_fast_alloc_init);
	}
	else
	{
		objc_clear_class_flag(metaclass, objc_class_flag_fast_alloc_init);
	}
	Class initOwner = ownerForMethod(cls, init);
	if ((initOwner == nil) || (initOwner->isa == isTrivialOwner))
	{
		objc_set_class_flag(cls, objc_class_flag_fast_alloc_init);
	}
	else
	{
		objc_clear_class_flag(cls, objc_class_flag_fast_alloc_init);
	}
}

/**
 * Checks whether the class implements memory management methods, and whether
 * they are safe to use with ARC.
 */
static void checkARCAccessors(Class cls)
{
	checkFastAllocInit(cls);
	static SEL retainStorage, releaseStorage, autoreleaseStorage, arcStorage;
	SEL retain = objc2_get_or_register_selector(&retainStorage, "retain");
	SEL release = objc2_get_or_register_selector(&releaseStorage, "release");
	SEL autorelease = objc2_get_or_register_selector(
		&autoreleaseStorage, "autorelease");
	SEL isARC = objc2_get_or_register_selector(
		&arcStorage, "_ARCCompliantRetainRelease");
	Class owner = ownerForMethod(cls, retain);
	if ((NULL != owner) && !ownsMethod(owner, isARC))
	{
		ARC_DEBUG_LOG("%s does not support ARC correctly (implements retain)\n", cls->name);
		objc_clear_class_flag(cls, objc_class_flag_fast_arc);
		return;
	}
	owner = ownerForMethod(cls, release);
	if ((NULL != owner) && !ownsMethod(owner, isARC))
	{
		ARC_DEBUG_LOG("%s does not support ARC correctly (implements release)\n", cls->name);
		objc_clear_class_flag(cls, objc_class_flag_fast_arc);
		return;
	}
	owner = ownerForMethod(cls, autorelease);
	if ((NULL != owner) && !ownsMethod(owner, isARC))
	{
		ARC_DEBUG_LOG("%s does not support ARC correctly (implements autorelease)\n", cls->name);
		objc_clear_class_flag(cls, objc_class_flag_fast_arc);
		return;
	}
	objc_set_class_flag(cls, objc_class_flag_fast_arc);
}

static BOOL selEqualUnTyped(SEL expected, SEL untyped)
{
	return (expected->index == untyped->index)
#ifdef TYPE_DEPENDENT_DISPATCH
		|| (get_untyped_idx(expected) == untyped->index)
#endif
		;
}

PRIVATE void checkARCAccessorsSlow(Class cls)
{
	if (cls->dtable != uninstalled_dtable)
	{
		return;
	}
	static SEL retainStorage, releaseStorage, autoreleaseStorage, arcStorage;
	SEL retain = objc2_get_or_register_selector(&retainStorage, "retain");
	SEL release = objc2_get_or_register_selector(&releaseStorage, "release");
	SEL autorelease = objc2_get_or_register_selector(
		&autoreleaseStorage, "autorelease");
	SEL isARC = objc2_get_or_register_selector(
		&arcStorage, "_ARCCompliantRetainRelease");
	BOOL superIsFast = YES;
	if (cls->super_class != Nil)
	{
		checkARCAccessorsSlow(cls->super_class);
		superIsFast = objc_test_class_flag(cls->super_class, objc_class_flag_fast_arc);
	}
	BOOL selfImplementsRetainRelease = NO;
	for (struct objc_method_list *l=cls->methods ; l != NULL ; l= l->next)
	{
		for (int i=0 ; i<l->count ; i++)
		{
			SEL s = method_at_index(l, i)->selector;
			if (selEqualUnTyped(s, retain) ||
			    selEqualUnTyped(s, release) ||
			    selEqualUnTyped(s, autorelease))
			{
				selfImplementsRetainRelease = YES;
			}
			else if (selEqualUnTyped(s, isARC))
			{
				objc_set_class_flag(cls, objc_class_flag_fast_arc);
				return;
			}
		}
	}
	if (superIsFast && !selfImplementsRetainRelease)
	{
		objc_set_class_flag(cls, objc_class_flag_fast_arc);
	}
}

static void collectMethodsForMethodListToSparseArray(
		struct objc_method_list *list,
		SparseArray *sarray,
		BOOL recurse)
{
	if (recurse && (NULL != list->next))
	{
		collectMethodsForMethodListToSparseArray(list->next, sarray, YES);
	}
	for (unsigned i=0 ; i<list->count ; i++)
	{
		sparse_array_insert_or_abort(sarray, method_at_index(list, i)->selector->index,
				(void*)method_at_index(list, i));
	}
}


PRIVATE void init_dispatch_tables ()
{
	INIT_LOCK(initialize_lock);
	uninstalled_dtable = new_sparse_array_or_abort(dtable_depth);
#ifdef OBJC2_TRACING_SUPPORTED
	tracing_dtable = new_sparse_array_or_abort(dtable_depth);
#endif
}

#ifdef OBJC2_TRACING_SUPPORTED
#define TRACE_CONTEXT_WORDS 6
#define TRACE_CONTEXT_DEPTH 512

static __thread void *trace_return_stack[TRACE_CONTEXT_WORDS * TRACE_CONTEXT_DEPTH];
static __thread size_t trace_return_depth;

PRIVATE void *pushTraceReturnStack(void)
{
	if (trace_return_depth == TRACE_CONTEXT_DEPTH) { abort(); }
	void **context = trace_return_stack + (trace_return_depth * TRACE_CONTEXT_WORDS);
	trace_return_depth++;
	return context;
}

PRIVATE void *popTraceReturnStack(void)
{
	if (trace_return_depth == 0) { abort(); }
	trace_return_depth--;
	return trace_return_stack + (trace_return_depth * TRACE_CONTEXT_WORDS);
}

PRIVATE objc_tracing_hook objc2_tracing_hook_for_selector(SEL selector)
{
	if (selector == NULL) { return NULL; }
	return (objc_tracing_hook)SparseArrayLookup(tracing_dtable, selector->index);
}

#endif

PRIVATE size_t objc2_countTracingHookReferences(uintptr_t base, size_t size)
{
#ifdef OBJC2_TRACING_SUPPORTED
	size_t count = 0; uint32_t idx = 0; void *hook;
	while ((hook = SparseArrayNext(tracing_dtable, &idx)) != NULL)
	{
		uintptr_t address = (uintptr_t)hook;
		if ((address >= base) && ((address - base) < size)) { count++; }
	}
	return count;
#else
	(void)base; (void)size; return 0;
#endif
}

int objc_registerTracingHook(SEL aSel, objc_tracing_hook aHook)
{
#ifdef OBJC2_TRACING_SUPPORTED
	if (aSel == NULL) { return EINVAL; }
	SEL stackBuffer[16];
	SEL *selectors = stackBuffer;
	unsigned count = 1;
	if (sel_getType_np(aSel) == 0)
	{
		count = sel_copyTypedSelectors_np(sel_getName(aSel), stackBuffer, 16);
		if (count > 16)
		{
			selectors = calloc(count, sizeof(SEL)); if (selectors == NULL) { return ENOMEM; }
			unsigned actual = sel_copyTypedSelectors_np(sel_getName(aSel), selectors, count);
			if (actual > count) { free(selectors); return EAGAIN; } count = actual;
		}
	}
	mosaic_objc_beginRuntimeMutation();
	if (selectors != stackBuffer)
	{
		for (unsigned i = 0; i < count; ++i)
		{
			if (!SparseArrayInsert(tracing_dtable, selectors[i]->index, aHook))
			{
				mosaic_objc_endRuntimeMutation();
				free(selectors);
				return ENOMEM;
			}
		}
		free(selectors);
	}
	else if (sel_getType_np(aSel) == 0)
	{
		for (unsigned i = 0; i < count; ++i)
		{
			if (!SparseArrayInsert(tracing_dtable, selectors[i]->index, aHook))
			{
				mosaic_objc_endRuntimeMutation();
				return ENOMEM;
			}
		}
	}
	if (!SparseArrayInsert(tracing_dtable, aSel->index, aHook))
	{
		mosaic_objc_endRuntimeMutation();
		return ENOMEM;
	}
	mosaic_objc_endRuntimeMutation();
	return 0;
#else
	return ENOTSUP;
#endif
}

/**
 * Installs a new method in the dtable for `class`.  If `replaceMethod` is
 * `YES` then this will replace any dtable entry where the original is
 * `method_to_replace`.  This is used when a superclass method is replaced, to
 * replace all subclass dtable entries that are inherited, but not ones that
 * are overridden.
 */
static BOOL installMethodInDtable(Class class,
                                  SparseArray *dtable,
                                  struct objc_method *method,
                                  struct objc_method *method_to_replace,
                                  BOOL replaceExisting,
                                  BOOL updateCxxCache)
{
	ASSERT(uninstalled_dtable != dtable);
	uint32_t sel_id = method->selector->index;
	struct objc_method *oldMethod = SparseArrayLookup(dtable, sel_id);
	// If we're being asked to replace an existing method, don't if it's the
	// wrong one.
	if ((replaceExisting) && (method_to_replace != oldMethod))
	{
		return NO;
	}
	// If we're not being asked to replace existing methods and there is an
	// existing one, don't replace it.
	if (!replaceExisting && (oldMethod != NULL))
	{
		return NO;
	}
	// If this method is the one already installed, pretend to install it again.
	if (NULL != oldMethod && (oldMethod->imp == method->imp))
	{
		return NO;
	}
	sparse_array_insert_or_abort(dtable, sel_id, method);
	// In TDD mode, we also register the first typed method that we
	// encounter as the untyped version.
#ifdef TYPE_DEPENDENT_DISPATCH
	uint32_t untyped_idx = get_untyped_idx(method->selector);
	sparse_array_insert_or_abort(dtable, untyped_idx, method);
#endif

	static SEL cxxConstructStorage, cxxDestructStorage;
	SEL cxx_construct = objc2_get_or_register_selector(
		&cxxConstructStorage, ".cxx_construct");
	SEL cxx_destruct = objc2_get_or_register_selector(
		&cxxDestructStorage, ".cxx_destruct");
	if (updateCxxCache && selEqualUnTyped(method->selector, cxx_construct))
	{
		class->cxx_construct = method->imp;
	}
	else if (updateCxxCache && selEqualUnTyped(method->selector, cxx_destruct))
	{
		class->cxx_destruct = method->imp;
	}

	for (struct objc_class *subclass=class->subclass_list ; 
		Nil != subclass ; subclass = subclass->sibling_class)
	{
		// Don't bother updating dtables for subclasses that haven't been
		// initialized yet
		if (!classHasDtable(subclass)) { continue; }

		// Recursively install this method in all subclasses
		installMethodInDtable(subclass,
		                      dtable_for_class(subclass),
		                      method,
		                      oldMethod,
		                      YES,
		                      NO);
	}

	// Invalidate the old slot, if there is one.
	if (NULL != oldMethod)
	{
#ifndef NO_SAFE_CACHING
		objc_method_cache_version++;
#endif
	}
	return YES;
}

static void installMethodsInClass(Class cls,
                                  SparseArray *methods_to_replace,
                                  SparseArray *methods,
                                  BOOL replaceExisting)
{
	SparseArray *dtable = dtable_for_class(cls);
	assert(uninstalled_dtable != dtable);

	uint32_t idx = 0;
	struct objc_method *m;
	while ((m = SparseArrayNext(methods, &idx)))
	{
		struct objc_method *method_to_replace = methods_to_replace
			?  SparseArrayLookup(methods_to_replace, m->selector->index)
			: NULL;
		if (!installMethodInDtable(cls, dtable, m, method_to_replace, replaceExisting, YES))
		{
			// Remove this method from the list, if it wasn't actually installed
			sparse_array_insert_or_abort(methods, idx, 0);
		}
	}
}

Class class_getSuperclass(Class);

PRIVATE void objc_update_dtable_for_class(Class cls)
{
	// Only update real dtables
	if (!classHasDtable(cls)) { return; }

	LOCK_RUNTIME_FOR_SCOPE();

	SparseArray *methods = new_sparse_array_or_abort(dtable_depth);
	collectMethodsForMethodListToSparseArray((void*)cls->methods, methods, YES);
	SparseArray *super_dtable = cls->super_class ? dtable_for_class(cls->super_class)
	                                             : NULL;
	installMethodsInClass(cls, super_dtable, methods, YES);
	SparseArrayDestroy(methods);
	checkARCAccessors(cls);
}

static void rebaseDtableRecursive(Class cls, Class newSuper)
{
	dtable_t parentDtable = dtable_for_class(newSuper);
	// Collect all of the methods for this class:
	dtable_t temporaryDtable = new_sparse_array_or_abort(dtable_depth);

	for (struct objc_method_list *list = cls->methods ; list != NULL ; list = list->next)
	{
		for (unsigned i=0 ; i<list->count ; i++)
		{
			struct objc_method *m = method_at_index(list, i);
			uint32_t idx = m->selector->index;
			// Don't replace existing methods - we're doing the traversal
			// pre-order so we'll see methods from categories first.
			if (SparseArrayLookup(temporaryDtable, idx) == NULL)
			{
				sparse_array_insert_or_abort(temporaryDtable, idx, m);
			}
		}
	}


	dtable_t dtable = dtable_for_class(cls);
	uint32_t idx = 0;
	struct objc_method *method;
	// Install all methods from the parent that aren't overridden here.
	while ((method = SparseArrayNext(parentDtable, &idx)))
	{
		if (SparseArrayLookup(temporaryDtable, idx) == NULL)
		{
			sparse_array_insert_or_abort(dtable, idx, method);
			sparse_array_insert_or_abort(temporaryDtable, idx, method);
		}
	}
	idx = 0;
	// Now look at all of the methods in the dtable.  If they're not ones from
	// the dtable that we've just created, then they must have come from the
	// original superclass, so remove them by replacing them with NULL.
	while ((method = SparseArrayNext(dtable, &idx)))
	{
		if (SparseArrayLookup(temporaryDtable, idx) == NULL)
		{
			sparse_array_insert_or_abort(dtable, idx, NULL);
		}
	}
	SparseArrayDestroy(temporaryDtable);

	// merge can make a class ARC-compatible.
	checkARCAccessors(cls);

	// Now visit all of our subclasses and propagate the changes downwards.
	for (struct objc_class *subclass=cls->subclass_list ;
	     Nil != subclass ; subclass = subclass->sibling_class)
	{
		// Don't bother updating dtables for subclasses that haven't been
		// initialized yet
		if (!classHasDtable(subclass)) { continue; }
		rebaseDtableRecursive(subclass, cls);
	}

}

PRIVATE void objc_update_dtable_for_new_superclass(Class cls, Class newSuper)
{
	// Only update real dtables
	if (!classHasDtable(cls)) { return; }

	LOCK_RUNTIME_FOR_SCOPE();
	rebaseDtableRecursive(cls, newSuper);
	// Invalidate all caches after this operation.
#ifndef NO_SAFE_CACHING
		objc_method_cache_version++;
#endif

	return;
}

PRIVATE void add_method_list_to_class(Class cls,
                                      struct objc_method_list *list)
{
	// Only update real dtables
	if (!classHasDtable(cls)) { return; }

	LOCK_RUNTIME_FOR_SCOPE();

	SparseArray *methods = new_sparse_array_or_abort(dtable_depth);
	SparseArray *super_dtable = cls->super_class ? dtable_for_class(cls->super_class)
	                                             : NULL;
	collectMethodsForMethodListToSparseArray(list, methods, NO);
	installMethodsInClass(cls, super_dtable, methods, YES);
	// Methods now contains only the new methods for this class.
	SparseArrayDestroy(methods);
	checkARCAccessors(cls);
}

PRIVATE dtable_t create_dtable_for_class(Class class, dtable_t root_dtable)
{
	// Don't create a dtable for a class that already has one
	if (classHasDtable(class)) { return dtable_for_class(class); }

	LOCK_RUNTIME_FOR_SCOPE();

	// Make sure that another thread didn't create the dtable while we were
	// waiting on the lock.
	if (classHasDtable(class)) { return dtable_for_class(class); }

	Class super = class_getSuperclass(class);
	dtable_t dtable;
	dtable_t super_dtable = NULL;

	if (Nil == super)
	{
		dtable = new_sparse_array_or_abort(dtable_depth);
	}
	else
	{
		super_dtable = dtable_for_class(super);
		if (super_dtable == uninstalled_dtable)
		{
			if (super->isa == class)
			{
				super_dtable = root_dtable;
			}
			else
			{
				abort();
			}
		}
		dtable = copy_sparse_array_or_abort(super_dtable);
	}

	// When constructing the initial dtable for a class, we iterate along the
	// method list in forward-traversal order.  The first method that we
	// encounter is always the one that we want to keep, so we instruct
	// installMethodInDtable() to replace only methods that are inherited from
	// the superclass.
	struct objc_method_list *list = (void*)class->methods;

	while (NULL != list)
	{
		for (unsigned i=0 ; i<list->count ; i++)
		{
			struct objc_method *super_method = super_dtable
				? SparseArrayLookup(super_dtable, method_at_index(list, i)->selector->index)
				: NULL;
			installMethodInDtable(class, dtable, method_at_index(list, i), super_method, YES, YES);
		}
		list = list->next;
	}

	return dtable;
}


Class class_table_next(void **e);

static BOOL class_owns_method(Class cls, struct objc_method *method)
{
	for (struct objc_method_list *list = cls->methods; list != NULL; list = list->next)
	{
		for (int i = 0; i < list->count; i++)
		{
			if (method_at_index(list, i) == method) { return YES; }
		}
	}
	return NO;
}

static void refresh_cxx_cache_for_class(Class cls, struct objc_method *method,
                                        BOOL construct)
{
	if (!class_owns_method(cls, method)) { return; }
	if (construct) { cls->cxx_construct = method->imp; }
	else { cls->cxx_destruct = method->imp; }
}

PRIVATE void objc_refresh_cxx_method_caches(struct objc_method *method)
{
	if (method == NULL) { return; }
	const char *name = sel_getName(method->selector);
	BOOL construct = strcmp(name, ".cxx_construct") == 0;
	if (!construct && (strcmp(name, ".cxx_destruct") != 0)) { return; }

	LOCK_RUNTIME_FOR_SCOPE();
	void *enumerator = NULL;
	Class cls;
	while ((cls = class_table_next(&enumerator)) != Nil)
	{
		refresh_cxx_cache_for_class(cls, method, construct);
		if (cls->isa != Nil) { refresh_cxx_cache_for_class(cls->isa, method, construct); }
	}
}

static inline uint64_t dtable_capacity(uint32_t depth)
{
	return UINT64_C(1) << depth;
}

static uint32_t dtable_depth_for_size(uint32_t size)
{
	uint32_t depth = dtable_depth;
	while ((depth < 32) && (dtable_capacity(depth) < size))
	{
		depth += 8;
	}
	return depth;
}

static dtable_t expand_dtable_or_abort(dtable_t dtable, uint32_t targetDepth)
{
	dtable_t expanded = SparseArrayExpandingArray((SparseArray*)dtable, targetDepth);
	if (expanded == NULL) { abort(); }
	return expanded;
}

PRIVATE void objc_resize_dtables(uint32_t newSize)
{
	if (dtable_capacity(dtable_depth) >= newSize) { return; }

	LOCK_RUNTIME_FOR_SCOPE();
	if (dtable_capacity(dtable_depth) >= newSize) { return; }

	const uint32_t targetDepth = dtable_depth_for_size(newSize);
	const uint32_t oldShift = uninstalled_dtable->shift;
	dtable_t old_uninstalled_dtable = uninstalled_dtable;

	uninstalled_dtable = expand_dtable_or_abort(uninstalled_dtable, targetDepth);
#ifdef OBJC2_TRACING_SUPPORTED
	tracing_dtable = expand_dtable_or_abort(tracing_dtable, targetDepth);
#endif
	{
		LOCK_FOR_SCOPE(&initialize_lock);
		for (InitializingDtable *buffer = temporary_dtables ; NULL != buffer ; buffer = buffer->next)
		{
			buffer->dtable = expand_dtable_or_abort(buffer->dtable, targetDepth);
		}
	}

	void *e = NULL;
	struct objc_class *next;
	while ((next = class_table_next(&e)))
	{
		if (next->dtable == old_uninstalled_dtable)
		{
			next->dtable = uninstalled_dtable;
			next->isa->dtable = uninstalled_dtable;
			continue;
		}
		if (NULL != next->dtable && ((SparseArray*)next->dtable)->shift == oldShift)
		{
			next->dtable = expand_dtable_or_abort((void*)next->dtable, targetDepth);
			next->isa->dtable = expand_dtable_or_abort((void*)next->isa->dtable, targetDepth);
		}
	}
	dtable_depth = targetDepth;
}

PRIVATE void free_dtable(dtable_t dtable)
{
	SparseArrayDestroy(dtable);
}

LEGACY void update_dispatch_table_for_class(Class cls)
{
	static BOOL warned = NO;
	if (!warned)
	{
		fprintf(stderr, 
			"Warning: Calling deprecated private ObjC runtime function %s\n", __func__);
		warned = YES;
	}
	objc_update_dtable_for_class(cls);
}

BOOL objc_resolve_class(Class);

__attribute__((unused)) static void objc_release_object_lock(id *x)
{
	objc_sync_exit(*x);
}
/**
 * Macro that is equivalent to @synchronize, for use in C code.
 */
#define LOCK_OBJECT_FOR_SCOPE(obj) \
	__attribute__((cleanup(objc_release_object_lock)))\
	__attribute__((unused)) id lock_object_pointer = obj;\
	objc_sync_enter(obj);

/**
 * Remove a buffer from an entry in the initializing dtables list.  This is
 * called as a cleanup to ensure that it runs even if +initialize throws an
 * exception.
 */
static void remove_dtable(InitializingDtable* meta_buffer)
{
	LOCK(&initialize_lock);
	InitializingDtable *buffer = meta_buffer->next;
	// Install the dtable:
	meta_buffer->owner->dtable = meta_buffer->dtable;
	buffer->owner->dtable = buffer->dtable;
	// Remove the look-aside buffer entry.
	if (temporary_dtables == meta_buffer)
	{
		temporary_dtables = buffer->next;
	}
	else
	{
		InitializingDtable *prev = temporary_dtables;
		while (prev->next->owner != meta_buffer->owner)
		{
			prev = prev->next;
		}
		prev->next = buffer->next;
	}
	UNLOCK(&initialize_lock);
}

/**
 * Send a +initialize message to the receiver, if required.  
 */
OBJC_PUBLIC void objc_send_initialize(id object)
{
	Class class = classForObject(object);
	// If the first message is sent to an instance (weird, but possible and
	// likely for things like NSConstantString, make sure +initialize goes to
	// the class not the metaclass.  
	if (objc_test_class_flag(class, objc_class_flag_meta))
	{
		class = (Class)object;
	}
	Class meta = class->isa;


	// Make sure that the class is resolved.
	objc_resolve_class(class);

	// Make sure that the superclass is initialized first.
	if (Nil != class->super_class)
	{
		objc_send_initialize((id)class->super_class);
	}

	// Lock the runtime while we're creating dtables and before we acquire the
	// init lock.  This prevents a lock-order reversal when dtable_for_class is
	// called from something holding the runtime lock while we're still holding
	// the initialize lock.  We should ensure that we never acquire the runtime
	// lock after acquiring the initialize lock.
	LOCK_RUNTIME();

	// Superclass +initialize might possibly send a message to this class, in
	// which case this method would be called again.  See NSObject and
	// NSAutoreleasePool +initialize interaction in GNUstep.
	if (objc_test_class_flag(class, objc_class_flag_initialized))
	{
		// We know that initialization has started because the flag is set.
		// Check that it's finished by grabbing the class lock.  This will be
		// released once the class has been fully initialized. The runtime
		// lock needs to be released first to prevent a deadlock between the
		// runtime lock and the class-specific lock.
		UNLOCK_RUNTIME();

		objc_sync_enter((id)meta);
		objc_sync_exit((id)meta);
		assert(dtable_for_class(class) != uninstalled_dtable);
		return;
	}

	// We should try to acquire the class lock before any runtime/init locks.
	// If another thread is in the middle of running `allocateHiddenClass()` it 
	// has acquired a spinlock and will be trying to acquire the runtime lock. 
	// When this happens there is a small chance we could hit the same spinlock
	// and deadlock the process (as any further attempts to acquire the runtime 
	// will also block forever).
	UNLOCK_RUNTIME();

	LOCK_OBJECT_FOR_SCOPE((id)meta);
	LOCK_RUNTIME();
	LOCK(&initialize_lock);
	if (objc_test_class_flag(class, objc_class_flag_initialized))
	{
		UNLOCK(&initialize_lock);
		UNLOCK_RUNTIME();
		return;
	}
	BOOL skipMeta = objc_test_class_flag(meta, objc_class_flag_initialized);
	// Mark metaclasses as never needing refcount manipulation for their
	// instances (classes).
	if (!skipMeta)
	{
		objc_set_class_flag(meta, objc_class_flag_permanent_instances);
	}

	// Set the initialized flag on both this class and its metaclass, to make
	// sure that +initialize is only ever sent once.
	objc_set_class_flag(class, objc_class_flag_initialized);
	objc_set_class_flag(meta, objc_class_flag_initialized);

	dtable_t class_dtable = create_dtable_for_class(class, uninstalled_dtable);
	dtable_t dtable = skipMeta ? 0 : create_dtable_for_class(meta, class_dtable);
	// Now we've finished doing things that may acquire the runtime lock, so we
	// can hold onto the initialise lock to make anything doing
	// dtable_for_class block until we've finished updating temporary dtable
	// lists.
	// If another thread holds the runtime lock, it can now proceed until it
	// gets into a dtable_for_class call, and then block there waiting for us
	// to finish setting up the temporary dtable.
	UNLOCK_RUNTIME();

	static SEL initializeStorage;
	SEL initializeSel = objc2_get_or_register_selector(
		&initializeStorage, "initialize");

	struct objc_method *initializeSlot = skipMeta ? 0 :
			objc_dtable_lookup(dtable, initializeSel->index);

	// If there's no initialize method, then don't bother installing and
	// removing the initialize dtable, just install both dtables correctly now
	if (0 == initializeSlot)
	{
		if (!skipMeta)
		{
			meta->dtable = dtable;
		}
		class->dtable = class_dtable;
		checkARCAccessors(class);
		UNLOCK(&initialize_lock);
		return;
	}



	// Create an entry in the dtable look-aside buffer for this.  When sending
	// a message to this class in future, the lookup function will check this
	// buffer if the receiver's dtable is not installed, and block if
	// attempting to send a message to this class.
	InitializingDtable buffer = { class, class_dtable, temporary_dtables };
	__attribute__((cleanup(remove_dtable)))
	InitializingDtable meta_buffer = { meta, dtable, &buffer };
	temporary_dtables = &meta_buffer;
	// We now release the initialize lock.  We'll reacquire it later when we do
	// the cleanup, but at this point we allow other threads to get the
	// temporary dtable and call +initialize in other threads.
	UNLOCK(&initialize_lock);
	// We still hold the class lock at this point.  dtable_for_class will block
	// there after acquiring the temporary dtable.

	checkARCAccessors(class);

	// Store the buffer in the temporary dtables list.  Note that it is safe to
	// insert it into a global list, even though it's a temporary variable,
	// because we will clean it up after this function.
	initializeSlot->imp((id)class, initializeSel);
}

