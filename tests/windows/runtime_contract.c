/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
 * and provenance details.
 */
#include <stdio.h>
#include "objc/runtime.h"
#include "objc/mosaic.h"

/* Historical GNU runtime symbol retained for ABI compatibility. */
id object_copy(id obj, size_t size);

static unsigned long long answer(id self, SEL _cmd)
{
    (void)self;
    (void)_cmd;
    return 42;
}

int main(void)
{
    mosaic_objc_runtime_initialize();

    Class cls = objc_allocateClassPair(Nil, "MosaicProbe", 0);
    if (!cls) return 10;

    SEL sel = sel_registerName("answer");
    if (!sel) return 11;
    if (!class_addMethod(cls, sel, __builtin_bit_cast(IMP, &answer), "Q@:")) return 12;

    objc_registerClassPair(cls);
    if ((Class)objc_getClass("MosaicProbe") != cls) return 13;
    id obj = class_createInstance(cls, 0);
    if (!obj) return 14;

    if (object_copy(nil, 0) != nil) return 17;
    const size_t instance_size = class_getInstanceSize(cls);
    if ((instance_size > 0) && (object_copy(obj, instance_size - 1) != nil)) return 18;
    id copy = object_copy(obj, instance_size);
    if (!copy) return 19;
    if (object_getClass(copy) != cls) return 20;
    object_dispose(copy);

    IMP imp = objc_msg_lookup(obj, sel);
    if (!imp) return 15;

    typedef unsigned long long (*answer_method_t)(id, SEL);
    answer_method_t typed_imp = __builtin_bit_cast(answer_method_t, imp);
    const unsigned long long value = typed_imp(obj, sel);
    object_dispose(obj);

    printf("objc-runtime-value=%llu\n", value);
    return value == 42 ? 0 : 16;
}
