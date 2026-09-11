/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 */
#include "test_support.h"
#include <errno.h>
#include <stdint.h>
#include "objc/runtime.h"
#include "objc/dispatch/message.h"
#include "objc/support/capabilities.h"
#include "objc/support/hooks.h"
#include "objc/mosaic.h"

static unsigned trace_entries;
static unsigned trace_exits;
static unsigned trace_depth;
static unsigned max_trace_depth;
static SEL inner_selector;

typedef uintptr_t (*word_send_fn)(id, SEL);

static uintptr_t inner_method(id self, SEL _cmd)
{
	(void)self;
	(void)_cmd;
	return 0x1234u;
}

static uintptr_t outer_method(id self, SEL _cmd)
{
	(void)_cmd;
	word_send_fn send = (word_send_fn)(void*)objc_msgSend;
	return send(self, inner_selector) + 1u;
}

static uintptr_t interposed_method(id self, SEL _cmd)
{
	(void)self;
	(void)_cmd;
	return 0xCAFEu;
}

static IMP tracing_hook(id receiver, SEL selector, IMP method, int leaving, void *result)
{
	(void)receiver;
	(void)selector;
	(void)method;
	(void)result;
	if (!leaving)
	{
		trace_entries++;
		trace_depth++;
		if (trace_depth > max_trace_depth) { max_trace_depth = trace_depth; }
		return (IMP)1;
	}
	trace_exits++;
	CHECK(trace_depth != 0);
	trace_depth--;
	return (IMP)0;
}

static IMP interpose_hook(id receiver, SEL selector, IMP method, int leaving, void *result)
{
	(void)receiver;
	(void)selector;
	(void)method;
	(void)result;
	CHECK(leaving == 0);
	return (IMP)(void*)interposed_method;
}

int main(void)
{
	mosaic_objc_runtime_initialize();
	SEL outer = sel_registerName("traceOuter");
	inner_selector = sel_registerName("traceInner");
	CHECK(outer != NULL && inner_selector != NULL);

	if (!objc_test_capability(OBJC_CAP_TRACING))
	{
		CHECK(objc_registerTracingHook(outer, tracing_hook) != 0);
		return 0;
	}

	Class cls = objc_allocateClassPair(Nil, "MosaicTracingContract", 0);
	CHECK(cls != Nil);
	CHECK(class_addMethod(cls, outer, (IMP)(void*)outer_method, "Q@:") == YES);
	CHECK(class_addMethod(cls, inner_selector, (IMP)(void*)inner_method, "Q@:") == YES);
	objc_registerClassPair(cls);
	id object = class_createInstance(cls, 0);
	CHECK(object != nil);

	CHECK(objc_registerTracingHook(outer, tracing_hook) == 0);
	CHECK(objc_registerTracingHook(inner_selector, tracing_hook) == 0);
	word_send_fn send = (word_send_fn)(void*)objc_msgSend;
	CHECK(send(object, outer) == 0x1235u);
	CHECK(trace_entries == 2);
	CHECK(trace_exits == 2);
	CHECK(trace_depth == 0);
	CHECK(max_trace_depth == 2);

	CHECK(objc_registerTracingHook(outer, interpose_hook) == 0);
	CHECK(send(object, outer) == 0xCAFEu);

	object_dispose(object);
	objc_disposeClassPair(cls);
	return 0;
}
