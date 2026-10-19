#include "video.h"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace tfx::recording {
namespace {

std::array<uint8_t,8> constexpr signature{'T','F','X','R','V','I','D',0};
uint32_t constexpr format_version{1};
unsigned int constexpr header_bytes{24}, frame_header_bytes{24}, palette_bytes{768};
auto constexpr maximum_frame_duration{std::chrono::minutes{10}};
enum class encoding : uint8_t { pixels, difference };

void write(std::ostream &output, std::span<uint8_t const> const bytes) {
  output.write(reinterpret_cast<char const*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
}
void read(std::istream &input, std::span<uint8_t> const bytes) {
  input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
}
void store(std::span<uint8_t> const bytes, unsigned int const offset, uint64_t const value, unsigned int const count) {
  for(unsigned int byte{0}; byte != count; ++byte) bytes[offset + byte] = static_cast<uint8_t>(value >> (byte * 8));
}
uint64_t load(std::span<uint8_t const> const bytes, unsigned int const offset, unsigned int const count) {
  uint64_t value{0};
  for(unsigned int byte{0}; byte != count; ++byte) value |= static_cast<uint64_t>(bytes[offset + byte]) << (byte * 8);
  return value;
}
std::vector<uint8_t> pack(std::span<uint8_t const> const pixels) {
  std::vector<uint8_t> result;
  result.reserve(pixels.size() / 2);
  size_t position{0};
  while(position < pixels.size()) {
    size_t run{1};
    while(run < 128 && position + run < pixels.size() && pixels[position + run] == pixels[position]) ++run;
    if(run >= 3) {
      result.push_back(static_cast<uint8_t>(257 - run));result.push_back(pixels[position]);position += run;
    } else {
      auto const start{position++};
      while(position - start < 128 && position < pixels.size()) {
        if(position + 2 < pixels.size() && pixels[position] == pixels[position + 1] && pixels[position] == pixels[position + 2]) break;
        ++position;
      }
      result.push_back(static_cast<uint8_t>(position - start - 1));
      result.insert(result.end(),pixels.begin() + static_cast<ptrdiff_t>(start),pixels.begin() + static_cast<ptrdiff_t>(position));
    }
  }
  return result;
}
void unpack(std::span<uint8_t const> const source, std::span<uint8_t> const target) {
  size_t at{0}, decoded{0};
  while(at < source.size()) {
    auto const control{source[at++]};
    if(control == 128) throw std::runtime_error{"reserved Redux video packet"};
    size_t const count{control < 128 ? control + 1u : 257u - control};
    size_t const stored{control < 128 ? count : 1};
    if(count > target.size() - decoded || stored > source.size() - at) throw std::runtime_error{"invalid Redux video packet length"};
    if(control < 128) std::copy_n(source.begin() + static_cast<ptrdiff_t>(at),count,target.begin() + static_cast<ptrdiff_t>(decoded));
    else std::fill_n(target.begin() + static_cast<ptrdiff_t>(decoded),count,source[at]);
    at += stored;decoded += count;
  }
  if(decoded != target.size()) throw std::runtime_error{"incomplete Redux video frame"};
}

} // anonymous namespace

video_recorder::video_recorder() { output.exceptions(std::ios::badbit | std::ios::failbit); }
video_recorder::~video_recorder() { try { finish(); } catch(...) {} }
bool video_recorder::active() const { return output.is_open(); }
uint64_t video_recorder::frame_count() const { return frames; }
void video_recorder::begin(std::filesystem::path const &path) {
  finish();output.clear();frames = 0;previous.clear();previous_size = {0,0};
  output.open(path,std::ios::binary | std::ios::trunc);
  std::array<uint8_t,header_bytes> header{};
  std::ranges::copy(signature,header.begin());store(header,8,format_version,4);write(output,header);
}
void video_recorder::append(render::framebuffer const &image, render::dac_palette const &palette, std::chrono::microseconds const duration) {
  if(!active()) throw std::runtime_error{"Redux video recorder is not open"};
  if(image.pixels.size() != render::framebuffer::pixel_count(image.size)) throw std::invalid_argument{"recorded image dimensions do not match its pixels"};
  if(duration.count() <= 0 || duration > maximum_frame_duration) throw std::invalid_argument{"invalid Redux video frame duration"};
  auto compressed{pack(image.pixels)};
  auto method{encoding::pixels};
  if(previous_size == image.size) {
    std::vector<uint8_t> difference(image.pixels.size());
    for(size_t at{0}; at != difference.size(); ++at) difference[at] = image.pixels[at] ^ previous[at];
    auto delta{pack(difference)};
    if(delta.size() < compressed.size()) { compressed = std::move(delta);method = encoding::difference; }
  }
  std::array<uint8_t,frame_header_bytes> header{};
  store(header,0,static_cast<unsigned int>(image.size.x),4);store(header,4,static_cast<unsigned int>(image.size.y),4);
  store(header,8,static_cast<uint64_t>(duration.count()),8);store(header,16,compressed.size(),4);header[20] = std::to_underlying(method);
  write(output,header);write(output,palette);write(output,compressed);
  previous = image.pixels;previous_size = vec2<int>{image.size};++frames;
}
void video_recorder::finish() {
  if(!active()) return;
  std::array<uint8_t,8> count{};store(count,0,frames,8);
  output.seekp(16);write(output,count);output.close();previous.clear();previous_size = {0,0};
}
bool video_playback::recognises(std::filesystem::path const &path) {
  std::ifstream input{path,std::ios::binary};
  std::array<uint8_t,signature.size()> bytes{};
  input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
  return input.gcount() == static_cast<std::streamsize>(bytes.size()) && bytes == signature;
}
video_playback::video_playback(std::filesystem::path const &path) {
  input.exceptions(std::ios::badbit | std::ios::failbit);
  input.open(path,std::ios::binary | std::ios::ate);
  auto const length{static_cast<std::streamoff>(input.tellg())};
  if(length < header_bytes) throw std::runtime_error{"truncated Redux video header"};
  input.seekg(0);
  std::array<uint8_t,header_bytes> header{};read(input,header);
  if(!std::ranges::equal(std::span{header}.first(signature.size()),signature) || load(header,8,4) != format_version || load(header,12,4) != 0) throw std::runtime_error{"unsupported Redux video header"};
  frames = remaining = load(header,16,8);bytes_remaining = static_cast<uint64_t>(length) - header_bytes;
  if(frames > bytes_remaining / (frame_header_bytes + palette_bytes + 1) || (!frames && bytes_remaining)) throw std::runtime_error{"invalid Redux video frame count"};
}
uint64_t video_playback::frame_count() const { return frames; }
std::optional<video_frame> video_playback::next() {
  if(!remaining) return {};
  if(bytes_remaining < frame_header_bytes + palette_bytes) throw std::runtime_error{"truncated Redux video frame"};
  std::array<uint8_t,frame_header_bytes> header{};read(input,header);
  auto const width{load(header,0,4)}, height{load(header,4,4)};
  if(width > 16384 || height > 16384) throw std::runtime_error{"invalid Redux video dimensions"};
  vec2<int> const size{static_cast<int>(width),static_cast<int>(height)};
  auto const pixels{render::framebuffer::pixel_count(size)};
  auto const duration{load(header,8,8)}, count{load(header,16,4)};
  if(!duration || duration > static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(maximum_frame_duration).count()) || count > pixels + (pixels + 127) / 128 || count > bytes_remaining - frame_header_bytes - palette_bytes) throw std::runtime_error{"invalid Redux video frame length"};
  auto const method{static_cast<encoding>(header[20])};
  if((method != encoding::pixels && method != encoding::difference) || load(header,21,3) || (method == encoding::difference && (remaining == frames || previous.size != size))) throw std::runtime_error{"invalid Redux video frame encoding"};
  video_frame result{
    .image{},
    .palette{},
    .duration{static_cast<int64_t>(duration)},
  };
  result.image.resize(size);read(input,result.palette);
  std::vector<uint8_t> compressed(static_cast<size_t>(count));read(input,compressed);unpack(compressed,result.image.pixels);
  if(method == encoding::difference) for(size_t at{0}; at != pixels; ++at) result.image.pixels[at] ^= previous.pixels[at];
  previous = result.image;
  bytes_remaining -= frame_header_bytes + palette_bytes + count;--remaining;
  if(!remaining && bytes_remaining) throw std::runtime_error{"trailing data after Redux video frames"};
  return std::optional<video_frame>{std::in_place,std::move(result)};
}

} // namespace tfx::recording
