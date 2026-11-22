#include "notes.h"
#include <algorithm>
#include "presentation/drawing.h"

namespace tfx::planning {
namespace {
uint16_t key(uint8_t const waypoint, uint8_t const route) {
  return static_cast<uint16_t>(waypoint | static_cast<unsigned int>(route) << 8);
}
} // anonymous namespace

notes::notes(std::string_view const source) {
  append(source);
}
void notes::append(std::string_view const source) {
  // Native 0xaac40 removes CR and NUL before indexing ESC-terminated blocks.
  std::string compact;
  for(char const value : source) {
    if(value != '\r' && value != '\0') compact += value;
  }
  size_t at{0};
  unsigned int constexpr maximum_notes{500};
  for(unsigned int index{0}; index != maximum_notes && at < compact.size(); ++index) {
    auto const end{compact.find('\x1b', at)};
    if(end == std::string::npos) break;
    std::string_view const block{compact.data() + at, end - at};
    // The indexing scanner stops at a blank line (returning zero without advancing),
    // whereas the later widget-header scanner skips it. Retained mission 829 has
    // blank lines after ESC: its notes 4 and 6 consequently replace key zero.
    size_t index_at{0};
    auto const index_number{[&]()->uint8_t {
      if(index_at < block.size() && block[index_at] == '\n') return 0;
      while(index_at < block.size() && (block[index_at] < '0' || block[index_at] > '9') && block[index_at] != '\n') ++index_at;
      uint32_t value{0};
      while(index_at < block.size() && block[index_at] >= '0' && block[index_at] <= '9') value = value * 10 + static_cast<unsigned int>(block[index_at++] - '0');
      return static_cast<uint8_t>(value);
    }};
    uint8_t const waypoint{index_number()};
    uint8_t const route{index_number()};
    auto const identifier{key(waypoint, route)};
    entries.insert_or_assign(identifier, std::string{block});
    decoded.erase(identifier);
    // The source reader skips ESC and its following newline.
    at = std::min(end + 2, compact.size());
  }
}
annotation const *notes::find(uint8_t const waypoint, uint8_t const route) const {
  auto const identifier{key(waypoint, route)};
  if(auto const cached{decoded.find(identifier)}; cached != decoded.end()) return &cached->second;
  auto const found{entries.find(identifier)};
  if(found == entries.end()) return nullptr;
  // Native indexing retains raw blocks and interprets widget fields only when a
  // note is actually requested. French mission 829 has a stray "3" block that
  // is replaced later; eager widget decoding would incorrectly reject the file.
  std::string_view const block{found->second};
  std::array<uint8_t, 6> fields{};
  size_t number{0};
  for(auto &field : fields) {
    while(number < block.size() && (block[number] < '0' || block[number] > '9')) ++number;
    if(number == block.size()) throw std::runtime_error{"Incomplete planner annotation header"};
    uint32_t value{0};
    while(number < block.size() && block[number] >= '0' && block[number] <= '9') value = value * 10 + static_cast<unsigned int>(block[number++] - '0');
    field = static_cast<uint8_t>(value);
  }
  auto const newline{block.find('\n', number)};
  if(newline == std::string_view::npos) throw std::runtime_error{"Planner annotation has no text line"};
  unsigned int constexpr maximum_text_bytes{400};
  annotation note{
    .waypoint{waypoint},
    .route{route},
    .position{fields[2], fields[3]},
    .size{std::max(uint8_t{1}, fields[4]), std::max(uint8_t{1}, fields[5])},
    .text{block.substr(newline + 1, maximum_text_bytes)},
  };
  return &decoded.emplace(identifier, std::move(note)).first->second;
}
std::map<uint16_t, annotation> const &notes::all() const {
  for(auto const &[identifier, text] : entries) find(static_cast<uint8_t>(identifier), static_cast<uint8_t>(identifier >> 8));
  return decoded;
}

void draw_annotation(render::framebuffer &image, presentation::widget_font const &font,
  annotation const &note) {
  // The original eight-pixel widget grid includes an eight-pixel frame on each side.
  int const x{note.position.x * 8};
  int const y{note.position.y * 8};
  int const width{std::min<int>(note.size.x, 78 - note.position.x) * 8 + 16};
  int const height{std::min<int>(note.size.y, 48 - note.position.y) * 8 + 16};
  int const right{x + width};
  int const bottom{y + height};
  for(int row{std::max(0, y + 8)}; row <= std::min(render::screen_height - 1, bottom - 9); ++row) {
    for(int column{std::max(0, x + 8)}; column <= std::min(render::screen_width - 1, right - 9); ++column) image.pixels[row * render::screen_width + column] = 246;
  }
  for(int const inset : {7, 6}) {
    int const left{x + inset};
    int const top{y + inset};
    int const r{right - inset - 1};
    int const b{bottom - inset - 1};
    presentation::draw_line(image, {left, top}, {r, top}, 253);
    presentation::draw_line(image, {r, top}, {r, b}, 245);
    presentation::draw_line(image, {r, b}, {left, b}, 245);
    presentation::draw_line(image, {left, b}, {left, top}, 253);
  }
  // 0xce904 is passed a 32000-pixel offset, deliberately disabling its word wrapping.
  int row{y + 9};
  std::string_view text{note.text};
  while(!text.empty() && row < bottom - 8) {
    auto const end{text.find('\n')};
    font.draw(image, {x + 9, row}, text.substr(0, end), 255);
    if(end == std::string_view::npos) break;
    text.remove_prefix(end + 1);
    row += 8;
  }
}

} // namespace tfx::planning
