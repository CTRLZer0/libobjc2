/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
 * and provenance details.
 */
#include "test_support.h"
#include "objc/runtime.h"
#include "objc/objc-arc.h"
#include "objc/mosaic.h"

static unsigned retains;
static unsigned releases;

static id retain_object(id self, SEL _cmd)
{
	(void)_cmd;
	retains++;
	return self;
}

static void release_object(id self, SEL _cmd)
{
	(void)self;
	(void)_cmd;
	releases++;
}

static void install_memory_methods(Class cls)
{
	CHECK(class_addMethod(cls, sel_registerName("retain"),
		__builtin_bit_cast(IMP, &retain_object), "@@:"));
	CHECK(class_addMethod(cls, sel_registerName("release"),
		__builtin_bit_cast(IMP, &release_object), "v@:"));
}

int main(void)
{
	static char key;
	mosaic_objc_runtime_initialize();

	Class cls = objc_allocateClassPair(Nil, "MosaicMemoryContract", 0);
	CHECK(cls != Nil);
	install_memory_methods(cls);
	objc_registerClassPair(cls);

	id holder = class_createInstance(cls, 0);
	id value = class_createInstance(cls, 0);
	CHECK(holder != nil && value != nil);

	CHECK(objc_retain(value) == value);
	objc_release(value);
	CHECK(retains == 1);
	CHECK(releases == 1);

	objc_setAssociatedObject(holder, &key, value, OBJC_ASSOCIATION_RETAIN);
	CHECK(object_getClass(holder) == cls);
	CHECK(retains == 2);
	void *pool = objc_autoreleasePoolPush();
	CHECK(objc_getAssociatedObject(holder, &key) == value);
	CHECK(retains == 3);
	objc_autoreleasePoolPop(pool);
	CHECK(releases == 2);
	objc_removeAssociatedObjects(holder);
	CHECK(objc_getAssociatedObject(holder, &key) == nil);
	CHECK(releases == 3);

	object_dispose(value);
	object_dispose(holder);
	return 0;
}
