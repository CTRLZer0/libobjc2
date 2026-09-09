/* SPDX-License-Identifier: AGPL-3.0-only */
#include <assert.h>
#include <string.h>
#include "objc/runtime.h"
#include "objc/mosaic.h"

int main(void)
{
	mosaic_objc_runtime_initialize();

	SEL first = sel_registerName("mosaicSelector");
	SEL second = sel_registerName("mosaicSelector");
	SEL other = sel_registerName("mosaicOtherSelector");

	assert(first != NULL);
	assert(first == second);
	assert(first != other);
	assert(sel_isEqual(first, second));
	assert(!sel_isEqual(first, other));
	assert(strcmp(sel_getName(first), "mosaicSelector") == 0);
	return 0;
}
