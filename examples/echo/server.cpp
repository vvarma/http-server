// Copyright 2023 Vinay Varma; Subject to the MIT License.
#include <spdlog/spdlog.h>

#include <asio.hpp>

#include "http-server/enum.h"
#include "http-server/http-server.h"
#include "http-server/route.h"

struct Handler : public hs::Handler {
  hs::Generator<hs::Response> Handle(const hs::Request &req) override {
    std::string resp = fmt::format("Hello, {}!", req.Param("name").value_or("world"));
    co_yield hs::StatusCode::Ok;
    hs::Headers headers{
        {"Server", "echo"},
        {"Content-Type", "text/plain"},
        {"Content-Length", fmt::format("{}", resp.size())},
    };
    co_yield headers;
    co_yield std::make_shared<hs::WritableResponseBody<std::string>>(resp);
  }
};
struct Route : public hs::Route {
  hs::Method GetMethod() const override { return hs::Method::GET; }
  std::string GetPath() const override { return "/echo"; }
  hs::Handler::Ptr GetHandler() const override {
    return std::make_shared<Handler>();
  }
};

int main(int argc, char *argv[]) {
  asio::io_context io_context(1);

  asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto sig) {
    spdlog::info("shutting down server. sig: {}", sig);
    io_context.stop();
  });
  auto server =
      std::make_shared<hs::HttpServer>(hs::Config("echo", "localhost", 5555));
  server->AddRoute(std::make_shared<Route>());
  co_spawn(io_context, server->ServeAsync(), asio::detached);
  io_context.run();
  return 0;
}
