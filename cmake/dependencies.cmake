find_package(asio REQUIRED)
find_package(spdlog REQUIRED)
find_package(doctest REQUIRED)

include(FetchContent)

FetchContent_Declare(coro GIT_REPOSITORY https://github.com/vvarma/coro.git GIT_TAG main)
FetchContent_MakeAvailable(coro)
