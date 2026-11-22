#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>
#include "audio/requests.h"
#include "flight/aerodynamic_powerplant.h"
#include "flight/engine_controls.h"

int main(int const argc, char const *const argv[]) try {
  if(argc > 2) throw std::runtime_error{"expected optional engine-key fixture"};
  std::ifstream fixture;
  if(argc == 2) {
    fixture.open(argv[1]);
    if(!fixture) throw std::runtime_error{"cannot open engine-key fixture"};
  }
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int side;
  unsigned int condition;
  unsigned int other;
  unsigned int afterburner;
  unsigned int control;
  unsigned int display;
  unsigned int cases{0};
  uint64_t factor;
  bool inhibited;
  bool crashed;
  bool disabled;
  while(input >> side >> condition >> other >> afterburner >> factor >> inhibited >> crashed >> disabled >> control >> display) {
    auto const result{tfx::flight::press_engine_key({
      .side{static_cast<tfx::flight::engine_side>(side)},
      .condition{static_cast<tfx::flight::system_condition>(condition)},
      .other_condition{static_cast<tfx::flight::system_condition>(other)},
      .afterburner{afterburner != 0},
      .inhibited{inhibited},
      .crashed{crashed},
      .disabled{disabled},
    })};
    tfx::flight::aerodynamic_powerplant plant;
    plant.afterburner = static_cast<uint8_t>(afterburner);
    plant.afterburner_control = static_cast<uint8_t>(control);
    plant.afterburner_display = static_cast<uint8_t>(display);
    std::vector<uint64_t> events;
    for(auto const event : result.ordered) {
      switch(event) {
      case tfx::flight::engine_key_event::stop_afterburner:
        plant.stop_afterburner();
        events.insert(events.end(), {0, 1, 1, 8, 2, 10});
        break;
      case tfx::flight::engine_key_event::stop_sample:
        events.insert(events.end(), {0, 2, 1, std::to_underlying(tfx::audio::sample_id::engine_stop)});
        break;
      case tfx::flight::engine_key_event::start_samples:
        events.insert(events.end(), {0, 1, tfx::audio::engine_start_sequence.size()});
        events.insert(events.end(), tfx::audio::engine_start_sequence.begin(), tfx::audio::engine_start_sequence.end());
        break;
      case tfx::flight::engine_key_event::start_synth:
        events.insert(events.end(), {1, 0});
        break;
      case tfx::flight::engine_key_event::stop_synth:
        events.insert(events.end(), {1, 2});
        break;
      }
    }
    if(result.running) {
      factor = std::bit_cast<uint64_t>(*result.running ? 1.0 : 0.0);
      condition = std::to_underlying(*result.running ? tfx::flight::system_condition::active : tfx::flight::system_condition::normal);
    }
    std::vector<uint64_t> values{factor, condition, plant.afterburner, plant.afterburner_control, plant.afterburner_display, events.size()};
    values.insert(values.end(), events.begin(), events.end());
    for(auto const value : values) {
      if(argc == 2) {
        uint64_t expected;
        if(!(input >> expected) || expected != value) throw std::runtime_error{"engine-key mismatch at case " + std::to_string(cases)};
      } else {
        std::cout << value << ' ';
      }
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
