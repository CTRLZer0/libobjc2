/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
 * and provenance details.
 */
#include "test_support.h"
#include <string.h>
#include "objc/runtime.h"
#include "objc/mosaic.h"

int main(void)
{
	mosaic_objc_runtime_initialize();

	Class cls = objc_allocateClassPair(Nil, "MosaicClassContract", 0);
	CHECK(cls != Nil);
	CHECK(class_isMetaClass(cls) == NO);
	CHECK(strcmp(class_getName(cls), "MosaicClassContract") == 0);

	Class meta = object_getClass((id)cls);
	CHECK(meta != Nil);
	CHECK(class_isMetaClass(meta) == YES);

	objc_registerClassPair(cls);
	CHECK((Class)objc_getClass("MosaicClassContract") == cls);
	CHECK((Class)objc_getMetaClass("MosaicClassContract") == meta);

	Class replacement = objc_allocateClassPair(Nil, "MosaicClassContractReplacement", 0);
	CHECK(replacement != Nil);
	objc_registerClassPair(replacement);

	id object = class_createInstance(cls, 0);
	CHECK(object != nil);
	CHECK(object_getClass(object) == cls);
	CHECK(object_setClass(object, replacement) == cls);
	CHECK(object_getClass(object) == replacement);
	CHECK(object_setClass(object, cls) == replacement);
	CHECK(object_getClass(object) == cls);
	object_dispose(object);
	return 0;
}
