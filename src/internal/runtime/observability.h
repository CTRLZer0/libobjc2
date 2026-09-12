#ifndef __OBJC_RUNTIME_OBSERVABILITY_H_INCLUDED__
#define __OBJC_RUNTIME_OBSERVABILITY_H_INCLUDED__

#include <objc/extensions/mosaic.h>
#include "visibility.h"

PRIVATE void mosaic_objc_emitRuntimeEvent(struct mosaic_objc_runtime_event *event);
PRIVATE uint64_t mosaic_objc_runtimeMutationEpoch(void);

#endif
