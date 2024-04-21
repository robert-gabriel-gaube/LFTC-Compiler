set(SOURCES src/at.c)

add_library(AT ${SOURCES})
target_include_directories(AT PUBLIC ./include ../AD/include ../VM/include)
