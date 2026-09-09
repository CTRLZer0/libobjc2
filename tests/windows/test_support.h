/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
 * and provenance details.
 */
#ifndef MOSAIC_TEST_SUPPORT_H
#define MOSAIC_TEST_SUPPORT_H

#include <stdio.h>
#include <stdlib.h>

static inline void mosaic_test_fail(const char *expression,
                                    const char *file, int line)
{
    fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
    fflush(stderr);
    exit(EXIT_FAILURE);
}

#define CHECK(expression) \
    do { if (!(expression)) { mosaic_test_fail(#expression, __FILE__, __LINE__); } } while (0)

#endif // MOSAIC_TEST_SUPPORT_H
