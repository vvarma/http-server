cmake_minimum_required(VERSION 3.23)
project(http-server)
set(CMAKE_CXX_STANDARD 20)
if (NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
include(cmake/dependencies.cmake)

add_library(${PROJECT_NAME} STATIC src/http-server.cpp src/request.cpp src/route.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PUBLIC fmt::fmt coro asio::asio PRIVATE spdlog::spdlog)

if (ENABLE_TESTS)
  include(CTest)
  FILE(GLOB TEST_SOURCES test/*.cpp)
  add_executable(${PROJECT_NAME}-tests ${TEST_SOURCES})
  target_link_libraries(${PROJECT_NAME}-tests PRIVATE ${PROJECT_NAME} doctest::doctest)
  add_test(NAME ${PROJECT_NAME}-tests COMMAND ${PROJECT_NAME}-tests)
endif()

add_subdirectory(examples)

