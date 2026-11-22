#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "audio/radar_voice.h"

int main(int const argc, char const *const argv[]) try {
  if(argc > 2) throw std::runtime_error{"radar_voice_probe [FIXTURE]"};
  std::ifstream file;
  if(argc == 2) {
    file.open(argv[1]);
    if(!file) throw std::runtime_error{"cannot read radar speech fixture"};
  }
  auto &input{file.is_open() ? file : std::cin};
  int16_t bearing;
  int16_t altitude;
  unsigned int cases{0};
  while(input >> bearing >> altitude) {
    auto const sequence{tfx::audio::radar_report_voice(bearing, altitude)};
    if(file.is_open()) {
      unsigned int count;
      unsigned int sample;
      if(!(input >> count) || count != sequence.size()) throw std::runtime_error{"native radar speech length differs"};
      for(auto const expected : sequence) {
        if(!(input >> sample) || sample != expected) throw std::runtime_error{"native radar speech sample differs"};
      }
    } else {
      std::cout << sequence.size();
      for(auto const sample : sequence) std::cout << ' ' << sample;
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases || !input.eof()) throw std::runtime_error{"incomplete radar speech input"};
  if(file.is_open()) std::cout << cases << " native radar voice requests passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
