#include <cstdlib>
#include <iostream>
#include "audio/osl_resource.h"

int main() try {
  tfx::resources::bytes bytes(0x600);
  auto const word{[&](unsigned int const at, uint16_t const value){
    bytes[at] = static_cast<uint8_t>(value);bytes[at + 1] = static_cast<uint8_t>(value >> 8);
  }};
  auto const pointer{[&](unsigned int const at, uint16_t const value){ word(at,value);word(at + 2,0); }};
  bytes[0] = 'O';bytes[1] = 'S';bytes[2] = 'L';bytes[3] = '1';bytes[7] = 2;
  word(0x26,1);pointer(0x48,0x2fa);word(0x4c,1);pointer(0x4e,0x80);
  word(0x84,1);bytes[0xa2] = 12;bytes[0xa3] = 127;
  word(0x348,1);pointer(0x44a,0x500);
  word(0x502,1);bytes[0x520] = 10;bytes[0x521] = 75;
  tfx::audio::osl_resource const resource{std::move(bytes)};
  auto const ordinary{resource.instrument_parameters(0)}, alias{resource.instrument_parameters(255)};
  if(ordinary[0x18] != 12 || ordinary[0x19] != 127) throw std::runtime_error{"ordinary instrument prefix changed"};
  if(alias[0x18] != 10 || alias[0x19] != 75) throw std::runtime_error{"native initial-index pattern alias changed"};
  if(!resource.instrument_parameters(254).empty()) throw std::runtime_error{"unrelocated data acquired an invented instrument target"};
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
