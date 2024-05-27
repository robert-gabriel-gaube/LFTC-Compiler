set(SOURCES src/vm.c)

add_library(VM ${SOURCES})

target_include_directories(VM PUBLIC ./include ../ALEX/include ../AD/include ../ALEX/include)
target_link_libraries(VM PUBLIC ALEX AD)
