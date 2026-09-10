#ifndef __OBJC_COMPAT_OBJECT_H_INCLUDED__
#define __OBJC_COMPAT_OBJECT_H_INCLUDED__

#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#include <objc/runtime.h>

@interface Object
{
	Class isa;
}
@end

#endif // __OBJC_COMPAT_OBJECT_H_INCLUDED__
