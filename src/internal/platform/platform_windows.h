#ifndef __LIBOBJC_PLATFORM_WINDOWS_H_INCLUDED__
#define __LIBOBJC_PLATFORM_WINDOWS_H_INCLUDED__

#include "../exceptions/safewindows.h"

typedef struct objc_platform_tls_key
{
	DWORD slot;
	objc_platform_tls_cleanup_t cleanup;
} objc_platform_tls_key_t;

typedef struct objc_platform_mutex
{
	CRITICAL_SECTION native;
} objc_platform_mutex_t;

#endif