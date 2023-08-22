// Copyright 2023 Vinay Varma; Subject to the MIT License.
#ifndef HTTP_SERVER_INTERNAL_ROUTE_H
#define HTTP_SERVER_INTERNAL_ROUTE_H
#include <string_view>
#include <vector>

#include "http-server/internal/request-impl.h"
#include "http-server/route.h"
namespace hs::internal {

typedef std::optional<std::pair<Route::Ptr, std::vector<std::string>>>
    RouteMatch;

struct Node {
  std::string path_part;
  std::vector<Node> children;
  std::vector<Route::Ptr> routes;
  Node(std::string_view path);
  RouteMatch Match(const Method method, std::vector<std::string> params) const;
  RouteMatch Match(const Method method, std::string_view path) const;
  void Add(const Route::Ptr &route, std::string_view path);
};

class Router {
 public:
  Router() noexcept;
  void AddRoute(const Route::Ptr &route);
  RouteMatch Match(const RequestImpl::Ptr &request);

 private:
  Node root_;
};
}  // namespace hs::internal
#endif
