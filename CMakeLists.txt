cmake_minimum_required(VERSION 3.16)
project(TRANSLATOR)

set(TARGET_NAME translator)

if(DEBUG)
    add_definitions(-DDEBUG=1)
endif()

add_subdirectory(ALEX)
add_subdirectory(ADSR)
add_subdirectory(AD)
add_subdirectory(AT)
add_subdirectory(VM)

add_executable(${TARGET_NAME} main.c)

target_link_libraries(${TARGET_NAME} ALEX)
target_link_libraries(${TARGET_NAME} ADSR)
target_link_libraries(${TARGET_NAME} AD)
target_link_libraries(${TARGET_NAME} AT)
target_link_libraries(${TARGET_NAME} VM)
