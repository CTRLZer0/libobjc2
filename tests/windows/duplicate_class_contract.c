/* SPDX-License-Identifier: MIT */
#include "test_support.h"
#include <stdint.h>
#include "objc/runtime.h"
#include "objc/dispatch/message.h"
#include "objc/mosaic.h"

typedef uintptr_t (*word_send_fn)(id, SEL);

static uintptr_t original_method(id self, SEL _cmd)
{
	(void)self; (void)_cmd;
	return 0x1111u;
}

static uintptr_t duplicate_method(id self, SEL _cmd)
{
	(void)self; (void)_cmd;
	return 0x2222u;
}

int main(void)
{
	mosaic_objc_runtime_initialize();
	SEL selector = sel_registerName("duplicateValue");
	CHECK(selector != NULL);

	Class original = objc_allocateClassPair(Nil, "MosaicDuplicateOriginal", 0);
	CHECK(original != Nil);
	uint8_t alignment = sizeof(void*) == 8 ? 3 : 2;
	CHECK(class_addIvar(original, "payload", sizeof(void*), alignment, "@") == YES);
	CHECK(class_addMethod(original, selector, (IMP)(void*)original_method, "Q@:") == YES);
	objc_registerClassPair(original);

	// Initialize and install the original dtable before duplicating it.  The
	// duplicate must build an independent dtable on its own first message.
	word_send_fn send = (word_send_fn)(void*)objc_msgSend;
	id warmup = class_createInstance(original, 0);
	CHECK(warmup != nil && send(warmup, selector) == 0x1111u);
	object_dispose(warmup);

	CHECK(objc_duplicateClass(original, "MosaicDuplicateOriginal", 0) == Nil);
	CHECK(objc_duplicateClass(object_getClass((id)original), "MosaicDuplicateMeta", 0) == Nil);
	Class duplicate = objc_duplicateClass(original, "MosaicDuplicateCopy", 64);
	CHECK(duplicate != Nil);
	CHECK(objc_lookUpClass("MosaicDuplicateCopy") == duplicate);
	CHECK(object_getClass((id)duplicate) == object_getClass((id)original));
	CHECK(class_getSuperclass(duplicate) == class_getSuperclass(original));
	CHECK(class_getInstanceSize(duplicate) == class_getInstanceSize(original));

	Ivar originalIvar = class_getInstanceVariable(original, "payload");
	Ivar duplicateIvar = class_getInstanceVariable(duplicate, "payload");
	CHECK(originalIvar != NULL && duplicateIvar != NULL && originalIvar != duplicateIvar);
	CHECK(ivar_getOffset(originalIvar) == ivar_getOffset(duplicateIvar));
	Method originalMethod = class_getInstanceMethod(original, selector);
	Method duplicateMethod = class_getInstanceMethod(duplicate, selector);
	CHECK(originalMethod != NULL && duplicateMethod != NULL && originalMethod != duplicateMethod);

	id originalObject = class_createInstance(original, 0);
	id duplicateObject = class_createInstance(duplicate, 0);
	CHECK(originalObject != nil && duplicateObject != nil);
	CHECK(send(originalObject, selector) == 0x1111u);
	CHECK(send(duplicateObject, selector) == 0x1111u);
	CHECK(class_replaceMethod(duplicate, selector, (IMP)(void*)duplicate_method, "Q@:") == (IMP)(void*)original_method);
	CHECK(send(duplicateObject, selector) == 0x2222u);
	CHECK(send(originalObject, selector) == 0x1111u);
	object_dispose(duplicateObject);
	object_dispose(originalObject);

	objc_disposeClassPair(duplicate);
	CHECK(objc_lookUpClass("MosaicDuplicateCopy") == Nil);
	CHECK(objc_lookUpClass("MosaicDuplicateOriginal") == original);
	id survivor = class_createInstance(original, 0);
	CHECK(survivor != nil && send(survivor, selector) == 0x1111u);
	object_dispose(survivor);
	objc_disposeClassPair(original);
	return 0;
}
