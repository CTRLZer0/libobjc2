/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
 * and provenance details.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "objc/runtime.h"
#include "objc/mosaic.h"

static volatile uintptr_t benchmark_sink;

static double elapsed_ns(LARGE_INTEGER start, LARGE_INTEGER end, LARGE_INTEGER frequency)
{
    return ((double)(end.QuadPart - start.QuadPart) * 1000000000.0) /
        (double)frequency.QuadPart;
}

static void report(const char *name, uint64_t iterations, double ns)
{
    const double ns_per_op = ns / (double)iterations;
    printf("%-24s %8.3f ns/op  %8.2f Mops/s\n",
        name, ns_per_op, 1000.0 / ns_per_op);
}

int main(int argc, char **argv)
{
    uint64_t iterations = 5000000;
    if (argc > 1) {
        char *end = NULL;
        errno = 0;
        unsigned long long parsed = strtoull(argv[1], &end, 10);
        if (errno == ERANGE || end == argv[1] || *end != '\0' || parsed == 0)
        {
            return 2;
        }
        iterations = (uint64_t)parsed;
    }

    mosaic_objc_runtime_initialize();
    Class cls = objc_allocateClassPair(Nil, "MosaicBenchmarkClass", 0);
    Class alternate = objc_allocateClassPair(Nil, "MosaicBenchmarkAlternate", 0);
    if (cls == Nil || alternate == Nil) { return 3; }
    objc_registerClassPair(cls);
    objc_registerClassPair(alternate);
    id object = class_createInstance(cls, 0);
    if (object == nil) { return 4; }

    const char *class_name = "MosaicBenchmarkClass";
    const char *selector_name = "mosaicBenchmarkSelector:";
    (void)sel_registerName(selector_name);
    LARGE_INTEGER frequency, start, end;
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
    {
        return 6;
    }

    uintptr_t sink = 0;
    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i) {
        sink += (uintptr_t)object_getClass(object);
    }
    QueryPerformanceCounter(&end);
    report("object_getClass", iterations, elapsed_ns(start, end, frequency));

    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i) {
        sink += (uintptr_t)objc_getClass(class_name);
    }
    QueryPerformanceCounter(&end);
    report("objc_getClass", iterations, elapsed_ns(start, end, frequency));

    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i) {
        sink += (uintptr_t)sel_registerName(selector_name);
    }
    QueryPerformanceCounter(&end);
    report("sel_registerName", iterations, elapsed_ns(start, end, frequency));

    QueryPerformanceCounter(&start);
    for (uint64_t i = 0; i < iterations; ++i) {
        Class next = (i & 1) ? cls : alternate;
        sink += (uintptr_t)object_setClass(object, next);
    }
    QueryPerformanceCounter(&end);
    report("object_setClass", iterations, elapsed_ns(start, end, frequency));

    benchmark_sink = sink;
    object_setClass(object, cls);
    object_dispose(object);
    return benchmark_sink == 0 ? 5 : 0;
}
