#if defined _WIN32 || defined __CYGWIN__
#	if defined(__OBJC_RUNTIME_STATIC__)
#		define OBJC_PUBLIC
#	elif defined(__OBJC_RUNTIME_INTERNAL__)
#		define OBJC_PUBLIC __attribute__((dllexport))
#	else
#		define OBJC_PUBLIC __attribute__((dllimport))
#	endif
#else
#	define OBJC_PUBLIC
#endif
