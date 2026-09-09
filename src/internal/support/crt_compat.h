/*
 * SPDX-License-Identifier: AGPL-3.0-only
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * Original CTRLZer0 work; see LICENSE-CTRLZERO and NOTICE.md for licensing
 * and provenance details.
 */
#ifndef LIBOBJC2_CRT_COMPAT_H
#define LIBOBJC2_CRT_COMPAT_H

#include <stdio.h>
#include <string.h>

static inline char *objc2_strdup(const char *string)
{
#if defined(_WIN32) && defined(_MSC_VER)
	return _strdup(string);
#else
	return strdup(string);
#endif
}

static inline FILE *objc2_fopen(const char *path, const char *mode)
{
#if defined(_WIN32) && defined(_MSC_VER)
	FILE *file = NULL;
	return fopen_s(&file, path, mode) == 0 ? file : NULL;
#else
	return fopen(path, mode);
#endif
}

#endif // LIBOBJC2_CRT_COMPAT_H
