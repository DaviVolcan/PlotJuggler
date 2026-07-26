add_library(rtt_data_map STATIC rtt_data_map.cpp)
target_include_directories(rtt_data_map PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rtt_data_map PUBLIC Qt5::Core)
set_target_properties(rtt_data_map PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(binary_parser STATIC binary_parser.cpp)
target_include_directories(binary_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(binary_parser PUBLIC rtt_data_map)
set_target_properties(binary_parser PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(csv_logger STATIC csv_logger.cpp)
target_include_directories(csv_logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(csv_logger PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(rtt_banner STATIC rtt_banner.cpp)
target_include_directories(rtt_banner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(rtt_banner PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(DataStreamRTT SHARED datastream_rtt.cpp)

target_link_libraries(DataStreamRTT PRIVATE Qt5::Widgets Qt5::Network Qt5::Xml
                                            plotjuggler_base binary_parser rtt_data_map csv_logger
                                            rtt_banner)

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
  add_executable(test_rtt_data_map tests/test_rtt_data_map.cpp)
  target_link_libraries(test_rtt_data_map PRIVATE rtt_data_map GTest::gtest_main)
  add_executable(test_csv_logger tests/test_csv_logger.cpp)
  target_link_libraries(test_csv_logger PRIVATE csv_logger GTest::gtest_main)
  add_executable(test_binary_parser tests/test_binary_parser.cpp)
  target_link_libraries(test_binary_parser PRIVATE binary_parser GTest::gtest_main)
  add_executable(test_rtt_banner tests/test_rtt_banner.cpp)
  target_link_libraries(test_rtt_banner PRIVATE rtt_banner GTest::gtest_main)
  enable_testing()
  add_test(NAME rtt_data_map COMMAND test_rtt_data_map)
  add_test(NAME csv_logger COMMAND test_csv_logger)
  add_test(NAME binary_parser COMMAND test_binary_parser)
  add_test(NAME rtt_banner COMMAND test_rtt_banner)
endif()
