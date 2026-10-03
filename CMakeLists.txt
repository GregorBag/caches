cmake_minimum_required(VERSION 3.16)
project(self_cache LANGUAGES CXX)

include(CTest)

add_subdirectory(LRU)
add_subdirectory(LFU)
add_subdirectory(TWO-QUEUE)
add_subdirectory(ARC)
add_subdirectory(LIRS)