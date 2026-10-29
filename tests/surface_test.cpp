#include "fixture_check.h"
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include "actors/surface.h"

namespace {

tfx::resources::bytes bytes(std::string const &hex) {
  if(hex.size() % 2) throw std::runtime_error{"odd reference hex extent"};
  auto const digit{[](char const c)->unsigned int {
    if(c >= '0' && c <= '9') return static_cast<unsigned int>(c - '0');
    if(c >= 'a' && c <= 'f') return static_cast<unsigned int>(c - 'a') + 10;
    throw std::runtime_error{"invalid reference hex digit"};
  }};
  tfx::resources::bytes result;
  for(unsigned int i{0}; i < hex.size(); i += 2) result.push_back(static_cast<uint8_t>((digit(hex[i]) << 4) | digit(hex[i + 1])));
  return result;
}

} // anonymous namespace

auto main(int const argc, char const *const argv[])->int try {
  if(argc != 2) throw std::runtime_error{"surface test requires its native fixture"};
  std::ifstream input{argv[1]};
  if(!input) throw std::runtime_error{"cannot read surface reference fixture"};
  unsigned int ticks{0}, cases{0};
  std::string profile_hex, program_hex;
  while(input >> ticks >> profile_hex >> program_hex) {
    tfx::test::fixture_expectation<uint64_t> const expected{input};
    auto const profiles{tfx::actors::profiles(bytes(profile_hex))};
    tfx::actors::program const program{bytes(program_hex)};
    std::array<tfx::actors::formation_offset,tfx::actors::aircraft_per_group> const formation{};
    auto const actors{tfx::actors::initialise(program,profiles,formation)};
    if(actors.size() != 1) throw std::runtime_error{"expected one reference surface actor"};
    auto state{tfx::actors::create_surface(actors[0],program)};
    uint64_t digest{14695981039346656037ULL};
    for(unsigned int tick{0}; tick != ticks; ++tick) {
      tfx::actors::advance_surface(state,profiles[0]);
      auto const word{[&](uint32_t const value){
        for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((value >> shift) & 255)) * 1099511628211ULL;
      }};
      word(static_cast<uint32_t>(state.pose.position.x));
      word(static_cast<uint32_t>(state.pose.position.y));
      word(static_cast<uint32_t>(state.pose.position.z));
      word(state.pose.angles.y);
      word(static_cast<uint32_t>(state.pose.speed));
      word(static_cast<uint32_t>(state.heading_rate));
      word(static_cast<uint32_t>(state.acceleration));
      word(static_cast<uint32_t>(state.input_ticks));
      word(static_cast<uint32_t>(state.wait_seconds));
      word(state.actions);
      word(state.cursor);
      word(state.pose.flags);
    }
    if(!expected.matches(digest)) throw std::runtime_error{"native surface trace differs in case " + std::to_string(cases)};
    ++cases;
  }
  if(!input.eof() || !cases) throw std::runtime_error{"invalid surface reference fixture"};
  std::cout << cases << " native surface route sequences passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
