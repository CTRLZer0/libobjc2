#ifndef __LIBOBJC_PLATFORM_POSIX_H_INCLUDED__
#define __LIBOBJC_PLATFORM_POSIX_H_INCLUDED__

#include <pthread.h>

typedef struct objc_platform_tls_key
{
	pthread_key_t native;
} objc_platform_tls_key_t;

typedef struct objc_platform_mutex
{
	pthread_mutex_t native;
} objc_platform_mutex_t;

#endif