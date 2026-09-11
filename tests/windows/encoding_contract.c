/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 */
#include "test_support.h"
#include <stddef.h>
#include "objc/encoding.h"

int main(void)
{
	CHECK(objc_sizeof_type("v") == 0);
	CHECK(objc_alignof_type("v") == 0);
	CHECK(objc_aligned_size("v") == 0);
	CHECK(objc_aligned_size("?") == 0);

	CHECK(objc_sizeof_type("{MosaicPair=ci}") == 8);
	CHECK(objc_alignof_type("{MosaicPair=ci}") == 4);
	CHECK(objc_aligned_size("{MosaicPair=ci}") == 8);
	CHECK(objc_sizeof_type("[3i]") == 12);
	CHECK(objc_alignof_type("[3i]") == 4);
	return 0;
}
