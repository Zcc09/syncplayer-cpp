// Startup tracing: a crash in window construction gives no message, so each phase records
// that it was reached. Read %TEMP%\\sp_winui.log; the last line is the phase that died.
#pragma once

#include <cstdio>
#include <string>

namespace SyncPlayer {
inline void sp_log(const char* step) {
  const char* base = std::getenv("TEMP");
  std::string path = std::string(base ? base : "C:\\Windows\\Temp") + "\\sp_winui.log";
  if (FILE* f = std::fopen(path.c_str(), "a")) {
    std::fprintf(f, "%s\n", step);
    std::fclose(f);
  }
}
}  // namespace SyncPlayer
