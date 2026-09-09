/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * See COPYING and NOTICE.md for licensing and provenance details.
 */
#include "test_support.h"
#include "objc/runtime.h"
#include "objc/mosaic.h"
#include "ivar.h"
#include <limits.h>
#include <stdint.h>

int main(void)
{
	mosaic_objc_runtime_initialize();

	Class cls = objc_allocateClassPair(Nil, "MosaicIvarContract", 0);
	CHECK(cls != Nil);
	CHECK(class_addIvar(cls, "byte", 1, 0, "c"));
	CHECK(class_addIvar(cls, "payload", sizeof(id), 3, "@"));
	CHECK(class_addIvar(cls, "widePayload", sizeof(id), 4, "@"));
#if SIZE_MAX > UINT32_MAX
	CHECK(!class_addIvar(cls, "oversized", (size_t)UINT32_MAX + 1, 0, "?"));
#endif
	CHECK(!class_addIvar(cls, "badAlignment", 1,
	                    (uint8_t)(sizeof(size_t) * CHAR_BIT), "c"));

	Ivar payload = class_getInstanceVariable(cls, "payload");
	Ivar wide = class_getInstanceVariable(cls, "widePayload");
	CHECK(payload != NULL && wide != NULL);
	CHECK(ivarGetAlign(payload) == 8);
	CHECK(ivarGetAlign(wide) == 16);

	objc_registerClassPair(cls);
	payload = class_getInstanceVariable(cls, "payload");
	wide = class_getInstanceVariable(cls, "widePayload");
	CHECK(payload != NULL && wide != NULL);
	CHECK((ivar_getOffset(wide) % 16) == 0);
	CHECK(class_getInstanceSize(cls) >= sizeof(id));

	id object = class_createInstance(cls, 0);
	id value = class_createInstance(cls, 0);
	CHECK(object != nil && value != nil);
	object_setIvar(object, payload, value);
	CHECK(object_getIvar(object, payload) == value);

	object_dispose(value);
	object_dispose(object);
	return 0;
}
