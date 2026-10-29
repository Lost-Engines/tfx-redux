#include "fixture_check.h"
#include <array>
#include <bit>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "air_snapshot.h"
#include "actors/air.h"

namespace {

tfx::resources::bytes bytes(std::string const &hex) {
  if(hex.size() % 2) throw std::runtime_error{"odd air fixture hex extent"};
  tfx::resources::bytes result;
  for(unsigned int at{0}; at != hex.size(); at += 2) {
    unsigned int value{0};
    auto const [end,error]{std::from_chars(hex.data() + at,hex.data() + at + 2,value,16)};
    if(error != std::errc{} || end != hex.data() + at + 2) throw std::runtime_error{"invalid air fixture hex"};
    result.push_back(static_cast<uint8_t>(value));
  }
  return result;
}

} // anonymous namespace

auto main(int const argc, char const *const argv[])->int try {
  if(argc != 2) throw std::runtime_error{"air_tests REFERENCE_FIXTURE"};
  std::ifstream input{argv[1]};
  if(!input) throw std::runtime_error{"cannot open air reference fixture"};
  unsigned int ticks{0},intact{0},cases{0};
  std::string profile_hex,program_hex,formation_hex;
  while(input >> ticks >> intact >> profile_hex >> program_hex >> formation_hex) {
    tfx::test::fixture_expectation<uint64_t> const expected{input};
    auto const profiles{tfx::actors::profiles(bytes(profile_hex))};
    tfx::actors::program const program{bytes(program_hex)};
    auto const formation_bytes{bytes(formation_hex)};
    if(formation_bytes.size() != tfx::actors::aircraft_per_group * 14) throw std::runtime_error{"invalid air formation fixture"};
    tfx::resources::binary_view const view{formation_bytes};
    std::array<tfx::actors::formation_offset,tfx::actors::aircraft_per_group> formation;
    for(unsigned int index{0}; index != formation.size(); ++index) formation[index] = {
      .heading{view.le16(index * 14)},
      .distance{std::bit_cast<int32_t>(view.le32(index * 14 + 6))},
      .height{std::bit_cast<int32_t>(view.le32(index * 14 + 10))},
      .forward_distance{std::bit_cast<int32_t>(view.le32(index * 14 + 2))},
    };
    auto const initial{tfx::actors::initialise(program,profiles,formation)};
    tfx::actors::air_runtime state{program,initial,profiles,formation,{}};
    tfx::actors::air_context const context{
      .intact_ground_target{[intact](vec2<int16_t> const&) { return intact != 0; }},
    };
    uint64_t digest{14695981039346656037ULL};
    for(unsigned int tick{0}; tick != ticks; ++tick) {
      auto const requests{state.advance(context)};
      auto const sample{tfx::test::air_snapshot(program,state,requests)};
      for(unsigned int shift{0}; shift != 64; shift += 8) digest = (digest ^ ((sample >> shift) & 255)) * 1099511628211ULL;
    }
    if(!expected.matches(digest)) throw std::runtime_error{"native aircraft route differs in case " + std::to_string(cases)};
    ++cases;
  }
  if(!input.eof() || !cases) throw std::runtime_error{"incomplete aircraft route fixture"};
  std::cout << cases << " native aircraft route and shared-program sequences passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
