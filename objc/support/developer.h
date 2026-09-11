#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __OBJC_DEVELOPER_H_INCLUDED__
#define __OBJC_DEVELOPER_H_INCLUDED__

#include <objc/runtime/types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum objc_developer_mode_np
{
	/** User mode - the default. */
	objc_developer_mode_user,
	/** Developer mode - allows replacing classes. */
	objc_developer_mode_developer,
	/** Safe reload mode - only overlays classes with ABI-compatible layouts. */
	objc_developer_mode_safe_reload
};
/*
 * Sets the developer mode.  When in user mode (the default),
 * loading two classes with the same name will cause the program to abort.  In
 * developer mode, the new class will replace the old one when layouts differ.
 * In safe reload mode, only layout-compatible definitions are overlaid; an
 * incompatible definition is rejected and the canonical class is unchanged.
 * This mode is intended for live-reload tooling that must preserve existing
 * instance and subclass validity.
 */
OBJC_PUBLIC void objc_setDeveloperMode_np(enum objc_developer_mode_np);

#ifdef __cplusplus
}
#endif

#endif // __OBJC_DEVELOPER_H_INCLUDED__
