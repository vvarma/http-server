#ifndef HTTP_SERVER_RESPONSE_WRITER_H
#define HTTP_SERVER_RESPONSE_WRITER_H
#include <fmt/core.h>

#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>
#include <concepts>
#include <memory>

namespace hs {
enum Version { HTTP_1_0, HTTP_1_1 };
enum StatusCode {
  Ok = 200,
  BadRequest = 400,
  NotFound = 404,
  InternalServerError = 500
};
template <typename T>
concept Writable = requires(T t) {
  { t.data() } -> std::convertible_to<const void *>;
  { t.size() } -> std::convertible_to<std::size_t>;
};

class ResponseWriter : public std::enable_shared_from_this<ResponseWriter> {
 public:
  typedef std::shared_ptr<ResponseWriter> Ptr;

  ResponseWriter(Version version,
                 std::shared_ptr<asio::ip::tcp::socket> socket);
  void SetStatusCode(StatusCode code);
  void SetHeader(std::string_view key, std::string_view value);
  void SetContentType(std::string_view contentType);
  void SetContentLength(size_t contentLength);
  bool IsOpen() const;
  template <Writable T>
  void Write(T data) {
    auto executor = socket_->get_executor();
    co_spawn(executor, WriteAsync(data), asio::detached);
  }
  asio::awaitable<void> WriteHeaders();

 private:
  template <Writable T>
  asio::awaitable<void> WriteAsync(T data) {
    if (!headersSent) {
      co_await shared_from_this()->WriteHeaders();
    }
    co_await socket_->async_write_some(asio::buffer(data.data(), data.size()),
                                       asio::use_awaitable);
  }
  Version version_;
  std::shared_ptr<asio::ip::tcp::socket> socket_;
  StatusCode statusCode = StatusCode::Ok;
  std::unordered_map<std::string, std::string> headers;
  std::atomic<bool> headersSent = false;
};

}  // namespace hs
namespace fmt {
template <>
struct formatter<hs::StatusCode> {
  char presentation = 's';
  constexpr auto parse(format_parse_context &ctx)
      -> format_parse_context::iterator {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && (*it == 's' || *it == 'd')) presentation = *it++;

    // Check if reached the end of the range:
    // if (it != end && *it != '}')
    //  ctx.on_error("invalid format");

    // Return an iterator past the end of the parsed range:
    return it;
  };

  template <typename FormatContext>
  auto format(const hs::StatusCode &code, FormatContext &ctx)
      -> decltype(ctx.out()) {
    if (presentation == 'd') return fmt::format_to(ctx.out(), "{}", (int)code);
    switch (code) {
      case hs::Ok:
        return fmt::format_to(ctx.out(), "Ok");
      case hs::BadRequest:
        return fmt::format_to(ctx.out(), "BadRequest");
      case hs::NotFound:
        return fmt::format_to(ctx.out(), "NotFound");
      case hs::InternalServerError:
      default:
        return fmt::format_to(ctx.out(), "InternalServerError");
    }
  }
};
template <>
struct formatter<hs::Version> {
  template <typename ParseContext>
  constexpr auto parse(ParseContext &ctx) {
    return ctx.begin();
  };

  template <typename FormatContext>
  auto format(const hs::Version &v, FormatContext &ctx) -> decltype(ctx.out()) {
    switch (v) {
      case hs::HTTP_1_0:
        return fmt::format_to(ctx.out(), "HTTP/1.0");
      case hs::HTTP_1_1:
      default:
        return fmt::format_to(ctx.out(), "HTTP/1.1");
    }
  }
};
}  // namespace fmt
#endif  // !#ifndef HTTP_SERVER_RESPONSE_WRITER_H
