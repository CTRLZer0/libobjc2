#include <stdlib.h>
#include <assert.h>
#include "objc/runtime.h"
#include "objc/extensions/mosaic.h"
#include "objc/memory/auto.h"
#include "objc/memory/arc.h"
#include "lock.h"
#include "loader.h"
#include "visibility.h"
#include "crt_compat.h"
#include "legacy.h"
#ifdef ENABLE_GC
#include <gc/gc.h>
#endif
#include <stdio.h>
#include <string.h>

/**
 * Runtime lock.  This is exposed in 
 */
PRIVATE mutex_t runtime_mutex;
LEGACY void *__objc_runtime_mutex = &runtime_mutex;

void log_selector_memory_usage(void);

static void log_memory_stats(void)
{
	log_selector_memory_usage();
}

/* Number of threads that are alive.  */
int __objc_runtime_threads_alive = 1;			/* !T:MUTEX */

// libdispatch hooks for registering threads
__attribute__((weak)) void (*dispatch_begin_thread_4GC)(void);
__attribute__((weak)) void (*dispatch_end_thread_4GC)(void);
__attribute__((weak)) void *(*_dispatch_begin_NSAutoReleasePool)(void);
__attribute__((weak)) void (*_dispatch_end_NSAutoReleasePool)(void *);

static void init_runtime(void)
{
	static BOOL first_run = YES;
	if (first_run)
	{
		// Create the main runtime lock.  This is not safe in theory, but in
		// practice the first time that this function is called will be in the
		// loader, from the main thread.  Future loaders may run concurrently,
		// but that is likely to break the semantics of a lot of languages, so
		// we don't have to worry about it for a long time.
		//
		// The only case when this can potentially go badly wrong is when a
		// pure-C main() function spawns two threads which then, concurrently,
		// call dlopen() or equivalent, and the platform's implementation of
		// this does not perform any synchronization.
		INIT_LOCK(runtime_mutex);
		// Create the various tables that the runtime needs.
		init_selector_tables();
		init_dispatch_tables();
		init_protocol_table();
		init_class_tables();
		init_alias_table();
		init_early_blocks();
		init_arc();
#if defined(EMBEDDED_BLOCKS_RUNTIME)
		init_trampolines();
#endif
		init_builtin_classes();
		first_run = NO;
		if (objc2_getenv_exists("LIBOBJC_MEMORY_PROFILE"))
		{
			atexit(log_memory_stats);
		}
		if (dispatch_begin_thread_4GC != 0) {
			dispatch_begin_thread_4GC = objc_registerThreadWithCollector;
		}
		if (dispatch_end_thread_4GC != 0) {
			dispatch_end_thread_4GC = objc_unregisterThreadWithCollector;
		}
		if (_dispatch_begin_NSAutoReleasePool != 0) {
			_dispatch_begin_NSAutoReleasePool = objc_autoreleasePoolPush;
		}
		if (_dispatch_end_NSAutoReleasePool != 0) {
			_dispatch_end_NSAutoReleasePool = objc_autoreleasePoolPop;
		}
	}
}

OBJC_PUBLIC void mosaic_objc_runtime_initialize(void)
{
	init_runtime();
}

/**
 * Structure for a class alias.
 */
struct objc_alias
{
	/**
	 * The name by which this class is referenced.
	 */
	const char *alias_name;
	/**
	 * A pointer to the indirection variable for this class.
	 */
	Class *alias;
};

/**
 * Type of the NSConstantString structure.
 */
struct nsstr
{
	/** Class pointer. */
	id isa;
	/**
	 * Flags.  Low 2 bits store the encoding:
	 * 0: ASCII
	 * 1: UTF-8
	 * 2: UTF-16
	 * 3: UTF-32
	 *
	 * Low 16 bits are reserved for the compiler, high 32 bits are reserved for
	 * the Foundation framework.
	 */
	uint32_t flags;
	/**
	 * Number of UTF-16 code units in the string.
	 */
	uint32_t length;
	/**
	 * Number of bytes in the string.
	 */
	uint32_t size;
	/**
	 * Hash (Foundation framework defines the hash algorithm).
	 */
	uint32_t hash;
	/**
	 * Character data.
	 */
	const char *data;
};

// begin: objc_init
struct objc_init
{
	uint64_t version;
	SEL sel_begin;
	SEL sel_end;
	Class *cls_begin;
	Class *cls_end;
	Class *cls_ref_begin;
	Class *cls_ref_end;
	struct objc_category *cat_begin;
	struct objc_category *cat_end;
	struct objc_protocol *proto_begin;
	struct objc_protocol *proto_end;
	struct objc_protocol **proto_ref_begin;
	struct objc_protocol **proto_ref_end;
	struct objc_alias *alias_begin;
	struct objc_alias *alias_end;
	struct nsstr *strings_begin;
	struct nsstr *strings_end;
};
// end: objc_init

struct mosaic_objc_image_record
{
	struct objc_init *init;
	uint64_t generation;
	char *identifier;
	char *provider;
	const void *base_address;
	struct mosaic_objc_image_record *next;
};
static struct mosaic_objc_image_record *loaded_objc_images;
static struct mosaic_objc_image_record *loaded_objc_images_tail;
static uint64_t next_objc_image_generation = 1;

static struct mosaic_objc_image_record *register_loaded_objc_image(struct objc_init *init)
{
	struct mosaic_objc_image_record *image = calloc(1, sizeof(*image));
	if (image == NULL) { abort(); }
	image->init = init;
	image->generation = next_objc_image_generation++;
	if (loaded_objc_images_tail == NULL)
	{
		loaded_objc_images = loaded_objc_images_tail = image;
	}
	else
	{
		loaded_objc_images_tail->next = image;
		loaded_objc_images_tail = image;
	}
	return image;
}

static size_t objc_image_range_count(const void *begin, const void *end, size_t itemSize)
{
	if ((begin == NULL) || (end == NULL) || (itemSize == 0)) { return 0; }
	uintptr_t first = (uintptr_t)begin;
	uintptr_t last = (uintptr_t)end;
	if (last < first) { return 0; }
	size_t bytes = (size_t)(last - first);
	return (bytes % itemSize) == 0 ? bytes / itemSize : 0;
}

static BOOL objc_image_is_registered(mosaic_objc_image_t image)
{
	for (struct mosaic_objc_image_record *candidate = loaded_objc_images;
	     candidate != NULL; candidate = candidate->next)
	{
		if (candidate == image) { return YES; }
	}
	return NO;
}

static void remap_init_class_references(struct objc_init *init)
{
    for (Class *slot = init->cls_begin; slot < init->cls_end; slot++) {
        if (*slot == Nil) { continue; }
        *slot = objc_remap_class(*slot);
        (*slot)->super_class = objc_remap_class((*slot)->super_class);
    }
    for (Class *ref = init->cls_ref_begin; ref < init->cls_ref_end; ref++) {
        if (*ref != Nil) { *ref = objc_remap_class(*ref); }
    }
}

static void remap_loaded_class_references(void)
{
    for (struct mosaic_objc_image_record *image = loaded_objc_images;
         image != NULL; image = image->next) {
        remap_init_class_references(image->init);
    }
}

mosaic_objc_image_t *mosaic_objc_copyImageList(size_t *outCount)
{
	init_runtime();
	LOCK_RUNTIME_FOR_SCOPE();
	size_t count = 0;
	for (struct mosaic_objc_image_record *image = loaded_objc_images;
	     image != NULL; image = image->next)
	{
		if (count == SIZE_MAX) { return NULL; }
		count++;
	}
	if (outCount != NULL) { *outCount = count; }
	if (count == 0) { return NULL; }
	if (count > (SIZE_MAX / sizeof(mosaic_objc_image_t)))
	{
		if (outCount != NULL) { *outCount = 0; }
		return NULL;
	}
	mosaic_objc_image_t *images = malloc(count * sizeof(*images));
	if (images == NULL)
	{
		if (outCount != NULL) { *outCount = 0; }
		return NULL;
	}
	size_t index = 0;
	for (struct mosaic_objc_image_record *image = loaded_objc_images;
	     image != NULL; image = image->next)
	{
		images[index++] = image;
	}
	return images;
}

BOOL mosaic_objc_imageGetInfo(mosaic_objc_image_t image,
                              struct mosaic_objc_image_info *outInfo)
{
	if ((image == NULL) || (outInfo == NULL)) { return NO; }
	init_runtime();
	LOCK_RUNTIME_FOR_SCOPE();
	if (!objc_image_is_registered(image)) { return NO; }
	struct objc_init *init = image->init;
	*outInfo = (struct mosaic_objc_image_info){
		.generation = image->generation,
		.identifier = image->identifier,
		.provider = image->provider,
		.base_address = image->base_address,
		.class_count = objc_image_range_count(init->cls_begin, init->cls_end, sizeof(Class)),
		.class_reference_count = objc_image_range_count(init->cls_ref_begin, init->cls_ref_end, sizeof(Class)),
		.category_count = objc_image_range_count(init->cat_begin, init->cat_end, sizeof(struct objc_category)),
		.protocol_count = objc_image_range_count(init->proto_begin, init->proto_end, sizeof(struct objc_protocol)),
	};
	return YES;
}

BOOL mosaic_objc_imageSetIdentity(mosaic_objc_image_t image,
                                  const char *identifier,
                                  const char *provider,
                                  const void *baseAddress)
{
	if (image == NULL) { return NO; }
	init_runtime();
	LOCK_RUNTIME_FOR_SCOPE();
	if (!objc_image_is_registered(image)) { return NO; }
	if ((identifier != NULL) && (image->identifier != NULL) &&
	    (strcmp(identifier, image->identifier) != 0)) { return NO; }
	if ((provider != NULL) && (image->provider != NULL) &&
	    (strcmp(provider, image->provider) != 0)) { return NO; }
	if ((baseAddress != NULL) && (image->base_address != NULL) &&
	    (baseAddress != image->base_address)) { return NO; }

	char *identifierCopy = NULL;
	char *providerCopy = NULL;
	if ((identifier != NULL) && (image->identifier == NULL))
	{
		identifierCopy = objc2_strdup(identifier);
		if (identifierCopy == NULL) { return NO; }
	}
	if ((provider != NULL) && (image->provider == NULL))
	{
		providerCopy = objc2_strdup(provider);
		if (providerCopy == NULL)
		{
			free(identifierCopy);
			return NO;
		}
	}
	if (identifierCopy != NULL) { image->identifier = identifierCopy; }
	if (providerCopy != NULL) { image->provider = providerCopy; }
	if ((baseAddress != NULL) && (image->base_address == NULL))
	{
		image->base_address = baseAddress;
	}
	return YES;
}

mosaic_objc_image_t mosaic_objc_imageForClass(Class cls)
{
	if (cls == Nil) { return NULL; }
	init_runtime();
	LOCK_RUNTIME_FOR_SCOPE();
	for (struct mosaic_objc_image_record *image = loaded_objc_images;
	     image != NULL; image = image->next)
	{
		struct objc_init *init = image->init;
		size_t count = objc_image_range_count(init->cls_begin, init->cls_end, sizeof(Class));
		for (size_t i = 0; i < count; i++)
		{
			if (init->cls_begin[i] == cls) { return image; }
		}
	}
	return NULL;
}

mosaic_objc_image_t mosaic_objc_imageForProtocol(Protocol *protocol)
{
	if (protocol == NULL) { return NULL; }
	init_runtime();
	LOCK_RUNTIME_FOR_SCOPE();
	for (struct mosaic_objc_image_record *image = loaded_objc_images;
	     image != NULL; image = image->next)
	{
		struct objc_init *init = image->init;
		size_t count = objc_image_range_count(init->proto_begin, init->proto_end, sizeof(struct objc_protocol));
		for (size_t i = 0; i < count; i++)
		{
			if ((Protocol *)&init->proto_begin[i] == protocol) { return image; }
		}
	}
	return NULL;
}

Class mosaic_objc_imageGetClass(mosaic_objc_image_t image, size_t index)
{
	if (image == NULL) { return Nil; }
	init_runtime();
	LOCK_RUNTIME_FOR_SCOPE();
	if (!objc_image_is_registered(image)) { return Nil; }
	struct objc_init *init = image->init;
	size_t count = objc_image_range_count(init->cls_begin, init->cls_end, sizeof(Class));
	return index < count ? init->cls_begin[index] : Nil;
}

Protocol *mosaic_objc_imageGetProtocol(mosaic_objc_image_t image, size_t index)
{
	if (image == NULL) { return NULL; }
	init_runtime();
	LOCK_RUNTIME_FOR_SCOPE();
	if (!objc_image_is_registered(image)) { return NULL; }
	struct objc_init *init = image->init;
	size_t count = objc_image_range_count(init->proto_begin, init->proto_end, sizeof(struct objc_protocol));
	return index < count ? (Protocol *)&init->proto_begin[index] : NULL;
}

const char *mosaic_objc_imageGetCategoryName(mosaic_objc_image_t image, size_t index)
{
	if (image == NULL) { return NULL; }
	init_runtime();
	LOCK_RUNTIME_FOR_SCOPE();
	if (!objc_image_is_registered(image)) { return NULL; }
	struct objc_init *init = image->init;
	size_t count = objc_image_range_count(init->cat_begin, init->cat_end, sizeof(struct objc_category));
	return index < count ? init->cat_begin[index].name : NULL;
}

const char *mosaic_objc_imageGetCategoryClassName(mosaic_objc_image_t image, size_t index)
{
	if (image == NULL) { return NULL; }
	init_runtime();
	LOCK_RUNTIME_FOR_SCOPE();
	if (!objc_image_is_registered(image)) { return NULL; }
	struct objc_init *init = image->init;
	size_t count = objc_image_range_count(init->cat_begin, init->cat_end, sizeof(struct objc_category));
	return index < count ? init->cat_begin[index].class_name : NULL;
}

#ifdef DEBUG_LOADING
#include <dlfcn.h>
#endif

static enum {
	LegacyABI,
	NewABI,
	UnknownABI
} CurrentABI = UnknownABI;

void registerProtocol(Protocol *proto);

OBJC_PUBLIC void __objc_load(struct objc_init *init)
{
	init_runtime();
#ifdef DEBUG_LOADING
	Dl_info info;
	if (dladdr(init, &info))
	{
		fprintf(stderr, "Loading %p from object: %s (%p)\n", init, info.dli_fname, __builtin_return_address(0));
	}
	else
	{
		fprintf(stderr, "Loading %p from unknown object\n", init);
	}
#endif
	LOCK_RUNTIME_FOR_SCOPE();
	BOOL isFirstLoad = NO;
	switch (CurrentABI)
	{
		case LegacyABI:
			fprintf(stderr, "Version 2 Objective-C ABI may not be mixed with earlier versions.\n");
			abort();
		case UnknownABI:
			isFirstLoad = YES;
			CurrentABI = NewABI;
			break;
		case NewABI:
			break;
	}

	// If we've already loaded this module, don't load it again.
	if (init->version == ULONG_MAX)
	{
		return;
	}
	register_loaded_objc_image(init);

	assert(init->version == 0);
	assert((((uintptr_t)init->sel_end-(uintptr_t)init->sel_begin) % sizeof(*init->sel_begin)) == 0);
	assert((((uintptr_t)init->cls_end-(uintptr_t)init->cls_begin) % sizeof(*init->cls_begin)) == 0);
	assert((((uintptr_t)init->cat_end-(uintptr_t)init->cat_begin) % sizeof(*init->cat_begin)) == 0);
	for (SEL sel = init->sel_begin ; sel < init->sel_end ; sel++)
	{
		if (sel->name == 0)
		{
			continue;
		}
		objc_register_selector(sel);
	}
	for (struct objc_protocol *proto = init->proto_begin ; proto < init->proto_end ;
	     proto++)
	{
		if (proto->name == NULL)
		{
			continue;
		}
		registerProtocol((struct objc_protocol*)proto);
	}
	for (struct objc_protocol **proto = init->proto_ref_begin ; proto < init->proto_ref_end ;
	     proto++)
	{
		if (*proto == NULL)
		{
			continue;
		}
		struct objc_protocol *p = objc_getProtocol((*proto)->name);
		assert(p);
		*proto = p;
	}
	int classesLoaded = 0;
	BOOL resolvedFutureClass = NO;
	for (Class *slot = init->cls_begin; slot < init->cls_end; slot++) {
		if (*slot == Nil) { continue; }
		Class incoming = *slot;
		Class canonical = objc_claim_future_class(incoming);
		if (canonical != incoming) {
			*slot = canonical;
			resolvedFutureClass = YES;
		}
	}
	if (resolvedFutureClass) { remap_loaded_class_references(); }
	else { remap_init_class_references(init); }

	for (Class *cls = init->cls_begin ; cls < init->cls_end ; cls++)
	{
		if (*cls == NULL) { continue; }
#ifdef DEBUG_LOADING
		fprintf(stderr, "Loading class %s\n", (*cls)->name);
#endif
		objc_load_class(*cls);
		classesLoaded++;
	}
	if (isFirstLoad && (classesLoaded == 0))
	{
		CurrentABI = UnknownABI;
	}
	for (struct objc_category *cat = init->cat_begin ; cat < init->cat_end ;
	     cat++)
	{
		if ((cat == NULL) || (cat->class_name == NULL))
		{
			continue;
		}
		objc_try_load_category(cat);
#ifdef DEBUG_LOADING
		fprintf(stderr, "Loading category %s (%s)\n", cat->class_name, cat->name);
#endif
	}
	// Load categories and statics that were deferred.
	objc_load_buffered_categories();
	// Fix up the class links for loaded classes.
	objc_resolve_class_links();
	for (struct objc_category *cat = init->cat_begin ; cat < init->cat_end ;
	     cat++)
	{
		Class class = (Class)objc_getClass(cat->class_name);
		if ((Nil != class) && 
		    objc_test_class_flag(class, objc_class_flag_resolved))
		{
			objc_send_load_message(class);
		}
	}
	// Register aliases
	for (struct objc_alias *alias = init->alias_begin ; alias < init->alias_end ;
	     alias++)
	{
		if (alias->alias_name)
		{
			class_registerAlias_np(*alias->alias, alias->alias_name);
		}
	}
#if 0
	// If future versions of the ABI need to do anything with constant strings,
	// they may do so here.
	for (struct nsstr *string = init->strings_begin ; string < init->strings_end ;
	     string++)
	{
		if (string->isa)
		{
		}
	}
#endif
	init->version = ULONG_MAX;
}

#ifdef OLDABI_COMPAT
OBJC_PUBLIC void __objc_exec_class(struct objc_module_abi_8 *module)
{
	init_runtime();

	switch (CurrentABI)
	{
		case UnknownABI:
			CurrentABI = LegacyABI;
			break;
		case LegacyABI:
			break;
		case NewABI:
			fprintf(stderr, "Version 2 Objective-C ABI may not be mixed with earlier versions.\n");
			abort();
	}

	// Check that this module uses an ABI version that we recognise.  
	// In future, we should pass the ABI version to the class / category load
	// functions so that we can change various structures more easily.
	assert(objc_check_abi_version(module));


	// The runtime mutex is held for the entire duration of a load.  It does
	// not need to be acquired or released in any of the called load functions.
	LOCK_RUNTIME_FOR_SCOPE();

	struct objc_symbol_table_abi_8 *symbols = module->symbol_table;
	// Register all of the selectors used in this module.
	if (symbols->selectors)
	{
		objc_register_selector_array(symbols->selectors,
				symbols->selector_count);
	}

	unsigned short defs = 0;
	// Load the classes from this module
	for (unsigned short i=0 ; i<symbols->class_count ; i++)
	{
		objc_load_class(objc_upgrade_class(symbols->definitions[defs++]));
	}
	unsigned int category_start = defs;
	// Load the categories from this module
	for (unsigned short i=0 ; i<symbols->category_count; i++)
	{
		objc_try_load_category(objc_upgrade_category(symbols->definitions[defs++]));
	}
	// Load the static instances
	struct objc_static_instance_list **statics = (void*)symbols->definitions[defs];
	while (NULL != statics && NULL != *statics)
	{
		objc_init_statics(*(statics++));
	}

	// Load categories and statics that were deferred.
	objc_load_buffered_categories();
	objc_init_buffered_statics();
	// Fix up the class links for loaded classes.
	objc_resolve_class_links();
	for (unsigned short i=0 ; i<symbols->category_count; i++)
	{
		struct objc_category *cat = (struct objc_category*)
			symbols->definitions[category_start++];
		Class class = (Class)objc_getClass(cat->class_name);
		if ((Nil != class) && 
		    objc_test_class_flag(class, objc_class_flag_resolved))
		{
			objc_send_load_message(class);
		}
	}
}
#endif
