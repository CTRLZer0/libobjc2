/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
 * and provenance details.
 */
#ifndef OBJC_MOSAIC_H_INCLUDED
#define OBJC_MOSAIC_H_INCLUDED

#include <objc/support/visibility.h>
#include <objc/runtime/types.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the libobjc2 runtime core without loading a compiler-emitted
 * GNUstep Objective-C module. Mosaic uses its own Mach-O metadata loader.
 */
OBJC_PUBLIC void mosaic_objc_runtime_initialize(void);

/** Opaque handle for a compiler-emitted Objective-C image known to the runtime. */
typedef struct mosaic_objc_image_record *mosaic_objc_image_t;

/** Stable diagnostic metadata for a registered Objective-C image. */
struct mosaic_objc_image_info
{
    uint64_t generation;
    const char *identifier;
    const char *provider;
    const void *base_address;
    size_t class_count;
    size_t class_reference_count;
    size_t category_count;
    size_t protocol_count;
};

/** Returns a malloc-owned image list in load order. */
OBJC_PUBLIC mosaic_objc_image_t *mosaic_objc_copyImageList(size_t *outCount);
/** Copies stable metadata for an image handle. */
OBJC_PUBLIC BOOL mosaic_objc_imageGetInfo(mosaic_objc_image_t image,
                                           struct mosaic_objc_image_info *outInfo);
/**
 * Attaches immutable diagnostic identity to an image.  Non-NULL string fields
 * are copied.  Repeating an already-set field with a different value fails.
 */
OBJC_PUBLIC BOOL mosaic_objc_imageSetIdentity(mosaic_objc_image_t image,
                                               const char *identifier,
                                               const char *provider,
                                               const void *baseAddress);
OBJC_PUBLIC mosaic_objc_image_t mosaic_objc_imageForClass(Class cls);
OBJC_PUBLIC mosaic_objc_image_t mosaic_objc_imageForProtocol(Protocol *protocol);
OBJC_PUBLIC Class mosaic_objc_imageGetClass(mosaic_objc_image_t image, size_t index);
OBJC_PUBLIC Protocol *mosaic_objc_imageGetProtocol(mosaic_objc_image_t image, size_t index);
OBJC_PUBLIC const char *mosaic_objc_imageGetCategoryName(mosaic_objc_image_t image, size_t index);
OBJC_PUBLIC const char *mosaic_objc_imageGetCategoryClassName(mosaic_objc_image_t image, size_t index);

#ifdef __cplusplus
}
#endif

#endif
