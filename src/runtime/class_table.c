#include "objc/runtime.h"
#include "objc/support/hooks.h"
#include "objc/support/developer.h"
#include "alias.h"
#include "class.h"
#include "method.h"
#include "selector.h"
#include "lock.h"
#include "dtable.h"
#include "legacy.h"
#include "visibility.h"
#include "crt_compat.h"
#include "allocation.h"
#include "observability.h"
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

typedef void (*loadIMP)(Class, SEL);

void objc_init_protocols(struct objc_protocol_list *protos);
void objc_compute_ivar_offsets(Class class);
void objc_register_selectors_from_class(Class class);

////////////////////////////////////////////////////////////////////////////////
// +load method hash table
////////////////////////////////////////////////////////////////////////////////
static int imp_compare(const void *i1, void *i2)
{
	return i1 == i2;
}
static int32_t imp_hash(const void *imp)
{
	return (int32_t)(((uintptr_t)imp) >> 4);
}
#define MAP_TABLE_NAME load_messages
#define MAP_TABLE_COMPARE_FUNCTION imp_compare
#define MAP_TABLE_HASH_KEY imp_hash
#define MAP_TABLE_HASH_VALUE imp_hash
#include "hash_table.h"

static load_messages_table *load_table;

SEL loadSel;

PRIVATE void objc_init_load_messages_table(void)
{
	load_messages_initialize(&load_table, 4096);
	loadSel = sel_registerName("load");
}

PRIVATE void objc_send_load_message(Class class)
{
	Class meta = class->isa;
	for (struct objc_method_list *l=meta->methods ; NULL!=l ; l=l->next)
	{
		for (int i=0 ; i<l->count ; i++)
		{
			Method m = method_at_index(l, i);
			if (sel_isEqual(m->selector, loadSel))
			{
				if (load_messages_table_get(load_table, m->imp) == 0)
				{
					((loadIMP)m->imp)(class, loadSel);
					load_messages_insert(load_table, m->imp);
				}
			}
		}
	}
}

// Get the functions for string hashing
#include "string_hash.h"

static int class_compare(const char *name, const Class class)
{
	return string_compare(name, class->name);
}
static int class_hash(const Class class)
{
	return string_hash(class->name);
}
#define MAP_TABLE_NAME class_table_internal
#define MAP_TABLE_COMPARE_FUNCTION class_compare
#define MAP_TABLE_HASH_KEY string_hash
#define MAP_TABLE_HASH_VALUE class_hash
// This defines the maximum number of classes that the runtime supports.
/*
#define MAP_TABLE_STATIC_SIZE 2048
#define MAP_TABLE_STATIC_NAME class_table
*/
#include "hash_table.h"

static class_table_internal_table *class_table;

/* Pending name -> stable placeholder mappings for objc_getFutureClass(). */
#define MAP_TABLE_NAME future_class_internal
#define MAP_TABLE_COMPARE_FUNCTION class_compare
#define MAP_TABLE_HASH_KEY string_hash
#define MAP_TABLE_HASH_VALUE class_hash
#include "hash_table.h"
static future_class_internal_table *future_class_table;

struct objc_class_remap
{
	Class source;
	Class target;
};
static int remap_compare(const void *key, const struct objc_class_remap value)
{
	return key == (const void*)value.source;
}
static int32_t remap_pointer_hash(const void *value)
{
	uintptr_t x = (uintptr_t)value;
	x ^= x >> 17;
	x *= (uintptr_t)0xed5ad4bbu;
	x ^= x >> 11;
	return (int32_t)x;
}
static int32_t remap_value_hash(const struct objc_class_remap value)
{
	return remap_pointer_hash(value.source);
}
static int remap_is_null(const struct objc_class_remap value)
{
	return value.source == Nil;
}
static struct objc_class_remap null_remap;
#define MAP_TABLE_NAME remapped_class_internal
#define MAP_TABLE_COMPARE_FUNCTION remap_compare
#define MAP_TABLE_HASH_KEY remap_pointer_hash
#define MAP_TABLE_HASH_VALUE remap_value_hash
#define MAP_TABLE_VALUE_TYPE struct objc_class_remap
#define MAP_TABLE_VALUE_NULL remap_is_null
#define MAP_TABLE_VALUE_PLACEHOLDER null_remap
#include "hash_table.h"
static remapped_class_internal_table *remapped_class_table;

static uint64_t class_table_generation = 1;
// A small 2-way TLS cache avoids pointer-layout collision cliffs while keeping
// repeated class-name lookup allocation-free and independent between threads.
enum
{
	CLASS_LOOKUP_CACHE_WAYS = 2,
	CLASS_LOOKUP_CACHE_SETS = 16
};
_Static_assert((CLASS_LOOKUP_CACHE_SETS & (CLASS_LOOKUP_CACHE_SETS - 1)) == 0,
	"class lookup cache set count must be a power of two");
static __thread struct
{
	uint64_t generation;
	const char *keys[CLASS_LOOKUP_CACHE_SETS][CLASS_LOOKUP_CACHE_WAYS];
	Class entries[CLASS_LOOKUP_CACHE_SETS][CLASS_LOOKUP_CACHE_WAYS];
	unsigned char victim[CLASS_LOOKUP_CACHE_SETS];
} class_lookup_cache;

static inline unsigned class_lookup_cache_set(const char *name)
{
	uintptr_t key = (uintptr_t)name >> 4;
	key ^= key >> 16;
	key *= (uintptr_t)0x9e3779b1u;
	key ^= key >> 13;
	return (unsigned)(key & (CLASS_LOOKUP_CACHE_SETS - 1));
}

static inline void class_table_invalidate_cache(void)
{
	__atomic_add_fetch(&class_table_generation, 1, __ATOMIC_RELEASE);
}

#define unresolved_class_next subclass_list
#define unresolved_class_prev sibling_class
/**
 * Linked list using the subclass_list pointer in unresolved classes.
 */
static Class unresolved_class_list;

static enum objc_developer_mode_np mode;

void objc_setDeveloperMode_np(enum objc_developer_mode_np newMode)
{
	mode = newMode;
}

////////////////////////////////////////////////////////////////////////////////
// Class table manipulation
////////////////////////////////////////////////////////////////////////////////

PRIVATE Class zombie_class;

PRIVATE void class_table_insert(Class class)
{
	if (!objc_test_class_flag(class, objc_class_flag_resolved))
	{
		if (Nil != unresolved_class_list)
		{
			unresolved_class_list->unresolved_class_prev = class;
		}
		class->unresolved_class_next = unresolved_class_list;
		unresolved_class_list = class;
	}
	if ((0 == zombie_class) && (strcmp("NSZombie", class->name) == 0))
	{
		zombie_class = class;
	}
	if (class_table_internal_insert(class_table, class))
	{
		class_table_invalidate_cache();
	}
}

PRIVATE Class class_table_get_safe(const char *class_name)
{
	if (NULL == class_name) { return Nil; }
	uint64_t generation = __atomic_load_n(&class_table_generation, __ATOMIC_ACQUIRE);
	if (class_lookup_cache.generation != generation)
	{
		memset(class_lookup_cache.keys, 0, sizeof(class_lookup_cache.keys));
		memset(class_lookup_cache.entries, 0, sizeof(class_lookup_cache.entries));
		memset(class_lookup_cache.victim, 0, sizeof(class_lookup_cache.victim));
		class_lookup_cache.generation = generation;
	}
	const unsigned set = class_lookup_cache_set(class_name);
	for (unsigned way = 0; way < CLASS_LOOKUP_CACHE_WAYS; ++way)
	{
		Class cached = class_lookup_cache.entries[set][way];
		if ((cached != Nil) && (class_lookup_cache.keys[set][way] == class_name) &&
		    string_compare(class_name, cached->name))
		{
			class_lookup_cache.victim[set] = (unsigned char)(way ^ 1u);
			return cached;
		}
	}
	Class cls = class_table_internal_table_get(class_table, class_name);
	if (cls != Nil)
	{
		unsigned way = class_lookup_cache.victim[set];
		for (unsigned candidate = 0; candidate < CLASS_LOOKUP_CACHE_WAYS; ++candidate)
		{
			if (class_lookup_cache.entries[set][candidate] == Nil)
			{
				way = candidate;
				break;
			}
		}
		class_lookup_cache.keys[set][way] = class_name;
		class_lookup_cache.entries[set][way] = cls;
		class_lookup_cache.victim[set] = (unsigned char)(way ^ 1u);
	}
	return cls;
}

PRIVATE Class class_table_next(void **e)
{
	return class_table_internal_next(class_table,
			(struct class_table_internal_table_enumerator**)e);
}

PRIVATE BOOL objc_resolve_class(Class cls);
PRIVATE void init_class_tables(void)
{
	class_table_internal_initialize(&class_table, 4096);
	future_class_internal_initialize(&future_class_table, 32);
	remapped_class_internal_initialize(&remapped_class_table, 32);
	objc_init_load_messages_table();
}

////////////////////////////////////////////////////////////////////////////////
// Loader functions
////////////////////////////////////////////////////////////////////////////////

PRIVATE Class objc_remap_class(Class cls)
{
	if (cls == Nil) { return Nil; }
	struct objc_class_remap remap =
		remapped_class_internal_table_get(remapped_class_table, cls);
	return remap.source == Nil ? cls : remap.target;
}

PRIVATE Class objc_claim_future_class(Class cls)
{
	if ((cls == Nil) || (cls->name == NULL)) { return cls; }
	Class alreadyRemapped = objc_remap_class(cls);
	if (alreadyRemapped != cls) { return alreadyRemapped; }

	Class future = future_class_internal_table_get(future_class_table, cls->name);
	if (future == Nil) { return cls; }

	struct objc_class_remap remap = { cls, future };
	if (!remapped_class_internal_insert(remapped_class_table, remap))
	{
		abort();
	}
	future_class_internal_remove(future_class_table, (void*)cls->name);
	char *reservedName = (char*)future->name;
	memcpy(future, cls, sizeof(struct objc_class));
	free(reservedName);
	return future;
}

PRIVATE BOOL objc_resolve_class(Class cls)
{
	// Skip this if the class is already resolved.
	if (objc_test_class_flag(cls, objc_class_flag_resolved)) { return YES; }

	// We can only resolve the class if its superclass is resolved.
	if (cls->super_class)
	{
		Class super = cls->super_class;

		if (!objc_test_class_flag(super, objc_class_flag_resolved))
		{
			if (!objc_resolve_class(super))
			{
				return NO;
			}
		}
	}
#ifdef OLDABI_COMPAT
	else
	{
		struct objc_class_gsv1 *ocls = objc_legacy_class_for_class(cls);
		if (ocls != NULL)
		{
			const char *super_name = (const char*)ocls->super_class;
			if (super_name)
			{
				Class super = (Class)objc_getClass(super_name);
				if (super == Nil)
				{
					return NO;
				}
				cls->super_class = super;
				return objc_resolve_class(cls);
			}
		}
	}
#endif


	// Remove the class from the unresolved class list
	if (Nil == cls->unresolved_class_prev)
	{
		unresolved_class_list = cls->unresolved_class_next;
	}
	else
	{
		cls->unresolved_class_prev->unresolved_class_next =
			cls->unresolved_class_next;
	}
	if (Nil != cls->unresolved_class_next)
	{
		cls->unresolved_class_next->unresolved_class_prev =
			cls->unresolved_class_prev;
	}
	cls->unresolved_class_prev = Nil;
	cls->unresolved_class_next = Nil;

	// The superclass for the metaclass.  This is the metaclass for the
	// superclass if one exists, otherwise it is the root class itself
	Class superMeta = Nil;
	// The metaclass for the metaclass.  This is always the root class's
	// metaclass.
	Class metaMeta = Nil;

	// Resolve the superclass pointer

	if (NULL == cls->super_class)
	{
		superMeta = cls;
		metaMeta = cls->isa;
	}
	else
	{
		// Resolve the superclass if it isn't already resolved
		Class super = cls->super_class;
		if (!objc_test_class_flag(super, objc_class_flag_resolved))
		{
			objc_resolve_class(super);
		}
		superMeta = super->isa;
		// Set the superclass pointer for the class and the superclass
		do
		{
			metaMeta = super->isa;
			super = super->super_class;
		} while (Nil != super);
	}
	Class meta = cls->isa;

	// Make the root class the superclass of the metaclass (e.g. NSObject is
	// the superclass of all metaclasses in classes that inherit from NSObject)
	meta->super_class = superMeta;
	meta->isa = metaMeta;

	// Don't register root classes as children of anything
	if (Nil != cls->super_class)
	{
		// Set up the class links
		cls->sibling_class = cls->super_class->subclass_list;
		cls->super_class->subclass_list = cls;
	}
	// Set up the metaclass links
	meta->sibling_class = superMeta->subclass_list;
	superMeta->subclass_list = meta;

	// Mark this class (and its metaclass) as resolved
	objc_set_class_flag(cls, objc_class_flag_resolved);
	objc_set_class_flag(cls->isa, objc_class_flag_resolved);


	// Fix up the ivar offsets
	objc_compute_ivar_offsets(cls);
#ifdef OLDABI_COMPAT
	struct objc_class_gsv1 *oldCls = objc_legacy_class_for_class(cls);
	if (oldCls)
	{
		oldCls->super_class = cls->super_class;
		oldCls->isa->super_class = cls->isa->super_class;
	}
#endif
	// Send the +load message, if required
	if (!objc_test_class_flag(cls, objc_class_flag_user_created))
	{
		objc_send_load_message(cls);
	}
	if (_objc_load_callback)
	{
		_objc_load_callback(cls, 0);
	}
	return YES;
}

PRIVATE void objc_resolve_class_links(void)
{
	LOCK_RUNTIME_FOR_SCOPE();
	BOOL resolvedClass;
	do
	{
		Class class = unresolved_class_list;
		resolvedClass = NO;
		while ((Nil != class))
		{
			Class next = class->unresolved_class_next;
			// If the class has been resolved, then this means that the last
			// call to objc_resolve_class resolved it as part of resolving
			// superclasses and removed it from the list.  We now don't have a
			// pointer into the linked list, so abort and try again from the
			// start.
			if (objc_test_class_flag(class, objc_class_flag_resolved))
			{
				assert(resolvedClass);
				break;
			}
			objc_resolve_class(class);
			if (resolvedClass ||
				objc_test_class_flag(class, objc_class_flag_resolved))
			{
				resolvedClass = YES;
			}
			class = next;
		}
	} while (resolvedClass);
}
PRIVATE void __objc_resolve_class_links(void)
{
	static BOOL warned = NO;
	if (!warned)
	{
		fprintf(stderr,
			"Warning: Calling deprecated private ObjC runtime function %s\n", __func__);
		warned = YES;
	}
	objc_resolve_class_links();
}

static BOOL reload_string_equal(const char *a, const char *b)
{
	return (a == b) || ((a != NULL) && (b != NULL) && (strcmp(a, b) == 0));
}

static BOOL safe_reload_superclass_matches(Class candidate, Class canonical)
{
	Class candidateSuper = candidate->super_class;
	Class canonicalSuper = canonical->super_class;
	if (candidateSuper == canonicalSuper) { return YES; }
	if ((candidateSuper == Nil) || (canonicalSuper == Nil)) { return NO; }
	// Compiler-emitted metadata stores the superclass name in this field until
	// class resolution.  Do not dereference it as a Class before resolving it.
	Class resolved = class_table_get_safe((const char*)candidateSuper);
	return resolved == canonicalSuper;
}

static struct objc_ivar_list *copy_reload_ivars(struct objc_ivar_list *source,
                                                int **offsetStorage)
{
	*offsetStorage = NULL;
	if (source == NULL) { return NULL; }
	if ((source->count < 0) || (source->size < sizeof(struct objc_ivar))) { return NULL; }
	size_t payload = 0;
	size_t bytes = 0;
	if (!objc2_size_multiply((size_t)source->count, source->size, &payload) ||
	    !objc2_size_add(sizeof(struct objc_ivar_list), payload, &bytes))
	{
		return NULL;
	}
	struct objc_ivar_list *copy = malloc(bytes);
	if (copy == NULL) { return NULL; }
	memcpy(copy, source, bytes);
	if (source->count == 0) { return copy; }
	size_t offsetBytes = 0;
	if (!objc2_size_multiply((size_t)source->count, sizeof(int), &offsetBytes))
	{
		free(copy);
		return NULL;
	}
	int *offsets = malloc(offsetBytes);
	if (offsets == NULL)
	{
		free(copy);
		return NULL;
	}
	for (int i = 0; i < source->count; i++)
	{
		struct objc_ivar *src = ivar_at_index(source, i);
		struct objc_ivar *dst = ivar_at_index(copy, i);
		if (src->offset == NULL)
		{
			free(offsets);
			free(copy);
			return NULL;
		}
		offsets[i] = *src->offset;
		dst->offset = &offsets[i];
	}
	*offsetStorage = offsets;
	return copy;
}

static BOOL safe_reload_layout_matches(Class candidate, Class canonical,
                                       const char **reason)
{
	if (!safe_reload_superclass_matches(candidate, canonical))
	{
		*reason = "superclass";
		return NO;
	}
	int *offsetStorage = NULL;
	struct objc_ivar_list *ivars = copy_reload_ivars(candidate->ivars, &offsetStorage);
	if ((candidate->ivars != NULL) && (ivars == NULL))
	{
		*reason = "ivar-metadata";
		return NO;
	}
	struct objc_class normalized = *candidate;
	normalized.super_class = canonical->super_class;
	normalized.ivars = ivars;
	objc_set_class_flag(&normalized, objc_class_flag_resolved);
	objc_compute_ivar_offsets(&normalized);

	BOOL match = normalized.instance_size == canonical->instance_size;
	if (!match) { *reason = "instance-size"; }
	if (match && ((normalized.ivars == NULL) != (canonical->ivars == NULL)))
	{
		match = NO;
		*reason = "ivar-presence";
	}
	if (match && (normalized.ivars != NULL))
	{
		if (normalized.ivars->count != canonical->ivars->count)
		{
			match = NO;
			*reason = "ivar-count";
		}
	}
	for (int i = 0; match && (normalized.ivars != NULL) &&
	     (i < normalized.ivars->count); i++)
	{
		struct objc_ivar *fresh = ivar_at_index(normalized.ivars, i);
		struct objc_ivar *live = ivar_at_index(canonical->ivars, i);
		if (!reload_string_equal(fresh->name, live->name) ||
		    !reload_string_equal(fresh->type, live->type) ||
		    (fresh->size != live->size) || (fresh->flags != live->flags) ||
		    (fresh->offset == NULL) || (live->offset == NULL) ||
		    (*fresh->offset != *live->offset))
		{
			match = NO;
			*reason = "ivar-layout";
		}
	}
	free(offsetStorage);
	free(ivars);
	return match;
}

struct safe_reload_method_entry
{
	Method target;
	SEL selector;
	IMP implementation;
	const char *types;
};

struct safe_reload_method_plan
{
	Class target;
	struct safe_reload_method_entry *entries;
	size_t entry_count;
	struct objc_method_list *additions;
};

static Method safe_reload_find_method(Class target, const char *name)
{
	for (struct objc_method_list *list = target->methods; list != NULL; list = list->next)
	{
		if ((list->count < 0) || (list->size < sizeof(struct objc_method))) { return NULL; }
		for (int i = 0; i < list->count; i++)
		{
			Method method = method_at_index(list, i);
			const char *methodName = sel_getName(method->selector);
			if ((methodName != NULL) && (strcmp(methodName, name) == 0)) { return method; }
		}
	}
	return NULL;
}

static BOOL safe_reload_plan_contains_name(const struct safe_reload_method_plan *plan,
                                           const char *name)
{
	for (size_t i = 0; i < plan->entry_count; i++)
	{
		const char *entryName = sel_getName(plan->entries[i].selector);
		if ((entryName != NULL) && (strcmp(entryName, name) == 0)) { return YES; }
	}
	return NO;
}

static void safe_reload_destroy_plan(struct safe_reload_method_plan *plan)
{
	if (plan->additions != NULL)
	{
		for (int i = 0; i < plan->additions->count; i++)
		{
			free((void*)method_at_index(plan->additions, i)->types);
		}
		free(plan->additions);
	}
	free(plan->entries);
	memset(plan, 0, sizeof(*plan));
}

static BOOL safe_reload_count_methods(Class source, size_t *outCount)
{
	size_t count = 0;
	for (struct objc_method_list *list = source->methods; list != NULL; list = list->next)
	{
		if ((list->count < 0) || (list->size < sizeof(struct objc_method))) { return NO; }
		if ((size_t)list->count > SIZE_MAX - count) { return NO; }
		count += (size_t)list->count;
	}
	*outCount = count;
	return YES;
}

static BOOL safe_reload_prepare_plan(Class target, Class source,
                                     struct safe_reload_method_plan *plan,
                                     const char **reason)
{
	memset(plan, 0, sizeof(*plan));
	plan->target = target;
	size_t total = 0;
	if (!safe_reload_count_methods(source, &total))
	{
		*reason = "method-metadata";
		return NO;
	}
	if (total == 0) { return YES; }
	size_t entryBytes = 0;
	if (!objc2_size_multiply(total, sizeof(*plan->entries), &entryBytes))
	{
		*reason = "method-metadata";
		return NO;
	}
	plan->entries = calloc(1, entryBytes);
	if (plan->entries == NULL)
	{
		*reason = "allocation";
		return NO;
	}

	size_t additions = 0;
	for (struct objc_method_list *list = source->methods; list != NULL; list = list->next)
	{
		for (int i = 0; i < list->count; i++)
		{
			Method incoming = method_at_index(list, i);
			const char *name = sel_getName(incoming->selector);
			if ((name == NULL) || (incoming->types == NULL) || (incoming->imp == NULL))
			{
				*reason = "method-metadata";
				safe_reload_destroy_plan(plan);
				return NO;
			}
			if (safe_reload_plan_contains_name(plan, name)) { continue; }
			Method existing = safe_reload_find_method(target, name);
			if ((existing != NULL) && !reload_string_equal(existing->types, incoming->types))
			{
				*reason = "method-signature";
				safe_reload_destroy_plan(plan);
				return NO;
			}
			struct safe_reload_method_entry *entry = &plan->entries[plan->entry_count++];
			entry->target = existing;
			entry->selector = incoming->selector;
			entry->implementation = incoming->imp;
			entry->types = incoming->types;
			if (existing == NULL) { additions++; }
		}
	}

	if (additions == 0) { return YES; }
	if (additions > INT_MAX)
	{
		*reason = "method-metadata";
		safe_reload_destroy_plan(plan);
		return NO;
	}
	size_t additionBytes = 0;
	if (!objc2_flexible_array_size(sizeof(struct objc_method_list), additions,
	                               sizeof(struct objc_method), &additionBytes))
	{
		*reason = "method-metadata";
		safe_reload_destroy_plan(plan);
		return NO;
	}
	plan->additions = calloc(1, additionBytes);
	if (plan->additions == NULL)
	{
		*reason = "allocation";
		safe_reload_destroy_plan(plan);
		return NO;
	}
	plan->additions->count = (int)additions;
	plan->additions->size = sizeof(struct objc_method);

	size_t additionIndex = 0;
	for (size_t i = 0; i < plan->entry_count; i++)
	{
		struct safe_reload_method_entry *entry = &plan->entries[i];
		if (entry->target != NULL) { continue; }
		Method addition = method_at_index(plan->additions, (int)additionIndex++);
		addition->selector = entry->selector;
		addition->imp = entry->implementation;
		addition->types = objc2_strdup(entry->types);
		if (addition->types == NULL)
		{
			*reason = "allocation";
			safe_reload_destroy_plan(plan);
			return NO;
		}
	}
	return YES;
}

static void safe_reload_emit_method_event(enum mosaic_objc_runtime_event_kind kind,
                                          Class cls, Method method,
                                          IMP oldImp, IMP newImp)
{
	struct mosaic_objc_runtime_event event = {0};
	event.kind = kind;
	event.cls = cls;
	event.method = method;
	event.selector = method->selector;
	event.old_implementation = oldImp;
	event.new_implementation = newImp;
	event.name = sel_getName(method->selector);
	mosaic_objc_emitRuntimeEvent(&event);
}

static void safe_reload_commit_plan(struct safe_reload_method_plan *plan)
{
	if (plan->additions != NULL)
	{
		plan->additions->next = plan->target->methods;
		plan->target->methods = plan->additions;
		if (classHasDtable(plan->target))
		{
			add_method_list_to_class(plan->target, plan->additions);
		}
		for (int i = 0; i < plan->additions->count; i++)
		{
			Method added = method_at_index(plan->additions, i);
			safe_reload_emit_method_event(MOSAIC_OBJC_EVENT_METHOD_ADDED,
			                              plan->target, added, NULL, added->imp);
		}
		plan->additions = NULL;
	}
	for (size_t i = 0; i < plan->entry_count; i++)
	{
		struct safe_reload_method_entry *entry = &plan->entries[i];
		if (entry->target == NULL) { continue; }
		IMP oldImp = entry->target->imp;
		entry->target->imp = entry->implementation;
		safe_reload_emit_method_event(MOSAIC_OBJC_EVENT_METHOD_REPLACED,
		                              plan->target, entry->target,
		                              oldImp, entry->implementation);
	}
	free(plan->entries);
	plan->entries = NULL;
	plan->entry_count = 0;
}

static void refresh_reload_cxx_cache(Class target)
{
	target->cxx_construct = NULL;
	target->cxx_destruct = NULL;
	for (struct objc_method_list *list = target->methods; list != NULL; list = list->next)
	{
		for (int i = 0; i < list->count; i++)
		{
			Method method = method_at_index(list, i);
			const char *name = sel_getName(method->selector);
			if ((target->cxx_construct == NULL) && (strcmp(name, ".cxx_construct") == 0))
			{
				target->cxx_construct = method->imp;
			}
			else if ((target->cxx_destruct == NULL) && (strcmp(name, ".cxx_destruct") == 0))
			{
				target->cxx_destruct = method->imp;
			}
		}
	}
}

static BOOL safe_reload_class(struct objc_class *candidate, struct objc_class *canonical)
{
	const char *reason = "layout-incompatible";
	if ((candidate->isa == Nil) || (canonical->isa == Nil))
	{
		reason = "metaclass";
		goto reject;
	}
	if (!safe_reload_layout_matches(candidate, canonical, &reason)) { goto reject; }
	objc_register_selectors_from_class(candidate);
	objc_register_selectors_from_class(candidate->isa);

	struct safe_reload_method_plan instancePlan = {0};
	struct safe_reload_method_plan classPlan = {0};
	if (!safe_reload_prepare_plan(canonical, candidate, &instancePlan, &reason) ||
	    !safe_reload_prepare_plan(canonical->isa, candidate->isa, &classPlan, &reason))
	{
		safe_reload_destroy_plan(&instancePlan);
		safe_reload_destroy_plan(&classPlan);
		goto reject;
	}
	safe_reload_commit_plan(&instancePlan);
	safe_reload_commit_plan(&classPlan);
	refresh_reload_cxx_cache(canonical);
	refresh_reload_cxx_cache(canonical->isa);

	{
		struct mosaic_objc_runtime_event event = {0};
		event.kind = MOSAIC_OBJC_EVENT_CLASS_RELOADED;
		event.cls = canonical;
		event.name = canonical->name;
		event.detail = "layout-compatible";
		mosaic_objc_emitRuntimeEvent(&event);
	}
	return YES;

reject:
	{
		struct mosaic_objc_runtime_event event = {0};
		event.kind = MOSAIC_OBJC_EVENT_CLASS_RELOAD_REJECTED;
		event.cls = canonical;
		event.name = canonical->name;
		event.detail = reason;
		mosaic_objc_emitRuntimeEvent(&event);
	}
	return NO;
}

static void permissive_reload_class(struct objc_class *class, struct objc_class *old)
{
	const char *superclassName = (char*)class->super_class;
	class->super_class = class_table_get_safe(superclassName);
	BOOL equalLayouts = (class->super_class == old->super_class) &&
		(class->instance_size == old->instance_size);
	if ((NULL == class->ivars) || (NULL == old->ivars))
	{
		equalLayouts &= (class->ivars == old->ivars);
	}
	else
	{
		for (int i=0 ; equalLayouts && (i<old->ivars->count) ; i++)
		{
			struct objc_ivar *oldIvar = ivar_at_index(old->ivars, i);
			struct objc_ivar *newIvar = ivar_at_index(class->ivars, i);
			equalLayouts &= strcmp(oldIvar->name, newIvar->name) == 0;
			equalLayouts &= strcmp(oldIvar->type, newIvar->type) == 0;
			equalLayouts &= (oldIvar->offset == newIvar->offset);
		}
	}

	// If the layouts are equal, then we can simply tack the class's method
	// list on to the front of the old class and update the dtable.
	if (equalLayouts)
	{
		class->methods->next = old->methods;
		old->methods = class->methods;
		objc_update_dtable_for_class(old);
		return;
	}

	// If we get to here, then we are adding a new class.  This is where things
	// start to get a bit tricky...

	// Ideally, we'd want to capture the subclass list here.  Unfortunately,
	// this is not possible because the subclass will contain methods that
	// refer to ivars in the superclass.
	//
	// We can't use the non-fragile ABI's offset facility easily, because we'd
	// have to have two (or more) offsets for the same ivar.  This gets messy
	// very quickly.  Ideally, we'd want every class to include ivar offsets
	// for every single (public) ivar in its superclasses.  These could then be
	// updated by copies of the class.  Defining a development ABI is something
	// to consider for a future release.
	class->subclass_list = NULL;

	// Replace the old class with this one in the class table.  New lookups for
	// this class will now return this class.
	class_table_internal_table_set(class_table, (void*)class->name, class);
	class_table_invalidate_cache();

	// Set the uninstalled dtable.  The compiler could do this as well.
	class->dtable = uninstalled_dtable;
	class->isa->dtable = uninstalled_dtable;

	// If this is a root class, make the class into the metaclass's superclass.
	// This means that all instance methods will be available to the class.
	if (NULL == superclassName)
	{
		class->isa->super_class = class;
	}

	if (class->protocols)
	{
		objc_init_protocols(class->protocols);
	}
}

/**
 * Loads a class.  This function assumes that the runtime mutex is locked.
 */
PRIVATE void objc_load_class(struct objc_class *class)
{
	struct objc_class *existingClass = class_table_get_safe(class->name);
	if (Nil != existingClass)
	{
		if (objc_developer_mode_safe_reload == mode)
		{
			(void)safe_reload_class(class, existingClass);
			return;
		}
		if (objc_developer_mode_developer != mode)
		{
			fprintf(stderr,
				"Loading two versions of %s.  The class that will be used is undefined\n",
				class->name);
			return;
		}
		permissive_reload_class(class, existingClass);
		return;
	}

#ifdef _WIN32
	// On Windows, the super_class pointer may point to the local __imp_
	// symbol, rather than to the external symbol.  The runtime must remove the
	// extra indirection.
	if (class->super_class)
	{
		Class superMeta = class->super_class->isa;
		if (!class_isMetaClass(superMeta))
		{
			class->super_class = superMeta;
		}
	}
#endif

	// Work around a bug in some versions of GCC that don't initialize the
	// class structure correctly.
	class->subclass_list = NULL;

	// Insert the class into the class table
	class_table_insert(class);

	// Set the uninstalled dtable.  The compiler could do this as well.
	class->dtable = uninstalled_dtable;
	class->isa->dtable = uninstalled_dtable;

	// Mark constant string instances as never needing refcount manipulation.
	if (strcmp(class->name, "NSConstantString") == 0)
	{
		objc_set_class_flag(class, objc_class_flag_permanent_instances);
	}

	// If this is a root class, make the class into the metaclass's superclass.
	// This means that all instance methods will be available to the class.
	if (NULL == class->super_class)
	{
		class->isa->super_class = class;
	}

	if (class->protocols)
	{
		objc_init_protocols(class->protocols);
	}
}

PRIVATE Class SmallObjectClasses[7];

BOOL objc_registerSmallObjectClass_np(Class class, uintptr_t mask)
{
	if ((mask & OBJC_SMALL_OBJECT_MASK) != mask)
	{
		return NO;
	}
	if (sizeof(void*) == 4)
	{
		if (Nil == SmallObjectClasses[0])
		{
			SmallObjectClasses[0] = class;
			return YES;
		}
		return NO;
	}
	if (Nil != SmallObjectClasses[mask])
	{
		return NO;
	}
	SmallObjectClasses[mask] = class;
	return YES;
}

PRIVATE void class_table_remove(Class cls)
{
	assert(objc_test_class_flag(cls, objc_class_flag_user_created));
	class_table_internal_remove(class_table, (void*)cls->name);
	class_table_invalidate_cache();
}


////////////////////////////////////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////////////////////////////////////

int objc_getClassList(Class *buffer, int bufferLen)
{
	if (buffer == NULL || bufferLen == 0)
	{
		return class_table->table_used;
	}
	int count = 0;
	struct class_table_internal_table_enumerator *e = NULL;
	Class next;
	while (count < bufferLen &&
		(next = class_table_internal_next(class_table, &e)))
	{
		buffer[count++] = next;
	}
	free(e);
	return count;
}
Class *objc_copyClassList(unsigned int *outCount)
{
	int count = class_table->table_used;
	Class *buffer = calloc(count, sizeof(Class));
	if (NULL != outCount)
	{
		*outCount = count;
	}
	objc_getClassList(buffer, count);
	return buffer;
}

Class class_getSuperclass(Class cls)
{
	if (Nil == cls) { return Nil; }
	if (!objc_test_class_flag(cls, objc_class_flag_resolved))
	{
		objc_resolve_class(cls);
	}
	return cls->super_class;
}


Class objc_getFutureClass(const char *name)
{
	if (name == NULL) { return Nil; }
	LOCK_RUNTIME_FOR_SCOPE();
	Class cls = class_table_get_safe(name);
	if (cls != Nil) { return cls; }
	cls = future_class_internal_table_get(future_class_table, name);
	if (cls != Nil) { return cls; }

	Class future = calloc(1, sizeof(struct objc_class));
	if (future == Nil) { return Nil; }
	char *nameCopy = objc2_strdup(name);
	if (nameCopy == NULL)
	{
		free(future);
		return Nil;
	}
	future->name = nameCopy;
	future->dtable = uninstalled_dtable;
	future->info = objc_class_flag_future;
	if (!future_class_internal_insert(future_class_table, future))
	{
		free(nameCopy);
		free(future);
		return Nil;
	}
	return future;
}

id objc_getClass(const char *name)
{
	id class = (id)class_table_get_safe(name);

	if (nil != class) { return class; }

	// Second chance lookup via @compatibilty_alias:
	class = (id)alias_getClass(name);
	if (nil != class) { return class; }

	// Third chance lookup via the hook:
	if (0 != _objc_lookup_class)
	{
		class = (id)_objc_lookup_class(name);
	}

	return class;
}

Class objc_lookUpClass(const char *name)
{
	return class_table_get_safe(name);
}


id objc_getMetaClass(const char *name)
{
	Class cls = (Class)objc_getClass(name);
	return cls == Nil ? nil : (id)cls->isa;
}

// Legacy interface compatibility

id objc_get_class(const char *name)
{
	return objc_getClass(name);
}

id objc_lookup_class(const char *name)
{
	return objc_getClass(name);
}

id objc_get_meta_class(const char *name)
{
	return objc_getMetaClass(name);
}

Class objc_next_class(void **enum_state)
{
  return class_table_next ( enum_state);
}

Class class_pose_as(Class impostor, Class super_class)
{
	fprintf(stderr, "Class posing is no longer supported.\n");
	fprintf(stderr, "Please use class_replaceMethod() instead.\n");
	abort();
}
