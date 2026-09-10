/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
 * and provenance details.
 */
#include "test_support.h"
#include <stdlib.h>
#include <string.h>
#include "objc/runtime.h"
#include "objc/mosaic.h"

static id copied_method(id self, SEL _cmd, ...)
{
	(void)_cmd;
	return self;
}

static int method_list_contains(Method *list, unsigned int count, SEL selector)
{
	for (unsigned int i = 0; i < count; i++)
	{
		if (sel_isEqual(method_getName(list[i]), selector)) { return 1; }
	}
	return 0;
}

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

	uint8_t pointerAlignment = sizeof(void*) == 8 ? 3 : 2;
	CHECK(class_addIvar(cls, "copiedIvar", sizeof(void*), pointerAlignment, "@") == YES);
	SEL copiedSelector = sel_registerName("mosaicCopiedMethod");
	CHECK(copiedSelector != NULL);
	CHECK(class_addMethod(cls, copiedSelector, copied_method, "@@:") == YES);
	Protocol *classProtocol = objc_allocateProtocol("MosaicClassCopyProtocol");
	CHECK(classProtocol != NULL);
	objc_registerProtocol(classProtocol);
	CHECK(class_addProtocol(cls, classProtocol) == YES);

	objc_registerClassPair(cls);
	CHECK((Class)objc_getClass("MosaicClassContract") == cls);
	CHECK((Class)objc_getMetaClass("MosaicClassContract") == meta);

	unsigned int copiedCount = 0;
	Ivar *copiedIvars = class_copyIvarList(cls, &copiedCount);
	CHECK(copiedIvars != NULL && copiedCount == 1);
	CHECK(copiedIvars[0] == class_getInstanceVariable(cls, "copiedIvar"));
	CHECK(copiedIvars[copiedCount] == NULL);
	free(copiedIvars);

	Method *copiedMethods = class_copyMethodList(cls, &copiedCount);
	CHECK(copiedMethods != NULL && copiedCount >= 1);
	CHECK(method_list_contains(copiedMethods, copiedCount, copiedSelector));
	CHECK(copiedMethods[copiedCount] == NULL);
	free(copiedMethods);

	Protocol **copiedProtocols = class_copyProtocolList(cls, &copiedCount);
	CHECK(copiedProtocols != NULL && copiedCount == 1);
	CHECK(copiedProtocols[0] == classProtocol);
	CHECK(copiedProtocols[copiedCount] == NULL);
	free(copiedProtocols);
	CHECK(class_getProperty(cls, NULL) == NULL);

	objc_property_attribute_t propertyAttributes[] = {{"T", "i"}, {"N", ""}};
	CHECK(class_addProperty(cls, "number", propertyAttributes, 2) == YES);
	objc_property_t property = class_getProperty(cls, "number");
	CHECK(property != NULL);
	unsigned int attributeCount = 0;
	objc_property_attribute_t *copiedAttributes =
		property_copyAttributeList(property, &attributeCount);
	CHECK(copiedAttributes != NULL && attributeCount == 2);
	free(copiedAttributes);
	char *type = property_copyAttributeValue(property, "T");
	CHECK(type != NULL && strcmp(type, "i") == 0);
	free(type);
	CHECK(property_copyAttributeValue(property, "V") == NULL);
	CHECK(class_addProperty(cls, "invalid", NULL, 1) == NO);

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
