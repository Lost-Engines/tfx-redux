#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "audio/osl_la_driver.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"expected original START.EXE"};
  std::vector<uint8_t> output;
  tfx::audio::osl_la_driver driver{tfx::resources::read_file(argv[1]),[&](uint8_t const byte){ output.push_back(byte); }};
  unsigned int operation,channel,first,second,bend,note,volume,extra;
  while(std::cin >> operation >> channel >> first >> second >> bend >> note >> volume >> extra) {
    tfx::audio::osl_channel state{
      .pitch_bend{static_cast<uint16_t>(bend)},
      .previous_note{static_cast<uint8_t>(note)},
      .sent_volume{static_cast<uint8_t>(volume)},
    };
    output.clear();
    switch(operation) {
    case 0: driver.initialise();break;
    case 1: driver.note(state,channel,static_cast<uint8_t>(first),static_cast<uint8_t>(second));break;
    case 2: driver.extra_note(state,channel,static_cast<uint8_t>(first));break;
    case 3: driver.silence(channel);break;
    case 4: driver.program(channel,static_cast<uint8_t>(first));break;
    case 5: driver.pan(channel,static_cast<uint8_t>(first));break;
    case 6: driver.volume(state,channel,static_cast<uint8_t>(first));break;
    case 7: driver.pitch(state,channel);break;
    case 8: driver.modulation(channel,static_cast<uint8_t>(first));break;
    case 9: driver.reverb({static_cast<uint8_t>(first),static_cast<uint8_t>(second),static_cast<uint8_t>(extra)});break;
    case 10: {
      unsigned int count;
      std::cin >> count;
      tfx::resources::bytes patch(count);
      for(auto &byte : patch) { unsigned int value;std::cin >> value;byte = static_cast<uint8_t>(value); }
      driver.instrument(first,patch);
      break;
    }
    default: throw std::runtime_error{"unknown LA device operation"};
    }
    std::cout << state.pitch_bend << ' ' << static_cast<unsigned int>(state.previous_note) << ' ' << static_cast<unsigned int>(state.sent_volume) << ' ' << output.size();
    for(auto const byte : output) std::cout << ' ' << static_cast<unsigned int>(byte);
    std::cout << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
