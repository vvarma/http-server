#ifndef HTTP_SERVER_ENUM_H
#define HTTP_SERVER_ENUM_H
#include <fmt/core.h>
#include <fmt/format.h>
namespace hs {

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
enum Version { HTTP_1_0, HTTP_1_1 };
enum StatusCode {
  Ok = 200,
  BadRequest = 400,
  NotFound = 404,
  InternalServerError = 500
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
#endif  // !#ifndef HTTP_SERVER_ENUM_H
