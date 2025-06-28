#pragma once

#include <casket/predefined/compiler.h>

// clang-format off

#if !defined BUILD_TYPE_IS_DETECTED
    #if COMPILER_IS_GCC_COMPATIBLE
        #if defined NDEBUG
            #define BUILD_TYPE_IS_RELEASE 1
            #define BUILD_TYPE_IS_DETECTED 1
        #else
            #define BUILD_TYPE_IS_DEBUG 1
            #define BUILD_TYPE_IS_DETECTED 1
        #endif
    #else
        #define BUILD_TYPE_IS_RELEASE 1
        #define BUILD_TYPE_IS_DETECTED 0

    #endif

#endif

#if !defined BUILD_TYPE_IS_DEBUG
    #define BUILD_TYPE_IS_DEBUG 0
#endif

#if !defined BUILD_TYPE_IS_RELEASE
    #define BUILD_TYPE_IS_RELEASE 0
#endif

// clang-format on