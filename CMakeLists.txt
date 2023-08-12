cmake_minimum_required(VERSION 3.23)
project(http-server)
set(CMAKE_CXX_STANDARD 20)
if (NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
include(cmake/dependencies.cmake)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DASIO_ENABLE_HANDLER_TRACKING")

add_library(${PROJECT_NAME} STATIC src/http-server.cpp src/response-writer.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PUBLIC fmt::fmt asio::asio PRIVATE spdlog::spdlog)

add_subdirectory(test)

add_subdirectory(examples)

