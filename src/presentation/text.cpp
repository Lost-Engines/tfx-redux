#include "text.h"
#include <algorithm>
#include <stdexcept>

namespace tfx::presentation {
namespace {

std::string substitute(std::string_view const line, variables const &state) {
  // Native 0xb14dc recognises a variable up to a space or terminator, and formats numeric values as signed long.
  std::string result;
  unsigned int cursor{0};
  while(cursor < line.size() && line[cursor]) {
    if(line[cursor] != '%') { result.push_back(line[cursor++]);continue; }
    std::string name;
    while(++cursor < line.size() && line[cursor] && line[cursor] != ' ') {
      char const letter{line[cursor]};
      name.push_back(letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter + ('a' - 'A')) : letter);
    }
    auto const found{state.find(name)};
    if(found == state.end()) throw std::runtime_error{"undefined presentation variable: " + name};
    if(auto const number{std::get_if<uint32_t>(&found->second)}) result += std::to_string(std::bit_cast<int32_t>(*number));
    else result += std::get<std::string>(found->second);
  }
  return result;
}

} // anonymous namespace

prepared_text prepare_text(text_block const &source, variables const &state, presentation::fonts const &fonts, external_text resolve) {
  std::string raw{source.text};
  unsigned int constexpr maximum_external_depth{32}, maximum_lines{20}, line_buffer_bytes{80};
  // A caret replaces the whole text block with the named language file, preserving its drawing settings.
  for(unsigned int depth{0};; ++depth) {
    auto const caret{raw.find('^')};
    if(caret == std::string::npos) break;
    if(!resolve) throw std::runtime_error{"presentation text needs its external language resource"};
    if(depth == maximum_external_depth) throw std::runtime_error{"external presentation text refers to itself"};
    auto const begin{caret + 1}, end{raw.find_first_of(std::string_view{"\r\n \t\0",5},begin)};
    raw = resolve(std::string_view{raw}.substr(begin,end == std::string::npos ? end : end - begin));
  }
  prepared_text result{
    .lines{},
    .font{source.font},
    .colour{source.colour},
    .shadow{source.shadow},
  };
  unsigned int cursor{0};
  while(cursor < raw.size()) {
    std::string line;
    while(cursor < raw.size() && raw[cursor] != '\r' && raw[cursor] != '\n') {
      char const letter{raw[cursor++]};
      line.push_back(letter >= 'a' && letter <= 'z' ? static_cast<char>(letter - ('a' - 'A')) : letter);
    }
    line = substitute(line,state);
    // The shipped text fits the native twenty 80-byte buffers; reject overflow instead of corrupting adjacent rows/state.
    if(result.lines.size() == maximum_lines || line.size() >= line_buffer_bytes) throw std::runtime_error{"presentation text exceeds the native line buffers"};
    int16_t const width{math::signed_word(static_cast<uint16_t>(fonts.measure(source.font,line)))};
    int x{source.position.x};
    if(source.alignment == text_alignment::right) x += render::screen_width - 1 - width;
    else if(source.alignment == text_alignment::centred) x += render::centre_x - width / 2;
    else if(source.alignment != text_alignment::left) throw std::runtime_error{"unknown native text alignment"};
    int const y{source.position.y + static_cast<int>(result.lines.size()) * static_cast<int>(fonts.line_height(source.font))};
    result.characters = static_cast<uint16_t>(result.characters + line.size());
    result.lines.push_back({
      .text{std::move(line)},
      .position{math::signed_word(static_cast<uint16_t>(x)),math::signed_word(static_cast<uint16_t>(y))},
      .width{width},
    });
    if(cursor < raw.size()) cursor += raw[cursor] == '\r' ? 2 : 1;             // The original consumes two bytes for CR, even without LF.
  }
  return result;
}

text_reveal::text_reveal(uint16_t const characters, text_counter const initial_counter) : total{characters}, progress{.previous{initial_counter}} {}

bool text_reveal::advance(text_counter const counter, bool const skip) {
  // Native 0xb1b28 / 0xb1268 / 0xb1c16: Space completes the current reveal before the frame's clock increment.
  bool const consumed{skip && math::signed_word(progress.visible) < math::signed_word(total)};
  if(consumed) progress.visible = total;
  int const elapsed{math::signed_word(static_cast<uint16_t>(counter - progress.previous))};
  progress.previous = counter;
  int constexpr minimum_increment{1}, maximum_increment{50}, ticks_per_character{2};
  progress.remainder = static_cast<uint16_t>(progress.remainder + std::clamp(elapsed,minimum_increment,maximum_increment));
  int const characters{math::signed_word(progress.remainder) / ticks_per_character};
  progress.visible = static_cast<uint16_t>(progress.visible + characters);
  progress.remainder = static_cast<uint16_t>(progress.remainder - characters * ticks_per_character);
  if(math::signed_word(progress.visible) > math::signed_word(total)) progress.visible = total;
  return consumed;
}

reveal_progress const &text_reveal::state() const {
  return progress;
}

void draw_text(render::framebuffer &target, prepared_text const &text, presentation::fonts const &fonts, unsigned int characters, render::framebuffer *const coverage) {
  for(auto const &line : text.lines) {
    auto const count{std::min(characters,static_cast<unsigned int>(line.text.size()))};
    auto const visible{std::string_view{line.text}.substr(0,count)};
    vec2<int> const position{line.position.x,line.position.y};
    if(text.shadow) fonts.draw(target,text.font,position + vec2<int>{1,1},visible,0,coverage);
    fonts.draw(target,text.font,position,visible,text.colour,coverage);
    characters -= count;
    if(!characters) break;
  }
}

} // namespace tfx::presentation
