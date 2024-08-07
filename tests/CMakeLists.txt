# doctest
set(DOCTEST_INCLUDE_DIR ./doctest/doctest/)
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${DOCTEST_INCLUDE_DIR})

# tests
add_executable(MementoCPPTests ./memento_cpp_tests.cpp)
target_link_libraries(MementoCPPTests MementoCPPLib doctest)
add_test(NAME test_memento_cpp COMMAND MementoCPPTests)

add_executable(tests tests.cpp)
target_link_libraries(tests mementolib)
add_test(NAME test_all COMMAND tests)
