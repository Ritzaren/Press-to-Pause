cmake_minimum_required(VERSION 3.21)
project(PressToPause LANGUAGES CXX)

find_package(CommonLibSSE CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)

add_commonlibsse_plugin(PressToPause SOURCES src/press_to_pause_plugin.cpp)
target_compile_features(PressToPause PRIVATE cxx_std_23)
target_precompile_headers(PressToPause PRIVATE src/PCH.h)