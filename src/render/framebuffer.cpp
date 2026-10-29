#include "framebuffer.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace tfx::render {

size_t framebuffer::pixel_count(vec2<int> const &dimensions) {
  size_t constexpr maximum_pixels{64 * 1024 * 1024};
  if(dimensions.x <= 0 || dimensions.y <= 0 || dimensions.x > 16384 || dimensions.y > 16384 || static_cast<size_t>(dimensions.x) * static_cast<size_t>(dimensions.y) > maximum_pixels) throw std::invalid_argument{"render resolution must have dimensions 1..16384 and at most 64 million pixels"};
  return static_cast<size_t>(dimensions.x) * static_cast<size_t>(dimensions.y);
}
void framebuffer::resize(vec2<int> const &dimensions) {
  auto const count{pixel_count(dimensions)};
  size = vec2<int>{dimensions};
  pixels.assign(count,0);
  coverage.clear();hud_coverage.clear();hud_capture=nullptr;
}
void framebuffer::track_writes() {
  coverage.assign(pixels.size(),0);
}


std::vector<rgba_pixel> framebuffer::colourise(palette const &colours) const {
  std::vector<rgba_pixel> result;
  colourise(colours,result);
  return result;
}
void framebuffer::colourise(palette const &colours, std::vector<rgba_pixel> &result) const {
  if(!hud_coverage.empty() && hud_coverage.size()!=pixels.size()) throw std::logic_error{"HUD coverage dimensions do not match the framebuffer"};
  // Expand the small palette once, then copy complete RGBA pixels. The live
  // window retains its output allocation across frames and resolution changes.
  std::array<rgba_pixel,256> lookup;
  for(size_t i=0;i<lookup.size();++i) lookup[i]={colours[i].r,colours[i].g,colours[i].b,255};
  result.resize(pixels.size());
  for(size_t i=0;i<pixels.size();++i) result[i]=lookup[pixels[i]];
  if(!hud_coverage.empty()) {
    std::array<vec3<uint8_t>,256> light;
    auto const &emission{colours[6]};
    for(size_t coverage=0;coverage<light.size();++coverage) for(unsigned channel=0;channel<3;++channel)
      light[coverage][channel]=static_cast<uint8_t>((static_cast<unsigned>(emission[channel])*coverage*9+1275)/2550);
    for(size_t i=0;i<result.size();++i) if(auto const coverage=hud_coverage[i]) for(unsigned channel=0;channel<3;++channel)
      result[i][channel]=static_cast<uint8_t>(std::min(255u,static_cast<unsigned>(result[i][channel])+light[coverage][channel]));
  }
}

void framebuffer::screenshot(std::filesystem::path const &path, palette const &colours) const {
  std::ofstream stream{path, std::ios::binary};
  stream.exceptions(std::ios::failbit | std::ios::badbit);
  stream << "P6\n" << size.x << ' ' << size.y << "\n255\n";
  for(auto const &colour : colourise(colours)) stream.write(reinterpret_cast<char const*>(colour.data()), 3);
}

palette decode_palette(std::span<uint8_t const> const source) {
  /// Expand VGA's six-bit DAC values without blending between palette entries
  if(source.size() != 256 * 3) throw std::runtime_error{"palette does not contain 256 RGB triples"};
  palette result;
  for(unsigned int i{0}; i != result.size(); ++i) {
    for(unsigned int channel{0}; channel != 3; ++channel) {
      auto const value{source[i * 3 + channel]};
      if(value > 63) throw std::runtime_error{"palette component exceeds the VGA DAC range"};
      result[i][channel] = static_cast<uint8_t>((value * 255 + 31) / 63);
    }
  }
  return result;
}

palette expand_dac_palette(dac_palette const &source) {
  // VGA's six-bit DAC ignores the upper two bits. Live HUDCOL brightness
  // deliberately stores 90..127; authored palette files remain strictly checked.
  dac_palette visible;
  std::ranges::transform(source,visible.begin(),[](uint8_t const value) { return static_cast<uint8_t>(value & 63); });
  return decode_palette(visible);
}

} // namespace tfx::render
