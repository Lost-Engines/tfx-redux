#pragma once

#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <vector>

namespace tfx::resources {

using bytes = std::vector<uint8_t>;

inline bytes read_file(std::filesystem::path const &path) {
  /// Read an external game resource without depending on host endianness
  std::ifstream stream{path, std::ios::binary | std::ios::ate};
  if(!stream) throw std::runtime_error{"cannot open " + path.string()};
  auto const size{stream.tellg()};
  if(size < 0) throw std::runtime_error{"cannot determine file size: " + path.string()};
  bytes result(static_cast<size_t>(size));
  stream.seekg(0);
  if(!stream.read(reinterpret_cast<char *>(result.data()), size)) throw std::runtime_error{"cannot read " + path.string()};
  return result;
}

class binary_view {
private:
  std::span<uint8_t const> source;

public:
  binary_view(std::span<uint8_t const> source) : source{source} {
  }

  std::span<uint8_t const> slice(size_t offset, size_t count) const {
    /// Check subtraction rather than overflow-prone offset plus count
    if(offset > source.size() || count > source.size() - offset) throw std::runtime_error{"resource field exceeds its buffer"};
    return source.subspan(offset, count);
  }
  uint8_t byte(size_t const offset) const {
    return slice(offset, 1)[0];
  }
  uint16_t le16(size_t const offset) const {
    auto const b{slice(offset, 2)};
    return static_cast<uint16_t>(b[0] | (b[1] << 8));
  }
  uint32_t le32(size_t const offset) const {
    return static_cast<uint32_t>(le16(offset)) | (static_cast<uint32_t>(le16(offset + 2)) << 16);
  }
  uint16_t be16(size_t const offset) const {
    auto const b{slice(offset, 2)};
    return static_cast<uint16_t>((b[0] << 8) | b[1]);
  }
  int16_t signed_be16(size_t const offset) const {
    return std::bit_cast<int16_t>(be16(offset));
  }
  int16_t signed_le16(size_t const offset) const {
    return std::bit_cast<int16_t>(le16(offset));
  }
};

} // namespace tfx::resources
