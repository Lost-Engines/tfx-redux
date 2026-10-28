#include "virtual_displays.h"
#include <cmath>
#include <stdexcept>

namespace tfx::cockpit {

void virtual_displays::add(unsigned int const page, render::texture_patch const &patch) {
  auto &regions{patches.at(page)};
  regions.push_back(patch);
  int const left{std::max(0,static_cast<int>(std::floor(patch.minimum.x+std::min(0.,patch.left_shift))))};
  int const right{std::min(256,static_cast<int>(std::ceil(patch.maximum.x+std::max(0.,patch.right_shift))))};
  for(int y=std::max(0,static_cast<int>(std::floor(patch.minimum.y))); y<std::min(192,static_cast<int>(std::ceil(patch.maximum.y))); ++y)
    for(int x=left; x<right; ++x) cells[page][static_cast<size_t>(y*256+x)]=static_cast<uint8_t>(regions.size());
}
virtual_displays::virtual_displays(instrument_assets const &instruments) {
  for(auto &page : cells) page.resize(256*192);
  auto const display{[&](unsigned int screen, vec2d minimum, vec2d maximum, unsigned int page, double left=0, double right=0) {
    add(page,{vec2d{minimum},vec2d{maximum},vec2d{instruments.display_origins[screen]},vec2d{instruments.display_sizes[screen]}+vec2d{1,1},left,right});
  }};
  // global_2 is the port half; global_1 is starboard. The outer screens have
  // shallow bevels. Their source pixels are no longer resized to atlas texels.
  display(0,{53,126},{111,161},1,2,0);
  display(2,{51,126},{109,161},0,0,-2);
  auto const centre{vec2d{instruments.display_origins[1]}};
  auto const size{vec2d{instruments.display_sizes[1]}+vec2d{1,1}};
  double const seam{size.x*28/58};
  add(1,{{132,144},{160,187},vec2d{centre},{seam,size.y}});
  add(0,{{0,144},{30,187},centre+vec2d{seam,0},{size.x-seam,size.y}});

  // Top annunciators: GEAR/AUTO, brakes/hook, stall, damage, engine fire,
  // permission-to-fire, tracked-weapon and threat lamps. Include their idle
  // artwork so clearing a warning restores the correct label/background.
  add(1,{{77,97},{117,115},{78,105},{40,17}});
  add(1,{{55,107},{77,116},{57,115},{21,9}});
  add(0,{{49,97},{79,116},{209,104},{30,19}});
  add(0,{{79,108},{104,116},{239,116},{26,8}});
  // Main warning lamps and both black message windows cross the atlas seam.
  add(1,{{121,92},{160,121},{121,100},{39,29}});
  add(0,{{0,92},{41,121},{160,100},{41,29}});

  // Port readouts: autopilot mode, clock, engine lamps, fuel, throttle and
  // reheat pips. The static 3D artwork spaces these differently from F1.
  add(1,{{17,128},{30,134},{18,137},{14,7}});
  add(1,{{6,141},{32,151},{7,152},{27,8}});
  add(1,{{14,153},{29,159},{17,162},{15,7}});
  add(1,{{13,163},{34,172},{13,171},{21,9}});
  add(1,{{14,175},{36,184},{13,183},{22,9}});
  add(1,{{14,185},{36,191},{13,193},{22,7}});
  // Starboard status, chaff and flare counts; retain the original labels.
  add(0,{{128,146},{151,155},{289,153},{21,10}});
  add(0,{{141,160},{149,167},{299,168},{9,7}});
  add(0,{{137,172},{146,180},{297,181},{9,7}});
  // Landing/ground and approach cues live on the uprights beside the HUD.
  add(1,{{79,66},{86,87},{71,74},{9,24}});
  add(0,{{76,66},{83,87},{244,74},{9,24}});
}
void virtual_displays::update(render::framebuffer const &frame) {
  if(frame.size != vec2<int>{320,200}) throw std::invalid_argument{"virtual cockpit needs the native instrument canvas"};
  std::ranges::copy(frame.pixels,panel.begin());
}
render::texture_overlay virtual_displays::texture(std::string_view const model, models::texture_binding const &binding) const {
  if(binding.source != models::texture_source::global || binding.slot > 1) return {};
  if(model != "cpt_efa" && model != "cpt_f17" && model != "cpt_f22"
     && model != "cptefa_f" && model != "cptf17_f" && model != "cptf22_f") return {};
  return {.patches{patches[binding.slot]},.cells{cells[binding.slot]},.source{panel}};
}

} // namespace tfx::cockpit
