/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 */
#include "test_support.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "objc/runtime.h"
#include "objc/runtime/dispatch.h"
#include "objc/mosaic.h"

static unsigned construct_count;
static unsigned destruct_count;
static unsigned replacement_construct_count;
static unsigned replacement_destruct_count;
static unsigned late_construct_count;
static unsigned preregister_construct_count;
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

static void cxx_construct_replacement(id self, SEL _cmd)
{
	(void)_cmd;
	CHECK(self != nil);
	replacement_construct_count++;
}

static void cxx_destruct_replacement(id self, SEL _cmd)
{
	(void)_cmd;
	CHECK(self != nil);
	replacement_destruct_count++;
}

static void late_cxx_construct(id self, SEL _cmd)
{
	(void)_cmd;
	CHECK(self != nil);
	late_construct_count++;
}

static void preregister_cxx_construct(id self, SEL _cmd)
{
	(void)_cmd;
	CHECK(self != nil);
}

static void preregister_cxx_construct_replacement(id self, SEL _cmd)
{
	(void)_cmd;
	CHECK(self != nil);
	preregister_construct_count++;
}

static id late_probe(id self, SEL _cmd)
{
	(void)_cmd;
	return self;
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
	SEL probeSelector = sel_registerName("mosaicProbe");
	objc_registerClassPair(cls);

	Class subclass = objc_allocateClassPair(cls, "MosaicConstructDestructSubclass", 0);
	CHECK(subclass != Nil);
	objc_registerClassPair(subclass);
	SEL constructSelector = sel_registerName(".cxx_construct");
	SEL destructSelector = sel_registerName(".cxx_destruct");

	Class preregistered = objc_allocateClassPair(Nil, "MosaicPreregisterCxxContract", 0);
	CHECK(preregistered != Nil);
	CHECK(class_addMethod(preregistered, constructSelector,
	                      (IMP)(void*)preregister_cxx_construct, "v@:") == YES);
	Method preregisterMethod = class_getInstanceMethod(preregistered, constructSelector);
	CHECK(preregisterMethod != NULL);
	CHECK(method_setImplementation(preregisterMethod,
	                               (IMP)(void*)preregister_cxx_construct_replacement) ==
	      (IMP)(void*)preregister_cxx_construct);
	objc_registerClassPair(preregistered);
	size_t preregisterSize = class_getInstanceSize(preregistered);
	void *preregisterStorage = calloc(1, preregisterSize);
	CHECK(preregisterStorage != NULL);
	id preregisterObject = objc_constructInstance(preregistered, preregisterStorage);
	CHECK(preregisterObject == (id)preregisterStorage);
	CHECK(preregister_construct_count == 1);
	CHECK(objc_destructInstance(preregisterObject) == preregisterStorage);
	free(preregisterStorage);
	objc_disposeClassPair(preregistered);
	Method constructMethod = class_getInstanceMethod(cls, constructSelector);
	Method destructMethod = class_getInstanceMethod(cls, destructSelector);
	CHECK(constructMethod != NULL);
	CHECK(destructMethod != NULL);
	CHECK(method_setImplementation(constructMethod, (IMP)(void*)cxx_construct_replacement) ==
	      (IMP)(void*)cxx_construct);
	CHECK(method_setImplementation(destructMethod, (IMP)(void*)cxx_destruct_replacement) ==
	      (IMP)(void*)cxx_destruct);

	size_t subclassSize = class_getInstanceSize(subclass);
	void *subclassStorage = calloc(1, subclassSize);
	CHECK(subclassStorage != NULL);
	id subclassObject = objc_constructInstance(subclass, subclassStorage);
	CHECK(subclassObject == (id)subclassStorage);
	CHECK(replacement_construct_count == 1);
	CHECK(construct_count == 0);
	CHECK(objc_destructInstance(subclassObject) == subclassStorage);
	CHECK(replacement_destruct_count == 1);
	CHECK(destruct_count == 0);

	method_exchangeImplementations(constructMethod, destructMethod);
	memset(subclassStorage, 0, subclassSize);
	subclassObject = objc_constructInstance(subclass, subclassStorage);
	CHECK(subclassObject == (id)subclassStorage);
	CHECK(replacement_destruct_count == 2);
	CHECK(objc_destructInstance(subclassObject) == subclassStorage);
	CHECK(replacement_construct_count == 2);
	method_exchangeImplementations(constructMethod, destructMethod);
	CHECK(method_setImplementation(constructMethod, (IMP)(void*)cxx_construct) ==
	      (IMP)(void*)cxx_construct_replacement);
	CHECK(method_setImplementation(destructMethod, (IMP)(void*)cxx_destruct) ==
	      (IMP)(void*)cxx_destruct_replacement);
	free(subclassStorage);

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
	objc_disposeClassPair(subclass);
	objc_disposeClassPair(cls);

	Class lateBase = objc_allocateClassPair(Nil, "MosaicLateCxxBase", 0);
	CHECK(lateBase != Nil);
	CHECK(class_addMethod(lateBase, probeSelector, (IMP)(void*)late_probe, "@@:") == YES);
	objc_registerClassPair(lateBase);
	Class lateSubclass = objc_allocateClassPair(lateBase, "MosaicLateCxxSubclass", 0);
	CHECK(lateSubclass != Nil);
	objc_registerClassPair(lateSubclass);

	/* Force an installed instance dtable before adding the C++ constructor. */
	size_t probeSize = class_getInstanceSize(lateSubclass);
	void *probeStorage = calloc(1, probeSize);
	CHECK(probeStorage != NULL);
	id probeObject = objc_constructInstance(lateSubclass, probeStorage);
	CHECK(probeObject == (id)probeStorage);
	IMP probeImp = objc_msg_lookup(probeObject, probeSelector);
	CHECK(probeImp == (IMP)(void*)late_probe);
	typedef id (*probe_fn_t)(id, SEL);
	CHECK(((probe_fn_t)(void*)probeImp)(probeObject, probeSelector) == probeObject);
	CHECK(objc_destructInstance(probeObject) == probeStorage);
	free(probeStorage);

	CHECK(class_addMethod(lateBase, constructSelector, (IMP)(void*)late_cxx_construct, "v@:") == YES);

	size_t lateSize = class_getInstanceSize(lateSubclass);
	void *lateStorage = calloc(1, lateSize);
	CHECK(lateStorage != NULL);
	id lateObject = objc_constructInstance(lateSubclass, lateStorage);
	CHECK(lateObject == (id)lateStorage);
	CHECK(late_construct_count == 1);
	CHECK(objc_destructInstance(lateObject) == lateStorage);
	free(lateStorage);
	objc_disposeClassPair(lateSubclass);
	objc_disposeClassPair(lateBase);
	return 0;
}
