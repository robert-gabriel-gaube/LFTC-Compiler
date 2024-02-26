cmake_minimum_required(VERSION 3.16)
project(ALEX)

set(SOURCES src/lexer.c src/utils.c src/main.c)
add_executable(ALEX ${SOURCES})
target_include_directories(ALEX PRIVATE ./include/)
