cmake_minimum_required(VERSION 3.8.2)
project(open_hikv)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

message("CMAKE_C_FLAGS_DEBUG is ${CMAKE_C_FLAGS_DEBUG}")
message("CMAKE_C_FLAGS_RELEASE is ${CMAKE_C_FLAGS_RELEASE}")
message("CMAKE_C_FLAGS_RELWITHDEBINFO is ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
message("CMAKE_C_FLAGS_MINSIZEREL is ${CMAKE_C_FLAGS_MINSIZEREL}")

message("CMAKE_CXX_FLAGS_DEBUG is ${CMAKE_CXX_FLAGS_DEBUG}")
message("CMAKE_CXX_FLAGS_RELEASE is ${CMAKE_CXX_FLAGS_RELEASE}")
message("CMAKE_CXX_FLAGS_RELWITHDEBINFO is ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message("CMAKE_CXX_FLAGS_MINSIZEREL is ${CMAKE_CXX_FLAGS_MINSIZEREL}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -mcx16")
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -mrtm")

add_executable(open_hikv main.cpp
        error_code.h
        msg_queue.h
        open_hikv.cpp
        open_hikv.h
        ordered_index.h
        store.h
        unit.h
        unordered_index.h
        test/bplustree_test.cpp
        test/open_hikv_test.cpp
        plain_vanilla_impl/plain_msg_queue_impl.cpp
        plain_vanilla_impl/plain_msg_queue_impl.h
        plain_vanilla_impl/plain_ordered_index_impl.cpp
        plain_vanilla_impl/plain_ordered_index_impl.h
        plain_vanilla_impl/plain_store_impl.cpp
        plain_vanilla_impl/plain_store_impl.h
        plain_vanilla_impl/plain_unordered_index_impl.cpp
        plain_vanilla_impl/plain_unordered_index_impl.h
        pmem_impl/bplustree.c
        pmem_impl/bplustree.h
        pmem_impl/cpu.c
        pmem_impl/cpu.h
        pmem_impl/pmem_msg_queue_impl.cpp
        pmem_impl/pmem_msg_queue_impl.h
        pmem_impl/pmem_op.cpp
        pmem_impl/pmem_op.h
        pmem_impl/pmem_ordered_index_impl.cpp
        pmem_impl/pmem_ordered_index_impl.h
        pmem_impl/pmem_store_impl.cpp
        pmem_impl/pmem_store_impl.h
        pmem_impl/pmem_unordered_index_impl.cpp
        pmem_impl/pmem_unordered_index_impl.h)

include_directories(./)

if (CMAKE_VERSION VERSION_LESS "3.1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
else ()
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(open_hikv Threads::Threads)
endif ()