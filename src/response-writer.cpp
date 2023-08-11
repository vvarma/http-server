#include "http-server/response-writer.h"
#include <asio/ip/tcp.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
using asio::awaitable;
using asio::ip::tcp;

#if defined(ASIO_ENABLE_HANDLER_TRACKING)
#define use_awaitable                                                          \
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

awaitable<void> ResponseWriter::WriteHeaders(size_t contentLength) {
  std::stringstream ss;
  ss << fmt::format("{} {:d} {:s}\n", version_, statusCode, statusCode);
  for (auto &header : headers) {
    auto headerLine = fmt::format("{}: {}\n", header.first, header.second);
    spdlog::info("Sending header: {}", headerLine);
    ss << headerLine;
  }
  ss << fmt::format("Content-Length: {:d}\n", contentLength);
  ss << "\r\n";
  std::string resp = ss.str();
  co_await socket_->async_write_some(asio::buffer(resp), use_awaitable);
}
} // namespace hs
