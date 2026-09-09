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

static id weak_load(id object)
{
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
	CHECK(objc_storeWeak(&weak, object) == object);
	CHECK(weak == object);

	objc_delete_weak_refs(object);
	CHECK(weak == nil);
	objc_destroyWeak(&weak);
	object_dispose(object);
	return 0;
}
