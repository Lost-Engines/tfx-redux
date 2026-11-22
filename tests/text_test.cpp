#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "presentation/text.h"

namespace {

void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}
template<typename function> void rejects(function const &action) {
  try {
    action();
  } catch(std::runtime_error const &) {
    return;
  }
  throw std::runtime_error{"invalid text was accepted"};
}

} // anonymous namespace

int main() try {
  tfx::resources::bytes source(1032503);
  source[0x70aa4 + 0x5bb51 + 1] = 5;
  tfx::presentation::fonts const fonts{source};
  tfx::presentation::variables state;
  state["score"] = uint32_t{0xfffffffe};
  state["callsign"] = std::string{"MiG"};
  tfx::presentation::text_block block{
    .position{0, 0},
    .alignment{tfx::presentation::text_alignment::centred},
    .shadow{true},
    .reveal{false},
    .font{1},
    .colour{15},
    .text{"ab %score\r\n%callsign\n"},
  };
  auto text{tfx::presentation::prepare_text(block, state, fonts)};
  require(text.lines.size() == 2 && text.lines[0].text == "AB -2" && text.lines[1].text == "MiG" && text.characters == 8, "native uppercase/substitution order, signed number or newline termination differs");
  require(text.lines[0].position == vec2<int16_t>{140, 0} && text.lines[1].position == vec2<int16_t>{148, 9}, "native per-line centring differs");
  block.text = "A\rBC";
  text = tfx::presentation::prepare_text(block, state, fonts);
  require(text.lines.size() == 2 && text.lines[1].text == "C", "bare native CR must retain its two-byte skip");
  block.text = "discard^note.txt trailing";
  text = tfx::presentation::prepare_text(block, state, fonts, [](std::string_view const name){
    require(name == "note.txt", "external filename boundary differs");
    return std::string{"hello\n"};
  });
  require(text.lines.size() == 1 && text.lines[0].text == "HELLO", "external text must replace the preceding block");
  block.text = "%missing";
  rejects([&]{tfx::presentation::prepare_text(block, state, fonts);});
  block.text = std::string(80, 'A');
  rejects([&]{tfx::presentation::prepare_text(block, state, fonts);});
  block.text = "^self";
  rejects([&]{
    tfx::presentation::prepare_text(block, state, fonts, [](std::string_view){return std::string{"^self"};});
  });
  block.text = "A A";
  block.alignment = tfx::presentation::text_alignment::left;
  block.position = {2, 3};
  text = tfx::presentation::prepare_text(block, state, fonts);
  tfx::render::framebuffer output;
  tfx::render::framebuffer coverage;
  output.pixels.fill(99);
  tfx::presentation::draw_text(output, text, fonts, 1, &coverage);
  require(output.pixels[3 * 320 + 2] == 15 && output.pixels[4 * 320 + 3] == 0 && coverage.pixels[4 * 320 + 3] == 255 && output.pixels[3 * 320 + 18] == 99, "revealed prefix or black shadow coverage differs");
  std::cout << "Native text substitution, external replacement, line coordinates, CR skip, shadows and bounded reveal prefixes passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
