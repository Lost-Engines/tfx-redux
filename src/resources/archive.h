#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "resources/binary.h"

namespace tfx::resources {

using resource_id = uint32_t;
struct archive_entry {
  resource_id id;
  uint32_t offset;
  uint32_t size;
};

class archive {
private:
  bytes storage;
  std::optional<std::filesystem::path> directory;
  std::optional<std::filesystem::path> loose_file(std::string_view name) const;
  std::vector<std::string> directories;
  std::vector<std::string> extensions;
  std::vector<archive_entry> index;
  std::unordered_map<resource_id, size_t> lookup;

public:
  archive(bytes source);
  archive(std::filesystem::path const &path);
  std::span<archive_entry const> entries() const;
  resource_id hash_name(std::string_view name) const;
  bool contains(std::string_view name) const;
  bytes read(resource_id id) const;
  bytes read(std::string_view name) const;
};

resource_id hash_short_name(std::string_view name);
std::filesystem::path find_game_archive(std::filesystem::path const &directory);

} // namespace tfx::resources
