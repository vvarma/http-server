// Copyright 2023 Vinay Varma; Subject to the MIT License.
#include "http-server/response-writer.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <asio/ip/tcp.hpp>
using asio::awaitable;
using asio::ip::tcp;

#if defined(ASIO_ENABLE_HANDLER_TRACKING)
#define use_awaitable \
  asio::use_awaitable_t(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#else
using asio::use_awaitable;
#endif

namespace hs {
ResponseWriter::ResponseWriter(Version version,
                               std::shared_ptr<tcp::socket> socket)
    : version_(version), socket_(socket) {}

void ResponseWriter::SetStatusCode(StatusCode code) { statusCode = code; }

void ResponseWriter::SetHeader(std::string_view name, std::string_view value) {
  headers[std::string(name)] = std::string(value);
}
void ResponseWriter::SetContentType(std::string_view contentType) {
  SetHeader("Content-Type", contentType);
}
void ResponseWriter::SetContentLength(size_t contentLength) {
  SetHeader("Content-Length", std::to_string(contentLength));
}
bool ResponseWriter::IsOpen() const { return socket_->is_open(); }

awaitable<void> ResponseWriter::WriteHeaders() {
  if (!headersSent) headersSent = true;
  std::stringstream ss;
  ss << fmt::format("{} {:d} {:s}\r\n", version_, statusCode, statusCode);
  for (auto &header : headers) {
    auto headerLine = fmt::format("{}: {}\r\n", header.first, header.second);
    ss << headerLine;
  }
  ss << "\r\n";
  std::string resp = ss.str();
  spdlog::debug("Response: {}", resp);
  co_await socket_->async_write_some(asio::buffer(resp), use_awaitable);
}
}  // namespace hs
