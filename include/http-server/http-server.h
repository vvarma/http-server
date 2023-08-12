// Copyright 2023 Vinay Varma; Subject to the MIT License.
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <fmt/core.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "http-server/response-writer.h"

namespace hs {

namespace internal {
class HttpServerImpl;
struct RequestImpl;
}  // namespace internal
enum class Method {
  GET,
  POST,
  PUT,
  DELETE,
  HEAD,
  //  OPTIONS,
  //  PATCH,
  //  TRACE,
  //  CONNECT
};

class Request {
 public:
  Request(std::shared_ptr<internal::RequestImpl> pimpl);
  ~Request();

  Method GetMethod() const;
  Version GetVersion() const;
  std::string_view Path() const;
  std::optional<std::string_view> Header(std::string_view key) const;
  std::optional<std::string_view> Param(std::string_view key) const;

 private:
  std::shared_ptr<internal::RequestImpl> pimpl_;
};

struct Route {
  typedef std::function<void(Request, ResponseWriter::Ptr)> Handler;
  Method method;
  std::string path;
  Handler handler;
  // Ask client to close connection when done
  bool keep_socket_open = true;
  Route(Method method, const std::string &path, Handler handler);
};

struct Config {
  std::string program_name;
  std::string bind_address;
  uint16_t port;
  Config(std::string program_name, std::string bind_address, uint16_t port);
};

class HttpServer : public std::enable_shared_from_this<HttpServer> {
 public:
  typedef std::shared_ptr<HttpServer> Ptr;

  HttpServer(const Config &config);
  void AddRoute(const Route &route);
  void Serve();
  asio::awaitable<void> ServeAsync();
  ~HttpServer();

 private:
  std::unique_ptr<internal::HttpServerImpl> pimpl_;
};

class Exception : public std::exception {
  StatusCode code_;
  std::string message_;

 public:
  Exception(StatusCode code, std::string message);
  const char *what() const noexcept override;
};

}  // namespace hs

#endif  // !#ifndef HTTP_SERVER_H
