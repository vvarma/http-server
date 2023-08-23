// Copyright 2023 Vinay Varma; Subject to the MIT License.
#ifndef HTTP_SERVER_STATIC_ROUTES_H
#define HTTP_SERVER_STATIC_ROUTES_H
#include "http-server/enum.h"
#include "http-server/route.h"

namespace hs {

class StaticRoute : public Route {
 public:
  StaticRoute(const std::string& path, const std::string& dir);
  Method GetMethod() const override;
  std::string GetPath() const override;
  Handler::Ptr GetHandler() const override;

 private:
  std::string path_;
  std::string dir_;
};
}  // namespace hs
#endif
