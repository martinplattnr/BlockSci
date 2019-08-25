cmake_minimum_required(VERSION 3.5)
project(fork_support)

file(GLOB EXAMPLE_HEADERS "*.hpp")
file(GLOB EXAMPLE_SOURCES "*.cpp")

add_executable(fork_support EXCLUDE_FROM_ALL ${EXAMPLE_SOURCES} ${EXAMPLE_HEADERS})

target_link_libraries( fork_support blocksci)
