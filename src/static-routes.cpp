#include "http-server/static-routes.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>

#include "coro/async_generator.hpp"
#include "http-server/enum.h"
#include "http-server/http-server.h"
#include "http-server/route.h"
namespace hs {

std::string_view GetContentType(std::string_view filename) {
  if (filename.ends_with(".html")) return "text/html";
  if (filename.ends_with(".css")) return "text/css";
  if (filename.ends_with(".js")) return "application/javascript";
  if (filename.ends_with(".png")) return "image/png";
  return "text/plain";
}

class StaticRouteHandler : public Handler {
 public:
  StaticRouteHandler(const std::string& dir) : dir_(dir) {}
  std::string CheckFile(const std::vector<std::string>& params) {
    if (params.empty()) {
      throw Exception(StatusCode::BadRequest, "no resource requested");
    }
    std::stringstream ss;
    ss << dir_;
    for (auto& p : params) ss << "/" << p;
    auto filename = ss.str();
    auto status = std::filesystem::status(filename);
    if (status.type() != std::filesystem::file_type::regular) {
      throw Exception(StatusCode::NotFound,
                      fmt::format("{} not found", filename));
    }
    return filename;
  }
  coro::async_generator<Response> Handle(const Request req) override {
    auto filename = CheckFile(req.Params());
    auto size = std::filesystem::file_size(filename);
    co_yield StatusCode::Ok;
    hs::Headers headers{
        {"Content-Type", std::string(GetContentType(filename))},
        {"Content-Length", fmt::format("{}", size)},
    };
    co_yield headers;
    std::ifstream ifs(filename);
    std::stringstream ss;
    ss<<ifs.rdbuf();
    std::string ret = ss.str();
    co_yield std::make_shared<WritableResponseBody<std::string>>(
        std::move(ret));
  }

 private:
  std::string dir_;
};

StaticRoute::StaticRoute(const std::string& path, const std::string& dir)
    : path_(path), dir_(dir) {}

Method StaticRoute::GetMethod() const { return Method::GET; }
std::string StaticRoute::GetPath() const { return path_; }
Handler::Ptr StaticRoute::GetHandler() const {
  return std::make_shared<StaticRouteHandler>(dir_);
}

}  // namespace hs
