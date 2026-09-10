#include "Test.h"

static void checkWeakInitialization(id object)
{
	id __weak initialized = object;
	assert(initialized == object);
}

int main(int argc, const char * argv[])
{
	id __weak ref;
	@autoreleasepool {
		__block int val;
		id block = ^() { return val++; };
		assert(block != nil);
		checkWeakInitialization(block);
		ref = block;
		assert(ref != nil);
	}
	assert(ref == nil);
	return 0;
}
