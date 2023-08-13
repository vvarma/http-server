#include "http-server/route.h"

#include <spdlog/spdlog.h>

namespace hs {
Route::~Route() { spdlog::debug("destroying route"); }
void Handler::SetDone() { done = true; }
bool Handler::IsDone() { return done; }
Handler::~Handler() { spdlog::debug("destroying handler"); }
ResponseBody::~ResponseBody() { spdlog::debug("destroying response body"); }
}  // namespace hs
