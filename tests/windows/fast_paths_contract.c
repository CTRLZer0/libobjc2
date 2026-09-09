/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
 * and provenance details.
 */
#include "objc/runtime.h"
#include "objc/mosaic.h"
#include "test_support.h"

typedef void (*void_method_fn)(id, SEL);
typedef id (*alloc_method_fn)(id, SEL);
typedef id (*alloc_zone_method_fn)(id, SEL, void *);
typedef id (*init_method_fn)(id, SEL);

static unsigned alloc_calls;
static unsigned alloc_zone_calls;
static unsigned init_calls;
static unsigned redirected_init_calls;
static Class redirect_target;

static void trivial_alloc_init(id self, SEL cmd)
{
    (void)self;
    (void)cmd;
}

static id slow_alloc(id self, SEL cmd)
{
    (void)cmd;
    ++alloc_calls;
    return class_createInstance((Class)self, 0);
}
static id slow_alloc_with_zone(id self, SEL cmd, void *zone)
{
    (void)cmd;
    (void)zone;
    ++alloc_zone_calls;
    return class_createInstance((Class)self, 0);
}

static id slow_init(id self, SEL cmd)
{
    (void)cmd;
    ++init_calls;
    return self;
}

static id redirect_alloc(id self, SEL cmd)
{
    (void)self;
    (void)cmd;
    return class_createInstance(redirect_target, 0);
}

static id redirected_init(id self, SEL cmd)
{
    (void)cmd;
    ++redirected_init_calls;
    return self;
}

static IMP imp_from_void(void_method_fn fn)
{
    return __builtin_bit_cast(IMP, fn);
}

static IMP imp_from_alloc(alloc_method_fn fn)
{
    return __builtin_bit_cast(IMP, fn);
}

static IMP imp_from_alloc_zone(alloc_zone_method_fn fn)
{
    return __builtin_bit_cast(IMP, fn);
}

static IMP imp_from_init(init_method_fn fn)
{
    return __builtin_bit_cast(IMP, fn);
}
int main(void)
{
    mosaic_objc_runtime_initialize();

    Class fast = objc_allocateClassPair(Nil, "MosaicFastPathBase", 0);
    CHECK(fast != Nil);
    Class fast_meta = object_getClass((id)fast);
    CHECK(fast_meta != Nil);
    CHECK(class_addMethod(fast_meta, sel_registerName("_TrivialAllocInit"),
        imp_from_void(trivial_alloc_init), "v@:"));
    objc_registerClassPair(fast);

    id fast_object = objc_alloc_init(fast);
    CHECK(fast_object != nil);
    CHECK(object_getClass(fast_object) == fast);
    CHECK(alloc_calls == 0);
    CHECK(init_calls == 0);
    object_dispose(fast_object);

    Class slow = objc_allocateClassPair(fast, "MosaicFastPathOverride", 0);
    CHECK(slow != Nil);
    Class slow_meta = object_getClass((id)slow);
    CHECK(slow_meta != Nil);
    CHECK(class_addMethod(slow_meta, sel_registerName("alloc"),
        imp_from_alloc(slow_alloc), "@@:"));
    CHECK(class_addMethod(slow_meta, sel_registerName("allocWithZone:"),
        imp_from_alloc_zone(slow_alloc_with_zone), "@@:^v"));
    CHECK(class_addMethod(slow, sel_registerName("init"),
        imp_from_init(slow_init), "@@:"));
    objc_registerClassPair(slow);

    id slow_object = objc_alloc_init(slow);
    CHECK(slow_object != nil);
    CHECK(object_getClass(slow_object) == slow);
    CHECK(alloc_calls == 1);
    CHECK(init_calls == 1);
    object_dispose(slow_object);

    id zone_object = objc_allocWithZone(slow);
    CHECK(zone_object != nil);
    CHECK(object_getClass(zone_object) == slow);
    CHECK(alloc_zone_calls == 1);
    object_dispose(zone_object);

    redirect_target = objc_allocateClassPair(fast, "MosaicFastPathRedirectTarget", 0);
    CHECK(redirect_target != Nil);
    CHECK(class_addMethod(redirect_target, sel_registerName("init"),
        imp_from_init(redirected_init), "@@:"));
    objc_registerClassPair(redirect_target);

    Class redirect_source = objc_allocateClassPair(fast, "MosaicFastPathRedirectSource", 0);
    CHECK(redirect_source != Nil);
    Class redirect_source_meta = object_getClass((id)redirect_source);
    CHECK(class_addMethod(redirect_source_meta, sel_registerName("alloc"),
        imp_from_alloc(redirect_alloc), "@@:"));
    objc_registerClassPair(redirect_source);

    id redirected = objc_alloc_init(redirect_source);
    CHECK(redirected != nil);
    CHECK(object_getClass(redirected) == redirect_target);
    CHECK(redirected_init_calls == 1);
    object_dispose(redirected);

    CHECK(objc_alloc(Nil) == nil);
    CHECK(objc_allocWithZone(Nil) == nil);
    CHECK(objc_alloc_init(Nil) == nil);
    return 0;
}
