set(SOURCES src/gc.c)

add_library(GC ${SOURCES})

target_include_directories(GC PUBLIC ./include ../ALEX/include ../AD/include ../AT/include)
target_link_libraries(GC PUBLIC ALEX AD)
