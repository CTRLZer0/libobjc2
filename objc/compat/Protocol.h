#ifndef __OBJC_COMPAT_PROTOCOL_H_INCLUDED__
#define __OBJC_COMPAT_PROTOCOL_H_INCLUDED__

#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#import <objc/compat/Object.h>

@interface Protocol : Object @end

#endif // __OBJC_COMPAT_PROTOCOL_H_INCLUDED__
