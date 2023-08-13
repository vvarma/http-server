// Copyright 2023 Vinay Varma; Subject to the MIT License.
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <asio.hpp>
#include <chrono>
#include <memory>
#include <thread>

#include "http-server/http-server.h"
#include "http-server/route.h"
using namespace std::literals::chrono_literals;

struct Handler : public hs::Handler {
  hs::Generator<hs::Response> Handle(const hs::Request &req) override {
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
  asio::io_context io_context(2);

  asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto sig) {
    spdlog::info("shutting down server. sig: {}", sig);
    io_context.stop();
  });
  auto server = std::make_shared<hs::HttpServer>(
      hs::Config("multipart", "localhost", 5555));
  server->AddRoute(std::make_shared<Route>());
  co_spawn(io_context, server->ServeAsync(), asio::detached);
  io_context.run();
  return 0;
}
