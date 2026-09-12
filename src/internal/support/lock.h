/**
 * libobjc requires recursive mutexes.  The operating-system implementation
 * lives behind the internal platform contract so runtime code is host-agnostic.
 */

#ifndef __LIBOBJC_LOCK_H_INCLUDED__
#define __LIBOBJC_LOCK_H_INCLUDED__

#include <assert.h>
#include "../platform/platform.h"

typedef objc_platform_mutex_t mutex_t;

static inline void init_recursive_mutex(mutex_t *mutex)
{
	int result = objc_platform_mutex_init_recursive(mutex);
	assert(result == 0);
	(void)result;
}

#define INIT_LOCK(x) init_recursive_mutex(&(x))
#define LOCK(x) objc_platform_mutex_lock(x)
#define UNLOCK(x) objc_platform_mutex_unlock(x)
#define DESTROY_LOCK(x) objc_platform_mutex_destroy(&(x))

__attribute__((unused)) static void objc_release_lock(void *x)
{
	mutex_t *lock = *(mutex_t**)x;
	UNLOCK(lock);
}
/**
 * Concatenate strings during macro expansion.
 */
#define LOCK_HOLDERN_NAME_CAT(x, y) x ## y
/**
 * Concatenate string with unique variable during macro expansion.
 */
#define LOCK_HOLDER_NAME_COUNTER(x, y) LOCK_HOLDERN_NAME_CAT(x, y)
/**
 * Create a unique name for a lock holder variable
 */
#define LOCK_HOLDER_NAME(x) LOCK_HOLDER_NAME_COUNTER(x, __COUNTER__)

/**
 * Acquires the lock and automatically releases it at the end of the current
 * scope.
 */
#define LOCK_FOR_SCOPE(lock) \
	__attribute__((cleanup(objc_release_lock)))\
	__attribute__((unused)) mutex_t *LOCK_HOLDER_NAME(lock_pointer) = lock;\
	LOCK(lock)

/**
 * The global runtime mutex.
 */
extern 
#ifdef __cplusplus
"C"
#endif
mutex_t runtime_mutex;

#define LOCK_RUNTIME() LOCK(&runtime_mutex)
#define UNLOCK_RUNTIME() UNLOCK(&runtime_mutex)
#define LOCK_RUNTIME_FOR_SCOPE() LOCK_FOR_SCOPE(&runtime_mutex)

#ifdef __cplusplus
/**
 * C++ wrapper around our mutex, for use with std::lock_guard and friends.
 */
class RecursiveMutex
{
	/// The underlying mutex
	mutex_t mutex;

	public:
	/**
	 * Explicit initialisation of the underlying lock, so that this can be a
	 * global.
	 */
	void init()
	{
		INIT_LOCK(mutex);
	}

	/// Acquire the lock.
	void lock()
	{
		LOCK(&mutex);
	}

	/// Release the lock.
	void unlock()
	{
		UNLOCK(&mutex);
	}
};
#endif

#endif // __LIBOBJC_LOCK_H_INCLUDED__
