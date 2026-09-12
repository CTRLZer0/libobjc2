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
    MOSAIC_OBJC_EVENT_METHOD_IMPLEMENTATIONS_EXCHANGED,
    MOSAIC_OBJC_EVENT_CLASS_RELOADED,
    MOSAIC_OBJC_EVENT_CLASS_RELOAD_REJECTED,
    MOSAIC_OBJC_EVENT_IMAGE_RETIRED,
    MOSAIC_OBJC_EVENT_IMAGE_DETACHED
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

enum mosaic_objc_image_state
{
    MOSAIC_OBJC_IMAGE_ACTIVE = 1,
    MOSAIC_OBJC_IMAGE_RETIRED,
    MOSAIC_OBJC_IMAGE_DETACHED
};

enum mosaic_objc_image_unload_blocker
{
    MOSAIC_OBJC_IMAGE_BLOCKER_NOT_RETIRED = 1u << 0,
    MOSAIC_OBJC_IMAGE_BLOCKER_ADDRESS_RANGE_UNKNOWN = 1u << 1,
    MOSAIC_OBJC_IMAGE_BLOCKER_CLASS_METADATA = 1u << 2,
    MOSAIC_OBJC_IMAGE_BLOCKER_PROTOCOL_METADATA = 1u << 3,
    MOSAIC_OBJC_IMAGE_BLOCKER_CATEGORY_METADATA = 1u << 4,
    MOSAIC_OBJC_IMAGE_BLOCKER_EXECUTABLE_CODE = 1u << 5,
    MOSAIC_OBJC_IMAGE_BLOCKER_ANALYSIS_INCOMPLETE = 1u << 6,
    MOSAIC_OBJC_IMAGE_BLOCKER_LOADER_METADATA = 1u << 7,
    MOSAIC_OBJC_IMAGE_BLOCKER_GLOBAL_HOOK_CODE = 1u << 8,
    MOSAIC_OBJC_IMAGE_BLOCKER_TRACING_HOOK_CODE = 1u << 9,
    MOSAIC_OBJC_IMAGE_BLOCKER_NOT_DETACHED = 1u << 10,
    MOSAIC_OBJC_IMAGE_BLOCKER_HOST_NOT_QUIESCENT = 1u << 11,
    MOSAIC_OBJC_IMAGE_BLOCKER_STALE_EPOCH = 1u << 12
};

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

struct mosaic_objc_image_unload_report
{
    enum mosaic_objc_image_state state;
    uint32_t blockers;
    /** Consistency token for imageDetach; not an authorization to unmap code. */
    uint64_t mutation_epoch;
    size_t mapped_size;
    size_t class_metadata_count;
    size_t protocol_metadata_count;
    size_t category_metadata_count;
    size_t loader_metadata_count;
    size_t executable_reference_count;
    size_t global_hook_reference_count;
    size_t tracing_hook_reference_count;
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
/** Sets the immutable mapped address range used by unload analysis. */
OBJC_PUBLIC BOOL mosaic_objc_imageSetAddressRange(mosaic_objc_image_t image,
                                                   const void *baseAddress,
                                                   size_t mappedSize);
/** Marks an image retired. Retirement is monotonic and does not unmap memory. */
OBJC_PUBLIC BOOL mosaic_objc_imageRetire(mosaic_objc_image_t image);
/**
 * Detaches a blocker-free retired image using a fresh mutation-epoch token.
 * Detach drops registry references to loader ranges; it does not unmap code.
 */
OBJC_PUBLIC BOOL mosaic_objc_imageDetach(mosaic_objc_image_t image,
                                          uint64_t expectedMutationEpoch);
/** Produces a conservative snapshot of known unload blockers. */
OBJC_PUBLIC BOOL mosaic_objc_imageGetUnloadReport(
    mosaic_objc_image_t image, struct mosaic_objc_image_unload_report *outReport);
/** True only when a retired image has no currently known runtime blockers. */
OBJC_PUBLIC BOOL mosaic_objc_imageIsUnloadCandidate(
    mosaic_objc_image_t image, struct mosaic_objc_image_unload_report *outReport);
/**
 * Point-in-time physical-unmap predicate. Requires DETACHED state, a fresh
 * epoch, zero runtime code references, and host quiescence. The host must
 * serialize publication of new executable references through the actual unmap.
 */
OBJC_PUBLIC BOOL mosaic_objc_imageIsPhysicalUnloadReady(
    mosaic_objc_image_t image, uint64_t expectedMutationEpoch, BOOL hostQuiescent,
    struct mosaic_objc_image_unload_report *outReport);
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
