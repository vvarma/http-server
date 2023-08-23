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
using namespace std::literals::chrono_literals;

struct Handler : public hs::Handler {
  coro::async_generator<hs::Response> Handle(const hs::Request req) override {
    int count = 0;
    co_yield hs::StatusCode::Ok;
    hs::Headers headers{
        {"Server", "echo"},
        {"Content-Type", "text/plain"},
    };
    co_yield headers;
    while (!IsDone()) {
      auto msg = fmt::format("Message, {}!", count++);
      auto resp = std::make_shared<hs::WritableResponseBody<std::string>>(
          fmt::format("Content-Type: text/plain\r\nContent-Length: {}\r\n\r\n",
                      msg.size()));
      co_yield resp;
      resp = std::make_shared<hs::WritableResponseBody<std::string>>(msg);
      co_yield resp;
      resp = std::make_shared<hs::WritableResponseBody<std::string>>(
          "\r\n--hs-bd\r\n");
      co_yield resp;

      std::this_thread::sleep_for(1s);
    }
  }
};
struct Route : public hs::Route {
  hs::Method GetMethod() const override { return hs::Method::GET; }
  std::string GetPath() const override { return "/mp"; }
  hs::Handler::Ptr GetHandler() const override {
    return std::make_shared<Handler>();
  }
};

int main(int argc, char *argv[]) {
  spdlog::set_level(spdlog::level::debug);
  asio::io_context io_context;
  auto server = std::make_shared<hs::HttpServer>(
      hs::Config("multipart", "localhost", 5555));
  server->AddRoute(std::make_shared<Route>());
  std::jthread t([&]() { coro::sync_wait(server->ServeAsync(io_context)); });
  std::this_thread::sleep_for(1s);
  // signals.async_wait([&](auto, auto) { io_context.stop(); });
  io_context.run();
  return 0;
}
