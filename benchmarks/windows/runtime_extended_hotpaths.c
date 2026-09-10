/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * See COPYING and NOTICE.md for licensing and provenance details.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "objc/runtime.h"
#include "objc/objc-arc.h"
#include "objc/mosaic.h"

typedef id (*message_fn)(id, SEL);
static volatile uintptr_t benchmark_sink;
static int association_key;

static id echo_method(id self, SEL selector)
{
    (void)selector;
    return self;
}

static double elapsed_ns(LARGE_INTEGER start, LARGE_INTEGER end,
                         LARGE_INTEGER frequency)
{
    return ((double)(end.QuadPart - start.QuadPart) * 1000000000.0) /
        (double)frequency.QuadPart;
}
static void report(const char *name, uint64_t iterations, double nanoseconds)
{
    double per_op = nanoseconds / (double)iterations;
    printf("%-28s %8.3f ns/op  %8.2f Mops/s\n",
           name, per_op, 1000.0 / per_op);
}

static uint64_t parse_iterations(int argc, char **argv)
{
    if (argc <= 1) { return 5000000; }
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(argv[1], &end, 10);
    if ((errno == ERANGE) || (end == argv[1]) || (*end != '\0') || (value == 0))
    {
        return 0;
    }
    return (uint64_t)value;
}

static IMP message_to_imp(message_fn function)
{
    return __builtin_bit_cast(IMP, function);
}

static message_fn imp_to_message(IMP implementation)
{
    return __builtin_bit_cast(message_fn, implementation);
}
int main(int argc, char **argv)
{
    uint64_t iterations = parse_iterations(argc, argv);
    if (iterations == 0) { return 2; }

    mosaic_objc_runtime_initialize();
    SEL selector = sel_registerName("mosaicExtendedBenchmark");
    Class cls = objc_allocateClassPair(Nil, "MosaicExtendedBenchmarkClass", 0);
    Class valueClass = objc_allocateClassPair(Nil, "MosaicExtendedBenchmarkValue", 0);
    if ((selector == NULL) || (cls == Nil) || (valueClass == Nil)) { return 3; }
    if (!class_addMethod(cls, selector, message_to_imp(echo_method), "@@:")) { return 4; }
    objc_registerClassPair(cls);
    objc_registerClassPair(valueClass);
    const char *extra_names[6] = {
        "MosaicExtendedBenchmarkExtra0", "MosaicExtendedBenchmarkExtra1",
        "MosaicExtendedBenchmarkExtra2", "MosaicExtendedBenchmarkExtra3",
        "MosaicExtendedBenchmarkExtra4", "MosaicExtendedBenchmarkExtra5"};
    Class extra_classes[6];
    for (unsigned i = 0; i < 6; ++i)
    {
        extra_classes[i] = objc_allocateClassPair(Nil, extra_names[i], 0);
        if (extra_classes[i] == Nil) { return 9; }
        objc_registerClassPair(extra_classes[i]);
    }

    char bulk_names[32][64];
    for (unsigned i = 0; i < 32; ++i)
    {
        snprintf(bulk_names[i], sizeof(bulk_names[i]), "MosaicExtendedBulk%02u", i);
        Class bulk = objc_allocateClassPair(Nil, bulk_names[i], 0);
        if (bulk == Nil) { return 10; }
        objc_registerClassPair(bulk);
    }

    id object = class_createInstance(cls, 0);
    id value = class_createInstance(valueClass, 0);
    if ((object == nil) || (value == nil)) { return 5; }

    IMP warm_imp = objc_msg_lookup(object, selector);
    if ((warm_imp == NULL) || (imp_to_message(warm_imp)(object, selector) != object))
    {
        return 6;
    }
    objc_setAssociatedObject(object, &association_key, value, OBJC_ASSOCIATION_ASSIGN);
    id weak = nil;
    id weak_nil = nil;
    if (objc_initWeak(&weak, object) != object) { return 7; }

    LARGE_INTEGER frequency, start, end;
    if (!QueryPerformanceFrequency(&frequency) || (frequency.QuadPart <= 0)) { return 8; }
    uintptr_t sink = 0;
    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i)
    {
        sink += (uintptr_t)objc_lookUpClass("MosaicExtendedBenchmarkClass");
    }
    QueryPerformanceCounter(&end);
    report("objc_lookUpClass", iterations, elapsed_ns(start, end, frequency));

    const char *class_names[2] = {
        "MosaicExtendedBenchmarkClass", "MosaicExtendedBenchmarkValue"};
    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i)
    {
        sink += (uintptr_t)objc_lookUpClass(class_names[i & 1]);
    }
    QueryPerformanceCounter(&end);
    report("class lookup alternating", iterations, elapsed_ns(start, end, frequency));

    const char *class_names4[4] = {class_names[0], class_names[1], extra_names[0], extra_names[1]};
    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i)
    {
        sink += (uintptr_t)objc_lookUpClass(class_names4[i & 3]);
    }
    QueryPerformanceCounter(&end);
    report("class lookup 4-way", iterations, elapsed_ns(start, end, frequency));

    const char *class_names8[8] = {class_names[0], class_names[1], extra_names[0], extra_names[1],
        extra_names[2], extra_names[3], extra_names[4], extra_names[5]};
    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i)
    {
        sink += (uintptr_t)objc_lookUpClass(class_names8[i & 7]);
    }
    QueryPerformanceCounter(&end);
    report("class lookup 8-way", iterations, elapsed_ns(start, end, frequency));

    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i)
    {
        sink += (uintptr_t)objc_lookUpClass(bulk_names[i & 31]);
    }
    QueryPerformanceCounter(&end);
    report("class lookup 32-way", iterations, elapsed_ns(start, end, frequency));

    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i)
    {
        sink += (uintptr_t)objc_msg_lookup(object, selector);
    }
    QueryPerformanceCounter(&end);
    report("objc_msg_lookup", iterations, elapsed_ns(start, end, frequency));

    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i)
    {
        IMP implementation = objc_msg_lookup(object, selector);
        sink += (uintptr_t)imp_to_message(implementation)(object, selector);
    }
    QueryPerformanceCounter(&end);
    report("lookup + IMP call", iterations, elapsed_ns(start, end, frequency));
    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i)
    {
        (void)objc_retain(object);
        objc_release(object);
    }
    QueryPerformanceCounter(&end);
    report("objc_retain + release", iterations, elapsed_ns(start, end, frequency));

    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i)
    {
        (void)objc_retain_fast_np(object);
        sink += (uintptr_t)objc_release_fast_no_destroy_np(object);
    }
    QueryPerformanceCounter(&end);
    report("fast retain + release", iterations, elapsed_ns(start, end, frequency));

    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i)
    {
        sink += (uintptr_t)objc_getAssociatedObject(object, &association_key);
    }
    QueryPerformanceCounter(&end);
    report("associated object get", iterations, elapsed_ns(start, end, frequency));
    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i)
    {
        sink += (uintptr_t)objc_loadWeakRetained(&weak_nil);
    }
    QueryPerformanceCounter(&end);
    report("weak load nil", iterations, elapsed_ns(start, end, frequency));

    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i)
    {
        id loaded = objc_loadWeakRetained(&weak);
        sink += (uintptr_t)loaded;
        objc_release(loaded);
    }
    QueryPerformanceCounter(&end);
    report("weak load retained", iterations, elapsed_ns(start, end, frequency));

    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i)
    {
        sink += (uintptr_t)objc_storeWeak(&weak, object);
    }
    QueryPerformanceCounter(&end);
    report("weak store same", iterations, elapsed_ns(start, end, frequency));

    benchmark_sink = sink;
    objc_destroyWeak(&weak);
    objc_setAssociatedObject(object, &association_key, nil, OBJC_ASSOCIATION_ASSIGN);
    object_dispose(value);
    object_dispose(object);
    return benchmark_sink == 0 ? 9 : 0;
}
