#ifndef HTTP_SERVER_RESPONSE_H
#define HTTP_SERVER_RESPONSE_H

#include <concepts>
#include <optional>
#include <string_view>

#include "http-server/enum.h"
namespace hs {

template <Writable T>
class Response {
 public:
  Response(StatusCode statusCode);
  void SetHeader(std::string_view key, std::string_view value);
  void SetContentType(std::string_view contentType);
  void SetContentLength(size_t contentLength);
  void SetWritable(T writable) { writable_ = writable; }
  void SetHasMore(bool more);

 private:
  std::optional<T> writable_;
};

}  // namespace hs

#endif  // !#ifndef HTTP_SERVER_RESPONSE_H
