/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
 * and provenance details.
 */
#ifndef OBJC_MOSAIC_H_INCLUDED
#define OBJC_MOSAIC_H_INCLUDED

#include <objc/support/visibility.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the libobjc2 runtime core without loading a compiler-emitted
 * GNUstep Objective-C module. Mosaic uses its own Mach-O metadata loader.
 */
OBJC_PUBLIC void mosaic_objc_runtime_initialize(void);

#ifdef __cplusplus
}
#endif

#endif
