/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * Original CTRLZer0 work; see LICENSE-CTRLZERO and NOTICE.md for licensing
 * and provenance details.
 */
#ifndef OBJC_MOSAIC_H_INCLUDED
#define OBJC_MOSAIC_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the libobjc2 runtime core without loading a compiler-emitted
 * GNUstep Objective-C module. Mosaic uses its own Mach-O metadata loader.
 */
void mosaic_objc_runtime_initialize(void);

#ifdef __cplusplus
}
#endif

#endif
