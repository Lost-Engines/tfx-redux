#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace tfx::resources {

std::filesystem::path default_game_directory();
std::filesystem::path select_game_archive(std::optional < std::filesystem::path > const & requested, std::optional < std::filesystem::path > const & cue = {});
std::vector<std::filesystem::path> rom_directories(std::filesystem::path const &game_directory, std::optional<std::filesystem::path> const &requested);

} // namespace tfx::resources
