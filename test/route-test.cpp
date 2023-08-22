#include "http-server/internal/route.h"

#include <doctest/doctest.h>

#include "http-server/internal/request-impl.h"

struct TestHandler : public hs::Handler {
  coro::async_generator<hs::Response> Handle(const hs::Request req) override {
    co_yield hs::StatusCode::Ok;
  }
};
ROUTE(TestRoute, hs::Method::GET, "/api/v1/users",
      []() { return std::make_shared<TestHandler>(); });
ROUTE(TestRouteChild, hs::Method::GET, "/api/v1/users/address",
      []() { return std::make_shared<TestHandler>(); });

TEST_SUITE_BEGIN("routes");
TEST_CASE("route") {
  TestRoute route;
  CHECK(route.GetMethod() == hs::Method::GET);
  CHECK(route.GetPath() == "/api/v1/users");
}
TEST_CASE("router") {
  hs::internal::Router router;
  auto request = std::make_shared<hs::internal::RequestImpl>();
  request->method = hs::Method::GET;
  request->path = "/api/v1/users";
  SUBCASE("test empty router") { CHECK(router.Match(request) == std::nullopt); }
  SUBCASE("non empty router") {
    auto test_route = std::make_shared<TestRoute>();
    auto test_route_child = std::make_shared<TestRouteChild>();
    router.AddRoute(test_route);
    router.AddRoute(test_route_child);
    SUBCASE("no params") {
      auto match = router.Match(request);
      REQUIRE(match != std::nullopt);
      auto [route, params] = match.value();
      CHECK(router.Match(request) != std::nullopt);
      CHECK(route->GetPath() == "/api/v1/users");
      REQUIRE(params.empty());
    }
    SUBCASE("single param") {
      request->path = "/api/v1/users/1";
      auto match = router.Match(request);
      REQUIRE(match != std::nullopt);
      auto [route, params] = match.value();
      CHECK(route->GetPath() == "/api/v1/users");
      REQUIRE(params.size() == 1);
      CHECK(params[0] == "1");
    }
    SUBCASE("multiple params") {
      request->path = "/api/v1/users/1/abc/def";
      auto match = router.Match(request);
      REQUIRE(match != std::nullopt);
      auto [route, params] = match.value();
      CHECK(route->GetPath() == "/api/v1/users");
      REQUIRE(params.size() == 3);
      CHECK(params[0] == "1");
      CHECK(params[1] == "abc");
      CHECK(params[2] == "def");
    }
    SUBCASE("no route - unregistered parent") {
      request->path = "/api/v1";
      auto match = router.Match(request);
      REQUIRE(match == std::nullopt);
    }
    SUBCASE("greedy child - prefer leaves") {
      request->path = "/api/v1/users/address";
      auto match = router.Match(request);
      REQUIRE(match != std::nullopt);
      auto [route, params] = match.value();
      CHECK(route->GetPath() == "/api/v1/users/address");
      REQUIRE(params.empty());
    }
  }
}
TEST_SUITE_END();
