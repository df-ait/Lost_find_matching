cmake_minimum_required(VERSION 3.16)

project(lost_find_matching LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CORE_SOURCES
    src/console_utf8.c
    src/ui_util.c
    src/net_util.c
    src/protocol.c
    src/list.c
    src/hash_table.c
    src/bst.c
    src/heap.c
    src/queue.c
    src/match.c
    src/item_mgr.c
    src/item_persist.c
    src/claim_mgr.c
)

add_library(lost_find_core STATIC ${CORE_SOURCES})
target_include_directories(lost_find_core PUBLIC include)

if(MSVC)
    target_compile_options(lost_find_core PRIVATE /W4 /utf-8)
else()
    target_compile_options(lost_find_core PRIVATE -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

find_package(Threads REQUIRED)

add_executable(lost_find_server src/server_main.c)
add_executable(lost_find_client src/client_main.c)

target_link_libraries(lost_find_server PRIVATE lost_find_core Threads::Threads ws2_32)
target_link_libraries(lost_find_client PRIVATE lost_find_core Threads::Threads ws2_32)

if(NOT MSVC)
    target_link_libraries(lost_find_core PRIVATE m)
endif()

if(NOT MSVC)
    target_compile_options(lost_find_server PRIVATE -Wall -Wextra)
    target_compile_options(lost_find_client PRIVATE -Wall -Wextra)
endif()
