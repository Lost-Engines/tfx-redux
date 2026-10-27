#include "virtual_displays.h"
#include <stdexcept>

namespace tfx::cockpit {

virtual_displays::virtual_displays(instrument_assets const &source) : instruments{source} {
  for(auto &page : pages) page.resize(256 * 192);
}
void virtual_displays::update(render::framebuffer const &frame) {
  if(frame.size != vec2<int>{320,200}) throw std::invalid_argument{"virtual cockpit needs the native instrument canvas"};
  auto const pixel{[&](unsigned int screen, int x, int y, int width, int height) {
    auto const &origin{instruments.display_origins[screen]};
    auto const &extent{instruments.display_sizes[screen]};
    int const sx{origin.x + x * (extent.x + 1) / width};
    int const sy{origin.y + y * (extent.y + 1) / height};
    return static_cast<uint16_t>(0x100 | frame.pixels[static_cast<size_t>(sy * 320 + sx)]);
  }};
  // global_2 is the port panel, global_1 starboard. Preserve the shallow
  // bevels at the upper outer corners; all UVs are authored in atlas pixels.
  for(int y=0; y<35; ++y) {
    int const left{53 + (2 * y / 34)}, right{109 - (2 * y / 34)};
    for(int x=left; x<111; ++x) pages[1][static_cast<size_t>((126+y)*256+x)] = pixel(0,x-left,y,111-left,35);
    for(int x=51; x<right; ++x) pages[0][static_cast<size_t>((126+y)*256+x)] = pixel(2,x-51,y,right-51,35);
  }
  // The centre display crosses the seam: 28 texels in global_2, then 30 in
  // global_1. Sample one continuous screen so neither half repeats/mirrors it.
  for(int y=0; y<43; ++y) for(int x=0; x<58; ++x) {
    auto &page{pages[x < 28 ? 1 : 0]};
    int const u{x < 28 ? 132+x : x-28};
    page[static_cast<size_t>((144+y)*256+u)] = pixel(1,x,y,58,43);
  }
}
std::span<uint16_t const> virtual_displays::texture(std::string_view const model, models::texture_binding const &binding) const {
  if(binding.source != models::texture_source::global || binding.slot > 1) return {};
  if(model != "cpt_efa" && model != "cpt_f17" && model != "cpt_f22"
     && model != "cptefa_f" && model != "cptf17_f" && model != "cptf22_f") return {};
  return pages[binding.slot];
}

} // namespace tfx::cockpit
