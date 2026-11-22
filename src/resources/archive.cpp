#include "archive.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include "resources/ra.h"

namespace tfx::resources {

archive::archive(bytes source) : storage{std::move(source)} {
  /// Read the footer index; entries retain their original on-disc order
  binary_view const view{storage};
  auto const index_offset{view.le32(0)};
  if(index_offset < 4) throw std::runtime_error{"DID index overlaps its header"};
  auto const count{view.le16(index_offset)};
  auto const directory_count{view.le16(index_offset + 2)};
  auto const extension_count{view.le16(index_offset + 4)};
  auto const names_size{view.le16(index_offset + 6)};
  size_t cursor{index_offset + 8u};
  auto const names_end{cursor + names_size};
  view.slice(cursor, names_size + static_cast<size_t>(count) * 12);
  for(unsigned int i{0}; i != static_cast<unsigned int>(directory_count) + extension_count; ++i) {
    if(cursor >= names_end) throw std::runtime_error{"truncated DID name table"};
    auto const length{view.byte(cursor++)};
    if(length > names_end - cursor) throw std::runtime_error{"DID name exceeds table"};
    auto const raw{view.slice(cursor, length)};
    auto &names{i < directory_count ? directories : extensions};
    names.emplace_back(raw.begin(), raw.end());
    cursor += length;
  }
  if(cursor != names_end) throw std::runtime_error{"DID name table size mismatch"};
  for(unsigned int i{0}; i != count; ++i, cursor += 12) {
    archive_entry const entry{
      .id{view.le32(cursor)},
      .offset{view.le32(cursor + 4)},
      .size{view.le32(cursor + 8)},
    };
    if(entry.offset < 4 || entry.offset > index_offset || entry.size > index_offset - entry.offset) throw std::runtime_error{"DID entry exceeds resource area"};
    if(!lookup.emplace(entry.id, index.size()).second) throw std::runtime_error{"DID contains an ambiguous duplicate identifier"};
    index.push_back(entry);
  }
}

archive::archive(std::filesystem::path const &path) : archive{read_file(path)} {
  directory = std::filesystem::absolute(path).parent_path();
}

std::span<archive_entry const> archive::entries() const {
  return index;
}

resource_id archive::hash_name(std::string_view const name) const {
  /// Reproduce native directory/extension substitution before byte-wise hashing (2.05 file 0xc8af0)
  std::string short_name{name};
  for(auto &c : short_name) c = c == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for(size_t i{0}; i != directories.size(); ++i) {
    if(short_name.starts_with(directories[i])) {
      short_name = static_cast<char>('0' + i) + short_name.substr(directories[i].size());
      break;
    }
  }
  auto const dot{short_name.find('.')};
  if(dot != std::string::npos) {
    auto const extension{std::ranges::find(extensions, short_name.substr(dot))};
    if(extension == extensions.end()) short_name.erase(dot, 1);
    else short_name = short_name.substr(0, dot) + static_cast<char>('A' + std::distance(extensions.begin(), extension));
  }
  return hash_short_name(short_name);
}

std::optional<std::filesystem::path> archive::loose_file(std::string_view const name) const {
  if(!directory) return {};
  auto const lower{[](std::string value){
    for(auto &byte : value) byte = static_cast<char>(std::tolower(static_cast<unsigned char>(byte)));
    return value;
  }};
  std::string relative{name};
  std::ranges::replace(relative,'\\','/');
  std::filesystem::path const path{relative};
  if(path.is_absolute()) return {};
  auto selected{*directory};
  for(auto const &part : path) {
    auto component{part.string()};
    if(component == ".") continue;
    if(component == "..") return {};
    // DOS ignores trailing spaces/dots in an 8.3 path component. In
    // particular the launcher's literal "tunes\\intro.rld " needs this.
    while(!component.empty() && (component.back() == ' ' || component.back() == '.')) component.pop_back();
    if(component.empty()) return {};
    auto candidate{selected / component};
    if(!std::filesystem::exists(candidate)) {
      if(!std::filesystem::is_directory(selected)) return {};
      bool found{false};
      for(auto const &entry : std::filesystem::directory_iterator{selected}) if(lower(entry.path().filename().string()) == lower(component)) {
        candidate = entry.path();found = true;break;
      }
      if(!found) return {};
    }
    selected = std::move(candidate);
  }
  return std::filesystem::is_regular_file(selected) ? std::optional{selected} : std::nullopt;
}
bool archive::contains(std::string_view const name) const {
  return lookup.contains(hash_name(name)) || loose_file(name).has_value();
}

bytes archive::read(resource_id const id) const {
  /// Decode only the requested resource; retained buffers own their bytes
  auto const found{lookup.find(id)};
  if(found == lookup.end()) {
    std::ostringstream message;
    message << "DID resource not found: 0x" << std::hex << id;
    throw std::runtime_error{message.str()};
  }
  auto const &entry{index[found->second]};
  auto const raw{binary_view{storage}.slice(entry.offset, entry.size)};
  if(is_ra(raw)) return decode_ra(raw);
  return {raw.begin(), raw.end()};
}

bytes archive::read(std::string_view const name) const {
  try {
    auto const id{hash_name(name)};
    if(lookup.contains(id)) return read(id);
    // Native 0xc8bec/0xc8cf0 prefer the indexed archive, then try a loose
    // DOS file. Both paths accept the same RA-compressed resource format.
    if(auto const file{loose_file(name)}) {
      auto bytes{read_file(*file)};
      return is_ra(bytes) ? decode_ra(bytes) : std::move(bytes);
    }
    return read(id);
  } catch(std::exception const &error) {
    throw std::runtime_error{std::string{name} + ": " + error.what()};
  }
}

resource_id hash_short_name(std::string_view const name) {
  /// Preserve byte wrapping and generated uppercase directory/extension codes (2.05 file 0xd6484)
  uint8_t total{0}, accumulated{0}, shifted{0}, rising{0}, previous{0};
  for(size_t i{0}; i != name.size(); ++i) {
    auto const c{static_cast<uint8_t>(name[i])};
    if(i && previous != '_') rising = static_cast<uint8_t>((rising << 1) | (previous < c));
    previous = c;
    if(c == '_') shifted = static_cast<uint8_t>(shifted - i);
    else {
      total = static_cast<uint8_t>(total + c);
      shifted = static_cast<uint8_t>((shifted << 1) ^ c);
      accumulated = static_cast<uint8_t>(accumulated + total);
    }
  }
  return (static_cast<uint32_t>(name.size() & 15) << 28) | ((total & 15u) << 24) | (accumulated << 16) | (shifted << 8) | rising;
}

std::filesystem::path find_game_archive(std::filesystem::path const &directory) {
  /// Accept either an installed directory or DID.DAT itself, including lower-case copies on Unix
  if(std::filesystem::is_regular_file(directory)) return directory;
  for(auto const *name : {"DID.DAT", "did.dat"}) {
    auto const path{directory / name};
    if(std::filesystem::is_regular_file(path)) return path;
  }
  throw std::runtime_error{"TFX needs the original game files. Run scripts/fetch-assets.sh (Windows: scripts\\fetch-assets.bat) to fetch the reference CD and ROMs, or use --data-dir /path/to/TFX to select an installation containing DID.DAT (TFX 2.05 is the reference version). Searched: " + directory.string()};
}

} // namespace tfx::resources
