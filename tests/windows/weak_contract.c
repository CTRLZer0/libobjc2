/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * Original CTRLZer0 work; see LICENSE-CTRLZERO and NOTICE.md for licensing
 * and provenance details.
 */
#include <assert.h>
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
	assert(cls != Nil);
	objc_registerClassPair(cls);

	id object = class_createInstance(cls, 0);
	id weak = nil;
	assert(objc_storeWeak(&weak, object) == object);
	assert(weak == object);

	objc_delete_weak_refs(object);
	assert(weak == nil);
	objc_destroyWeak(&weak);
	object_dispose(object);
	return 0;
}
