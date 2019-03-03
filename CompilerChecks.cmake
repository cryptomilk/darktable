include(AddCCompilerFlag)
include(AddCXXCompilerFlag)
include(CheckCCompilerFlagSSP)

if (UNIX)
    ##################### C ########################
    #
    # Check for -Werror turned on if possible
    #
    # This will prevent that compiler flags are detected incorrectly.
    #
    check_c_compiler_flag("-Werror" REQUIRED_FLAGS_WERROR)
    if (REQUIRED_FLAGS_WERROR)
        set(CMAKE_REQUIRED_FLAGS "-Werror")

        if (PICKY_DEVELOPER)
            list(APPEND SUPPORTED_C_COMPILER_FLAGS "-Werror")
        endif()
    endif()

    add_c_compiler_flag("-std=c11" SUPPORTED_C_COMPILER_FLAGS)
    if (NOT WITH_STD_C11_FLAG)
        message(FATAL_ERROR "The compiler ${CMAKE_C_COMPILER} has no C11 support. Please use a different C compiler.")
    endif()

    add_c_compiler_flag("-Wall" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Wshadow" SUPPORTED_C_COMPILER_FLAGS)
    #add_c_compiler_flag("-Wmissing-prototypes" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Wcast-align" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Werror=address" SUPPORTED_C_COMPILER_FLAGS)
    #add_c_compiler_flag("-Wstrict-prototypes" SUPPORTED_C_COMPILER_FLAGS)
    #add_c_compiler_flag("-Werror=strict-prototypes" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Wwrite-strings" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Werror=write-strings" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Werror-implicit-function-declaration" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Wpointer-arith" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Werror=pointer-arith" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Wreturn-type" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Werror=return-type" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Wuninitialized" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Werror=uninitialized" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Wimplicit-fallthrough" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Werror=strict-overflow" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Wstrict-overflow=2" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Wno-format-zero-length" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Wmissing-field-initializers" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Wtype-limits" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Wvla" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Wthread-safety" SUPPORTED_C_COMPILER_FLAGS)

    check_c_compiler_flag("-Wformat" REQUIRED_FLAGS_WFORMAT)
    if (REQUIRED_FLAGS_WFORMAT)
        list(APPEND SUPPORTED_C_COMPILER_FLAGS "-Wformat")
        set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Wformat")
    endif()
    add_c_compiler_flag("-Wformat-security" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("-Werror=format-security" SUPPORTED_C_COMPILER_FLAGS)

    # Allow zero for a variadic macro argument
    string(TOLOWER "${CMAKE_C_COMPILER_ID}" _C_COMPILER_ID)
    if ("${_C_COMPILER_ID}" STREQUAL "clang")
        add_c_compiler_flag("-Wno-gnu-zero-variadic-macro-arguments" SUPPORTED_C_COMPILER_FLAGS)
    endif()

    add_c_compiler_flag("-fno-common" SUPPORTED_C_COMPILER_FLAGS)

    if (CMAKE_BUILD_TYPE)
        string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
        if (CMAKE_BUILD_TYPE_LOWER MATCHES (release|relwithdebinfo|minsizerel))
            add_c_compiler_flag("-Wp,-D_FORTIFY_SOURCE=2" SUPPORTED_C_COMPILER_FLAGS)
        endif()
    endif()

    check_c_compiler_flag_ssp("-fstack-protector-strong" WITH_STACK_PROTECTOR_STRONG)
    if (WITH_STACK_PROTECTOR_STRONG)
        list(APPEND SUPPORTED_C_COMPILER_FLAGS "-fstack-protector-strong")
        # This is needed as Solaris has a seperate libssp
        if (SOLARIS)
            list(APPEND SUPPORTED_LINKER_FLAGS "-fstack-protector-strong")
        endif()
    else (WITH_STACK_PROTECTOR_STRONG)
        check_c_compiler_flag_ssp("-fstack-protector" WITH_STACK_PROTECTOR)
        if (WITH_STACK_PROTECTOR)
            list(APPEND SUPPORTED_C_COMPILER_FLAGS "-fstack-protector")
            # This is needed as Solaris has a seperate libssp
            if (SOLARIS)
                list(APPEND SUPPORTED_LINKER_FLAGS "-fstack-protector")
            endif()
        endif()
    endif (WITH_STACK_PROTECTOR_STRONG)

    check_c_compiler_flag_ssp("-fstack-clash-protection" WITH_STACK_CLASH_PROTECTION)
    if (WITH_STACK_CLASH_PROTECTION)
        list(APPEND SUPPORTED_C_COMPILER_FLAGS "-fstack-clash-protection")
    endif()

    if (BUILD_SSE2_CODEPATHS)
        add_c_compiler_flag("-msse2" SUPPORTED_C_COMPILER_FLAGS)

        if(NOT WITH_MSSE2_FLAG)
            message(WARNING "Building of SSE2-optimized codepaths is enabled, but the compiler does not understand -msse2.")
            set(BUILD_SSE2_CODEPATHS OFF)
        endif()
    endif()


    if (PICKY_DEVELOPER)
        add_c_compiler_flag("-Wno-error=deprecated-declarations" SUPPORTED_C_COMPILER_FLAGS)
        add_c_compiler_flag("-Wno-error=tautological-compare" SUPPORTED_C_COMPILER_FLAGS)
    endif()

    # Unset CMAKE_REQUIRED_FLAGS
    unset(CMAKE_REQUIRED_FLAGS)

    ##################### CXX ########################

    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    #
    # Check for -Werror turned on if possible
    #
    # This will prevent that compiler flags are detected incorrectly.
    #
    check_cxx_compiler_flag("-Werror" REQUIRED_FLAGS_WERROR)
    if (REQUIRED_FLAGS_WERROR)
        set(CMAKE_REQUIRED_FLAGS "-Werror")

        if (PICKY_DEVELOPER)
            list(APPEND SUPPORTED_CXX_COMPILER_FLAGS "-Werror")
        endif()
    endif()

    add_cxx_compiler_flag("-Wall" SUPPORTED_CXX_COMPILER_FLAGS)
    add_cxx_compiler_flag("-Wshadow" SUPPORTED_CXX_COMPILER_FLAGS)
    add_cxx_compiler_flag("-Wtype-limits" SUPPORTED_CXX_COMPILER_FLAGS)
    add_cxx_compiler_flag("-Wvla" SUPPORTED_CXX_COMPILER_FLAGS)
    add_cxx_compiler_flag("-Wthread-safety" SUPPORTED_CXX_COMPILER_FLAGS)

    # Unset CMAKE_REQUIRED_FLAGS
    unset(CMAKE_REQUIRED_FLAGS)
endif()

if (MSVC)
    add_c_compiler_flag("/D _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("/D _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("/D _CRT_NONSTDC_NO_WARNINGS=1" SUPPORTED_C_COMPILER_FLAGS)
    add_c_compiler_flag("/D _CRT_SECURE_NO_WARNINGS=1" SUPPORTED_C_COMPILER_FLAGS)
endif()

set(DEFAULT_C_COMPILE_FLAGS ${SUPPORTED_C_COMPILER_FLAGS} CACHE INTERNAL "Default C Compiler Flags" FORCE)
set(DEFAULT_CXX_COMPILE_FLAGS ${SUPPORTED_CXX_COMPILER_FLAGS} CACHE INTERNAL "Default C Compiler Flags" FORCE)
set(DEFAULT_LINK_FLAGS ${SUPPORTED_LINKER_FLAGS} CACHE INTERNAL "Default C Linker Flags" FORCE)
