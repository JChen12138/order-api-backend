cmake_minimum_required(VERSION 3.12)
project(OrderApiBackend)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include your headers
include_directories(
  ${PROJECT_SOURCE_DIR}/include
  "C:/Users/16210/vcpkg/installed/x64-windows/include"
)


# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Define executable
add_executable(server ${SOURCES})

# Force MSVC to use console subsystem
if (MSVC)
    set_target_properties(server PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

# Compiler options (for MSVC)
if (MSVC)
    add_definitions(-D_UNICODE)
    target_compile_options(server PRIVATE /utf-8 /wd4267 /wd4244 /wd4200)
endif()

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
target_compile_definitions(server PRIVATE _WIN32_WINNT=0x0A00)


# Point to your vcpkg-installed sqlite3.lib directly
set(VCPKG_LIB_DIR "C:/Users/16210/vcpkg/installed/x64-windows/lib")

target_link_libraries(server
    ${VCPKG_LIB_DIR}/sqlite3.lib
    ${VCPKG_LIB_DIR}/redis++.lib
    ${VCPKG_LIB_DIR}/hiredis.lib
    ${VCPKG_LIB_DIR}/fmt.lib
    ${VCPKG_LIB_DIR}/spdlog.lib
)

# Unit tests
enable_testing()
add_subdirectory(test)
