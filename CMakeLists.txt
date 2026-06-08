cmake_minimum_required(VERSION 3.16)

project(lost_find_matching LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(SOURCES
    src/console_utf8.c
    src/list.c
    src/hash_table.c
    src/bst.c
    src/heap.c
    src/queue.c
    src/match.c
    src/item_mgr.c
    src/claim_mgr.c
    src/main.c
)

add_executable(lost_find ${SOURCES})

target_include_directories(lost_find PRIVATE include)

if(MSVC)
    target_compile_options(lost_find PRIVATE /W4 /utf-8)
else()
    target_compile_options(lost_find PRIVATE -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8)
    target_link_libraries(lost_find PRIVATE m)
endif()
