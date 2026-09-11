/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 */
#include "test_support.h"
#include <limits.h>
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

static struct objc_init make_image(Class *classes, size_t classCount,
                                   Class *refs, size_t refCount)
{
    struct objc_init init = {0};
    init.sel_begin = init.sel_end = &empty_range;
    init.cls_begin = classes;
    init.cls_end = classes + classCount;
    init.cls_ref_begin = refs;
    init.cls_ref_end = refs + refCount;
    init.cat_begin = init.cat_end = &empty_range;
    init.proto_begin = init.proto_end = &empty_range;
    init.proto_ref_begin = init.proto_ref_end = &empty_range;
    init.alias_begin = init.alias_end = &empty_range;
    init.strings_begin = init.strings_end = &empty_range;
    return init;
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
int main(void)
{
    mosaic_objc_runtime_initialize();
    CHECK(objc_getFutureClass(NULL) == Nil);

    Class existing = objc_allocateClassPair(Nil, "MosaicFutureExisting", 0);
    CHECK(existing != Nil);
    objc_registerClassPair(existing);
    CHECK(objc_getFutureClass("MosaicFutureExisting") == existing);

    Class base = objc_allocateClassPair(Nil, "MosaicFutureBase", 0);
    CHECK(base != Nil);
    objc_registerClassPair(base);

    struct objc_class realClass, realMeta;
    struct objc_class childClass, childMeta;
    init_static_pair(&realClass, &realMeta, "MosaicFutureTarget", base);
    init_static_pair(&childClass, &childMeta, "MosaicFutureChild", &realClass);

    Class future = objc_getFutureClass("MosaicFutureTarget");
    CHECK(future != Nil);
    CHECK(objc_getFutureClass("MosaicFutureTarget") == future);
    CHECK(objc_lookUpClass("MosaicFutureTarget") == Nil);

    Class emptyClasses[1] = { Nil };
    Class earlyRefs[1] = { &realClass };
    struct objc_init earlyImage = make_image(emptyClasses, 0, earlyRefs, 1);
    __objc_load(&earlyImage);
    CHECK(earlyImage.version == ULONG_MAX);
    CHECK(earlyRefs[0] == &realClass);

    Class classSlots[2] = { &realClass, &childClass };
    Class imageRefs[2] = { &realClass, &childClass };
    struct objc_init definingImage = make_image(classSlots, 2, imageRefs, 2);
    __objc_load(&definingImage);

    CHECK(definingImage.version == ULONG_MAX);
    CHECK(classSlots[0] == future);
    CHECK(imageRefs[0] == future);
    CHECK(earlyRefs[0] == future);
    CHECK(childClass.super_class == future);
    CHECK(objc_lookUpClass("MosaicFutureTarget") == future);
    CHECK((Class)objc_getClass("MosaicFutureTarget") == future);
    CHECK(objc_getFutureClass("MosaicFutureTarget") == future);
    CHECK((Class)objc_getClass("MosaicFutureChild") == &childClass);
    CHECK(class_getSuperclass(&childClass) == future);
    CHECK(object_getClass((id)future) == future->isa);

    id futureObject = class_createInstance(future, 0);
    id childObject = class_createInstance(&childClass, 0);
    CHECK(futureObject != nil && childObject != nil);
    CHECK(object_getClass(futureObject) == future);
    CHECK(object_getClass(childObject) == &childClass);
    object_dispose(childObject);
    object_dispose(futureObject);

    Class lateEmptyClasses[1] = { Nil };
    Class lateRefs[1] = { &realClass };
    struct objc_init lateImage = make_image(lateEmptyClasses, 0, lateRefs, 1);
    __objc_load(&lateImage);
    CHECK(lateImage.version == ULONG_MAX);
    CHECK(lateRefs[0] == future);
    return 0;
}
