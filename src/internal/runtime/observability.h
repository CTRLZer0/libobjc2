#ifndef __OBJC_RUNTIME_OBSERVABILITY_H_INCLUDED__
#define __OBJC_RUNTIME_OBSERVABILITY_H_INCLUDED__

#include <objc/extensions/mosaic.h>
#include "visibility.h"

#ifdef __cplusplus
extern "C" {
#endif

PRIVATE void mosaic_objc_emitRuntimeEvent(struct mosaic_objc_runtime_event *event);
PRIVATE uint64_t mosaic_objc_runtimeMutationEpoch(void);
PRIVATE void mosaic_objc_beginRuntimeMutation(void);
PRIVATE void mosaic_objc_endRuntimeMutation(void);
PRIVATE uint64_t mosaic_objc_noteRuntimeMutation(void);
PRIVATE size_t mosaic_objc_countGlobalHookReferences(uintptr_t base, size_t size);

#ifdef __cplusplus
}
#endif

#endif
