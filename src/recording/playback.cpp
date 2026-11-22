#include "playback.h"
#include <algorithm>
#include <limits>

namespace tfx::recording {
namespace {
std::string shape_name(std::string name) {
  std::ranges::replace(name, '\\', '/');
  if(auto const slash{name.find_last_of('/')}; slash != std::string::npos) name.erase(0, slash + 1);
  for(auto &character : name) {
    if(character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
  }
  if(name.ends_with(".3")) name.resize(name.size() - 2);
  return name;
}
}

playback::playback(std::filesystem::path const &path)
  : input{},
    header{},
    remaining{0} {
  input.exceptions(std::ios::badbit | std::ios::failbit);
  input.open(path, std::ios::binary | std::ios::ate);
  auto const size{static_cast<std::streamoff>(input.tellg())};
  if(size < header_bytes || static_cast<uint64_t>(size) > std::numeric_limits<uint32_t>::max()) throw std::runtime_error{"invalid flight recording length"};
  input.seekg(0);
  std::array<uint8_t, header_bytes> bytes{};
  input.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
  header = decode_header(bytes);
  if(header.size != size || header.dictionary < header_bytes || header.dictionary > header.size) throw std::runtime_error{"flight recording has invalid file offsets"};
  auto const dictionary_bytes{header.size - header.dictionary};
  // C5DAC writes each name with a word-sized length. Bound allocation even
  // for a damaged dictionary offset, before reading its entry count.
  unsigned int constexpr maximum_dictionary_bytes{native_resource_limit * (std::numeric_limits<uint16_t>::max() + 2u) + 6u};
  if(dictionary_bytes > maximum_dictionary_bytes) throw std::runtime_error{"flight recording resource dictionary is too large"};
  resources::bytes dictionary(dictionary_bytes);
  input.seekg(header.dictionary);
  input.read(reinterpret_cast<char *>(dictionary.data()), static_cast<std::streamsize>(dictionary.size()));
  for(auto const &resource : decode_resources(dictionary)) names[resource.handle] = shape_name(resource.name);
  remaining = header.frames;
  if(!remaining && position != header.dictionary) throw std::runtime_error{"empty flight recording has frame data"};
}
uint16_t playback::frame_count() const {
  return header.frames;
}
std::optional<scene_frame> playback::next() {
  if(!remaining) return std::nullopt;
  auto const count{std::min(maximum_frame_bytes, header.dictionary - position)};
  input.seekg(position);
  input.read(reinterpret_cast<char *>(buffer.data()), count);
  auto decoded{decode_frame(std::span{buffer}.first(count))};
  position += decoded.bytes;
  --remaining;
  if(!remaining && position != header.dictionary) throw std::runtime_error{"flight recording has trailing frame data"};
  scene_frame result{
    .data{std::move(decoded.value)},
    .model_names{},
  };
  result.model_names.reserve(result.data.models.size());
  for(auto const &model : result.data.models) {
    auto const found{names.find(model.resource)};
    result.model_names.push_back(found == names.end() ? "badshape" : found->second);
  }
  return std::optional<scene_frame>{std::in_place, std::move(result)};
}

} // namespace tfx::recording
