// Copyright 2023 Vinay Varma; Subject to the MIT License.
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <coro/async_generator.hpp>
#include <memory>
#include <thread>

#include "coro/sync_wait.hpp"
#include "http-server/http-server.h"
#include "http-server/route.h"
#include "http-server/static-routes.h"
using namespace std::literals::chrono_literals;

int main(int argc, char *argv[]) {
  spdlog::set_level(spdlog::level::debug);
  if (argc != 2) {
    spdlog::error("Directory not specified");
    exit(1);
  }
  std::string base_dir = argv[1];

  asio::io_context io_context;
  auto server = std::make_shared<hs::HttpServer>(
      hs::Config("file-server", "localhost", 55555));
  server->AddRoute(std::make_shared<hs::StaticRoute>("/", base_dir));
  std::jthread t([&]() { coro::sync_wait(server->ServeAsync(io_context)); });
  std::this_thread::sleep_for(1s);
  io_context.run();
  return 0;
}
