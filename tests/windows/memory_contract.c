/* SPDX-License-Identifier: AGPL-3.0-only */
#include <assert.h>
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
	assert(class_addMethod(cls, sel_registerName("retain"),
		(IMP)retain_object, "@@:"));
	assert(class_addMethod(cls, sel_registerName("release"),
		(IMP)release_object, "v@:"));
}

int main(void)
{
	static char key;
	mosaic_objc_runtime_initialize();

	Class cls = objc_allocateClassPair(Nil, "MosaicMemoryContract", 0);
	assert(cls != Nil);
	install_memory_methods(cls);
	objc_registerClassPair(cls);

	id holder = class_createInstance(cls, 0);
	id value = class_createInstance(cls, 0);
	assert(holder != nil && value != nil);

	assert(objc_retain(value) == value);
	objc_release(value);
	assert(retains == 1);
	assert(releases == 1);

	objc_setAssociatedObject(holder, &key, value, OBJC_ASSOCIATION_RETAIN);
	assert(objc_getAssociatedObject(holder, &key) == value);
	assert(retains == 2);
	objc_removeAssociatedObjects(holder);
	assert(objc_getAssociatedObject(holder, &key) == nil);
	assert(releases == 2);

	object_dispose(value);
	object_dispose(holder);
	return 0;
}
