/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * Original CTRLZer0 work; see LICENSE-CTRLZERO and NOTICE.md for licensing
 * and provenance details.
 */
#include <stdio.h>
#include "objc/runtime.h"
#include "objc/mosaic.h"

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
    if (!class_addMethod(cls, sel, (IMP)answer, "Q@:")) return 12;

    objc_registerClassPair(cls);
    if ((Class)objc_getClass("MosaicProbe") != cls) return 13;
    id obj = class_createInstance(cls, 0);
    if (!obj) return 14;

    IMP imp = objc_msg_lookup(obj, sel);
    if (!imp) return 15;

    const unsigned long long value =
        ((unsigned long long (*)(id, SEL))imp)(obj, sel);
    object_dispose(obj);

    printf("objc-runtime-value=%llu\n", value);
    return value == 42 ? 0 : 16;
}
