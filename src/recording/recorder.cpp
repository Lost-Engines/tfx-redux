#include "recorder.h"
#include <limits>

namespace tfx::recording {
namespace {
void write(std::ofstream &output, std::span<uint8_t const> const bytes) {
  output.write(reinterpret_cast<char const *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}
uint32_t position(std::ofstream &output) {
  auto const at{static_cast<std::streamoff>(output.tellp())};
  if(at < 0 || static_cast<uint64_t>(at) > std::numeric_limits<uint32_t>::max()) throw std::runtime_error{"flight recording exceeds its native file offsets"};
  return static_cast<uint32_t>(at);
}
}

recorder::recorder() {
  output.exceptions(std::ios::badbit | std::ios::failbit);
}
recorder::~recorder() {
  try {
    finish();
  } catch(...) {}
}
bool recorder::active() const {
  return output.is_open();
}
uint16_t recorder::frame_count() const {
  return frames;
}
void recorder::begin(std::filesystem::path const &path) {
  finish();
  output.clear();
  dictionary = {};
  handles.clear();
  frames = 0;
  output.open(path, std::ios::binary | std::ios::trunc);
  std::array<uint8_t, header_bytes> const header{};
  write(output, header);
}
void recorder::append(scene_frame &source) {
  if(!active()) throw std::runtime_error{"flight recorder is not open"};
  if(source.model_names.size() != source.data.models.size()) throw std::runtime_error{"recorded scene has an incomplete resource-name binding"};
  for(unsigned int index{0}; index != source.data.models.size(); ++index) {
    auto const &name{source.model_names[index]};
    auto found{handles.find(name)};
    if(found == handles.end()) {
      if(dictionary.resources.size() == native_resource_limit) throw std::runtime_error{"flight recording exceeds its native resource table"};
      auto const id{static_cast<uint16_t>(dictionary.resources.size())};
      dictionary.resources.push_back({
        .handle{id},
        .name{name},
      });
      found = handles.emplace(name, id).first;
    }
    source.data.models[index].resource = found->second;
  }
  auto const encoded{encode_frame(source.data)};
  write(output, encoded);
  // Native C5BEC wraps this word and makes an overlong recording unplayable.
  // Close the complete file at its last representable frame instead.
  if(++frames == std::numeric_limits<uint16_t>::max()) finish();
}
void recorder::finish() {
  if(!active()) return;
  auto const trailer{position(output)};
  auto const encoded{encode(dictionary)};
  write(output, std::span{encoded}.subspan(header_bytes));
  auto const size{position(output)};
  std::array<uint8_t, header_bytes> header{};
  header[0] = static_cast<uint8_t>(frames);
  header[1] = static_cast<uint8_t>(frames >> 8);
  for(unsigned int index{0}; index != 4; ++index) {
    header[2 + index] = static_cast<uint8_t>(size >> (index * 8));
    header[6 + index] = static_cast<uint8_t>(trailer >> (index * 8));
  }
  output.seekp(0);
  write(output, header);
  output.close();
}

} // namespace tfx::recording
