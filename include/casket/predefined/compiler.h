#pragma once

// clang-format off

#if !defined COMPILER_IS_DETECTED
    #if defined( _MSC_VER ) && defined( __clang__ )
        #define COMPILER_IS_CLANG_CL 1
        #define COMPILER_IS_DETECTED 1

    #elif defined __clang__
        #define COMPILER_IS_CLANG 1
        #define COMPILER_IS_DETECTED 1

    #elif defined __LCC__
        #define COMPILER_IS_LCC 1
        #define COMPILER_IS_DETECTED 1

    #elif defined( __GNUC__ ) && !defined( __llvm__ ) && !defined( __clang__ ) && !defined( __INTEL_COMPILER ) && !defined( __LCC__ )
        #define COMPILER_IS_GCC 1
        #define COMPILER_IS_DETECTED 1

    #elif defined( _MSC_VER ) && !defined( __clang__ )
        #define COMPILER_IS_MSVC 1
        #define COMPILER_IS_DETECTED 1

    #elif defined __INTEL_COMPILER || defined __ICC || defined __ECC || defined __ICL
        #define COMPILER_IS_ICC 1
        #define COMPILER_IS_DETECTED 1

    #else
        #define COMPILER_IS_UNKNOWN 1
        #define COMPILER_IS_DETECTED 0

    #endif
#endif

#if !defined COMPILER_IS_CLANG
    #define COMPILER_IS_CLANG 0
#endif

#if !defined COMPILER_IS_CLANG_CL
    #define COMPILER_IS_CLANG_CL 0
#endif

#if !defined COMPILER_IS_LCC
    #define COMPILER_IS_LCC 0
#endif

#if !defined COMPILER_IS_GCC
    #define COMPILER_IS_GCC 0
#endif

#if !defined COMPILER_IS_MSVC
    #define COMPILER_IS_MSVC 0
#endif

#if !defined COMPILER_IS_ICC
    #define COMPILER_IS_ICC 0
#endif

#if !defined COMPILER_IS_UNKNOWN
    #define COMPILER_IS_UNKNOWN 0
#endif

#define COMPILER_IS_GCC_COMPATIBLE (COMPILER_IS_GCC || COMPILER_IS_LCC || COMPILER_IS_CLANG || COMPILER_IS_CLANG_CL)

// clang-format on