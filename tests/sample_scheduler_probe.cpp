#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include "audio/sample_scheduler.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  std::array<tfx::audio::sample_descriptor, 6> definitions;
  std::optional<tfx::audio::sample_scheduler> scheduler;
  unsigned int reset;
  unsigned int enabled;
  unsigned int active;
  unsigned int initial_priority;
  unsigned int elapsed;
  unsigned int priority;
  unsigned int complete;
  unsigned int count;
  unsigned int cases{0};
  while(input >> reset >> enabled >> active >> initial_priority >> elapsed >> priority >> complete >> count) {
    std::vector<unsigned int> sequence(count);
    for(auto &id : sequence) input >> id;
    for(auto &definition : definitions) {
      int delay;
      unsigned int samples;
      unsigned int available;
      input >> delay >> samples >> available;
      if(reset) {
        definition = {
          .retrigger_delay{static_cast<int16_t>(delay)},
          .samples{samples},
          .available{available != 0},
        };
      }
    }
    if(reset) {
      scheduler.emplace(definitions, tfx::audio::sample_scheduler_state{
        .enabled{enabled != 0},
        .active{active != 0},
        .priority{static_cast<uint8_t>(initial_priority)},
      });
    }
    if(!scheduler) throw std::runtime_error{"missing scheduler reset"};
    if(complete) scheduler->complete();
    scheduler->enable(enabled != 0);
    scheduler->advance(static_cast<tfx::simulation::timer_ticks>(elapsed));
    auto const request{scheduler->submit(static_cast<tfx::audio::sample_priority>(priority), sequence)};
    std::vector<int64_t> values{scheduler->value().active, scheduler->value().priority, request.interrupted, request.started, static_cast<int64_t>(request.sequence.size())};
    for(auto const id : request.sequence) values.push_back(id);
    for(auto const remaining : scheduler->remaining()) values.push_back(remaining);
    for(auto const value : values) {
      if(argc == 2) {
        int64_t expected;
        if(!(input >> expected) || value != expected) throw std::runtime_error{"sample scheduler mismatch at case " + std::to_string(cases)};
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
