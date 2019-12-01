cmake_minimum_required(VERSION 3.5)
project(ccc_analysis)

file(GLOB EXAMPLE_HEADERS "*.hpp")
file(GLOB EXAMPLE_SOURCES "*.cpp")

add_executable(ccc_analysis EXCLUDE_FROM_ALL ${EXAMPLE_SOURCES} ${EXAMPLE_HEADERS})

target_link_libraries( ccc_analysis blocksci blocksci_internal)
