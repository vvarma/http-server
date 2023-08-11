#include "http-server/http-server.h"
int main(int argc, char *argv[]) {

  hs::HttpServer server;
  server.AddRoute(
      hs::Route(hs::Method::GET, "/",
                [](hs::Request req, hs::ResponseWriter::Ptr res) -> void {
                  res->SetHeader("Content-Type", "text/plain");
                  auto resp = fmt::format("Hello, {}!",
                                          req.Param("name").value_or("world"));
                  res->Write(resp);
                }));
  server.Serve();
  return 0;
}
