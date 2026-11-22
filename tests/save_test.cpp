#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "presentation/save.h"

namespace {
void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}
tfx::presentation::program schema() {
  tfx::resources::bytes bytes;
  auto const byte{[&](unsigned int const value){ bytes.push_back(static_cast<uint8_t>(value)); }};
  auto const word{[&](unsigned int const value){ byte(value);byte(value >> 8); }};
  auto const zeros{[&](unsigned int const count){ for(unsigned int index{0}; index != count; ++index) byte(0); }};
  auto const string{[&](std::string_view const value, unsigned int const count){ for(unsigned int index{0}; index != count; ++index) byte(index < value.size() ? static_cast<uint8_t>(value[index]) : 0); }};
  word(2);zeros(8);byte(1);string("pilotname",21);zeros(9);string("score",21);
  word(1);zeros(12);word(1);string("main",11);byte(7);zeros(12);
  return tfx::presentation::program{bytes};
}
} // anonymous namespace
int main() try {
  auto const program{schema()};
  tfx::presentation::saved_state state{
    .values{{"pilotname",std::string{"ACE"}},{"score",uint32_t{0xffffffffu}}},
    .reserved{std::vector<uint32_t>(28,0x12345678u)},
    .callers{{0,0}},
    .resume{tfx::presentation::location{0,0}},
  };
  auto const bytes{tfx::presentation::encode_save(program,state)};
  auto const decoded{tfx::presentation::decode_save(program,bytes)};
  require(decoded.values == state.values && decoded.reserved == state.reserved && decoded.callers == state.callers && decoded.resume == state.resume,"native save logical state did not roundtrip");
  require(bytes.size() == 147 && bytes[3] == 0 && bytes[20] == 0,"native fixed string padding or fixed thirty-slot size differs");
  for(unsigned int length{0}; length != bytes.size(); ++length) {
    bool rejected{false};
    try { tfx::presentation::decode_save(program,std::span{bytes}.first(length)); } catch(std::runtime_error const&) { rejected = true; }
    require(rejected,"truncated save was accepted");
  }
  auto corrupt{bytes};corrupt.push_back(0);
  bool rejected{false};
  try { tfx::presentation::decode_save(program,corrupt); } catch(std::runtime_error const&) { rejected = true; }
  require(rejected,"trailing save bytes were accepted");
  state.values["pilotname"] = std::string(21,'A');rejected = false;
  try { tfx::presentation::encode_save(program,state); } catch(std::runtime_error const&) { rejected = true; }
  require(rejected,"oversized pilot name was accepted");
  state.values["pilotname"] = std::string{};state.callers.clear();state.resume.reset();
  auto const detached{tfx::presentation::encode_save(program,state)};
  require(!tfx::presentation::decode_save(program,detached).resume && detached.back() == 255,"native detached resume sentinel differs");
  state.resume = tfx::presentation::location{1,0};rejected = false;
  try { tfx::presentation::encode_save(program,state); } catch(std::runtime_error const&) { rejected = true; }
  require(rejected,"out-of-program saved position was accepted");
  std::cout << "Native fixed-slot saves, reserved dwords, parser positions, padding and malformed bounds passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << "ERROR: " << error.what() << '\n';return EXIT_FAILURE; }
