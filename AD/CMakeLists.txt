set(SOURCES src/ad.c)

add_library(AD ${SOURCES})
target_include_directories(AD PUBLIC ./include ../ALEX/include ../VM/include)
