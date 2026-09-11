/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 */
#include "test_support.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "objc/runtime.h"
#include "objc/mosaic.h"

static unsigned construct_count;
static unsigned destruct_count;
static char association_key;

static void cxx_construct(id self, SEL _cmd)
{
	(void)_cmd;
	CHECK(self != nil);
	construct_count++;
}

static void cxx_destruct(id self, SEL _cmd)
{
	(void)_cmd;
	CHECK(self != nil);
	destruct_count++;
}

int main(void)
{
	mosaic_objc_runtime_initialize();
	Class cls = objc_allocateClassPair(Nil, "MosaicConstructDestructContract", 0);
	CHECK(cls != Nil);
	CHECK(class_addIvar(cls, "payload", sizeof(uintptr_t), 3, "Q") == YES);
	CHECK(class_addMethod(cls, sel_registerName(".cxx_construct"),
	                      (IMP)(void*)cxx_construct, "v@:") == YES);
	CHECK(class_addMethod(cls, sel_registerName(".cxx_destruct"),
	                      (IMP)(void*)cxx_destruct, "v@:") == YES);
	objc_registerClassPair(cls);

	size_t size = class_getInstanceSize(cls);
	CHECK(size >= sizeof(void*) + sizeof(uintptr_t));
	void *storage = calloc(1, size);
	CHECK(storage != NULL);
	CHECK(objc_constructInstance(Nil, storage) == nil);
	CHECK(objc_constructInstance(cls, NULL) == nil);

	id obj = objc_constructInstance(cls, storage);
	CHECK(obj == (id)storage);
	CHECK(object_getClass(obj) == cls);
	CHECK(construct_count == 1);
	objc_setAssociatedObject(obj, &association_key, obj, OBJC_ASSOCIATION_ASSIGN);
	CHECK(objc_getAssociatedObject(obj, &association_key) == obj);

	CHECK(objc_destructInstance(obj) == storage);
	CHECK(destruct_count == 1);
	CHECK(objc_getAssociatedObject(obj, &association_key) == nil);
	CHECK(object_getClass(obj) == cls);
	CHECK(objc_destructInstance(nil) == NULL);

	memset(storage, 0, size);
	obj = objc_constructInstance(cls, storage);
	CHECK(obj == (id)storage);
	CHECK(construct_count == 2);
	CHECK(objc_destructInstance(obj) == storage);
	CHECK(destruct_count == 2);

	free(storage);
	objc_disposeClassPair(cls);
	return 0;
}
