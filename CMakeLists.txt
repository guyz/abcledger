project(DPFPIR)
cmake_minimum_required(VERSION 3.4.1)

enable_language(C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER clang)

include_directories("/usr/include/cryptopp")
include_directories("/usr/include/crypto++")

#include_directories(${Boost_INCLUDE_DIRS})

# Locate Crypto++ library
#find_package(CryptoPP REQUIRED)

# Include directories for Crypto++
include_directories(${CRYPTOPP_INCLUDE_DIR})

set(SRCS
        AES.cpp
        Defines.cpp
        Log.cpp
        PRNG.cpp
        dpf.cpp
        hashdatastore.cpp
        AlignedAllocator.cpp
        AlignedAllocator.h)

#set(CMAKE_C_FLAGS "-ffunction-sections -Wall -maes -msse2 -msse4.1 -mavx2 -mpclmul -Wfatal-errors -pthread -fopenmp -Wno-strict-overflow -Wno-ignored-attributes")
set(CMAKE_C_FLAGS "-ffunction-sections -Wall -maes -msse2 -msse4.1 -mavx2 -mavx512f -mpclmul -Wfatal-errors -pthread -Wno-strict-overflow -Wno-ignored-attributes")
#set(CMAKE_C_FLAGS "-ffunction-sections -Wall  -maes -msse2 -msse4.1 -mavx2 -mpclmul -Wfatal-errors -pthread -Wno-strict-overflow  -fPIC -Wno-ignored-attributes")
set(CMAKE_CXX_FLAGS  "${CMAKE_C_FLAGS}")

# Select flags.
SET(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -O2 -g -ggdb -rdynamic")
SET(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g -ggdb -rdynamic")
#SET(CMAKE_CXX_FLAGS_DEBUG  "-O3 -g -ggdb -rdynamic")

#add_compile_definitions(ENABLE_DEBUG)
add_compile_definitions(ENABLE_MULTI)
link_directories("/usr/lib/x86_64-linux-gnu")
#link_directories("/usr/lib/llvm-10/lib")

add_executable(prioram ${SRCS} main.cpp pirw.cpp pirw.h shamir.cpp shamir.h utils.cpp utils.h Server.cpp Server.h DebugPrint.cpp DebugPrint.h)
target_link_libraries(prioram crypto cryptopp)
#target_link_libraries(prioram crypto cryptopp omp)