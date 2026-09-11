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
static uintptr_t empty_range;

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
    size_t beforeCount = 0;
    mosaic_objc_image_t *before = mosaic_objc_copyImageList(&beforeCount);
    free(before);

    Class base = objc_allocateClassPair(Nil, "MosaicRegistryBase", 0);
    CHECK(base != Nil);
    objc_registerClassPair(base);
    struct objc_class cls, meta;
    init_static_pair(&cls, &meta, "MosaicRegistryClass", base);
    Class classSlots[1] = { &cls };
    Class classRefs[1] = { &cls };

    struct objc_category category = {0};
    category.name = "Diagnostics";
    category.class_name = "MosaicRegistryClass";

    struct objc_protocol protocol = {0};
    protocol.name = "MosaicRegistryProtocol";

    struct objc_init imageInit = {0};
    imageInit.sel_begin = imageInit.sel_end = &empty_range;
    imageInit.cls_begin = classSlots;
    imageInit.cls_end = classSlots + 1;
    imageInit.cls_ref_begin = classRefs;
    imageInit.cls_ref_end = classRefs + 1;
    imageInit.cat_begin = &category;
    imageInit.cat_end = &category + 1;
    imageInit.proto_begin = &protocol;
    imageInit.proto_end = &protocol + 1;
    imageInit.proto_ref_begin = imageInit.proto_ref_end = &empty_range;
    imageInit.alias_begin = imageInit.alias_end = &empty_range;
    imageInit.strings_begin = imageInit.strings_end = &empty_range;
    __objc_load(&imageInit);
    CHECK(imageInit.version == ULONG_MAX);
    size_t imageCount = 0;
    mosaic_objc_image_t *images = mosaic_objc_copyImageList(&imageCount);
    CHECK(images != NULL);
    CHECK(imageCount == beforeCount + 1);
    mosaic_objc_image_t image = images[imageCount - 1];
    free(images);

    struct mosaic_objc_image_info info = {0};
    CHECK(mosaic_objc_imageGetInfo(image, &info));
    CHECK(info.generation != 0);
    CHECK(info.identifier == NULL);
    CHECK(info.provider == NULL);
    CHECK(info.base_address == NULL);
    CHECK(info.class_count == 1);
    CHECK(info.class_reference_count == 1);
    CHECK(info.category_count == 1);
    CHECK(info.protocol_count == 1);

    CHECK(mosaic_objc_imageGetClass(image, 0) == &cls);
    CHECK(mosaic_objc_imageGetClass(image, 1) == Nil);
    CHECK(mosaic_objc_imageForClass(&cls) == image);
    CHECK(mosaic_objc_imageForClass(base) == NULL);
    Protocol *registeredProtocol = mosaic_objc_imageGetProtocol(image, 0);
    CHECK(registeredProtocol == (Protocol *)&protocol);
    CHECK(mosaic_objc_imageGetProtocol(image, 1) == NULL);
    CHECK(mosaic_objc_imageForProtocol(registeredProtocol) == image);
    CHECK(strcmp(protocol_getName(registeredProtocol), "MosaicRegistryProtocol") == 0);

    CHECK(strcmp(mosaic_objc_imageGetCategoryName(image, 0), "Diagnostics") == 0);
    CHECK(strcmp(mosaic_objc_imageGetCategoryClassName(image, 0),
                 "MosaicRegistryClass") == 0);
    CHECK(mosaic_objc_imageGetCategoryName(image, 1) == NULL);

    const void *baseAddress = (const void *)(uintptr_t)0x100000;
    CHECK(mosaic_objc_imageSetIdentity(image, "Registry.framework/Registry",
                                      "mosaic-test", baseAddress));
    CHECK(mosaic_objc_imageSetIdentity(image, "Registry.framework/Registry",
                                      "mosaic-test", baseAddress));
    CHECK(!mosaic_objc_imageSetIdentity(image, "Other.framework/Other",
                                       NULL, NULL));
    CHECK(!mosaic_objc_imageSetIdentity(image, NULL, "other-provider", NULL));
    CHECK(!mosaic_objc_imageSetIdentity(image, NULL, NULL,
                                       (const void *)(uintptr_t)0x200000));
    struct mosaic_objc_image_info identified = {0};
    CHECK(mosaic_objc_imageGetInfo(image, &identified));
    CHECK(strcmp(identified.identifier, "Registry.framework/Registry") == 0);
    CHECK(strcmp(identified.provider, "mosaic-test") == 0);
    CHECK(identified.base_address == baseAddress);
    CHECK(identified.generation == info.generation);

    CHECK(!mosaic_objc_imageGetInfo(NULL, &identified));
    CHECK(!mosaic_objc_imageGetInfo(image, NULL));
    CHECK(mosaic_objc_imageForProtocol(NULL) == NULL);
    CHECK(mosaic_objc_imageGetCategoryClassName(image, 1) == NULL);
    return 0;
}
