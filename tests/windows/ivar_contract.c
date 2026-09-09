/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * Original CTRLZer0 work; see LICENSE-CTRLZERO and NOTICE.md for licensing
 * and provenance details.
 */
#include "test_support.h"
#include "objc/runtime.h"
#include "objc/mosaic.h"

int main(void)
{
	mosaic_objc_runtime_initialize();

	Class cls = objc_allocateClassPair(Nil, "MosaicIvarContract", 0);
	CHECK(cls != Nil);
	CHECK(class_addIvar(cls, "payload", sizeof(id), 3, "@"));
	objc_registerClassPair(cls);

	Ivar ivar = class_getInstanceVariable(cls, "payload");
	CHECK(ivar != NULL);
	CHECK(ivar_getOffset(ivar) >= 0);
	CHECK(class_getInstanceSize(cls) >= sizeof(id));

	id object = class_createInstance(cls, 0);
	id value = class_createInstance(cls, 0);
	CHECK(object != nil && value != nil);
	object_setIvar(object, ivar, value);
	CHECK(object_getIvar(object, ivar) == value);

	object_dispose(value);
	object_dispose(object);
	return 0;
}
