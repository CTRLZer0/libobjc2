/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
 * and provenance details.
 */
#include "test_support.h"
#include "objc/runtime.h"
#include "objc/objc-arc.h"
#include "objc/hooks.h"
#include "objc/mosaic.h"

static id secondary_weak;
static unsigned weak_load_calls;
static unsigned weak_load_reentries;
static unsigned manual_retains;
static int inside_weak_load;

static id manual_retain(id self, SEL _cmd)
{
	(void)_cmd;
	manual_retains++;
	return objc_retain_fast_np(self);
}

static void manual_release(id self, SEL _cmd)
{
	(void)_cmd;
	objc_release_fast_np(self);
}

static id weak_load(id object)
{
	weak_load_calls++;
	if (!inside_weak_load && secondary_weak != nil)
	{
		inside_weak_load = 1;
		id nested = objc_loadWeakRetained(&secondary_weak);
		if (nested == object) { weak_load_reentries++; }
		if (nested != nil) { objc_release_fast_np(nested); }
		inside_weak_load = 0;
	}
	return object;
}

int main(void)
{
	mosaic_objc_runtime_initialize();
	_objc_weak_load = weak_load;

	Class cls = objc_allocateClassPair(Nil, "MosaicWeakContract", 0);
	CHECK(cls != Nil);
	objc_registerClassPair(cls);

	id object = class_createInstance(cls, 0);
	id weak = nil;
	CHECK(object != nil);
	CHECK(object_getRetainCount_np(object) == 1);
	CHECK(objc_initWeak(&weak, object) == object);
	CHECK(objc_loadWeakRetained(&weak) == object);
	CHECK(objc_release_fast_no_destroy_np(object) == NO);
	CHECK(objc_release_fast_no_destroy_np(object) == YES);
	CHECK(objc_loadWeakRetained(&weak) == nil);
	objc_destroyWeak(&weak);
	object_dispose(object);

	Class manual = objc_allocateClassPair(Nil, "MosaicManualWeakContract", 0);
	CHECK(manual != Nil);
	CHECK(class_addMethod(manual, sel_registerName("retain"),
		__builtin_bit_cast(IMP, &manual_retain), "@@:"));
	CHECK(class_addMethod(manual, sel_registerName("release"),
		__builtin_bit_cast(IMP, &manual_release), "v@:"));
	objc_registerClassPair(manual);

	id manual_object = class_createInstance(manual, 0);
	id primary_weak = nil;
	secondary_weak = nil;
	CHECK(manual_object != nil);
	CHECK(objc_initWeak(&primary_weak, manual_object) == manual_object);
	CHECK(objc_initWeak(&secondary_weak, manual_object) == manual_object);
	id retained = objc_loadWeakRetained(&primary_weak);
	CHECK(retained == manual_object);
	CHECK(weak_load_calls >= 2);
	CHECK(weak_load_reentries == 1);
	CHECK(manual_retains >= 2);
	objc_release_fast_np(retained);
	objc_destroyWeak(&primary_weak);
	objc_destroyWeak(&secondary_weak);
	CHECK(objc_release_fast_no_destroy_np(manual_object) == YES);
	object_dispose(manual_object);
	return 0;
}
