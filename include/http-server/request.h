#ifndef HTTP_SERVER_REQUEST_H
#define HTTP_SERVER_REQUEST_H

#include <coro/task.hpp>
#include <memory>
#include <optional>

#include "http-server/enum.h"
namespace hs {

namespace internal {
struct RequestImpl;
}  // namespace internal

class Request {
 public:
  Request(std::shared_ptr<internal::RequestImpl> pimpl);
  ~Request();

  Method GetMethod() const;
  Version GetVersion() const;
  std::string_view Path() const;
  std::optional<std::string_view> Header(std::string_view key) const;
  std::optional<std::string_view> Param(std::string_view key) const;
  std::optional<size_t> ContentLength() const;
  coro::task<std::string> Body() const;

 private:
  std::shared_ptr<internal::RequestImpl> pimpl_;
};
}  // namespace hs
#endif  // !#ifndef HTTP_SERVER_REQUEST_H
