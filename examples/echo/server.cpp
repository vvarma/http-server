// Copyright 2023 Vinay Varma; Subject to the MIT License.
#include <spdlog/spdlog.h>

#include <asio/io_context.hpp>
#include <coro/async_generator.hpp>

#include "coro/sync_wait.hpp"
#include "http-server/enum.h"
#include "http-server/http-server.h"
#include "http-server/route.h"
using namespace std::chrono_literals;

struct Handler : public hs::Handler {
  coro::async_generator<hs::Response> Handle(const hs::Request req) override {
    auto body = co_await req.Body();
    spdlog::info("Body: {}", body);
    co_yield hs::StatusCode::Ok;
    hs::Headers headers{
        {"Server", "echo"},
        {"Content-Type", "text/plain"},
        {"Content-Length", fmt::format("{}", body.size())},
    };
    co_yield headers;
    co_yield std::make_shared<hs::WritableResponseBody<std::string>>(body);
  }
};
struct Route : public hs::Route {
  hs::Method GetMethod() const override { return hs::Method::POST; }
  std::string GetPath() const override { return "/echo"; }
  hs::Handler::Ptr GetHandler() const override {
    return std::make_shared<Handler>();
  }
};

int main(int argc, char *argv[]) {
  asio::io_context io_context;
  // asio::signal_set signals(io_context, SIGINT, SIGTERM);
  auto server =
      std::make_shared<hs::HttpServer>(hs::Config("echo", "localhost", 5555));
  server->AddRoute(std::make_shared<Route>());
  std::jthread t([&]() { coro::sync_wait(server->ServeAsync(io_context)); });
  std::this_thread::sleep_for(1s);
  // signals.async_wait([&](auto, auto) { io_context.stop(); });
  io_context.run();
  return 0;
}
