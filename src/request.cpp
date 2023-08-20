#include "http-server/request.h"

#include <spdlog/spdlog.h>

#include <asio/buffer.hpp>
#include <asio/completion_condition.hpp>
#include <asio/read.hpp>
#include <asio/read_until.hpp>
#include <asio/streambuf.hpp>
#include <optional>
#include <string>
#include <unordered_map>

#include "coro/single_consumer_event.hpp"
#include "http-server/enum.h"
#include "http-server/http-server.h"
#include "request-impl.h"

// Helper function to remove trailing '\r' and leading/trailing whitespace from
// a string
std::string cleanString(const std::string &str) {
  size_t endPos = str.find_last_not_of(" \t\r\n");
  if (endPos != std::string::npos) {
    size_t startPos = str.find_first_not_of(" \t\r\n");
    return str.substr(startPos, endPos - startPos + 1);
  }
  return "";
}
namespace hs {
namespace internal {
auto ParseUrl(const std::string &url) {
  size_t npos = url.find('?', 0);
  std::unordered_map<std::string, std::string> params;

  if (npos == std::string::npos) {
    return std::make_pair(url, params);
  }
  std::string path = url.substr(0, npos);
  while (size_t qpos = url.find('&', npos + 1)) {
    auto query = url.substr(npos + 1, qpos - npos - 1);
    size_t spos = query.find('=');
    if (spos == std::string::npos) {
      throw Exception(StatusCode::BadRequest, "Malformed query");
    }
    params[query.substr(0, spos)] = query.substr(spos + 1);
    if (qpos == std::string::npos) {
      break;
    }
    npos = qpos;
  }
  return std::make_pair(path, params);
}

coro::task<RequestImpl::Ptr> ParseRequestLine(
    std::shared_ptr<tcp::socket> socket) {
  asio::streambuf buffer;
  coro::single_consumer_event event;
  async_read_until(*socket, buffer, "\r\n",
                   [&](auto ec, auto n) { event.set(); });
  co_await event;
  std::istream input(&buffer);

  auto req = std::make_shared<RequestImpl>();
  req->socket = socket;
  // Read and parse request line
  std::string requestLine;
  std::getline(input, requestLine);
  // Clean the line from trailing '\r' and whitespace
  requestLine = cleanString(requestLine);
  std::istringstream lineStream(requestLine);

  std::string methodStr, url, versionStr;
  lineStream >> methodStr >> url >> versionStr;

  if (methodStr == "GET") {
    req->method = Method::GET;
  } else if (methodStr == "POST") {
    req->method = Method::POST;
  } else {
    throw Exception(StatusCode::InternalServerError, "Unsupported method");
  }

  const auto [path, params] = ParseUrl(url);
  req->params = params;
  req->path = path;

  if (versionStr == "HTTP/1.0") {
    req->version = Version::HTTP_1_0;
  } else if (versionStr == "HTTP/1.1") {
    req->version = Version::HTTP_1_1;
  } else {
    throw Exception(StatusCode::BadRequest, "Unsupported version");
  }

  // Read and parse headers
  std::string headerLine;
  while (std::getline(input, headerLine) && !headerLine.empty()) {
    headerLine = cleanString(
        headerLine);  // Clean the line from trailing '\r' and whitespace
    if (headerLine.empty()) {
      break;
    }
    size_t colonPos = headerLine.find(':');
    if (colonPos != std::string::npos) {
      std::string headerName = headerLine.substr(0, colonPos);
      std::string headerValue =
          headerLine.substr(colonPos + 1);  // Skip ':' after colon
      req->headers[headerName] = cleanString(headerValue);
    }
  }
  req->body_part = std::string(std::istreambuf_iterator<char>(input), {});

  co_return req;
}
}  // namespace internal

Request::Request(std::shared_ptr<internal::RequestImpl> pimpl)
    : pimpl_(std::move(pimpl)) {}
Request::~Request() {}
Method Request::GetMethod() const { return pimpl_->method; }
Version Request::GetVersion() const { return pimpl_->version; }
std::string_view Request::Path() const { return pimpl_->path; }
std::optional<std::string_view> Request::Header(std::string_view name) const {
  auto it = pimpl_->headers.find(std::string(name));
  if (it != pimpl_->headers.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<std::string_view> Request::Param(std::string_view name) const {
  auto it = pimpl_->params.find(std::string(name));
  if (it != pimpl_->params.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<size_t> Request::ContentLength() const {
  auto cl = Header("Content-Length");
  if (cl) {
    return std::stoul(std::string(*cl));
  }
  return std::nullopt;
}

coro::task<std::string> Request::Body() const {
  auto cl = ContentLength();
  if (!cl) {
    throw Exception(StatusCode::BadRequest, "Missing Content-Length header");
  }
  if (pimpl_->body_part.size() >= cl.value()) {
    co_return pimpl_->body_part.substr(0, cl.value());
  }
  std::string ret;
  ret.reserve(cl.value() - pimpl_->body_part.size());
  coro::single_consumer_event event;
  asio::async_read(*pimpl_->socket,
                   asio::mutable_buffer(ret.data(), ret.size()),
                   asio::transfer_exactly(cl.value()),
                   [&](auto ec, auto n) { event.set(); });
  co_await event;

  co_return pimpl_->body_part + ret;
}
}  // namespace hs
