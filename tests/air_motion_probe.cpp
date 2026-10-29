#include "fixture_check.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "actors/air_motion.h"
#include "resources/archive.h"
#include "resources/executable_tables.h"

namespace {

uint64_t sample(std::istream &input, tfx::actors::air_performance const &performance, tfx::actors::air_control_context const &context, unsigned int const action, bool const emit) {
  tfx::actors::air_state state;
  auto &pose{state.pose};
  unsigned int pitch{0},heading{0},bank{0},flags{0};
  int rate_pitch{0},rate_heading{0},rate_bank{0},ticks{0};
  int32_t requested{0};
  if(!(input >> pose.position.x >> pose.position.y >> pose.position.z >> pitch >> heading >> bank >> pose.speed >> rate_pitch >> rate_heading >> rate_bank >> state.acceleration >> ticks >> flags >> requested)) throw std::runtime_error{"incomplete air-control sample"};
  pose.angles = vec3<tfx::math::angle>{static_cast<uint16_t>(pitch),static_cast<uint16_t>(heading),static_cast<uint16_t>(bank)};
  pose.flags = static_cast<uint16_t>(flags);
  state.angular_rate = vec3<int16_t>{static_cast<int16_t>(rate_pitch),static_cast<int16_t>(rate_heading),static_cast<int16_t>(rate_bank)};
  state.input_ticks = static_cast<int16_t>(ticks);
  int result{0};
  switch(action) {
  case 0: result = tfx::actors::maximum_turn_rate(state,performance,context); break;
  case 1: tfx::actors::command_speed(state,performance,requested); break;
  case 2: tfx::actors::command_pitch(state,performance,static_cast<uint16_t>(requested),context); break;
  case 3: result = tfx::actors::command_turn(state,performance,static_cast<int16_t>(requested),context); break;
  case 4: result = tfx::actors::altitude_pitch(state,performance,requested); break;
  case 5: tfx::actors::shorten_air_step(state,requested); break;
  case 6:
    if(auto const event{tfx::actors::advance_air_motion(state)}) result = static_cast<int>(*event);
    break;
  case 7: result = tfx::actors::cruise_speed(state,performance,context.turn_envelope); break;
  case 8: tfx::actors::reset_air_controls(state); break;
  default: throw std::runtime_error{"unknown air motion probe action"};
  }
  uint64_t digest{14695981039346656037ULL};
  auto const word{[&](int32_t const value) {
    if(emit) std::cout << value << ' ';
    for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
  }};
  for(auto const value : {result,pose.position.x,pose.position.y,pose.position.z,static_cast<int32_t>(pose.angles.x),static_cast<int32_t>(pose.angles.y),static_cast<int32_t>(pose.angles.z),pose.speed,static_cast<int32_t>(state.angular_rate.x),static_cast<int32_t>(state.angular_rate.y),static_cast<int32_t>(state.angular_rate.z),state.acceleration,static_cast<int32_t>(state.input_ticks),static_cast<int32_t>(pose.flags)}) word(value);
  if(emit) std::cout << '\n';
  return digest;
}

} // anonymous namespace

auto main(int const argc, char const *const argv[])->int try {
  bool const check{argc == 3 && std::string_view{argv[1]} == "--check"};
  bool const synthetic{argc == 2 && std::string_view{argv[1]} == "--synthetic"};
  if(argc != 2 && !check) throw std::runtime_error{"air_motion_probe DATA_DIRECTORY|--synthetic|--check FIXTURE"};
  if(check || synthetic) {
    // Test-authored envelopes, independent of copyrighted game resources.
    std::array<uint8_t,tfx::actors::envelope_bytes> envelope;
    for(unsigned int i{0}; i != envelope.size(); ++i) envelope[i] = static_cast<uint8_t>((i * 37 + 11) % 173);
    std::ifstream file;
    if(check) file.open(argv[2]);
    if(check && !file) throw std::runtime_error{"cannot open air motion fixture"};
    std::istream &input{check ? file : std::cin};
    unsigned int action{0},model{0},cases{0};
    while(input >> action >> model) {
      tfx::actors::air_performance performance{};
      input >> performance.acceleration >> performance.deceleration >> performance.envelope >> performance.turn_dividend >> performance.minimum_speed >> performance.maximum_speed >> performance.minimum_altitude >> performance.maximum_altitude >> performance.afterburner_speed >> performance.maximum_roll_rate;
      tfx::actors::air_control_context const context{
        .model{static_cast<tfx::simulation::flight_model>(model)},
        .turn_envelope{envelope},
      };
      auto const digest{sample(input,performance,context,action,!check)};
      if(check && (!tfx::test::fixture_matches(input,digest))) throw std::runtime_error{"native air motion differs in case " + std::to_string(cases)};
      ++cases;
    }
    if(!input.eof() || !cases) throw std::runtime_error{"invalid air motion fixture"};
    if(check) std::cout << cases << " native aircraft-control cases passed" << std::endl;
  } else {
    auto const archive_path{tfx::resources::find_game_archive(argv[1])};
    tfx::resources::archive const game{archive_path};
    tfx::resources::executable_tables const tables{archive_path.parent_path() / "TFX.DAT"};
    auto const profiles{tfx::actors::profiles(game.read("waybin/craftype.cfg"))};
    unsigned int id{0},action{0},model{0};
    while(std::cin >> id >> action >> model) {
      auto const found{std::ranges::find(profiles,id,&tfx::actors::profile::id)};
      if(found == profiles.end()) throw std::runtime_error{"missing aircraft profile"};
      auto const performance{tfx::actors::decode_air_performance(*found)};
      tfx::actors::air_control_context const context{
        .model{static_cast<tfx::simulation::flight_model>(model)},
        .turn_envelope{tables.aircraft_envelopes},
      };
      sample(std::cin,performance,context,action,true);
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
