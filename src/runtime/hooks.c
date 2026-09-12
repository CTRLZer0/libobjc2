#include "objc/runtime.h"
#include "objc/extensions/mosaic.h"
#include "observability.h"
#include "lifecycle.h"
#define OBJC_HOOK OBJC_PUBLIC
#include "objc/support/hooks.h"

static mosaic_objc_runtime_event_sink_t runtime_event_sink;
static void *runtime_event_context;
static uint64_t runtime_event_sequence;
static uint64_t runtime_mutation_epoch;
static unsigned char runtime_mutation_writer_lock;

void mosaic_objc_runtimeSetEventSink(mosaic_objc_runtime_event_sink_t sink,
                                     void *context)
{
	/* Publish NULL first so replacing a sink never observes a new context with
	 * a different old callback. Events may be dropped while reconfiguring. */
	mosaic_objc_beginRuntimeMutation();
	__atomic_store_n(&runtime_event_sink, NULL, __ATOMIC_RELEASE);
	__atomic_store_n(&runtime_event_context, context, __ATOMIC_RELEASE);
	__atomic_store_n(&runtime_event_sink, sink, __ATOMIC_RELEASE);
	mosaic_objc_endRuntimeMutation();
}

PRIVATE uint64_t mosaic_objc_runtimeMutationEpoch(void)
{
	return __atomic_load_n(&runtime_mutation_epoch, __ATOMIC_ACQUIRE);
}

PRIVATE void mosaic_objc_beginRuntimeMutation(void)
{
	while (__atomic_test_and_set(&runtime_mutation_writer_lock, __ATOMIC_ACQUIRE)) {}
	(void)__atomic_add_fetch(&runtime_mutation_epoch, 1, __ATOMIC_RELEASE);
}

PRIVATE void mosaic_objc_endRuntimeMutation(void)
{
	(void)__atomic_add_fetch(&runtime_mutation_epoch, 1, __ATOMIC_RELEASE);
	__atomic_clear(&runtime_mutation_writer_lock, __ATOMIC_RELEASE);
}

PRIVATE uint64_t mosaic_objc_noteRuntimeMutation(void)
{
	/* Preserve the low bit: odd epochs are reserved for an in-flight writer. */
	return __atomic_add_fetch(&runtime_mutation_epoch, 2, __ATOMIC_RELEASE);
}

static BOOL mosaic_objc_hookInRange(const void *address, uintptr_t base, size_t size)
{
	if ((address == NULL) || (size == 0)) { return NO; }
	uintptr_t value = (uintptr_t)address;
	return (value >= base) && ((value - base) < size);
}

PRIVATE size_t mosaic_objc_countGlobalHookReferences(uintptr_t base, size_t size)
{
	size_t count = 0;
#define COUNT_HOOK(hook) do { if (mosaic_objc_hookInRange((const void*)(hook), base, size)) { count++; } } while (0)
	COUNT_HOOK(runtime_event_sink);
	COUNT_HOOK(_objc_lookup_class);
	COUNT_HOOK(_objc_load_callback);
	COUNT_HOOK(objc_proxy_lookup);
	COUNT_HOOK(__objc_msg_forward3);
	COUNT_HOOK(__objc_msg_forward2);
	COUNT_HOOK(_objc_class_for_boxing_foreign_exception);
	COUNT_HOOK(_objc_selector_type_mismatch2);
	COUNT_HOOK(_objc_selector_type_mismatch);
	COUNT_HOOK(_objc_weak_load);
#undef COUNT_HOOK
	count += objc2_countExceptionHookReferences(base, size);
	return count;
}

PRIVATE void mosaic_objc_emitRuntimeEvent(struct mosaic_objc_runtime_event *event)
{
	if (event == NULL) { return; }
	mosaic_objc_noteRuntimeMutation();
	mosaic_objc_runtime_event_sink_t sink =
		__atomic_load_n(&runtime_event_sink, __ATOMIC_ACQUIRE);
	if (sink == NULL) { return; }
	void *context = __atomic_load_n(&runtime_event_context, __ATOMIC_ACQUIRE);
	if (sink != __atomic_load_n(&runtime_event_sink, __ATOMIC_ACQUIRE))
	{
		return;
	}
	event->sequence =
		__atomic_add_fetch(&runtime_event_sequence, 1, __ATOMIC_RELAXED);
	sink(event, context);
}
