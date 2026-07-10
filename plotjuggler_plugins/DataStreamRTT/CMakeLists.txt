add_library(line_parser STATIC line_parser.cpp)
target_include_directories(line_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(line_parser PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(csv_logger STATIC csv_logger.cpp)
target_include_directories(csv_logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(csv_logger PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(DataStreamRTT SHARED datastream_rtt.cpp)

target_link_libraries(DataStreamRTT PRIVATE Qt5::Widgets Qt5::Network Qt5::Xml
                                            plotjuggler_base line_parser csv_logger)

target_compile_definitions(DataStreamRTT PRIVATE QT_PLUGIN)

install(TARGETS DataStreamRTT DESTINATION ${PJ_PLUGIN_INSTALL_DIRECTORY})

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
  add_executable(test_csv_logger tests/test_csv_logger.cpp)
  target_link_libraries(test_csv_logger PRIVATE csv_logger line_parser GTest::gtest_main)
  enable_testing()
  add_test(NAME line_parser COMMAND test_line_parser)
  add_test(NAME csv_logger COMMAND test_csv_logger)
endif()
