#pragma once
#include <filesystem>
#include <string>

namespace NativeAgentHooks {
enum class Preset {Codex,ClaudeCode,Cursor};
std::filesystem::path home(Preset preset=Preset::Codex);
void connect(const std::filesystem::path& home,const std::filesystem::path& executable,Preset preset=Preset::Codex);
std::wstring notificationExample(const std::filesystem::path& executable);
}
