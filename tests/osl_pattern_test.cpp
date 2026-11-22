#include <cstdlib>
#include <fstream>
#include <iostream>
#include "audio/osl_resource.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"expected native pattern fixture"};
  std::ifstream input{argv[1]};
  unsigned int flags,channels,stored,count,cases{0};
  while(input >> flags >> channels >> stored >> count) {
    tfx::resources::bytes bytes(count);
    for(auto &byte : bytes) { unsigned int value;input >> value;byte = static_cast<uint8_t>(value); }
    uint64_t expected,hash{14695981039346656037ull};
    unsigned int cursor;
    input >> expected >> cursor;
    tfx::audio::osl_pattern_reader reader{{
      .bytes{bytes},
      .flags{static_cast<uint16_t>(flags)},
      .rows{static_cast<uint16_t>(flags & (flags & 0x8000 ? 0xfff : 0x7fff))},
      .stored_channels{static_cast<uint16_t>(stored)},
    }};
    auto const byte{[&](uint8_t const value){ hash = (hash ^ value) * 1099511628211ull; }};
    for(unsigned int row{0}; row != (flags & (flags & 0x8000 ? 0xfffu : 0x7fffu)); ++row) {
      auto const cells{reader.next(channels)};
      for(unsigned int channel{0}; channel != channels; ++channel) {
        auto const &cell{cells[channel]};
        for(auto const note : cell.notes) byte(note);
        byte(cell.instrument);byte(cell.effect);byte(cell.argument);byte(cell.auxiliary);
      }
      byte(static_cast<uint8_t>(reader.offset()));byte(static_cast<uint8_t>(reader.offset() >> 8));
    }
    if(hash != expected || reader.offset() != cursor) throw std::runtime_error{"original OSL pattern mismatch at case " + std::to_string(cases)};
    ++cases;
  }
  if(!cases || !input.eof()) throw std::runtime_error{"invalid native pattern fixture"};
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
