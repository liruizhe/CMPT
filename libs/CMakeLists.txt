project(CMPT_Lib)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(. DIR_SRCS)

set(CMAKE_C_COMPILER "icc")
set(CMAKE_C_FLAGS "-g")

add_library(cmpt STATIC ${DIR_SRCS})
