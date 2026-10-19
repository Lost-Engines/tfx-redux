#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include "recording/video.h"
#include "resources/binary.h"

namespace {
void require(bool const value, char const *const message) { if(!value) throw std::runtime_error{message}; }
struct temporary_directory {
  std::filesystem::path path{std::filesystem::temp_directory_path() / ("tfx-redux-video-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  temporary_directory() { std::filesystem::create_directory(path); }
  ~temporary_directory() { std::filesystem::remove_all(path); }
};
void save(std::filesystem::path const &path, std::span<uint8_t const> const bytes) {
  std::ofstream file{path,std::ios::binary};file.write(reinterpret_cast<char const*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
}
}

int main() try {
  using namespace tfx;
  temporary_directory temporary;
  auto const path{temporary.path / "flight.rec"};
  recording::video_recorder writer;
  writer.begin(path);
  render::framebuffer image;image.resize({1001,603});
  render::dac_palette palette{};
  std::vector<render::framebuffer> expected;
  std::vector<render::dac_palette> palettes;
  for(unsigned int frame{0}; frame != 5; ++frame) {
    if(frame == 3) image.resize({803,451});
    for(unsigned int y{0}; y != static_cast<unsigned int>(image.size.y); ++y) for(unsigned int x{0}; x != static_cast<unsigned int>(image.size.x); ++x) image.pixels[y * static_cast<unsigned int>(image.size.x) + x] = static_cast<uint8_t>(x / 17 + y / 11);
    image.pixels[17] = static_cast<uint8_t>(frame);palette[17] = static_cast<uint8_t>(frame * 7);
    expected.push_back(image);palettes.push_back(palette);
    writer.append(image,palette,std::chrono::microseconds{20000 + frame});
  }
  writer.finish();
  require(recording::video_playback::recognises(path),"Redux video signature was not recognised");
  recording::video_playback playback{path};require(playback.frame_count() == expected.size(),"frame count was lost");
  for(unsigned int frame{0}; frame != expected.size(); ++frame) {
    auto const decoded{playback.next()};
    require(decoded && decoded->image.size == expected[frame].size && decoded->image.pixels == expected[frame].pixels,"lossless video changed the displayed pixels or resolution");
    require(decoded->palette == palettes[frame] && decoded->duration == std::chrono::microseconds{20000 + frame},"palette changes or timing were lost");
  }
  require(!playback.next(),"video playback did not finish");
  auto const valid{resources::read_file(path)};
  require(valid.size() < image.pixels.size(),"flat-colour video did not compress");
  auto const rejected{[&](resources::bytes const &bytes) {
    auto const bad{temporary.path / "bad.rec"};save(bad,bytes);
    try { recording::video_playback movie{bad};while(movie.next()) {} } catch(std::exception const&) { return true; }
    return false;
  }};
  for(auto const length : {size_t{0},size_t{23},size_t{24},valid.size() - 1}) require(rejected({valid.begin(),valid.begin() + static_cast<ptrdiff_t>(length)}),"truncated video was accepted");
  for(auto const offset : {size_t{8},size_t{12},size_t{24},size_t{28},size_t{43},size_t{44},size_t{45}}) {
    auto bad{valid};bad[offset] = 255;require(rejected(bad),"invalid version, dimensions, packet size or encoding was accepted");
  }
  auto bad{valid};bad.push_back(0);require(rejected(bad),"trailing video data was accepted");
  // The new format must retain valid literal data as well as large flat runs.
  image.resize({257,17});uint32_t random{42};
  for(auto &pixel : image.pixels) { random = random * 1664525u + 1013904223u;pixel = static_cast<uint8_t>(random >> 24); }
  writer.begin(path);writer.append(image,palette,std::chrono::microseconds{80000});writer.append(image,palette,std::chrono::microseconds{90000});writer.finish();
  recording::video_playback noisy{path};require(noisy.next()->image.pixels == image.pixels && noisy.next()->image.pixels == image.pixels,"literal/delta packets changed pixels");
  // Exercise the old 16-bit frame-count boundary with tiny streaming frames.
  image.resize({1,1});writer.begin(path);
  for(unsigned int frame{0}; frame != 65537; ++frame) { image.pixels[0] = static_cast<uint8_t>(frame);writer.append(image,palette,std::chrono::microseconds{1}); }
  writer.finish();recording::video_playback long_movie{path};unsigned int count{0};
  while(auto frame{long_movie.next()}) require(frame->image.pixels[0] == static_cast<uint8_t>(count++),"long recording changed its frame sequence");
  require(count == 65537 && long_movie.frame_count() == count,"recording retained the DOS frame-count limit");
  std::cout << "Redux video: exact pixels, palettes, timing, resize, literal/delta compression, malformed files and 65,537 streamed frames passed\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
