#ifndef HTTP_SERVER_ROUTE_H
#define HTTP_SERVER_ROUTE_H
#include <coro/async_generator.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>

#include "http-server/enum.h"
#include "http-server/request.h"
namespace hs {
template <typename T>
concept Writable = requires(T t) {
  { t.data() } -> std::convertible_to<const void *>;
  { t.size() } -> std::convertible_to<std::size_t>;
};
typedef std::unordered_map<std::string, std::string> Headers;
struct ResponseBody {
  virtual size_t GetSize() const = 0;
  virtual const void *GetData() const = 0;
  typedef std::shared_ptr<ResponseBody> Ptr;
  virtual ~ResponseBody();
};
typedef std::variant<StatusCode, Headers, ResponseBody::Ptr> Response;

template <Writable T>
class WritableResponseBody : public ResponseBody {
 public:
  WritableResponseBody(T t) : data_(t) {}
  size_t GetSize() const override { return data_.size(); }
  const void *GetData() const override { return data_.data(); }

 private:
  T data_;
};

struct Handler {
  typedef std::shared_ptr<Handler> Ptr;

  virtual coro::async_generator<Response> Handle(const Request req) = 0;
  virtual ~Handler();
  void SetDone();
  bool IsDone();

 private:
  bool done = false;
};
struct Route {
  typedef std::shared_ptr<Route> Ptr;

  virtual Method GetMethod() const = 0;
  virtual std::string GetPath() const = 0;
  virtual Handler::Ptr GetHandler() const = 0;
  virtual ~Route();
};
}  // namespace hs
#define ROUTE(name, method, path, handler)                             \
  class name : public hs::Route {                                      \
   public:                                                             \
    hs::Method GetMethod() const override { return method; }           \
    std::string GetPath() const override { return path; }              \
    hs::Handler::Ptr GetHandler() const override { return handler(); } \
  }
#endif  // !#ifndef HTTP_SERVER_ROUTE_H
