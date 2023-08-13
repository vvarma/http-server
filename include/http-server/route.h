#ifndef HTTP_SERVER_ROUTE_H
#define HTTP_SERVER_ROUTE_H
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
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
class Generator {
 public:
  struct promise_type;
  using handle_type = std::coroutine_handle<promise_type>;
  struct promise_type {
    T value_;
    std::exception_ptr exception_;

    Generator get_return_object() {
      return Generator(handle_type::from_promise(*this));
    }
    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void unhandled_exception() { exception_ = std::current_exception(); }

    template <std::convertible_to<T> From>
    std::suspend_always yield_value(From &&from) {
      value_ = std::forward<From>(from);
      return {};
    }
    void return_void() {}
  };

  handle_type h_;

  Generator(handle_type h) : h_(h) {}
  ~Generator() { h_.destroy(); }
  explicit operator bool() {
    fill();
    return !h_.done();
  }
  T operator()() {
    fill();
    full_ = false;
    return std::move(h_.promise().value_);
  }

 private:
  bool full_ = false;

  void fill() {
    if (!full_) {
      h_();
      if (h_.promise().exception_)
        std::rethrow_exception(h_.promise().exception_);

      full_ = true;
    }
  }
};
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

  virtual Generator<Response> Handle(const Request &req) = 0;
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
#endif  // !#ifndef HTTP_SERVER_ROUTE_H
