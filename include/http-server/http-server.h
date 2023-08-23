// Copyright 2023 Vinay Varma; Subject to the MIT License.
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <fmt/core.h>

#include <asio/io_context.hpp>
#include <coro/task.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "http-server/enum.h"
#include "http-server/route.h"

namespace hs {

namespace internal {
class HttpServerImpl;
}  // namespace internal

struct Config {
  std::string program_name;
  std::string bind_address;
  uint16_t port;
  Config(const std::string &program_name, const std::string &bind_address,
         uint16_t port);
};

class HttpServer : public std::enable_shared_from_this<HttpServer> {
 public:
  typedef std::shared_ptr<HttpServer> Ptr;

  HttpServer(const Config &config);
  void AddRoute(const Route::Ptr &route);
  coro::task<void> ServeAsync(asio::io_context &io_context);
  ~HttpServer();

 private:
  std::unique_ptr<internal::HttpServerImpl> pimpl_;
};

class Exception : public std::exception {
  StatusCode code_;
  std::string message_;

 public:
  StatusCode Code() const;
  Exception(StatusCode code, std::string message);
  const char *what() const noexcept override;
};

}  // namespace hs

#endif  // !#ifndef HTTP_SERVER_H
