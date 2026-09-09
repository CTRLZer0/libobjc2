/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * Original CTRLZer0 work; see LICENSE-CTRLZERO and NOTICE.md for licensing
 * and provenance details.
 */
#ifndef LIBOBJC2_BLOCK_CLASSES_H
#define LIBOBJC2_BLOCK_CLASSES_H

#include "objc/runtime.h"

extern struct objc_class _NSConcreteGlobalBlock;
extern struct objc_class _NSConcreteStackBlock;
extern struct objc_class _NSConcreteMallocBlock;

static inline Class objc_global_block_class(void)
{
	return (Class)(void *)&_NSConcreteGlobalBlock;
}

static inline Class objc_stack_block_class(void)
{
	return (Class)(void *)&_NSConcreteStackBlock;
}

static inline Class objc_malloc_block_class(void)
{
	return (Class)(void *)&_NSConcreteMallocBlock;
}

#endif // LIBOBJC2_BLOCK_CLASSES_H
