project(DPFPIR)
cmake_minimum_required(VERSION 3.4.1)

enable_language(C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER clang)
# First, try to find Boost in the Homebrew location
#set(BOOST_ROOT "/opt/homebrew/Cellar/boost/1.83.0")
#find_package(Boost 1.83.0 QUIET)
# If that fails, try the standard location for Boost 1.71

set(BOOST_ROOT "/usr") # Standard location for Boost
find_package(Boost 1.71.0 REQUIRED)

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found")
endif()
include_directories("/usr/include")
include_directories("/usr/include/boost")

include_directories("/usr/include/cryptopp")
include_directories("/usr/include/crypto++")

include_directories(${Boost_INCLUDE_DIRS})

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
        hashdatastore.cpp)

set(CMAKE_C_FLAGS "-ffunction-sections -Wall -maes -msse2 -msse4.1 -mavx2 -mpclmul -Wfatal-errors -pthread -Wno-strict-overflow -Wno-ignored-attributes")
#set(CMAKE_C_FLAGS "-ffunction-sections -Wall  -maes -msse2 -msse4.1 -mavx2 -mpclmul -Wfatal-errors -pthread -Wno-strict-overflow  -fPIC -Wno-ignored-attributes")
set(CMAKE_CXX_FLAGS  "${CMAKE_C_FLAGS}")

# Select flags.
SET(CMAKE_CXX_FLAGS_RELEASE "-Ofast")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -O2 -g -ggdb -rdynamic")
SET(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g -ggdb -rdynamic")
#SET(CMAKE_CXX_FLAGS_DEBUG  "-O3 -g -ggdb -rdynamic")

#add_compile_definitions(ENABLE_DEBUG)
link_directories("/usr/lib/x86_64-linux-gnu")

add_executable(dpf_pir ${SRCS} main.cpp pirw.cpp pirw.h shamir.cpp shamir.h utils.cpp utils.h Server.cpp Server.h DebugPrint.cpp DebugPrint.h)
target_link_libraries(dpf_pir crypto cryptopp)
#target_link_options(dpf_pir PRIVATE -fopenmp)

add_executable(bench ${SRCS} bench.cpp aligned_allocator.cpp aligned_allocator.h)
target_link_libraries(bench crypto)
#target_link_options(bench PRIVATE -fopenmp)

add_executable(dpf_tests ${SRCS} test.cpp)
target_link_libraries(dpf_tests crypto)
#target_link_options(dpf_tests PRIVATE -fopenmp)