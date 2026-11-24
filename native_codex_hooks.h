#pragma once
#include <filesystem>

namespace NativeCodexHooks {
std::filesystem::path home();
void connect(const std::filesystem::path& home,const std::filesystem::path& executable);
}
