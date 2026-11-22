#include "installation.h"
#include <cstdlib>
#include <stdexcept>
#include <string>
#include "archive.h"
#include "cd_image.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#include <windows.h>
#endif // _WIN32

namespace tfx::resources {

std::filesystem::path default_game_directory() {
#ifdef _WIN32
  // Match the Windows fetch helper: assets live beside the packaged executable,
  // including when a shortcut or PATH launch has a different working directory.
  unsigned int constexpr maximum_path_characters{32768};
  std::wstring filename(maximum_path_characters,L'\0');
  auto const size{GetModuleFileNameW(nullptr,filename.data(),maximum_path_characters)};
  if(!size || size == maximum_path_characters) throw std::runtime_error{"cannot locate the TFX executable directory"};
  filename.resize(size);
  return std::filesystem::path{filename}.parent_path();
#else
  if(auto const *data{std::getenv("XDG_DATA_HOME")}; data && *data && std::filesystem::path{data}.is_absolute()) return std::filesystem::path{data} / "tfx";
  auto const *home{std::getenv("HOME")};
  return std::filesystem::path{home && *home ? home : "."} / ".local/share/tfx";
#endif // _WIN32
}

std::filesystem::path select_game_archive(std::optional<std::filesystem::path> const &requested, std::optional<std::filesystem::path> const &media) {
  auto const prepared{[&](std::filesystem::path const &directory){
    bool const installed{std::filesystem::is_regular_file(directory / "DID.DAT") || std::filesystem::is_regular_file(directory / "did.dat")};
    auto const managed{directory / "cd" / "tfx.cue"};
    if(std::filesystem::is_regular_file(managed)) install_reference_cd(managed,directory);
    else if(!installed && media && std::filesystem::is_regular_file(*media)) install_reference_cd(*media,directory);
    return find_game_archive(directory);
  }};
  if(requested) return prepared(*requested);
  for(auto const *name : {"DID.DAT","did.dat"}) if(std::filesystem::is_regular_file(name)) return find_game_archive(".");
  return prepared(default_game_directory());
}

std::vector<std::filesystem::path> rom_directories(std::filesystem::path const &game_directory, std::optional<std::filesystem::path> const &requested) {
  if(requested) return {*requested};
  std::vector<std::filesystem::path> result{game_directory};
  auto const fetched{default_game_directory()};
  std::error_code error;
  if(std::filesystem::absolute(game_directory).lexically_normal() != std::filesystem::absolute(fetched).lexically_normal()
    && !std::filesystem::equivalent(game_directory,fetched,error)) result.push_back(fetched);
  return result;
}

} // namespace tfx::resources
