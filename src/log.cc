#include <log.h>

namespace rthandle {
void log_init() {
  spdlog::set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%l] [%!(%s#%#)] %v");
  spdlog::set_level(spdlog::level::trace);
}
}  // namespace rthandle
