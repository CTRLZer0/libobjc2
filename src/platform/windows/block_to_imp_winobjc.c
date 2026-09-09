/*
 * SPDX-License-Identifier: MIT AND AGPL-3.0-only
 * Original libobjc2 portions: MIT; CTRLZer0 modifications: AGPL-3.0-only.
 * Copyright (C) 2026 CTRLZer0 contributors for CTRLZer0 modifications.
 * Original upstream copyright and attribution remain under COPYING and the
 * preserved source / repository history. See LICENSE-CTRLZERO and NOTICE.md.
 */

#include "objc/runtime.h"
#include "visibility.h"

/* These are stub implementations of the block->imp->block API functions.
 * Until we can emit the block trampolines at build time (like iOS appears to),
 * we'll have to live without these functions.
 */

IMP imp_implementationWithBlock(void *block)
{
	return NULL;
}

void *imp_getBlock(IMP anImp)
{
	return NULL;
}

BOOL imp_removeBlock(IMP anImp)
{
	return NO;
}

char *block_copyIMPTypeEncoding_np(void*block)
{
	return NULL;
}

PRIVATE void init_trampolines(void)
{
}
