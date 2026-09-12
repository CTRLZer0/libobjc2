// On some platforms, we need _GNU_SOURCE to expose asprintf()
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "crt_compat.h"
#include "objc/runtime.h"
#include "objc/blocks/runtime.h"
#include "blocks_runtime.h"
#include "block_lifecycle.h"
#include "observability.h"
#include "lock.h"
#include "platform.h"
#include "visibility.h"

/* QNX needs a special header for asprintf() */
#ifdef __QNXNTO__
#include <nbutil.h>
#endif

struct block_header
{
	void *block;
	void(*fnptr)(void);
	/*
	 * On 64-bit platforms, we have 16 bytes for instructions, which ought to
	 * be enough without padding.
	 * Note: If we add too much padding, then we waste space but have no other
	 * ill effects.  If we get this too small, then the assert in
	 * `init_trampolines` will fire on library load.
	 *
	 * PowerPC: We need INSTR_CNT * INSTR_LEN = 7*4 = 28 bytes
	 * for instruction. sizeof(block_header) must be a divisor of
	 * PAGE_SIZE, so we need to pad block_header to 32 bytes.
	 * On PowerPC 64-bit where sizeof(void *) = 8 bytes, we
	 * add 16 bytes of padding.
	 */
#if defined(__i386__) || (defined(__mips__) && !defined(__mips_n64)) || (defined(__powerpc__) && !defined(__powerpc64__))
	uint64_t padding[3];
#elif defined(__mips__) || defined(__ARM_ARCH_ISA_A64) || defined(__powerpc64__)
	uint64_t padding[2];
#elif defined(__arm__)
	uint64_t padding;
#endif
};

struct trampoline_set
{
	/*
	* Each trampoline loads its block and target method address
	* from the corresponding block_header
	* (one page before the start of the block structure).
	*
	* Page | Description
	*    1 | Page filled with block_header's
	*    2 | RX buffer page
	*/
	char  *region;
	struct trampoline_set *next;
	int first_free;
};


/*
 * Current page size of the system in bytes.
 * Set in init_trampolines.
 */
static int trampoline_page_size;
/*
 * Number of block_header's per page.
 * Calculated in init_trampolines after retrieving the current page size:
 */
static size_t trampoline_header_per_page;
/*
 * Size of a trampoline region in bytes.
 */
static size_t trampoline_region_size;
static mutex_t trampoline_lock;

/*
 * Size of the trampoline region (in pages)
 */
#define TRAMPOLINE_REGION_PAGES 2

#define REGION_HEADERS_START(metadata) ((struct block_header *) metadata->region)
#define REGION_RX_BUFFER_START(metadata) (metadata->region + trampoline_page_size)

struct wx_buffer
{
	void *w;
	void *x;
};
extern char __objc_block_trampoline;
extern char __objc_block_trampoline_end;
extern char __objc_block_trampoline_sret;
extern char __objc_block_trampoline_end_sret;

#if defined(__ARM_ARCH_ISA_A64)
extern char __objc_block_trampoline_16;
extern char __objc_block_trampoline_end_16;
extern char __objc_block_trampoline_sret_16;
extern char __objc_block_trampoline_end_sret_16;
#endif

// Cache the correct trampoline region
static void *trampoline_start;
static void *trampoline_end;
static void *trampoline_start_sret;
static void *trampoline_end_sret;

PRIVATE void init_trampolines(void)
{
	// Retrieve the page size
	#if defined(__powerpc64__)
	// For PowerPC we fix the page size to 64KiB.
	// We therefore effectively support all systems with page size <= 64 KiB.
	trampoline_page_size = 0x10000;
	// Check that the pagesize is greater or equal to the smallest size that we
	// can perform mprotect operations on.
	assert(objc_platform_page_size() <= (size_t)trampoline_page_size);
	#else
	trampoline_page_size = (int)objc_platform_page_size();
	#endif

	trampoline_region_size = trampoline_page_size * TRAMPOLINE_REGION_PAGES;
	trampoline_header_per_page = trampoline_page_size / sizeof(struct block_header);

	// Check that sizeof(struct block_header) is a divisor of the current page size
	assert(trampoline_header_per_page * sizeof(struct block_header) == trampoline_page_size);

    // Check that assumptions for all non-variable page size implementations
	// (currently everything except AArch64) are met
#if defined(__powerpc64__)
	assert(trampoline_page_size == 0x10000);
#elif defined(__ARM_ARCH_ISA_A64)
	assert(trampoline_page_size == 0x1000 || trampoline_page_size == 0x4000);
#else
	assert(trampoline_page_size == 0x1000);
#endif

	// Select the correct trampoline for our page size
#if defined(__ARM_ARCH_ISA_A64)
	if (trampoline_page_size == 0x4000) {
		trampoline_start = &__objc_block_trampoline_16;
		trampoline_end = &__objc_block_trampoline_end_16;
		trampoline_start_sret = &__objc_block_trampoline_sret_16;
		trampoline_end_sret = &__objc_block_trampoline_end_sret_16;
	} else {
#else
	{
#endif
		trampoline_start = &__objc_block_trampoline;
		trampoline_end = &__objc_block_trampoline_end;
		trampoline_start_sret = &__objc_block_trampoline_sret;
		trampoline_end_sret = &__objc_block_trampoline_end_sret;
	}	

	// Check that we can fit the body of the trampoline function inside a block_header
	assert(trampoline_end - trampoline_start <= sizeof(struct block_header));
	assert(trampoline_end_sret - trampoline_start_sret <= sizeof(struct block_header));

	INIT_LOCK(trampoline_lock);
}

static id invalid(id self, SEL _cmd)
{
	fprintf(stderr, "Invalid block method called for [%s %s]\n",
			class_getName(object_getClass(self)), sel_getName(_cmd));
	return nil;
}

static struct trampoline_set *alloc_trampolines(char *start, char *end)
{
	struct trampoline_set *metadata = calloc(1, sizeof(struct trampoline_set));
	if (metadata == NULL) { return NULL; }
	metadata->region = objc_platform_pages_allocate(trampoline_region_size);
	if (metadata->region == NULL)
	{
		free(metadata);
		return NULL;
	}
	metadata->first_free = 0;
	struct block_header *headers_start = REGION_HEADERS_START(metadata);
	char *rx_buffer_start = REGION_RX_BUFFER_START(metadata);
	for (int i=0 ; i<trampoline_header_per_page ; i++)
	{
		headers_start[i].fnptr = (void(*)(void))invalid;
		headers_start[i].block = &headers_start[i+1].block;
		char *block = rx_buffer_start + (i * sizeof(struct block_header));

		memcpy(block, start, end-start);
	}
	headers_start[trampoline_header_per_page-1].block = NULL;
	if (objc_platform_pages_protect(rx_buffer_start, trampoline_page_size,
	                                OBJC_PLATFORM_PAGE_READ | OBJC_PLATFORM_PAGE_EXEC) != 0)
	{
		objc_platform_pages_release(metadata->region, trampoline_region_size);
		free(metadata);
		return NULL;
	}
	objc_platform_instruction_cache_flush(rx_buffer_start, trampoline_page_size);

	return metadata;
}

static struct trampoline_set *sret_trampolines;
static struct trampoline_set *trampolines;

enum { BLOCK_LIFECYCLE_REFERENCE_CAPACITY = 7 };
struct block_lifecycle_snapshot
{
	uintptr_t references[BLOCK_LIFECYCLE_REFERENCE_CAPACITY];
	size_t count;
	struct block_lifecycle_snapshot *next;
};
static struct block_lifecycle_snapshot *retiring_block_snapshots;

static BOOL block_reference_in_range(uintptr_t address, uintptr_t base, size_t size)
{
	return (address >= base) && ((address - base) < size);
}

static BOOL block_header_is_free(struct trampoline_set *set, struct block_header *header)
{
	if (header->block == NULL) { return YES; }
	uintptr_t address = (uintptr_t)header->block;
	uintptr_t begin = (uintptr_t)REGION_HEADERS_START(set);
	return (address >= begin) && ((address - begin) < (size_t)trampoline_page_size);
}

static void block_snapshot_add(struct block_lifecycle_snapshot *snapshot, uintptr_t address)
{
	if ((address == 0) || (snapshot->count >= BLOCK_LIFECYCLE_REFERENCE_CAPACITY)) { return; }
	snapshot->references[snapshot->count++] = address;
}

static void block_snapshot_capture(struct block_lifecycle_snapshot *snapshot,
                                   struct block_header *header)
{
	struct Block_layout *block = (struct Block_layout *)header->block;
	block_snapshot_add(snapshot, (uintptr_t)(void *)header->fnptr);
	block_snapshot_add(snapshot, (uintptr_t)block);
	block_snapshot_add(snapshot, (uintptr_t)(void *)block->invoke);
	block_snapshot_add(snapshot, (uintptr_t)block->descriptor);
	if (block->descriptor == NULL) { return; }
	if ((block->flags & BLOCK_HAS_COPY_DISPOSE) != 0)
	{
		block_snapshot_add(snapshot, (uintptr_t)(void *)block->descriptor->copy_helper);
		block_snapshot_add(snapshot, (uintptr_t)(void *)block->descriptor->dispose_helper);
		if ((block->flags & BLOCK_HAS_SIGNATURE) != 0)
		{
			block_snapshot_add(snapshot, (uintptr_t)block->descriptor->encoding);
		}
	}
	else if ((block->flags & BLOCK_HAS_SIGNATURE) != 0)
	{
		struct Block_descriptor_basic *descriptor =
		    (struct Block_descriptor_basic *)block->descriptor;
		block_snapshot_add(snapshot, (uintptr_t)descriptor->encoding);
	}
}

static void count_block_snapshot(const struct block_lifecycle_snapshot *snapshot,
                                 uintptr_t base, size_t size, size_t *count)
{
	for (size_t i = 0; i < snapshot->count; i++)
	{
		if ((*count != SIZE_MAX) &&
		    block_reference_in_range(snapshot->references[i], base, size))
		{
			(*count)++;
		}
	}
}

static void count_block_set_references(struct trampoline_set *set,
                                       uintptr_t base, size_t size, size_t *count)
{
	for (; set != NULL; set = set->next)
	{
		struct block_header *headers = REGION_HEADERS_START(set);
		for (size_t i = 0; i < trampoline_header_per_page; i++)
		{
			struct block_header *header = &headers[i];
			if (block_header_is_free(set, header)) { continue; }
			struct block_lifecycle_snapshot snapshot = {0};
			block_snapshot_capture(&snapshot, header);
			count_block_snapshot(&snapshot, base, size, count);
		}
	}
}

PRIVATE size_t objc2_countBlockTrampolineReferences(uintptr_t base, size_t size)
{
	if (size == 0) { return 0; }
	size_t count = 0;
	LOCK_FOR_SCOPE(&trampoline_lock);
	count_block_set_references(trampolines, base, size, &count);
	count_block_set_references(sret_trampolines, base, size, &count);
	for (struct block_lifecycle_snapshot *snapshot = retiring_block_snapshots;
	     snapshot != NULL; snapshot = snapshot->next)
	{
		count_block_snapshot(snapshot, base, size, &count);
	}
	return count;
}

IMP imp_implementationWithBlock(id block)
{
	if (block == nil) { return 0; }
	struct Block_layout *b = (struct Block_layout *)block;
	void *start;
	void *end;
	struct trampoline_set **setptr;

	if ((b->flags & BLOCK_USE_SRET) == BLOCK_USE_SRET)
	{
		setptr = &sret_trampolines;
		start = trampoline_start_sret;
		end = trampoline_end_sret;
	}
	else
	{
		setptr = &trampolines;
		start = trampoline_start;
		end = trampoline_end;
	}
	if (0 >= (end - start)) { return 0; }
	block = Block_copy(block);
	if (block == nil) { return 0; }
	b = (struct Block_layout *)block;
	IMP result = 0;
	{
		LOCK_FOR_SCOPE(&trampoline_lock);
		struct trampoline_set *set = *setptr;
		while ((set != NULL) && (set->first_free == -1)) { set = set->next; }
		if (set == NULL)
		{
			set = alloc_trampolines(start, end);
			if (set != NULL)
			{
				set->next = *setptr;
				*setptr = set;
			}
		}
		if (set != NULL)
		{
			int i = set->first_free;
			struct block_header *headers_start = REGION_HEADERS_START(set);
			char *rx_buffer_start = REGION_RX_BUFFER_START(set);
			struct block_header *h = &headers_start[i];
			struct block_header *next = h->block;
			mosaic_objc_beginRuntimeMutation();
			set->first_free = next ? (next - headers_start) : -1;
			assert(set->first_free >= -1);
			assert((set->first_free == -1) ||
			       ((size_t)set->first_free < trampoline_header_per_page));
			h->fnptr = (void(*)(void))b->invoke;
			h->block = b;
			mosaic_objc_endRuntimeMutation();
			uintptr_t addr = (uintptr_t)&rx_buffer_start[i*sizeof(struct block_header)];
#if (__ARM_ARCH_ISA_THUMB == 2)
			addr |= 1;
#endif
			result = (IMP)addr;
		}
	}
	if (result == 0) { Block_release(block); }
	return result;
}

static int indexForIMP(IMP anIMP, struct trampoline_set **setptr)
{
	for (struct trampoline_set *set=*setptr ; set!=NULL ; set=set->next)
	{
		struct block_header *headers_start = REGION_HEADERS_START(set);
		char *rx_buffer_start = REGION_RX_BUFFER_START(set);
		if (((char *)anIMP >= rx_buffer_start) &&
		    ((char *)anIMP < &rx_buffer_start[trampoline_page_size]))
		{
			*setptr = set;
			ptrdiff_t offset = (char *)anIMP - rx_buffer_start;
			return offset / sizeof(struct block_header);
		}
	}
	return -1;
}

id imp_getBlock(IMP anImp)
{
	LOCK_FOR_SCOPE(&trampoline_lock);
	struct trampoline_set *set = trampolines;
	int idx = indexForIMP(anImp, &set);
	if (idx == -1)
	{
		set = sret_trampolines;
		idx = indexForIMP(anImp, &set);
	}
	if (idx == -1)
	{
		return NULL;
	}
	return REGION_HEADERS_START(set)[idx].block;
}

BOOL imp_removeBlock(IMP anImp)
{
	struct block_lifecycle_snapshot *retiring = calloc(1, sizeof(*retiring));
	if (retiring == NULL) { return NO; }
	id block = nil;
	{
		LOCK_FOR_SCOPE(&trampoline_lock);
		struct trampoline_set *set = trampolines;
		int idx = indexForIMP(anImp, &set);
		if (idx == -1)
		{
			set = sret_trampolines;
			idx = indexForIMP(anImp, &set);
		}
		if (idx == -1)
		{
			free(retiring);
			return NO;
		}
		struct block_header *header_start = REGION_HEADERS_START(set);
		struct block_header *h = &header_start[idx];
		block = h->block;
		block_snapshot_capture(retiring, h);
		mosaic_objc_beginRuntimeMutation();
		retiring->next = retiring_block_snapshots;
		retiring_block_snapshots = retiring;
		h->fnptr = (void(*)(void))invalid;
		h->block = set->first_free == -1 ? NULL : &header_start[set->first_free];
		set->first_free = h - header_start;
		mosaic_objc_endRuntimeMutation();
	}
	Block_release(block);
	{
		LOCK_FOR_SCOPE(&trampoline_lock);
		struct block_lifecycle_snapshot **cursor = &retiring_block_snapshots;
		while ((*cursor != NULL) && (*cursor != retiring)) { cursor = &(*cursor)->next; }
		if (*cursor == retiring)
		{
			mosaic_objc_beginRuntimeMutation();
			*cursor = retiring->next;
			mosaic_objc_endRuntimeMutation();
		}
	}
	free(retiring);
	return YES;
}

PRIVATE size_t lengthOfTypeEncoding(const char *types);

char *block_copyIMPTypeEncoding_np(id block)
{
	char *buffer = objc2_strdup(block_getType_np(block));
	if (NULL == buffer) { return NULL; }
	char *replace = buffer;
	// Skip the return type
	replace += lengthOfTypeEncoding(replace);
	while (isdigit(*replace)) { replace++; }
	// The first argument type should be @? (block), and we need to transform
	// it to @, so we have to delete the ?.  Assert here because this isn't a
	// block encoding at all if the first argument is not a block, and since we
	// got it from block_getType_np(), this means something is badly wrong.
	assert('@' == *replace);
	replace++;
	assert('?' == *replace);
	// Use strlen(replace) not replace+1, because we want to copy the NULL
	// terminator as well.
	memmove(replace, replace+1, strlen(replace));
	// The next argument should be an object, and we want to replace it with a
	// selector
	while (isdigit(*replace)) { replace++; }
	if ('@' != *replace)
	{
		free(buffer);
		return NULL;
	}
	*replace = ':';
	return buffer;
}
