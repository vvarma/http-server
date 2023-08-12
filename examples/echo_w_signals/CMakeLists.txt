project(echo_w_signals)

add_executable(${PROJECT_NAME} server.cpp)
target_link_libraries(${PROJECT_NAME} http-server spdlog::spdlog)

