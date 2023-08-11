#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <fmt/core.h>

#include "http-server/response-writer.h"

namespace hs {

namespace internal {
class HttpServerImpl;
struct RequestImpl;
} // namespace internal
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
  Route(Method method, const std::string &path, Handler handler);
};

class HttpServer {
public:
  HttpServer();
  void AddRoute(const Route &route);
  void Serve();
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

} // namespace hs


#endif // !#ifndef HTTP_SERVER_H
