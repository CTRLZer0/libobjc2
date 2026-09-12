/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 */
#include "test_support.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "objc/runtime.h"
#include "objc/mosaic.h"
#include "class.h"

struct objc_init
{
    uint64_t version;
    void *sel_begin, *sel_end;
    Class *cls_begin, *cls_end;
    Class *cls_ref_begin, *cls_ref_end;
    void *cat_begin, *cat_end;
    void *proto_begin, *proto_end;
    void *proto_ref_begin, *proto_ref_end;
    void *alias_begin, *alias_end;
    void *strings_begin, *strings_end;
};
extern void __objc_load(struct objc_init *init);
static uintptr_t empty_range;
static enum mosaic_objc_runtime_event_kind last_event;
static size_t event_count;

static void event_sink(const struct mosaic_objc_runtime_event *event, void *context)
{
    (void)context;
    if ((event->kind == MOSAIC_OBJC_EVENT_IMAGE_RETIRED) ||
        (event->kind == MOSAIC_OBJC_EVENT_IMAGE_DETACHED))
    {
        last_event = event->kind;
        event_count++;
    }
}

static uintptr_t mapped_method(id self, SEL _cmd)
{
    (void)self; (void)_cmd;
    return 0x1111u;
}

static uintptr_t replacement_method(id self, SEL _cmd)
{
    (void)self; (void)_cmd;
    return 0x2222u;
}

static mosaic_objc_image_t load_empty_image(struct objc_init *init)
{
    size_t before_count = 0;
    mosaic_objc_image_t *before = mosaic_objc_copyImageList(&before_count);
    free(before);
    memset(init, 0, sizeof(*init));
    init->sel_begin = init->sel_end = &empty_range;
    init->cls_begin = init->cls_end = (Class *)&empty_range;
    init->cls_ref_begin = init->cls_ref_end = (Class *)&empty_range;
    init->cat_begin = init->cat_end = &empty_range;
    init->proto_begin = init->proto_end = &empty_range;
    init->proto_ref_begin = init->proto_ref_end = &empty_range;
    init->alias_begin = init->alias_end = &empty_range;
    init->strings_begin = init->strings_end = &empty_range;
    __objc_load(init);
    CHECK(init->version == ULONG_MAX);

    size_t after_count = 0;
    mosaic_objc_image_t *after = mosaic_objc_copyImageList(&after_count);
    CHECK(after != NULL && after_count == before_count + 1);
    mosaic_objc_image_t image = after[after_count - 1];
    free(after);
    return image;
}

static void init_static_pair(struct objc_class *cls, struct objc_class *meta,
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
int main(void)
{
    mosaic_objc_runtime_initialize();
    mosaic_objc_runtimeSetEventSink(event_sink, NULL);

    struct objc_init empty_init;
    mosaic_objc_image_t image = load_empty_image(&empty_init);
    CHECK(image != NULL);
    memset(&empty_init, 0, sizeof(empty_init));

    SEL selector = sel_registerTypedName_np("lifecycleMappedMethod", "Q@:");
    CHECK(selector != NULL);
    Class holder = objc_allocateClassPair(Nil, "MosaicLifecycleHolder", 0);
    CHECK(holder != Nil);
    CHECK(class_addMethod(holder, selector, (IMP)(void *)mapped_method, "Q@:") == YES);
    objc_registerClassPair(holder);

    uintptr_t mapped_address = (uintptr_t)(void *)mapped_method;
    const void *mapped_base = (const void *)mapped_address;
    CHECK(mosaic_objc_imageSetAddressRange(image, mapped_base, 1));
    CHECK(mosaic_objc_imageSetAddressRange(image, mapped_base, 1));
    CHECK(!mosaic_objc_imageSetAddressRange(image, mapped_base, 2));
    CHECK(!mosaic_objc_imageSetAddressRange(
        image, (const void *)(mapped_address + 1), 1));

    struct mosaic_objc_image_unload_report report = {0};
    CHECK(mosaic_objc_imageGetUnloadReport(image, &report));
    CHECK(report.state == MOSAIC_OBJC_IMAGE_ACTIVE);
    CHECK((report.blockers & MOSAIC_OBJC_IMAGE_BLOCKER_NOT_RETIRED) != 0);
    CHECK((report.blockers & MOSAIC_OBJC_IMAGE_BLOCKER_EXECUTABLE_CODE) != 0);
    CHECK(report.loader_metadata_count == 0);
    CHECK(report.executable_reference_count >= 1);
    CHECK(!mosaic_objc_imageIsUnloadCandidate(image, NULL));
    CHECK(mosaic_objc_imageRetire(image));
    CHECK(mosaic_objc_imageRetire(image));
    CHECK(event_count == 1);
    CHECK(last_event == MOSAIC_OBJC_EVENT_IMAGE_RETIRED);

    Method mapped = class_getInstanceMethod(holder, selector);
    CHECK(mapped != NULL);
    CHECK(class_replaceMethod(holder, selector,
                              (IMP)(void *)replacement_method,
                              "Q@:") == (IMP)(void *)mapped_method);
    CHECK(method_getImplementation(mapped) == (IMP)(void *)replacement_method);

    memset(&report, 0, sizeof(report));
    CHECK(mosaic_objc_imageGetUnloadReport(image, &report));
    CHECK(report.state == MOSAIC_OBJC_IMAGE_RETIRED);
    CHECK(report.executable_reference_count == 0);
    CHECK(report.blockers == 0);
    CHECK(mosaic_objc_imageIsUnloadCandidate(image, &report));
    CHECK(report.blockers == 0);
    CHECK(report.mutation_epoch != 0);
    uint64_t stale_epoch = report.mutation_epoch;

    mosaic_objc_runtimeSetEventSink(NULL, NULL);
    SEL epoch_selector = sel_registerTypedName_np("lifecycleEpochMutation", "Q@:");
    CHECK(epoch_selector != NULL);
    CHECK(class_addMethod(holder, epoch_selector,
                          (IMP)(void *)replacement_method, "Q@:") == YES);
    CHECK(!mosaic_objc_imageDetach(image, stale_epoch));

    memset(&report, 0, sizeof(report));
    CHECK(mosaic_objc_imageGetUnloadReport(image, &report));
    CHECK(report.blockers == 0);
    CHECK(report.mutation_epoch > stale_epoch);
    uint64_t fresh_epoch = report.mutation_epoch;

    mosaic_objc_runtimeSetEventSink(event_sink, NULL);
    CHECK(mosaic_objc_imageDetach(image, fresh_epoch));
    CHECK(event_count == 2);
    CHECK(last_event == MOSAIC_OBJC_EVENT_IMAGE_DETACHED);
    memset(&report, 0, sizeof(report));
    CHECK(mosaic_objc_imageGetUnloadReport(image, &report));
    CHECK(report.state == MOSAIC_OBJC_IMAGE_DETACHED);
    CHECK(report.blockers == 0);
    CHECK(report.mutation_epoch > fresh_epoch);
    CHECK(mosaic_objc_imageDetach(image, report.mutation_epoch));
    CHECK(mosaic_objc_imageRetire(image));

    Class base = objc_allocateClassPair(Nil, "MosaicLifecycleBase", 0);
    CHECK(base != Nil);
    objc_registerClassPair(base);
    struct objc_class image_class, image_meta;
    init_static_pair(&image_class, &image_meta,
                     "MosaicLifecycleImageClass", base);

    size_t before_count = 0;
    mosaic_objc_image_t *before = mosaic_objc_copyImageList(&before_count);
    free(before);
    Class class_slots[1] = { &image_class };
    Class class_refs[1] = { &image_class };
    struct objc_init class_init = {0};
    class_init.sel_begin = class_init.sel_end = &empty_range;
    class_init.cls_begin = class_slots;
    class_init.cls_end = class_slots + 1;
    class_init.cls_ref_begin = class_refs;
    class_init.cls_ref_end = class_refs + 1;
    class_init.cat_begin = class_init.cat_end = &empty_range;
    class_init.proto_begin = class_init.proto_end = &empty_range;
    class_init.proto_ref_begin = class_init.proto_ref_end = &empty_range;
    class_init.alias_begin = class_init.alias_end = &empty_range;
    class_init.strings_begin = class_init.strings_end = &empty_range;
    __objc_load(&class_init);
    CHECK(class_init.version == ULONG_MAX);

    size_t after_count = 0;
    mosaic_objc_image_t *after = mosaic_objc_copyImageList(&after_count);
    CHECK(after != NULL && after_count == before_count + 1);
    mosaic_objc_image_t class_image = after[after_count - 1];
    free(after);

    CHECK(mosaic_objc_imageSetAddressRange(
        class_image, (const void *)(uintptr_t)0x100000, 0x1000));
    CHECK(mosaic_objc_imageRetire(class_image));
    CHECK(event_count == 3);

    memset(&report, 0, sizeof(report));
    CHECK(mosaic_objc_imageGetUnloadReport(class_image, &report));
    CHECK(report.state == MOSAIC_OBJC_IMAGE_RETIRED);
    CHECK(report.class_metadata_count == 1);
    CHECK((report.blockers & MOSAIC_OBJC_IMAGE_BLOCKER_CLASS_METADATA) != 0);
    CHECK(report.loader_metadata_count >= 1);
    CHECK((report.blockers & MOSAIC_OBJC_IMAGE_BLOCKER_LOADER_METADATA) != 0);
    CHECK((report.blockers & MOSAIC_OBJC_IMAGE_BLOCKER_NOT_RETIRED) == 0);
    CHECK((report.blockers & MOSAIC_OBJC_IMAGE_BLOCKER_ADDRESS_RANGE_UNKNOWN) == 0);
    CHECK(!mosaic_objc_imageIsUnloadCandidate(class_image, NULL));
    CHECK(!mosaic_objc_imageDetach(class_image, report.mutation_epoch));

    CHECK(!mosaic_objc_imageGetUnloadReport(NULL, &report));
    CHECK(!mosaic_objc_imageGetUnloadReport(image, NULL));
    CHECK(!mosaic_objc_imageSetAddressRange(NULL, mapped_base, 1));
    CHECK(!mosaic_objc_imageRetire(NULL));
    CHECK(!mosaic_objc_imageDetach(NULL, 1));
    CHECK(!mosaic_objc_imageDetach(image, 0));

    mosaic_objc_runtimeSetEventSink(NULL, NULL);
    return 0;
}
