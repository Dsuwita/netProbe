#pragma once

#include "../common.h"
#include <string>
#include <span>

namespace netprobe::commands {

int ping(std::span<const char*> args);
int trace(std::span<const char*> args);
int scan(std::span<const char*> args);
int bench(std::span<const char*> args);
int sniff(std::span<const char*> args);
int iperf(std::span<const char*> args);

} // namespace netprobe::commands
