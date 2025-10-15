file(GLOB TEST_SOURCES "*.cpp")

add_executable(unit_tests ${TEST_SOURCES})

target_compile_definitions(unit_tests PRIVATE _WIN32_WINNT=0x0A00)


set(VCPKG_LIB_DIR "C:/Users/16210/vcpkg/installed/x64-windows/lib")

target_link_libraries(unit_tests
    ${VCPKG_LIB_DIR}/sqlite3.lib
    ${VCPKG_LIB_DIR}/redis++.lib
    ${VCPKG_LIB_DIR}/hiredis.lib
    ${VCPKG_LIB_DIR}/fmt.lib
)

add_test(NAME doctest_unit_tests COMMAND unit_tests)
