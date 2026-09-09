/* SPDX-License-Identifier: AGPL-3.0-only */
#include <assert.h>
#include <string.h>
#include "objc/runtime.h"
#include "objc/mosaic.h"

int main(void)
{
	mosaic_objc_runtime_initialize();

	Class cls = objc_allocateClassPair(Nil, "MosaicClassContract", 0);
	assert(cls != Nil);
	assert(class_isMetaClass(cls) == NO);
	assert(strcmp(class_getName(cls), "MosaicClassContract") == 0);

	Class meta = object_getClass((id)cls);
	assert(meta != Nil);
	assert(class_isMetaClass(meta) == YES);

	objc_registerClassPair(cls);
	assert((Class)objc_getClass("MosaicClassContract") == cls);
	assert((Class)objc_getMetaClass("MosaicClassContract") == meta);

	Class replacement = objc_allocateClassPair(Nil, "MosaicClassContractReplacement", 0);
	assert(replacement != Nil);
	objc_registerClassPair(replacement);

	id object = class_createInstance(cls, 0);
	assert(object != nil);
	assert(object_getClass(object) == cls);
	assert(object_setClass(object, replacement) == cls);
	assert(object_getClass(object) == replacement);
	assert(object_setClass(object, cls) == replacement);
	assert(object_getClass(object) == cls);
	object_dispose(object);
	return 0;
}
