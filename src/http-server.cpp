// Copyright 2023 Vinay Varma; Subject to the MIT License.
#include "http-server/http-server.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/error.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read_until.hpp>
#include <asio/use_awaitable.hpp>
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <sstream>
#include <unordered_map>

#include "http-server/enum.h"
#include "http-server/route.h"

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::ip::tcp;

#if defined(ASIO_ENABLE_HANDLER_TRACKING)
#define use_awaitable \
  asio::use_awaitable_t(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#else
using asio::use_awaitable;
#endif

constexpr char kHeaderServer[] = "Server";
constexpr char kHeaderConnection[] = "Connection";
constexpr char kHeaderValueClose[] = "close";

namespace hs {
namespace internal {
struct RequestImpl {
  typedef std::shared_ptr<RequestImpl> Ptr;
  Method method;
  Version version;
  std::string path;
  std::unordered_map<std::string, std::string> headers;
  std::unordered_map<std::string, std::string> params;
  std::shared_ptr<tcp::socket> socket;
};
class Router {
 public:
  void AddRoute(const Route::Ptr &route) { routes_.push_back(route); }
  std::optional<Route::Ptr> Match(const RequestImpl::Ptr &request) {
    for (auto route : routes_) {
      if (route->GetMethod() == request->method &&
          route->GetPath() == request->path) {
        return route;
      }
    }
    return std::nullopt;
  }

 private:
  std::vector<Route::Ptr> routes_;
};

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

awaitable<RequestImpl::Ptr> ParseRequestLine(
    std::shared_ptr<tcp::socket> socket) {
  asio::streambuf buffer;
  co_await async_read_until(*socket, buffer, "\r\n", use_awaitable);
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
    size_t colonPos = headerLine.find(':');
    if (colonPos != std::string::npos) {
      std::string headerName = headerLine.substr(0, colonPos);
      std::string headerValue =
          headerLine.substr(colonPos + 1);  // Skip ':' after colon
      req->headers[headerName] = cleanString(headerValue);
    }
  }
  co_return req;
}
class Session : public std::enable_shared_from_this<Session> {
 public:
  Session(Handler::Ptr handler, RequestImpl::Ptr request)
      : handler_(handler), request_(request) {}

  awaitable<void> operator()(StatusCode statusCode) {
    asio::error_code ec;
    size_t n = co_await request_->socket->async_write_some(
        asio::buffer(fmt::format("{} {:d} {:s}\r\n", request_->version,
                                 statusCode, statusCode)),
        asio::redirect_error(use_awaitable, ec));
    spdlog::trace("Wrote statusCode{}; {} bytes to socket; ec:{}", statusCode,
                  n, ec.message());
    if (ec == asio::error::broken_pipe) {
      spdlog::trace("Got a broken pipe on write statuscode");
      handler_->SetDone();
    }
  }
  awaitable<void> operator()(Headers headers) {
    std::stringstream ss;
    for (auto &header : headers) {
      auto headerLine = fmt::format("{}: {}\r\n", header.first, header.second);
      ss << headerLine;
    }
    ss << "\r\n";
    std::string resp = ss.str();
    asio::error_code ec;
    size_t n = co_await request_->socket->async_write_some(
        asio::buffer(resp), asio::redirect_error(use_awaitable, ec));
    spdlog::trace("Wrote headers; {} bytes to socket; ec:{}", n, ec.message());
    if (ec == asio::error::broken_pipe) {
      spdlog::trace("Got a broken pipe on write header");
      handler_->SetDone();
    }
  }
  awaitable<void> operator()(ResponseBody::Ptr resp) {
    asio::error_code ec;
    size_t n = co_await request_->socket->async_write_some(
        asio::buffer(resp->GetData(), resp->GetSize()),
        asio::redirect_error(use_awaitable, ec));
    spdlog::trace("Wrote {} bytes to socket; ec:{}", n, ec.message());
    if (ec == asio::error::broken_pipe) {
      spdlog::trace("Got a broken pipe on write body");
      handler_->SetDone();
    }
  }

  awaitable<void> ProcessRequest() {
    auto gen = handler_->Handle(Request(request_));
    while (gen) {
      auto resp = gen();
      co_await std::visit(*this, resp);
    }
    spdlog::info("Finished processing request");
  }

 private:
  Handler::Ptr handler_;
  RequestImpl::Ptr request_;
};

awaitable<void> WriteOnFail(RequestImpl::Ptr req, StatusCode statusCode) {
  std::string response = fmt::format("{} {:d} {:s}\r\nContent-Length:0\r\n\r\n",
                                     req->version, statusCode, statusCode);
  co_await req->socket->async_write_some(asio::buffer(response), use_awaitable);
}

class HttpServerImpl {
 public:
  HttpServerImpl(const Config &config) : config_(config) {}
  awaitable<void> HandleRequest(RequestImpl::Ptr request) {
    auto route = router_.Match(request);

    if (route) {
      co_await std::make_shared<Session>(route.value()->GetHandler(), request)
          ->ProcessRequest();
    } else {
      co_await WriteOnFail(request, StatusCode::NotFound);
    }
  }
  awaitable<void> HandleConnection(std::shared_ptr<tcp::socket> socket) {
    for (;;) {
      auto req = co_await ParseRequestLine(socket);
      // if (req->version == Version::HTTP_1_1) {
      //   std::string line = "HTTP/1.1 100 Continue\r\n";
      //   co_await socket->async_write_some(asio::buffer(line), use_awaitable);
      // }
      auto version = req->version;
      co_await HandleRequest(std::move(req));
      if (version == Version::HTTP_1_0) {
        break;
      }
      // todo figure when to break
    }
  }
  awaitable<void> Serve() {
    auto executor = co_await asio::this_coro::executor;
    auto address = asio::ip::make_address_v4("0.0.0.0");
    spdlog::info("binding to {}:{}", config_.bind_address, config_.port);
    tcp::acceptor acceptor(executor, {address, config_.port});
    spdlog::info("starting server at {}:{}", config_.bind_address,
                 config_.port);
    for (;;) {
      auto socket = std::make_shared<tcp::socket>(executor);
      co_await acceptor.async_accept(*socket, use_awaitable);
      co_spawn(executor, HandleConnection(std::move(socket)), detached);
    }
  }
  void AddRoute(const Route::Ptr &route) { router_.AddRoute(route); }

 private:
  Router router_;
  Config config_;
};
}  // namespace internal

Config::Config(const std::string &program_name, const std::string &bind_address,
               uint16_t port)
    : program_name(program_name), bind_address(bind_address), port(port) {}

HttpServer::HttpServer(const Config &config)
    : pimpl_(std::make_unique<internal::HttpServerImpl>(config)) {}

void HttpServer::Serve() {
  asio::io_context io_context;
  co_spawn(io_context, pimpl_->Serve(), asio::detached);
  io_context.run();
}

awaitable<void> HttpServer::ServeAsync() {
  co_await shared_from_this()->pimpl_->Serve();
}

HttpServer::~HttpServer() {}

void HttpServer::AddRoute(const Route::Ptr &route) { pimpl_->AddRoute(route); }

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

Exception::Exception(StatusCode statusCode, std::string message)
    : code_(statusCode), message_(message) {}

const char *Exception::what() const noexcept { return message_.c_str(); }

}  // namespace hs
