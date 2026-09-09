/* SPDX-License-Identifier: AGPL-3.0-only */
#include "objc/runtime.h"
#include "objc/hooks.h"

Class (*_objc_lookup_class)(const char *name) = 0;
void (*_objc_load_callback)(Class cls, struct objc_category *category) = 0;
IMP (*__objc_msg_forward2)(id, SEL) = 0;
void (*_objc_unexpected_exception)(id exception) = 0;
Class (*_objc_class_for_boxing_foreign_exception)(int64_t exceptionClass) = 0;
id (*_objc_weak_load)(id object) = 0;
