/*
 * SPDX-License-Identifier: MIT
 * Portions adapted from GNUstep libobjc2 fast-path allocation support (MIT).
 * CTRLZer0 integration and Mosaic-specific adaptation are also licensed under MIT.
 * Copyright (C) 2026 CTRLZer0 contributors for CTRLZer0 modifications.
 * Original GNUstep/libobjc2 attribution remains under COPYING and upstream
 * repository history. See COPYING and NOTICE.md.
 */
#include "objc/runtime.h"
#include "class.h"
#include "dtable.h"

typedef id (*objc_id_send0_fn)(id, SEL);
typedef id (*objc_id_send1_fn)(id, SEL, void *);

static id send_id0(id receiver, const char *name)
{
	SEL selector = sel_registerName(name);
	IMP imp = objc_msg_lookup(receiver, selector);
	if (NULL == imp) { return nil; }
	objc_id_send0_fn call = __builtin_bit_cast(objc_id_send0_fn, imp);
	return call(receiver, selector);
}

static id send_id1(id receiver, const char *name, void *argument)
{
	SEL selector = sel_registerName(name);
	IMP imp = objc_msg_lookup(receiver, selector);
	if (NULL == imp) { return nil; }
	objc_id_send1_fn call = __builtin_bit_cast(objc_id_send1_fn, imp);
	return call(receiver, selector, argument);
}

id objc_alloc(Class cls)
{
	if (Nil == cls) { return nil; }
	if (!objc_test_class_flag(cls->isa, objc_class_flag_initialized))
	{
		objc_send_initialize((id)cls);
	}
	if (objc_test_class_flag(cls->isa, objc_class_flag_fast_alloc_init))
	{
		return class_createInstance(cls, 0);
	}
	return send_id0((id)cls, "alloc");
}

id objc_allocWithZone(Class cls)
{
	if (Nil == cls) { return nil; }
	if (!objc_test_class_flag(cls->isa, objc_class_flag_initialized))
	{
		objc_send_initialize((id)cls);
	}
	if (objc_test_class_flag(cls->isa, objc_class_flag_fast_alloc_init))
	{
		return class_createInstance(cls, 0);
	}
	return send_id1((id)cls, "allocWithZone:", NULL);
}

id objc_alloc_init(Class cls)
{
	id instance = objc_alloc(cls);
	if (nil == instance) { return nil; }
	cls = classForObject(instance);
	if (objc_test_class_flag(cls, objc_class_flag_fast_alloc_init))
	{
		return instance;
	}
	return send_id0(instance, "init");
}
