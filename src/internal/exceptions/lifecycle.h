#ifndef LIBOBJC2_INTERNAL_EXCEPTION_LIFECYCLE_H
#define LIBOBJC2_INTERNAL_EXCEPTION_LIFECYCLE_H

#include "visibility.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

PRIVATE size_t objc2_countExceptionHookReferences(uintptr_t base, size_t size);

#ifdef __cplusplus
}
#endif

#endif
