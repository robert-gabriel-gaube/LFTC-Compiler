set(SOURCES src/parser.c)

add_library(ADSR ${SOURCES})
target_include_directories(ADSR PUBLIC  ./include 
                                        ../ALEX/include 
                                        ../AD/include 
                                        ../VM/include
                                        ../AT/include
                                        ../GC/include
                                        )
