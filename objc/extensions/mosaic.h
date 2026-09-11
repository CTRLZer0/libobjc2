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

/** Initializes the runtime core without loading a compiler-emitted module. */
OBJC_PUBLIC void mosaic_objc_runtime_initialize(void);
/** Number of incompatible duplicate protocol members observed during merges. */
OBJC_PUBLIC uint64_t mosaic_objc_runtimeGetProtocolConflictCount(void);

/** Opaque handle for a compiler-emitted Objective-C image. */
typedef struct mosaic_objc_image_record *mosaic_objc_image_t;

enum mosaic_objc_runtime_event_kind
{
    MOSAIC_OBJC_EVENT_IMAGE_LOADED = 1,
    MOSAIC_OBJC_EVENT_CLASS_REGISTERED,
    MOSAIC_OBJC_EVENT_CATEGORY_ATTACHED,
    MOSAIC_OBJC_EVENT_PROTOCOL_REGISTERED,
    MOSAIC_OBJC_EVENT_PROTOCOL_MERGED,
    MOSAIC_OBJC_EVENT_PROTOCOL_CONFLICT,
    MOSAIC_OBJC_EVENT_METHOD_ADDED,
    MOSAIC_OBJC_EVENT_METHOD_REPLACED,
    MOSAIC_OBJC_EVENT_METHOD_IMPLEMENTATION_CHANGED,
    MOSAIC_OBJC_EVENT_METHOD_IMPLEMENTATIONS_EXCHANGED
};

struct mosaic_objc_runtime_event
{
    uint64_t sequence;
    enum mosaic_objc_runtime_event_kind kind;
    mosaic_objc_image_t image;
    Class cls;
    Protocol *protocol;
    Method method;
    Method other_method;
    SEL selector;
    IMP old_implementation;
    IMP new_implementation;
    const char *name;
    const char *detail;
};

typedef void (*mosaic_objc_runtime_event_sink_t)(
    const struct mosaic_objc_runtime_event *event, void *context);

/**
 * Installs a synchronous structural-event sink. The callback may run while
 * internal runtime locks are held and must therefore only copy or enqueue the
 * event; it must not call Objective-C runtime mutation APIs. Passing NULL
 * disables delivery. Reconfiguration may drop an in-flight event.
 */
OBJC_PUBLIC void mosaic_objc_runtimeSetEventSink(
    mosaic_objc_runtime_event_sink_t sink, void *context);

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
/** Attaches immutable diagnostic identity to an image. */
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
