#ifndef LIBOBJC2_INTERNAL_DISPATCH_TRACING_H
#define LIBOBJC2_INTERNAL_DISPATCH_TRACING_H

#if defined(WITH_TRACING) && !defined(_WIN32) && \
    (defined(__x86_64__) || defined(__x86_64) || \
     defined(__aarch64__) || defined(__arm64__) || defined(__ARM_ARCH_ISA_A64))
# define OBJC2_TRACING_SUPPORTED 1
#endif


#endif
