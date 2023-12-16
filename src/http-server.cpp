// Copyright 2023 Vinay Varma; Subject to the MIT License.
#include "http-server/http-server.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read_until.hpp>
#include <asio/write.hpp>
#include <atomic>
#include <coro/single_consumer_event.hpp>
#include <coro/task.hpp>
#include <coro/when_all.hpp>
#include <exception>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include "http-server/enum.h"
#include "http-server/internal/request-impl.h"
#include "http-server/internal/route.h"
#include "http-server/route.h"

using asio::ip::tcp;

namespace hs {
namespace internal {

class Session : public std::enable_shared_from_this<Session> {
 public:
  Session(Handler::Ptr handler, RequestImpl::Ptr request)
      : handler_(handler), request_(request) {
    if (request->headers.contains("Connection")) {
      keep_alive = request->headers["Connection"] != "close";
    }
  }

  coro::task<> WriteSome(asio::const_buffer buffer) {
    asio::error_code ec;
    coro::single_consumer_event event;
    asio::async_write(*request_->socket, buffer, [&](auto ec, auto n) {
      spdlog::trace("Wrote {} bytes of {} to socket; ec:{}", n, buffer.size(),
                    ec.message());
      if (ec == asio::error::broken_pipe) {
        spdlog::trace("Got a broken pipe on write");
        handler_->SetDone();
      }
      event.set();
    });
    co_await event;
  }
  coro::task<> operator()(StatusCode statusCode) {
    co_await WriteSome(asio::buffer(fmt::format(
        "{} {:d} {:s}\r\n", request_->version, statusCode, statusCode)));
  }
  coro::task<> operator()(Headers headers) {
    if (keep_alive) {
      if (headers.contains("Connection")) {
        keep_alive = headers["Connection"] != "Close";
      } else {
        headers["Connection"] = "Keep-Alive";
      }
    } else {
      headers["Connection"] = "Close";
    }
    std::stringstream ss;
    for (auto &header : headers) {
      auto headerLine = fmt::format("{}: {}\r\n", header.first, header.second);
      ss << headerLine;
    }
    ss << "\r\n";
    std::string resp = ss.str();
    co_await WriteSome(asio::buffer(resp));
  }
  coro::task<> operator()(ResponseBody::Ptr resp) {
    co_await WriteSome(asio::buffer(resp->GetData(), resp->GetSize()));
  }

  coro::task<bool> ProcessRequest() {
    auto gen = handler_->Handle(Request(request_));
    for (auto iter = co_await gen.begin(); iter != gen.end(); co_await ++iter) {
      co_await std::visit(*this, *iter);
    }
    spdlog::info("Finished processing request");
    co_return keep_alive;
  }

 private:
  Handler::Ptr handler_;
  RequestImpl::Ptr request_;
  bool keep_alive = true;
};

coro::task<> WriteOnFail(RequestImpl::Ptr req, StatusCode statusCode) {
  std::string response = fmt::format("{} {:d} {:s}\r\nContent-Length:0\r\n\r\n",
                                     req->version, statusCode, statusCode);
  coro::single_consumer_event event;

  req->socket->async_write_some(
      asio::buffer(response), [&event](auto ec, auto n) {
        spdlog::trace("Wrote {} bytes to socket; ec:{}", n, ec.message());
        event.set();
      });
  co_await event;
}

class HttpServerImpl {
 public:
  HttpServerImpl(const Config &config) : config_(config) {}
  coro::task<bool> HandleRequest(RequestImpl::Ptr request) {
    StatusCode statusCode = StatusCode::Ok;
    bool keep_alive = true;
    try {
      auto route_match = router_.Match(request);

      if (route_match) {
        auto [route, params] = route_match.value();
        request->path_params = std::move(params);
        keep_alive =
            co_await std::make_shared<Session>(route->GetHandler(), request)
                ->ProcessRequest();
      } else {
        co_await WriteOnFail(request, StatusCode::NotFound);
      }
      co_return keep_alive;
    } catch (const Exception &e) {
      spdlog::error("Handling exception {}", e.what());
      statusCode = e.Code();
    } catch (const std::exception &e) {
      spdlog::error("Handling std exception {}", e.what());
      statusCode = StatusCode::InternalServerError;
    }
    co_await WriteOnFail(request, statusCode);
    co_return keep_alive;
  }
  coro::task<> HandleConnection(std::shared_ptr<tcp::socket> socket) {
    for (;;) {
      auto req = co_await ParseRequestLine(socket);
      if (!req) break;
      auto version = req.value()->version;
      auto keep_alive = co_await HandleRequest(std::move(req.value()));
      if (version == Version::HTTP_1_0 || !keep_alive) {
        break;
      }
    }
  }

  coro::task<std::shared_ptr<tcp::socket>> Accept(tcp::acceptor &acceptor) {
    auto socket = std::make_shared<tcp::socket>(acceptor.get_executor());
    coro::single_consumer_event event;
    spdlog::info("Waiting for connection");
    acceptor.async_accept(*socket, [&](asio::error_code ec) {
      spdlog::info("Accepted connection");
      if (ec) {
        spdlog::error("Error accepting connection: {}", ec.message());
        throw std::runtime_error("Error accepting connection");
      }
      event.set();
    });
    co_await event;
    co_return socket;
  }

  coro::task<> Listen(tcp::acceptor &acceptor) {
    auto socket = co_await Accept(acceptor);
    co_await coro::when_all(Listen(acceptor), HandleConnection(socket));
  }

  coro::task<> Serve(asio::io_context &io_context) {
    auto address = asio::ip::make_address_v4("0.0.0.0");
    spdlog::info("binding to {}:{}", config_.bind_address, config_.port);
    tcp::acceptor acceptor(io_context, {address, config_.port});
    spdlog::info("starting server at {}:{}", config_.bind_address,
                 config_.port);
    co_await Listen(acceptor);
  }
  void AddRoute(const Route::Ptr &route) { router_.AddRoute(route); }

 private:
  Router router_;
  Config config_;
  std::jthread asio_thread_;
};
}  // namespace internal

Config::Config(const std::string &program_name, const std::string &bind_address,
               uint16_t port)
    : program_name(program_name), bind_address(bind_address), port(port) {}

HttpServer::HttpServer(const Config &config)
    : pimpl_(std::make_unique<internal::HttpServerImpl>(config)) {}

coro::task<void> HttpServer::ServeAsync(asio::io_context &io_context) {
  co_await shared_from_this()->pimpl_->Serve(io_context);
}

HttpServer::~HttpServer() {}

void HttpServer::AddRoute(const Route::Ptr &route) { pimpl_->AddRoute(route); }

Exception::Exception(StatusCode statusCode, std::string message)
    : code_(statusCode), message_(message) {}

const char *Exception::what() const noexcept { return message_.c_str(); }
StatusCode Exception::Code() const { return code_; }

}  // namespace hs
