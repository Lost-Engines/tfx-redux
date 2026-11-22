#include <cstdlib>
#include <fstream>
#include <iostream>
#include "audio/gus_driver.h"
#include "resources/binary.h"

int main(int const argc, char const *const argv[]) try {
  if(argc < 2 || argc > 3) throw std::runtime_error{"expected GUS.COM or --synthetic [FILE]"};
  std::ifstream fixture;
  if(argc == 3) fixture.open(argv[2]);
  if(argc == 3 && !fixture) throw std::runtime_error{"cannot read Gravis register fixture"};
  auto &input{argc == 3 ? fixture : std::cin};
  bool const synthetic{std::string_view{argv[1]} == "--synthetic"};
  tfx::audio::gus_parameters parameters{synthetic ? tfx::audio::gus_parameters{} : tfx::audio::load_gus_parameters(tfx::resources::read_file(argv[1]))};
  unsigned int command;
  unsigned int voice;
  unsigned int volume;
  unsigned int ramp;
  unsigned int control;
  unsigned int base;
  unsigned int cases{0};
  uint32_t begin;
  uint32_t end;
  uint32_t rate;
  while(input >> command >> voice >> begin >> end >> rate >> volume >> ramp >> control >> base) {
    if(synthetic) {
      parameters.frequency_base = static_cast<uint16_t>(base);
      for(unsigned int index{0}; index != parameters.volumes.size(); ++index) {
        parameters.volumes[index] = static_cast<uint16_t>((index * 13u + 17u) & 4095u);
      }
    }
    std::vector<uint32_t> events;
    uint16_t selected{0};
    tfx::audio::gus_driver driver{parameters, {
      .read{[&](uint16_t const port, uint8_t const size){
        if(port != tfx::audio::gus_driver::data_high || size != 1 || (selected != 0x80 && selected != 0x8d)) {
          throw std::runtime_error{"unexpected Gravis register read"};
        }
        auto const value{static_cast<uint16_t>(selected == 0x8d ? ramp : control)};
        events.insert(events.end(), {0, port, size, value});
        return value;
      }},
      .write{[&](tfx::audio::gus_port_write const &write){
        if(write.port == tfx::audio::gus_driver::register_select) selected = write.value;
        events.insert(events.end(), {1, write.port, write.size, write.value});
      }},
    }};
    int result{-1};
    if(command == 1) {
      driver.play(voice, begin, end, rate);
    } else if(command == 2) {
      driver.volume(voice, volume);
    } else if(command == 4) {
      result = driver.stopped(voice);
    } else if(command == 9) {
      driver.stop(voice);
    } else {
      throw std::runtime_error{"unexpected Gravis driver command"};
    }
    std::vector<int64_t> values{result, static_cast<int64_t>(events.size() / 4)};
    for(auto const event : events) values.push_back(event);
    for(auto const value : values) {
      if(argc == 3) {
        int64_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"Gravis register mismatch at case " + std::to_string(cases)};
      } else {
        std::cout << value << ' ';
      }
    }
    if(argc != 3) std::cout << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
