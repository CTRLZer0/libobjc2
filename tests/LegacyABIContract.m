/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../objc/runtime.h"

@protocol LegacyABIProtocol
@property(nonatomic, readonly) int protocolValue;
- (int)protocolPing;
@end

__attribute__((objc_root_class))
@interface LegacyABIRoot <LegacyABIProtocol>
{
	Class isa;
	int _value;
}
@property(nonatomic, assign) int value;
+ (id)new;
@end

@implementation LegacyABIRoot
@synthesize value = _value;
+ (id)new { return class_createInstance(self, 0); }
- (int)protocolValue { return _value; }
- (int)protocolPing { return 41; }
@end

@interface LegacyABIRoot (LegacyCategory)
- (int)categoryPing;
@end

@implementation LegacyABIRoot (LegacyCategory)
- (int)categoryPing { return 42; }
@end

int main(void)
{
	Class cls = objc_getClass("LegacyABIRoot");
	assert(cls != Nil);
	assert(class_getInstanceSize(cls) >= sizeof(int));

	unsigned int ivarCount = 0;
	Ivar *ivars = class_copyIvarList(cls, &ivarCount);
	assert(ivars != NULL && ivarCount >= 1);
	BOOL foundValue = NO;
	for (unsigned int i = 0; i < ivarCount; ++i)
	{
		const char *name = ivar_getName(ivars[i]);
		if ((name != NULL) && (strcmp(name, "_value") == 0)) { foundValue = YES; }
	}
	assert(foundValue);
	free(ivars);

	objc_property_t property = class_getProperty(cls, "value");
	assert(property != NULL);
	assert(property_getName(property) != NULL);

	Protocol *protocol = objc_getProtocol("LegacyABIProtocol");
	assert(protocol != NULL);
	assert(class_conformsToProtocol(cls, protocol));

	LegacyABIRoot *object = [LegacyABIRoot new];
	assert(object != nil);
	object.value = 7;
	assert(object.value == 7);
	assert([object protocolValue] == 7);
	assert([object protocolPing] == 41);
	assert([object categoryPing] == 42);
	object_dispose(object);
	return 0;
}
