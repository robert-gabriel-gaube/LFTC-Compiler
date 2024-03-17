set(SOURCES src/lexer.c src/utils.c)

add_library(ALEX ${SOURCES})
target_include_directories(ALEX PUBLIC ./include)
