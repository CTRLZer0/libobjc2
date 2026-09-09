/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
 * and provenance details.
 */
#ifndef LIBOBJC2_CRT_COMPAT_H
#define LIBOBJC2_CRT_COMPAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline char *objc2_strdup(const char *string)
{
	if (NULL == string) { return NULL; }
	size_t length = strlen(string);
	if (length == SIZE_MAX) { return NULL; }
	char *copy = (char *)malloc(length + 1);
	if (NULL == copy) { return NULL; }
	memcpy(copy, string, length + 1);
	return copy;
}

static inline FILE *objc2_fopen(const char *path, const char *mode)
{
#if defined(_MSC_VER) && !defined(__MINGW32__)
	FILE *file = NULL;
	return fopen_s(&file, path, mode) == 0 ? file : NULL;
#else
	return fopen(path, mode);
#endif
}

static inline int objc2_getenv_exists(const char *name)
{
#if defined(_MSC_VER) && !defined(__MINGW32__)
	char *value = NULL;
	size_t length = 0;
	int result = _dupenv_s(&value, &length, name);
	int exists = (result == 0) && (value != NULL);
	free(value);
	return exists;
#else
	return getenv(name) != NULL;
#endif
}

#endif // LIBOBJC2_CRT_COMPAT_H
