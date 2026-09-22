cmake_minimum_required(VERSION 3.16)
project(self_cache LANGUAGES CXX)

include(CTest)

add_subdirectory(lru-in-cxx)
add_subdirectory(lfu-in-cxx)
add_subdirectory(2Q-in-cxx)
add_subdirectory(arc-in-cxx)
