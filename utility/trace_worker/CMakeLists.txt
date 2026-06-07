cmake_minimum_required(VERSION 3.16)
project(trace_worker_tools LANGUAGES CXX)

add_executable(normalize_ast src/normalize_ast.cpp)
target_compile_features(normalize_ast PRIVATE cxx_std_17)
