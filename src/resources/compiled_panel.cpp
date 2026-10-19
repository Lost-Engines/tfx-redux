#include "compiled_panel.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace tfx::resources {

void screen_overlay::paint(render::framebuffer &target) const {
  for(unsigned int pixel{0}; pixel != target.pixels.size(); ++pixel) if(coverage.pixels[pixel]) target.write(pixel,image.pixels[pixel]);
}
screen_overlay decode_planner_panel(std::span<uint8_t const> const executable) {
  binary_view const source{executable};
  unsigned int constexpr paint_begin{0xa9c61}, paint_end{0xaa966}, masks_begin{paint_end};
  unsigned int constexpr plane_pixels{render::screen_width * render::screen_height / 4};
  unsigned int cursor{paint_begin}, masks{masks_begin}, pixel{0};
  uint32_t ink{0}, count{0};
  uint8_t repeat{0}, enabled_planes{15};
  bool mask_port{false};
  screen_overlay result;
  auto const low_ink{[&](uint8_t const value) { ink = (ink & ~uint32_t{255}) | value; }};
  auto const write{[&](unsigned int const at, uint8_t const value) {
    if(at >= plane_pixels) throw std::runtime_error{"compiled panel writes outside its image"};
    for(unsigned int plane{0}; plane != 4; ++plane) if(enabled_planes & (1u << plane)) {
      result.image.write(at * 4 + plane,value);
      result.coverage.pixels[at * 4 + plane] = 255;
    }
  }};
  auto const run{[&](unsigned int const bytes) {
    for(unsigned int channel{0}; channel != bytes; ++channel) write(pixel++,static_cast<uint8_t>(ink >> (channel * 8)));
  }};
  unsigned int constexpr paint_budget{65536};
  for(unsigned int step{0}; step != paint_budget && cursor < paint_end; ++step) {
    unsigned int const operation{cursor};
    switch(source.byte(cursor++)) {
    case 0x8b: // Start the stream at its framebuffer origin (EDI = EBP).
      if(source.byte(cursor++) != 0xfd) throw std::runtime_error{"unknown compiled panel origin"};
      pixel = 0;
      break;
    case 0x33:
      if(source.byte(cursor++) != 0xc9) throw std::runtime_error{"unknown compiled panel count reset"};
      count = 0;
      break;
    case 0xb0: low_ink(source.byte(cursor++));break;
    case 0x8a: // Duplicate the low ink byte into the second byte for a word run.
      if(source.byte(cursor++) != 0xe0) throw std::runtime_error{"unknown compiled panel ink copy"};
      ink = (ink & ~uint32_t{0xff00}) | ((ink & 255) << 8);
      break;
    case 0xb1:
      count = (count & ~uint32_t{255}) | source.byte(cursor++);
      break;
    case 0xb3: repeat = source.byte(cursor++);break;
    case 0xb8: ink = source.le32(cursor);cursor += 4;break;
    case 0xb9: count = source.le32(cursor);cursor += 4;break;
    case 0xbb: // Fixed auxiliary value from the compiled painter; no output dependency.
      if(source.le32(cursor) != 1) throw std::runtime_error{"unknown compiled panel auxiliary value"};
      cursor += 4;
      break;
    case 0xba:
      if(source.le32(cursor) != 0x3c4) throw std::runtime_error{"compiled panel selects an unknown VGA port"};
      cursor += 4;
      mask_port = false;
      break;
    case 0x2a:
      if(source.byte(cursor++) != 0xc0) throw std::runtime_error{"unknown compiled panel ink reset"};
      low_ink(0);
      break;
    case 0xfe:
      switch(source.byte(cursor++)) {
      case 0xc0: low_ink(static_cast<uint8_t>(ink + 1));break;
      case 0xcb: --repeat;break;
      case 0xc2: mask_port = true;break;
      default: throw std::runtime_error{"unknown compiled panel increment"};
      }
      break;
    case 0x75:
      {
        auto const displacement{std::bit_cast<int8_t>(source.byte(cursor++))};
        if(repeat) cursor = static_cast<unsigned int>(static_cast<int>(cursor) + displacement);
        if(cursor < paint_begin || cursor >= paint_end) throw std::runtime_error{"compiled panel loop leaves its stream"};
      }
      break;
    case 0xaa: run(1);break;
    case 0xf3:
      if(source.byte(cursor++) != 0xab || count > plane_pixels / 4) throw std::runtime_error{"unknown or oversized compiled panel run"};
      while(count) { run(4);--count; }
      break;
    case 0x66:
      switch(source.byte(cursor++)) {
      case 0xab: run(2);break;
      case 0x47: pixel = static_cast<uint16_t>(pixel + 1);break;
      case 0x83:
        if(source.byte(cursor++) != 0xc7) throw std::runtime_error{"unknown compiled panel cursor adjustment"};
        pixel = static_cast<uint16_t>(pixel + std::bit_cast<int8_t>(source.byte(cursor++)));
        break;
      default: throw std::runtime_error{"unknown compiled panel word operation"};
      }
      break;
    case 0x8d:
      if(source.byte(cursor++) != 0xbd) throw std::runtime_error{"unknown compiled panel position"};
      pixel = source.le32(cursor);
      cursor += 4;
      break;
    case 0x88:
      if(source.byte(cursor++) != 0x85) throw std::runtime_error{"unknown compiled panel pixel"};
      write(source.le32(cursor),static_cast<uint8_t>(ink));
      cursor += 4;
      break;
    case 0x6e:
      if(mask_port) {
        enabled_planes = source.byte(masks++);
        if(enabled_planes > 15) throw std::runtime_error{"compiled panel has an invalid VGA plane mask"};
      } else if(source.byte(masks++) != 2) throw std::runtime_error{"compiled panel selects an unknown VGA register"};
      break;
    case 0x5d: case 0x5f: case 0x5e: break; // End of the fixed artwork routine.
    case 0xc3:
      if(cursor != paint_end) throw std::runtime_error{"compiled panel ended before its declared extent"};
      return result;
    default: throw std::runtime_error{"unsupported compiled panel operation at " + std::to_string(operation)};
    }
  }
  throw std::runtime_error{"compiled panel has no bounded end"};
}

planner_artwork::planner_artwork(std::span<uint8_t const> const executable) : panel{decode_planner_panel(executable)} {
  binary_view const source{executable};
  unsigned int constexpr data{973476};
  std::ranges::copy(source.slice(0xa9b44,interface_colours.size()),interface_colours.begin());
  std::ranges::copy(source.slice(data + 0xa7ea,marker_colours.size()),marker_colours.begin());
  for(unsigned int index{0}; index != region_names.size(); ++index) {
    auto const address{data + source.le32(data + 0xa9c4 + index * 4)};
    auto const bytes{source.slice(address,32)};
    auto const end{std::ranges::find(bytes,uint8_t{0})};
    if(end == bytes.end()) throw std::runtime_error{"unterminated original planner region name"};
    region_names[index].assign(bytes.begin(),end);
  }
}
screen_overlay const &planner_artwork::backdrop() const {
  return panel;
}
std::array<uint8_t,256 * 3> planner_artwork::colours(std::span<uint8_t const> const map_colours) const {
  std::array<uint8_t,256 * 3> result;
  if(map_colours.size() != result.size()) throw std::runtime_error{"planner map palette has the wrong size"};
  std::ranges::copy(map_colours,result.begin());
  unsigned int constexpr interface_first{32}, marker_first{240};
  // Native 0xab3ac replaces map colours with its code-object UI ramp and data-object marker bank.
  std::ranges::copy(interface_colours,result.begin() + interface_first * 3);
  std::ranges::copy(marker_colours,result.begin() + marker_first * 3);
  return result;
}
std::string const &planner_artwork::region(std::string_view const world) const {
  auto const upper{[](char const value) { return value >= 'a' && value <= 'z' ? static_cast<char>(value - ('a' - 'A')) : value; }};
  for(unsigned int index{1}; index != region_names.size(); ++index) {
    if(world.size() >= 3 && std::ranges::equal(world.substr(0,3),std::string_view{region_names[index]}.substr(0,3),[&](char const a, char const b) { return upper(a) == upper(b); })) return region_names[index];
  }
  return region_names.front();
}

} // namespace tfx::resources
