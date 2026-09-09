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

	Class cached = objc_allocateClassPair(Nil, "MosaicClassCacheContract", 0);
	CHECK(cached != Nil);
	objc_registerClassPair(cached);
	CHECK(objc_lookUpClass("MosaicClassCacheContract") == cached);
	CHECK((Class)objc_getClass("MosaicClassCacheContract") == cached);
	objc_disposeClassPair(cached);
	CHECK(objc_lookUpClass("MosaicClassCacheContract") == Nil);

	Class reloaded = objc_allocateClassPair(Nil, "MosaicClassCacheContract", 0);
	CHECK(reloaded != Nil);
	objc_registerClassPair(reloaded);
	CHECK(objc_lookUpClass("MosaicClassCacheContract") == reloaded);
	CHECK((Class)objc_getClass("MosaicClassCacheContract") == reloaded);
	objc_disposeClassPair(reloaded);

	Class bufferA = objc_allocateClassPair(Nil, "MosaicClassBufferA", 0);
	Class bufferB = objc_allocateClassPair(Nil, "MosaicClassBufferB", 0);
	CHECK(bufferA != Nil && bufferB != Nil);
	objc_registerClassPair(bufferA);
	objc_registerClassPair(bufferB);
	char mutableName[64] = "MosaicClassBufferA";
	CHECK(objc_lookUpClass(mutableName) == bufferA);
	memcpy(mutableName, "MosaicClassBufferB", sizeof("MosaicClassBufferB"));
	CHECK(objc_lookUpClass(mutableName) == bufferB);
	return 0;
}
