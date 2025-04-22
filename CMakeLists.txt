cmake_minimum_required(VERSION 3.20)
project(Cache-Simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(Cache
        src/Cache.cpp
        src/MemoryManager.cpp
)

add_executable(
        CacheSingle
        src/MainSinCache.cpp
)
target_link_libraries(CacheSingle Cache)

add_executable(
        CacheMulti
        src/MainMulCache.cpp
)
target_link_libraries(CacheMulti Cache)
