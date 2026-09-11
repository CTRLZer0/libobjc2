/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 */
#include "test_support.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "objc/runtime.h"
#include "objc/mosaic.h"
#include "class.h"
#include "category.h"

struct objc_protocol
{
    id isa;
    char *name;
    void *protocol_list;
    void *instance_methods;
    void *class_methods;
    void *optional_instance_methods;
    void *optional_class_methods;
    void *properties;
    void *optional_properties;
    void *class_properties;
    void *optional_class_properties;
};
struct objc_init
{
    uint64_t version;
    void *sel_begin, *sel_end;
    Class *cls_begin, *cls_end;
    Class *cls_ref_begin, *cls_ref_end;
    struct objc_category *cat_begin, *cat_end;
    struct objc_protocol *proto_begin, *proto_end;
    void *proto_ref_begin, *proto_ref_end;
    void *alias_begin, *alias_end;
    void *strings_begin, *strings_end;
};
extern void __objc_load(struct objc_init *init);

struct event_log
{
    struct mosaic_objc_runtime_event events[64];
    unsigned count;
};

static void capture_event(const struct mosaic_objc_runtime_event *event,
                          void *context)
{
    struct event_log *log = context;
    if (log->count < 64) { log->events[log->count++] = *event; }
}
static int find_event(const struct event_log *log,
                      enum mosaic_objc_runtime_event_kind kind,
                      const char *name)
{
    for (unsigned i = 0; i < log->count; i++)
    {
        const struct mosaic_objc_runtime_event *event = &log->events[i];
        if (event->kind != kind) { continue; }
        if ((name == NULL) ||
            ((event->name != NULL) && strcmp(event->name, name) == 0))
        {
            return (int)i;
        }
    }
    return -1;
}

static void init_static_pair(struct objc_class *cls,
                             struct objc_class *meta,
                             const char *name, Class superclass)
{
    memset(cls, 0, sizeof(*cls));
    memset(meta, 0, sizeof(*meta));
    meta->name = name;
    meta->info = objc_class_flag_meta;
    cls->isa = meta;
    cls->super_class = superclass;
    cls->name = name;
}
static id imp_a(id self, SEL _cmd)
{
    (void)_cmd;
    return self;
}

static id imp_b(id self, SEL _cmd)
{
    (void)_cmd;
    return self;
}

static id imp_c(id self, SEL _cmd)
{
    (void)_cmd;
    return self;
}

int main(void)
{
    mosaic_objc_runtime_initialize();
    struct event_log log = {0};

    Class base = objc_allocateClassPair(Nil, "MosaicObservabilityBase", 0);
    CHECK(base != Nil);
    objc_registerClassPair(base);
    mosaic_objc_runtimeSetEventSink(capture_event, &log);
    struct objc_class imageClass, imageMeta;
    init_static_pair(&imageClass, &imageMeta,
                     "MosaicObservabilityImageClass", base);
    Class classSlots[1] = { &imageClass };
    Class classRefs[1] = { &imageClass };

    struct objc_category category = {0};
    category.name = "ObservabilityCategory";
    category.class_name = "MosaicObservabilityImageClass";

    struct objc_protocol imageProtocol = {0};
    imageProtocol.name = "MosaicObservabilityImageProtocol";
    uintptr_t emptyRange = 0;
    struct objc_init imageInit = {0};
    imageInit.sel_begin = imageInit.sel_end = &emptyRange;
    imageInit.cls_begin = classSlots;
    imageInit.cls_end = classSlots + 1;
    imageInit.cls_ref_begin = classRefs;
    imageInit.cls_ref_end = classRefs + 1;
    imageInit.cat_begin = &category;
    imageInit.cat_end = &category + 1;
    imageInit.proto_begin = &imageProtocol;
    imageInit.proto_end = &imageProtocol + 1;
    imageInit.proto_ref_begin = imageInit.proto_ref_end = &emptyRange;
    imageInit.alias_begin = imageInit.alias_end = &emptyRange;
    imageInit.strings_begin = imageInit.strings_end = &emptyRange;
    __objc_load(&imageInit);
    CHECK(imageInit.version == ULONG_MAX);
    int protocolRegistered = find_event(&log,
        MOSAIC_OBJC_EVENT_PROTOCOL_REGISTERED,
        "MosaicObservabilityImageProtocol");
    int imageClassRegistered = find_event(&log,
        MOSAIC_OBJC_EVENT_CLASS_REGISTERED,
        "MosaicObservabilityImageClass");
    int categoryAttached = find_event(&log,
        MOSAIC_OBJC_EVENT_CATEGORY_ATTACHED,
        "ObservabilityCategory");
    int imageLoaded = find_event(&log, MOSAIC_OBJC_EVENT_IMAGE_LOADED, NULL);
    CHECK(protocolRegistered >= 0);
    CHECK(imageClassRegistered > protocolRegistered);
    CHECK(categoryAttached > imageClassRegistered);
    CHECK(imageLoaded > categoryAttached);
    CHECK(log.events[imageClassRegistered].image != NULL);
    CHECK(log.events[imageClassRegistered].image == log.events[imageLoaded].image);
    CHECK(log.events[imageClassRegistered].cls == &imageClass);
    CHECK(log.events[protocolRegistered].protocol == (Protocol *)&imageProtocol);
    CHECK(strcmp(log.events[imageClassRegistered].detail, "image") == 0);

    mosaic_objc_image_t image = mosaic_objc_imageForClass(&imageClass);
    CHECK(image != NULL && image == log.events[imageLoaded].image);
    Class dynamic = objc_allocateClassPair(base,
        "MosaicObservabilityDynamic", 0);
    CHECK(dynamic != Nil);
    objc_registerClassPair(dynamic);
    CHECK(find_event(&log, MOSAIC_OBJC_EVENT_CLASS_REGISTERED,
                     "MosaicObservabilityDynamic") >= 0);

    SEL first = sel_registerName("mosaicObserveFirst");
    SEL second = sel_registerName("mosaicObserveSecond");
    CHECK(class_addMethod(dynamic, first, (IMP)imp_a, "@@:") == YES);
    CHECK(class_addMethod(dynamic, second, (IMP)imp_b, "@@:") == YES);
    CHECK(find_event(&log, MOSAIC_OBJC_EVENT_METHOD_ADDED,
                     "mosaicObserveFirst") >= 0);

    IMP previous = class_replaceMethod(dynamic, first, (IMP)imp_b, "@@:");
    CHECK(previous == (IMP)imp_a);
    int replaced = find_event(&log, MOSAIC_OBJC_EVENT_METHOD_REPLACED,
                              "mosaicObserveFirst");
    CHECK(replaced >= 0);
    CHECK(log.events[replaced].old_implementation == (IMP)imp_a);
    CHECK(log.events[replaced].new_implementation == (IMP)imp_b);

    Method firstMethod = class_getInstanceMethod(dynamic, first);
    Method secondMethod = class_getInstanceMethod(dynamic, second);
    CHECK(firstMethod != NULL && secondMethod != NULL);
    previous = method_setImplementation(firstMethod, (IMP)imp_c);
    CHECK(previous == (IMP)imp_b);
    int changed = find_event(&log,
        MOSAIC_OBJC_EVENT_METHOD_IMPLEMENTATION_CHANGED,
        "mosaicObserveFirst");
    CHECK(changed >= 0);
    CHECK(log.events[changed].method == firstMethod);
    CHECK(log.events[changed].new_implementation == (IMP)imp_c);

    method_exchangeImplementations(firstMethod, secondMethod);
    int exchanged = find_event(&log,
        MOSAIC_OBJC_EVENT_METHOD_IMPLEMENTATIONS_EXCHANGED,
        "mosaicObserveFirst");
    CHECK(exchanged >= 0);
    CHECK(log.events[exchanged].method == firstMethod);
    CHECK(log.events[exchanged].other_method == secondMethod);

    Class duplicate = objc_duplicateClass(dynamic,
        "MosaicObservabilityDuplicate", 0);
    CHECK(duplicate != Nil);
    int duplicateEvent = find_event(&log, MOSAIC_OBJC_EVENT_CLASS_REGISTERED,
                                    "MosaicObservabilityDuplicate");
    CHECK(duplicateEvent >= 0);
    CHECK(strcmp(log.events[duplicateEvent].detail, "duplicate") == 0);
    Protocol *mergeA = objc_allocateProtocol("MosaicObservabilityMerge");
    Protocol *mergeB = objc_allocateProtocol("MosaicObservabilityMerge");
    Protocol *mergeConflict = objc_allocateProtocol("MosaicObservabilityMerge");
    CHECK(mergeA != NULL && mergeB != NULL && mergeConflict != NULL);
    SEL shared = sel_registerName("mosaicObservabilityShared");
    SEL extra = sel_registerName("mosaicObservabilityExtra");
    protocol_addMethodDescription(mergeA, shared, "v@:", YES, YES);
    protocol_addMethodDescription(mergeB, shared, "v@:", YES, YES);
    protocol_addMethodDescription(mergeB, extra, "i@:", YES, YES);
    protocol_addMethodDescription(mergeConflict, shared, "i@:", YES, YES);
    objc_registerProtocol(mergeA);
    objc_registerProtocol(mergeB);
    objc_registerProtocol(mergeConflict);

    CHECK(find_event(&log, MOSAIC_OBJC_EVENT_PROTOCOL_REGISTERED,
                     "MosaicObservabilityMerge") >= 0);
    CHECK(find_event(&log, MOSAIC_OBJC_EVENT_PROTOCOL_MERGED,
                     "MosaicObservabilityMerge") >= 0);
    int conflict = find_event(&log, MOSAIC_OBJC_EVENT_PROTOCOL_CONFLICT,
                              "mosaicObservabilityShared");
    CHECK(conflict >= 0);
    CHECK(log.events[conflict].protocol == mergeA);
    CHECK(strcmp(log.events[conflict].detail, "required instance method") == 0);
    CHECK(log.count > 0);
    for (unsigned i = 1; i < log.count; i++)
    {
        CHECK(log.events[i].sequence > log.events[i - 1].sequence);
    }

    unsigned beforeDisable = log.count;
    mosaic_objc_runtimeSetEventSink(NULL, NULL);
    SEL silent = sel_registerName("mosaicObservabilitySilent");
    CHECK(class_addMethod(dynamic, silent, (IMP)imp_a, "@@:") == YES);
    CHECK(log.count == beforeDisable);

    objc_disposeClassPair(duplicate);
    objc_disposeClassPair(dynamic);
    return 0;
}
