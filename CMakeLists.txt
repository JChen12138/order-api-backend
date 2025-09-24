cmake_minimum_required(VERSION 3.12)
project(OrderApiBackend)

set(CMAKE_CXX_STANDARD 17)

# Output binary to ./bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include paths
include_directories(
    include
    /usr/local/include  # For redis-plus-plus, spdlog, etc.
)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Define executable
add_executable(server ${SOURCES})

# Link external libraries
target_link_libraries(server
    sqlite3
    redis++
    hiredis
    pthread
    fmt
)

# Optional: include doctest unit tests
enable_testing()
add_subdirectory(test)
