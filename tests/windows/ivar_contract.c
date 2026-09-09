/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
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
