#include "file.h"
#include <algorithm>
#include <limits>
#include <utility>

namespace tfx::recording {
namespace {

class writer {
public:
  resources::bytes bytes;
  void byte(uint8_t const value) {
    bytes.push_back(value);
  }
  void word(uint16_t const value) {
    byte(static_cast<uint8_t>(value));
    byte(static_cast<uint8_t>(value >> 8));
  }
  void dword(uint32_t const value) {
    word(static_cast<uint16_t>(value));
    word(static_cast<uint16_t>(value >> 16));
  }
  void raw(std::span<uint8_t const> const source) {
    bytes.insert(bytes.end(), source.begin(), source.end());
  }
  void patch_dword(unsigned int const at, uint32_t const value) {
    for(unsigned int index{0}; index != 4; ++index) bytes.at(at + index) = static_cast<uint8_t>(value >> (index * 8));
  }
  uint32_t position() const {
    if(bytes.size() > std::numeric_limits<uint32_t>::max()) throw std::runtime_error{"flight recording exceeds its native file offsets"};
    return static_cast<uint32_t>(bytes.size());
  }
  void packed(std::span<uint8_t const> const source, uint8_t const fill) {
    // Native C5958: pairs of fill-run and literal-run lengths, each capped
    // at 255. Even a final fill-only run has a zero literal count.
    writer block;
    size_t at{0};
    while(at != source.size()) {
      unsigned int count{0};
      while(at != source.size() && source[at] == fill && count != 255) {
        ++at;
        ++count;
      }
      block.byte(static_cast<uint8_t>(count));
      auto const first{at};
      count = 0;
      while(at != source.size() && source[at] != fill && count != 255) {
        ++at;
        ++count;
      }
      block.byte(static_cast<uint8_t>(count));
      block.raw(source.subspan(first, count));
    }
    if(block.bytes.size() > compression_buffer_bytes) throw std::runtime_error{"flight recording exceeds the original compression buffer"};
    word(static_cast<uint16_t>(block.bytes.size()));
    raw(block.bytes);
  }
};

class reader {
private:
  resources::binary_view source;
  size_t size;
public:
  size_t position{0};
  explicit reader(std::span<uint8_t const> const bytes)
    : source{bytes},
      size{bytes.size()} {}
  auto raw(size_t const count)->std::span<uint8_t const> {
    auto const result{source.slice(position, count)};
    position += count;
    return result;
  }
  uint8_t byte() {
    return raw(1).front();
  }
  uint16_t word() {
    auto const value{source.le16(position)};
    position += 2;
    return value;
  }
  uint32_t dword() {
    auto const value{source.le32(position)};
    position += 4;
    return value;
  }
  bool complete() const {
    return position == size;
  }
  resources::bytes packed(size_t const count, uint8_t const fill) {
    auto const size{word()};
    if(size > compression_buffer_bytes) throw std::runtime_error{"flight recording compressed block exceeds its native buffer"};
    reader block{raw(size)};
    resources::bytes output;
    output.reserve(count);
    while(output.size() != count) {
      auto const run{block.byte()};
      auto const literals{block.byte()};
      if(run > count - output.size()) throw std::runtime_error{"flight recording fill run exceeds its field"};
      output.insert(output.end(), run, fill);
      if(literals > count - output.size()) throw std::runtime_error{"flight recording literal run exceeds its field"};
      auto const bytes{block.raw(literals)};
      output.insert(output.end(), bytes.begin(), bytes.end());
    }
    if(!block.complete()) throw std::runtime_error{"flight recording compressed field has trailing bytes"};
    return output;
  }
};

void write_frame(writer &output, frame const &source) {
  auto const count{source.models.size()};
  if(count > render::model_queue_capacity || source.order.size() != count) throw std::runtime_error{"flight recording has an invalid model queue size"};
  for(auto const &entry : source.order) {
    if(entry.slot >= count) throw std::runtime_error{"flight recording refers to an absent model slot"};
  }
  output.raw(source.reserved);
  for(unsigned int axis{0}; axis != 3; ++axis) output.word(source.camera_angles[axis]);
  output.word(static_cast<uint16_t>(count));
  for(auto const &entry : source.order) output.word(static_cast<uint16_t>(entry.priority));
  for(auto const &entry : source.order) output.word(entry.slot);
  for(auto const &model : source.models) output.byte(std::to_underlying(model.kind));
  auto const bytes{[&](auto const &field, uint8_t const fill){
    writer block;
    for(auto const &model : source.models) block.byte(field(model));
    output.packed(block.bytes, fill);
  }};
  bytes([](model const &model){return model.initial_state;}, 0);
  for(auto const &model : source.models) output.byte(model.colour_row);
  bytes([](model const &model){return model.scale;}, 1);
  for(auto const &model : source.models) output.word(model.resource);
  for(unsigned int axis{0}; axis != 3; ++axis) {
    writer block;
    for(auto const &model : source.models) block.word(model.angles[axis]);
    output.packed(block.bytes, 0);
  }
  for(unsigned int axis{0}; axis != 3; ++axis) {
    for(auto const &model : source.models) output.dword(static_cast<uint32_t>(model.camera_origin[axis]));
  }
  writer parameters;
  for(auto const &model : source.models) parameters.raw(model.parameters);
  output.packed(parameters.bytes, 0);
}

frame read_frame(reader &source) {
  frame result;
  auto const reserved{source.raw(result.reserved.size())};
  std::copy(reserved.begin(), reserved.end(), result.reserved.begin());
  for(unsigned int axis{0}; axis != 3; ++axis) result.camera_angles[axis] = source.word();
  auto const count{source.word()};
  if(count > render::model_queue_capacity) throw std::runtime_error{"flight recording model queue exceeds the original limit"};
  result.order.resize(count);
  result.models.resize(count);
  for(auto &entry : result.order) entry.priority = std::bit_cast<int16_t>(source.word());
  for(auto &entry : result.order) {
    entry.slot = source.word();
    if(entry.slot >= count) throw std::runtime_error{"flight recording refers to an absent model slot"};
  }
  for(auto &model : result.models) {
    model.kind = static_cast<model_kind>(source.byte());
    if(model.kind != model_kind::unrotated && model.kind != model_kind::rotated && model.kind != model_kind::ground_shadow) throw std::runtime_error{"flight recording has an unknown model drawing path"};
  }
  auto const states{source.packed(count, 0)};
  for(unsigned int index{0}; index != count; ++index) result.models[index].initial_state = states[index];
  for(auto &model : result.models) model.colour_row = source.byte();
  auto const scales{source.packed(count, 1)};
  for(unsigned int index{0}; index != count; ++index) result.models[index].scale = scales[index];
  for(auto &model : result.models) model.resource = source.word();
  for(unsigned int axis{0}; axis != 3; ++axis) {
    auto const packed{source.packed(static_cast<size_t>(count) * 2, 0)};
    resources::binary_view const words{packed};
    for(unsigned int index{0}; index != count; ++index) result.models[index].angles[axis] = words.le16(index * 2);
  }
  for(unsigned int axis{0}; axis != 3; ++axis) {
    for(auto &model : result.models) model.camera_origin[axis] = std::bit_cast<int32_t>(source.dword());
  }
  auto const parameters{source.packed(static_cast<size_t>(count) * 8, 0)};
  for(unsigned int index{0}; index != count; ++index) std::copy_n(parameters.begin() + index * 8, 8, result.models[index].parameters.begin());
  return result;
}

} // anonymous namespace

resources::bytes encode_frame(frame const &source) {
  writer output;
  write_frame(output, source);
  return std::move(output.bytes);
}

resources::bytes encode(file const &source) {
  if(source.frames.size() > std::numeric_limits<uint16_t>::max()) throw std::runtime_error{"flight recording exceeds its native frame counter"};
  if(source.resources.size() < 2 || source.resources.size() > native_resource_limit) throw std::runtime_error{"flight recording exceeds its native resource table"};
  writer output;
  output.word(static_cast<uint16_t>(source.frames.size()));
  output.dword(0);
  output.dword(0);
  for(auto const &frame : source.frames) write_frame(output, frame);
  auto const trailer{output.position()};
  output.word(static_cast<uint16_t>(source.resources.size()));
  for(auto const &resource : source.resources) output.word(resource.handle);
  writer names;
  for(unsigned int index{2}; index != source.resources.size(); ++index) {
    auto const &name{source.resources[index].name};
    if(name.find('\0') != std::string::npos || name.size() >= std::numeric_limits<uint16_t>::max()) throw std::runtime_error{"flight recording has an invalid resource name"};
    for(unsigned char const character : name) names.byte(character);
    names.byte(0);
  }
  output.dword(names.position());
  output.raw(names.bytes);
  output.patch_dword(2, output.position());
  output.patch_dword(6, trailer);
  return std::move(output.bytes);
}

file_header decode_header(std::span<uint8_t const> const source) {
  resources::binary_view const input{source};
  return {
    .frames{input.le16(0)},
    .size{input.le32(2)},
    .dictionary{input.le32(6)},
  };
}
decoded_frame decode_frame(std::span<uint8_t const> const source) {
  reader input{source};
  return {
    .value{read_frame(input)},
    .bytes{static_cast<unsigned int>(input.position)},
  };
}
std::vector<resource> decode_resources(std::span<uint8_t const> const source) {
  reader dictionary{source};
  auto const count{dictionary.word()};
  if(count < 2 || count > native_resource_limit) throw std::runtime_error{"flight recording has an invalid resource table size"};
  std::vector<resource> result(count);
  result[1].name = "badshape.3";
  for(auto &resource : result) resource.handle = dictionary.word();
  auto const bytes{dictionary.dword()};
  auto const names{dictionary.raw(bytes)};
  size_t cursor{0};
  for(unsigned int index{2}; index != count; ++index) {
    auto const first{cursor};
    while(cursor != names.size() && names[cursor]) ++cursor;
    if(cursor == names.size()) throw std::runtime_error{"flight recording has an unterminated resource name"};
    result[index].name.assign(reinterpret_cast<char const *>(names.data() + first), cursor - first);
    ++cursor;
  }
  if(cursor != names.size() || !dictionary.complete()) throw std::runtime_error{"flight recording has trailing resource data"};
  return result;
}

file decode(std::span<uint8_t const> const source) {
  auto const header{decode_header(source)};
  if(header.size != source.size() || header.dictionary < header_bytes || header.dictionary > header.size) throw std::runtime_error{"flight recording has invalid file offsets"};
  file result;
  result.frames.reserve(header.frames);
  reader body{source.subspan(header_bytes, header.dictionary - header_bytes)};
  for(unsigned int index{0}; index != header.frames; ++index) result.frames.push_back(read_frame(body));
  if(!body.complete()) throw std::runtime_error{"flight recording has trailing frame data"};
  result.resources = decode_resources(source.subspan(header.dictionary));
  return result;
}

} // namespace tfx::recording
