add_library(line_parser STATIC line_parser.cpp)
target_include_directories(line_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(line_parser PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(BUILD_TESTING)
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz)
    FetchContent_MakeAvailable(googletest)
  endif()
  add_executable(test_line_parser tests/test_line_parser.cpp)
  target_link_libraries(test_line_parser PRIVATE line_parser GTest::gtest_main)
  add_test(NAME line_parser COMMAND test_line_parser)
endif()
