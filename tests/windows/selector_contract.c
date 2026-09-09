/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * Original CTRLZer0 work; see LICENSE-CTRLZERO and NOTICE.md for licensing
 * and provenance details.
 */
#include "test_support.h"
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
	return 0;
}
