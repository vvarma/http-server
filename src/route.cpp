#include "http-server/route.h"

#include <spdlog/spdlog.h>

#include "http-server/internal/route.h"

namespace hs {
Route::~Route() { spdlog::debug("destroying route"); }
void Handler::SetDone() { done = true; }
bool Handler::IsDone() { return done; }
Handler::~Handler() { spdlog::debug("destroying handler"); }
ResponseBody::~ResponseBody() { spdlog::debug("destroying response body"); }
namespace internal {

Node::Node(std::string_view path) : path_part(path) {}
RouteMatch Node::Match(const Method method,
                       std::vector<std::string> params) const {
  for (auto &route : routes) {
    if (route->GetMethod() == method) {
      return std::make_pair(route, params);
    }
  }
  return std::nullopt;
}

RouteMatch Node::Match(const Method method, std::string_view path) const {
  if (path.ends_with("/")) {
    path = path.substr(0, path.size() - 1);
  }
  std::vector<std::string> params;
  size_t pos = path.find("/");
  if (pos == std::string_view::npos) {
    return path == path_part ? Match(method, params) : std::nullopt;
  }
  auto base = path.substr(0, pos);
  if (base != path_part) return std::nullopt;
  auto rest = path.substr(pos + 1);
  for (auto &child : children) {
    auto route = child.Match(method, rest);
    if (route) {
      return route;
    }
  }
  while (pos = rest.find("/"), pos != std::string_view::npos) {
    auto param = rest.substr(0, pos);
    params.push_back(std::string(param));
    rest = rest.substr(pos + 1);
  }
  if (!rest.empty()) params.emplace_back(rest);
  return Match(method, params);
}

void Node::Add(const Route::Ptr &route, std::string_view path) {
  if (path.starts_with("/")) {
    path = path.substr(1);
  }
  if (path.ends_with("/")) {
    path = path.substr(0, path.size() - 1);
  }
  size_t pos = path.find("/");
  std::string_view base, rest;
  if (pos == std::string_view::npos) {
    if (path.empty()) {
      routes.push_back(route);
      return;
    }
    base = path;
    rest = "";
  } else {
    base = path.substr(0, pos);
    rest = path.substr(pos + 1);
  }
  for (auto &child : children) {
    if (child.path_part == base) {
      return child.Add(route, rest);
    }
  }
  Node child(base);
  child.Add(route, rest);
  children.push_back(child);
}

Router::Router() noexcept : root_("") {}
void Router::AddRoute(const Route::Ptr &route) {
  root_.Add(route, route->GetPath());
}
RouteMatch Router::Match(const RequestImpl::Ptr &request) {
  return root_.Match(request->method, request->path);
}
}  // namespace internal
}  // namespace hs
