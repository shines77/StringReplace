CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

project(StringReplace)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../cmake/Modules/")

include("${PROJECT_SOURCE_DIR}/../cmake/detect_cpu_architectures.cmake")

enable_language(ASM_NASM)

if (CMAKE_CL_64)
    set(CMAKE_CL_ARCH x64)
else()
    set(CMAKE_CL_ARCH x86)
endif()

##
## See: https://stackoverflow.com/questions/39258250/how-to-detect-if-64-bit-msvc-with-cmake
##
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    # 64 bits
    set(CMAKE_PLATFORM_ARCH Arch64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    # 32 bits
    set(CMAKE_PLATFORM_ARCH Arch32)
endif()

message("------------ Options -------------")
message("  CMAKE_BUILD_TYPE         : ${CMAKE_BUILD_TYPE}")
message("  CMAKE_CL_ARCH            : ${CMAKE_CL_ARCH}")
message("  CMAKE_PLATFORM_ARCH      : ${CMAKE_PLATFORM_ARCH}")
message("  CMAKE_CPU_ARCHITECTURES  : ${CMAKE_CPU_ARCHITECTURES}")
message("----------------------------------")

message("-------------- Env ---------------")
message("  CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message("  CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message("  PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")
message("  PROJECT_BINARY_DIR: ${PROJECT_BINARY_DIR}")
message("----------------------------------")

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DEBUG")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG")
endif()

if (NOT MSVC)
    ##
    ## See: https://www.cnblogs.com/stonehat/p/7000637.html
    ##
    ## Usage: (Ubuntu 16.04 Server)
    ##
    ##   apt-get install llvm clang clang-5.0 clang-6.0 clang-8
    ##

    ## For C_FLAGS
    ## -mmmx -msse -msse2 -msse3 -mssse3 -msse4 -msse4a -msse4.1 -msse4.2 -mavx -mavx2 -mavx512vl -mavx512f 
    set(CMAKE_C_FLAGS_DEFAULT "${CMAKE_C_FLAGS} -std=c89 -march=native -mtune=native -finput-charset=gbk -Wall -Wno-unused-function -Wno-deprecated-declarations -Wno-unused-variable -fPIC")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_DEFAULT} -O3 -DNDEBUG")
    set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEFAULT} -g -pg -D_DEBUG")

    ## For CXX_FLAGS
    ## -Wall -Werror -Wextra -Wno-format -Wno-unused-function
    ## -mmmx -msse -msse2 -msse3 -mssse3 -msse4 -msse4a -msse4.1 -msse4.2 -mavx -mavx2 -mavx512vl -mavx512f
    # -fexec-charset=gbk -finput-charset=gbk
    set(CMAKE_CXX_FLAGS_DEFAULT "${CMAKE_CXX_FLAGS} -std=c++14 -stdlib=libstdc++ -march=native -mtune=native -finput-charset=gbk -Wall -Wno-unused-function -Wno-deprecated-declarations -Wno-unused-variable -fPIC")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_DEFAULT} -O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEFAULT} -g -pg -D_DEBUG")

    set(CMAKE_AR        "/usr/bin/llvm-ar")
    set(CMAKE_LINKER    "/usr/bin/llvm-link")
    set(CMAKE_NM        "/usr/bin/llvm-nm")
    set(CMAKE_OBJDUMP   "/usr/bin/llvm-objdump")
    set(CMAKE_RANLIB    "/usr/bin/llvm-ranlib")

    set(CMAKE_EXE_LINKER_FLAGS "-lstdc++")

    ##
    ## See: https://cloud.tencent.com/developer/ask/101584
    ##
    ## "/usr/bin/c++", "/usr/bin/g++", "/usr/bin/clang++"
    message("  CMAKE_C_COMPILER:   ${CMAKE_C_COMPILER}")
    message("  CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
    if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        # using Clang
        if (CONAN_LIBCXX STREQUAL "libstdc++11")
            add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)
        elseif (CONAN_LIBCXX STREQUAL "libstdc++")
            add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
        endif()
    elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
        # using GCC
    elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Intel")
        # using Intel C++
    elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
        # using Visual Studio C++
    endif()
endif()

if (MSVC AND MSVC_STATIC_CRT)
    set(CompilerFlags
            CMAKE_CXX_FLAGS
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_C_FLAGS
            CMAKE_C_FLAGS_DEBUG
            CMAKE_C_FLAGS_RELEASE
            )
    foreach (CompilerFlag ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
endif()

if (WIN32)
    ## add_compile_options(-D__SSE3__ -D__SSE4A__ -D__SSE4_1__ -D__SSE4_2__)
    add_compile_options(-D_WIN32_WINNT=0x0601 -D_CRT_SECURE_NO_WARNINGS)
    set(EXTRA_LIBS ${EXTRA_LIBS} ws2_32 mswsock)
endif()

if (UNIX)
    set(EXTRA_LIBS ${EXTRA_LIBS} pthread)
else()
    set(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif()

include_directories(../include)
include_directories(../src)
include_directories(../src/main)
include_directories(../src/test)
include_directories(../src/unittest)

set(SOURCE_FILES
    ../src/benchmark/benchmark.cpp
    )

# add_subdirectory(main EXCLUDE_FROM_ALL ../src/main/asm)

##
## See: https://stackoverflow.com/questions/39258250/how-to-detect-if-64-bit-msvc-with-cmake
##
if ("amd64" STREQUAL "${CMAKE_CPU_ARCHITECTURES}" OR
    "x64" STREQUAL "${CMAKE_CPU_ARCHITECTURES}" OR
    "${CMAKE_PLATFORM_ARCH}" STREQUAL "Arch64")
    set(ASM_SOURCE_FILES
        ../src/main/asm/asmlib.h
        ../src/main/asm/instrset_x64.asm
        ../src/main/asm/strstr_see42_x64.asm
        ../src/main/asm/strstr_see42_v0_x64.asm
        ../src/main/asm/strstr_see42_v2_x64.asm
    )
elseif ("x86" STREQUAL "${CMAKE_CPU_ARCHITECTURES}" OR
    "${CMAKE_PLATFORM_ARCH}" STREQUAL "Arch32")
    set(ASM_SOURCE_FILES
        ../src/main/asm/asmlib.h
        ../src/main/asm/instrset_x86.asm
        ../src/main/asm/strstr_see42_x86.asm
        ../src/main/asm/strstr_see42_v0_x86.asm
        ../src/main/asm/strstr_see42_v2_x86.asm
    )
else()
    message("This platform is not support yasm.")
endif()

list(LENGTH ASM_SOURCE_FILES asm_files_length)
if (${asm_files_length} GREATER 0)
    ## STATIC, SHARED
    add_library(asmlib STATIC ${ASM_SOURCE_FILES})
    set(EXTRA_LIBS ${EXTRA_LIBS} asmlib)
endif()

add_custom_target(release
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Release ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
    COMMENT "Switch CMAKE_BUILD_TYPE to Release"
)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message("------------ Options -------------")
message("  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("----------------------------------")

add_executable(StringReplace ${SOURCE_FILES})
target_link_libraries(StringReplace ${EXTRA_LIBS})
