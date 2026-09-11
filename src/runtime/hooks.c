#include "objc/runtime.h"
#include "objc/extensions/mosaic.h"
#include "observability.h"
#define OBJC_HOOK OBJC_PUBLIC
#include "objc/support/hooks.h"

static mosaic_objc_runtime_event_sink_t runtime_event_sink;
static void *runtime_event_context;
static uint64_t runtime_event_sequence;

void mosaic_objc_runtimeSetEventSink(mosaic_objc_runtime_event_sink_t sink,
                                     void *context)
{
	/* Publish NULL first so replacing a sink never observes a new context with
	 * a different old callback. Events may be dropped while reconfiguring. */
	__atomic_store_n(&runtime_event_sink, NULL, __ATOMIC_RELEASE);
	__atomic_store_n(&runtime_event_context, context, __ATOMIC_RELEASE);
	__atomic_store_n(&runtime_event_sink, sink, __ATOMIC_RELEASE);
}

PRIVATE void mosaic_objc_emitRuntimeEvent(struct mosaic_objc_runtime_event *event)
{
	if (event == NULL) { return; }
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
