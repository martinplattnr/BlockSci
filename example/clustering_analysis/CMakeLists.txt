cmake_minimum_required(VERSION 3.5)
project(clustering_analysis)

file(GLOB EXAMPLE_HEADERS "*.hpp")
file(GLOB EXAMPLE_SOURCES "*.cpp")

add_executable(clustering_analysis EXCLUDE_FROM_ALL ${EXAMPLE_SOURCES} ${EXAMPLE_HEADERS})

target_link_libraries( clustering_analysis blocksci blocksci_internal)
