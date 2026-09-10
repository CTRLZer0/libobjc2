/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
 * and provenance details.
 */
#include "test_support.h"
#include <stdint.h>
#include <string.h>
#include "objc/runtime.h"
#include "objc/mosaic.h"

int main(void)
{
	mosaic_objc_runtime_initialize();

	SEL first = sel_registerName("mosaicSelector");
	SEL second = sel_registerName("mosaicSelector");
	SEL other = sel_registerName("mosaicOtherSelector");

	CHECK(first != NULL);
	CHECK(first == second);
	CHECK(first != other);
	CHECK(sel_isEqual(first, second));
	CHECK(!sel_isEqual(first, other));
	CHECK(strcmp(sel_getName(first), "mosaicSelector") == 0);

	static const char *typedEncodings[] = {
		"v@:", "i@:", "I@:", "l@:", "L@:", "q@:", "Q@:",
		"s@:", "S@:", "c@:", "C@:", "B@:", "f@:", "d@:",
		"D@:", "@@:", "#@:", ":@:", "^v@:", "*@:"
	};
	enum { TypedCount = sizeof(typedEncodings) / sizeof(typedEncodings[0]) };
	for (unsigned i = 0; i < TypedCount; ++i)
	{
		SEL typed = sel_registerTypedName_np("mosaicTypedSelector", typedEncodings[i]);
		CHECK(typed != NULL);
	}

	struct
	{
		uintptr_t before;
		SEL values[16];
		uintptr_t after;
	} bounded = { (uintptr_t)0x11223344u, { 0 }, (uintptr_t)0x55667788u };
	unsigned total = sel_copyTypedSelectors_np("mosaicTypedSelector", bounded.values, 16);
	CHECK(total == TypedCount);
	CHECK(bounded.before == (uintptr_t)0x11223344u);
	CHECK(bounded.after == (uintptr_t)0x55667788u);
	for (unsigned i = 0; i < 16; ++i)
	{
		CHECK(bounded.values[i] != NULL);
		CHECK(strcmp(sel_getName(bounded.values[i]), "mosaicTypedSelector") == 0);
	}
	CHECK(sel_copyTypedSelectors_np("mosaicTypedSelector", NULL, 0) == TypedCount);
	SEL all[TypedCount] = { 0 };
	CHECK(sel_copyTypedSelectors_np("mosaicTypedSelector", all, TypedCount) == TypedCount);
	for (unsigned i = 0; i < TypedCount; ++i) { CHECK(all[i] != NULL); }
	return 0;
}
