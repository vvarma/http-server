// Copyright 2023 Vinay Varma; Subject to the MIT License.
#include <spdlog/spdlog.h>

#include <asio.hpp>

#include "http-server/http-server.h"

int main(int argc, char *argv[]) {
  asio::io_context io_context(1);

  asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto sig) {
    spdlog::info("shutting down server. sig: {}", sig);
    io_context.stop();
  });
  auto server = std::make_shared<hs::HttpServer>();
  server->AddRoute(hs::Route(
      hs::Method::GET, "/",
      [](hs::Request req, hs::ResponseWriter::Ptr res) -> void {
        res->SetContentType("text/plain");
        auto resp =
            fmt::format("Hello, {}!", req.Param("name").value_or("world"));
        res->SetContentLength(resp.size());
        res->Write(resp);
      }));
  co_spawn(io_context, server->ServeAsync(), asio::detached);
  io_context.run();
  return 0;
}
