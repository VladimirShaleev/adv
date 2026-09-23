enable_testing()

find_package(GTest CONFIG REQUIRED)

file(GLOB UTILES_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp")
add_executable(tests ${UTILES_TEST_SOURCES})
set_default_properties(TARGET tests)
target_include_directories(tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tests PRIVATE GTest::gmock_main)
target_link_libraries(tests PRIVATE utils)

include(GoogleTest)
gtest_discover_tests(tests)
