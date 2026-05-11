cmake_minimum_required(VERSION 3.15)

project(SmartCitySystem LANGUAGES CXX)

# ==========================================
# C++ Standard
# ==========================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# Output Directory
# ==========================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ==========================================
# Collect Sources
# ==========================================
file(GLOB_RECURSE SMART_CITY_SOURCES
    src/*.cpp
)

# ==========================================
# Core Library
# ==========================================
add_library(SmartCityCore STATIC
    ${SMART_CITY_SOURCES}
)

# ==========================================
# Include Directories
# ==========================================
target_include_directories(SmartCityCore
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

# ==========================================
# Main Application
# ==========================================
add_executable(SmartCityApp
    src/main.cpp
)

target_link_libraries(SmartCityApp
    PRIVATE SmartCityCore
)

# ==========================================
# Tests
# ==========================================
enable_testing()

# Graph Test
add_executable(test_graph
    tests/test_graph.cpp
)

target_link_libraries(test_graph
    PRIVATE SmartCityCore
)

add_test(NAME GraphTest COMMAND test_graph)

# HashTable Test
add_executable(test_hashtable
    tests/test_hashtable.cpp
)

target_link_libraries(test_hashtable
    PRIVATE SmartCityCore
)

add_test(NAME HashTableTest COMMAND test_hashtable)

# Integration Test
add_executable(test_integration
    tests/test_integration.cpp
)

target_link_libraries(test_integration
    PRIVATE SmartCityCore
)

add_test(NAME IntegrationTest COMMAND test_integration)

# Priority Queue Test
add_executable(test_priorityqueue
    tests/test_priorityqueue.cpp
)

target_link_libraries(test_priorityqueue
    PRIVATE SmartCityCore
)

add_test(NAME PriorityQueueTest COMMAND test_priorityqueue)

# QuadTree Test
add_executable(test_quadtree
    tests/test_quadtree.cpp
)

target_link_libraries(test_quadtree
    PRIVATE SmartCityCore
)

add_test(NAME QuadTreeTest COMMAND test_quadtree)