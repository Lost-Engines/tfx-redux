#include <cstdlib>
#include <fstream>
#include <iostream>
#include "audio/gus_scheduler.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  std::array<tfx::audio::gus_sample_descriptor, 6> definitions;
  std::optional<tfx::audio::gus_scheduler> scheduler;
  unsigned int reset;
  unsigned int enabled;
  unsigned int initial_priority;
  unsigned int cursor;
  unsigned int elapsed;
  unsigned int priority;
  unsigned int stopped_mask;
  unsigned int count;
  unsigned int cases{0};
  while(input >> reset >> enabled >> initial_priority >> cursor >> elapsed >> priority >> stopped_mask >> count) {
    std::vector<unsigned int> sequence(count);
    for(auto &id : sequence) input >> id;
    for(auto &definition : definitions) {
      int delay;
      unsigned int samples;
      unsigned int available;
      unsigned int resident;
      input >> delay >> samples >> available >> resident;
      if(reset) {
        definition = {
          .sample{
            .retrigger_delay{static_cast<int16_t>(delay)},
            .samples{samples},
            .available{available != 0},
          },
          .resident{resident != 0},
        };
      }
    }
    if(!input) throw std::runtime_error{"truncated Gravis scheduler input"};
    if(reset) {
      scheduler.emplace(definitions, tfx::audio::gus_scheduler_state{
        .enabled{enabled != 0},
        .priority{static_cast<uint8_t>(initial_priority)},
        .resident_cursor{static_cast<uint8_t>(cursor)},
      });
    }
    if(!scheduler) throw std::runtime_error{"missing Gravis scheduler reset"};
    scheduler->enable(enabled != 0);
    scheduler->advance(static_cast<tfx::simulation::timer_ticks>(elapsed));
    std::array<bool, tfx::audio::gus_scheduler::voice_count> stopped;
    for(unsigned int voice{0}; voice != stopped.size(); ++voice) stopped[voice] = (stopped_mask & (1u << voice)) != 0;
    auto const request{scheduler->submit(static_cast<tfx::audio::sample_priority>(priority), sequence, stopped)};
    std::vector<int64_t> values{
      scheduler->value().priority, scheduler->value().resident_cursor, request.has_value(),
      request ? static_cast<int64_t>(request->voice) : -1, request ? static_cast<int64_t>(request->sequence.size()) : 0,
    };
    if(request) {
      for(auto const id : request->sequence) values.push_back(id);
    }
    for(auto const remaining : scheduler->remaining()) values.push_back(remaining);
    for(auto const value : values) {
      if(argc == 2) {
        int64_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"Gravis scheduler mismatch at case " + std::to_string(cases)};
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
