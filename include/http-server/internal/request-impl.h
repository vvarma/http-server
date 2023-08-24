// Copyright 2023 Vinay Varma; Subject to the MIT License.
#ifndef HTTP_SERVER_REQUEST_IMPL_H
#define HTTP_SERVER_REQUEST_IMPL_H
#include <asio/ip/tcp.hpp>
#include <coro/task.hpp>
#include <memory>
#include <string>
#include <unordered_map>

#include "http-server/enum.h"
using asio::ip::tcp;
namespace hs::internal {
struct RequestImpl {
  typedef std::shared_ptr<RequestImpl> Ptr;
  Method method;
  Version version;
  std::string path;
  std::unordered_map<std::string, std::string> headers;
  std::unordered_map<std::string, std::string> query_params;
  std::vector<std::string> path_params;
  std::shared_ptr<tcp::socket> socket;
  std::string body_part;
};
coro::task<std::optional<RequestImpl::Ptr>> ParseRequestLine(
    std::shared_ptr<tcp::socket> socket);
}  // namespace hs::internal

#endif  // !#ifndef HTTP_SERVER_REQUEST_IMPL_H
