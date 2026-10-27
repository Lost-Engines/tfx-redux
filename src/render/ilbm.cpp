#include "ilbm.h"
#include <span>
#include <string_view>

namespace tfx::render {
namespace {

unsigned int constexpr planes{8};
unsigned int constexpr bitmap_header_bytes{20}, chunk_header_bytes{8}, form_header_bytes{12};
unsigned int constexpr palette_bytes{256 * 3};
unsigned int constexpr image_prefix_bytes{form_header_bytes + chunk_header_bytes + bitmap_header_bytes + chunk_header_bytes + palette_bytes + chunk_header_bytes};

auto pack_row(std::span<uint8_t const> const row, std::vector<uint8_t> &output)->void {
  auto const start{output.size()};
  unsigned int position{0};
  while(position != row.size()) {
    unsigned int const begin{position++};
    if(position != row.size() && row[begin] == row[position]) {
      while(position != row.size() && row[position] == row[begin] && position - begin < 128) ++position;
      output.push_back(static_cast<uint8_t>(1 - static_cast<int>(position - begin)));
      output.push_back(row[begin]);
    } else {
      while(position != row.size() && (position + 1 == row.size() || row[position] != row[position + 1]) && position - begin < 128) ++position;
      output.push_back(static_cast<uint8_t>(position - begin - 1));
      output.insert(output.end(),row.begin() + begin,row.begin() + position);
    }
  }
  // Native 0xdfc30 retains compression at exactly 40 bytes; a longer row
  // falls back to one literal packet containing the whole bitplane row.
  if(output.size() - start > row.size()) {
    output.resize(start);
    for(size_t at{0}; at < row.size(); at += 128) {
      auto const count{std::min<size_t>(128,row.size() - at)};
      output.push_back(static_cast<uint8_t>(count - 1));
      output.insert(output.end(),row.begin() + static_cast<ptrdiff_t>(at),row.begin() + static_cast<ptrdiff_t>(at + count));
    }
  }
}

} // anonymous namespace

auto encode_ilbm(framebuffer const &image, dac_palette const &colours)->std::vector<uint8_t> {
  bool const optical{!image.hud_coverage.empty()};
  unsigned const image_planes{optical ? 24u : planes};
  auto const rgb{optical ? image.colourise(expand_dac_palette(colours)) : std::vector<rgba_pixel>{}};
  unsigned const prefix_bytes{optical ? image_prefix_bytes-chunk_header_bytes-palette_bytes : image_prefix_bytes};
  auto const plane_row_bytes{static_cast<unsigned int>((image.size.x + 15) / 16 * 2)};
  std::vector<uint8_t> body;
  body.reserve((plane_row_bytes + 1) * image_planes * static_cast<unsigned int>(image.size.y));
  std::vector<uint8_t> row(plane_row_bytes);
  for(unsigned int y{0}; y != static_cast<unsigned int>(image.size.y); ++y) {
    for(unsigned int plane{0}; plane != image_planes; ++plane) {
      for(unsigned int x{0}; x != row.size(); ++x) {
        uint8_t bits{0};
        for(unsigned int bit{0}; bit != planes; ++bit) {
          unsigned int const column{x * planes + bit};
          auto const at{y*static_cast<unsigned>(image.size.x)+column};
          unsigned int const pixel{column < static_cast<unsigned int>(image.size.x) ? (optical ? rgb[at][plane/8] : image.pixels[at]) : 0u};
          bits |= static_cast<uint8_t>(((pixel >> (plane%8)) & 1u) << (planes - bit - 1));
        }
        row[x] = bits;
      }
      pack_row(row,body);
    }
  }
  size_t const padding{optical ? body.size() & 1u : 0u};
  std::vector<uint8_t> result;
  result.reserve(prefix_bytes + body.size() + padding);
  auto const word{[&](unsigned int const value, unsigned int const bytes) {
    for(unsigned int remaining{bytes}; remaining; --remaining) result.push_back(static_cast<uint8_t>(value >> ((remaining - 1) * 8)));
  }};
  auto const tag{[&](std::string_view const name) { result.insert(result.end(),name.begin(),name.end()); }};
  tag("FORM");word(prefix_bytes - chunk_header_bytes + static_cast<unsigned int>(body.size() + padding),4);tag("ILBM");
  tag("BMHD");word(bitmap_header_bytes,4);
  word(static_cast<unsigned int>(image.size.x),2);word(static_cast<unsigned int>(image.size.y),2);word(0,2);word(0,2);
  word(image_planes,1);word(0,1);word(1,1);word(0,1);word(0,2);
  word(1,1);word(1,1);word(static_cast<unsigned int>(image.size.x),2);word(static_cast<unsigned int>(image.size.y),2);
  if(!optical) {
    tag("CMAP");word(static_cast<unsigned int>(colours.size()),4);
    for(auto const value : colours) result.push_back(static_cast<uint8_t>((value & 63) << 2));
  }
  tag("BODY");word(static_cast<unsigned int>(body.size()),4);
  result.insert(result.end(),body.begin(),body.end());
  // Native 0xdfe08 omitted padding; retain those indexed bytes while new
  // true-colour files obey IFF chunk padding.
  if(padding) result.push_back(0);
  return result;
}

} // namespace tfx::render
