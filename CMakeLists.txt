cmake_minimum_required(VERSION 3.10)

project(SaveBoxSockets LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

set(SAVEBOX_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SAVEBOX_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(SHARED_SOURCES
    ${SAVEBOX_SRC_DIR}/File.cpp
    ${SAVEBOX_SRC_DIR}/Utils.cpp
)

add_executable(server
    ${SAVEBOX_SRC_DIR}/Server.cpp
    ${SHARED_SOURCES}
)

add_executable(client
    ${SAVEBOX_SRC_DIR}/Client.cpp
    ${SHARED_SOURCES}
)

target_include_directories(server PRIVATE ${SAVEBOX_INCLUDE_DIR})
target_include_directories(client PRIVATE ${SAVEBOX_INCLUDE_DIR})

if(WIN32)
    target_link_libraries(server PRIVATE ws2_32)
    target_link_libraries(client PRIVATE ws2_32)
endif()