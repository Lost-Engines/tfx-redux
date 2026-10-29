#include "fixture_check.h"
#include <array>
#include <bit>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string_view>
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
  if(argc < 2 || argc > 3) throw std::runtime_error{"air_combat_tests REFERENCE_FIXTURE"};
  std::ifstream input{argv[1]};
  if(!input) throw std::runtime_error{"cannot open air reference fixture"};
  unsigned int ticks{0},intact{0},cases{0};
  int32_t x{0},y{0},z{0},speed{0};
  unsigned int pitch{0},heading{0},roll{0},aircraft{0},flags{0},weapons{0},incoming{0};
  std::string profile_hex,program_hex,formation_hex,manoeuvre_hex;
  unsigned int damage_tick{0};
  while(input >> ticks >> intact >> profile_hex >> program_hex >> formation_hex >> manoeuvre_hex >> x >> y >> z >> pitch >> heading >> roll >> speed >> aircraft >> flags >> weapons >> incoming) {
    if(argc == 3) input >> damage_tick;
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
    std::vector<tfx::actors::manoeuvre> manoeuvres;
    for(unsigned int start{0}; start < manoeuvre_hex.size();) {
      auto const end{manoeuvre_hex.find(',',start)};
      manoeuvres.emplace_back(bytes(manoeuvre_hex.substr(start,end - start)));
      if(end == std::string::npos) break;
      start = static_cast<unsigned int>(end + 1);
    }
    tfx::actors::air_runtime state{program,initial,profiles,formation,{}, {},manoeuvres};
    std::array<std::optional<tfx::actors::air_threat>,tfx::actors::player_air_target> threats;
    if(incoming) for(unsigned int index{0}; index != state.aircraft().size(); ++index) if(state.aircraft()[index]) {
      auto position{vec3<int32_t>{state.aircraft()[index]->flight.pose.position}};
      position.x += incoming % 2 ? 300000 : -300000;
      position.z += 200000;
      threats[index] = tfx::actors::air_threat{
        .position{vec3<int32_t>{position}},
        .weapon{static_cast<uint8_t>(incoming % 3)},
      };
    }
    tfx::actors::air_context const context{
      .intact_ground_target{[intact](vec2<int16_t> const&) { return intact != 0; }},
      .combat{true},
      .weapons_enabled{weapons != 0},
      .player{tfx::actors::air_contact{
        .position{x,y,z},
        .angles{static_cast<tfx::math::angle>(pitch),static_cast<tfx::math::angle>(heading),static_cast<tfx::math::angle>(roll)},
        .speed{speed},
      }},
      .player_aircraft{static_cast<tfx::flight::player_aircraft>(aircraft + 1)},
      .threats{threats},
      .combat_flags{static_cast<uint16_t>(flags)},
    };
    uint64_t digest{14695981039346656037ULL};
    for(unsigned int tick{0}; tick != ticks; ++tick) {
      std::vector<tfx::actors::air_request> requests;
      if(damage_tick && tick == damage_tick) for(unsigned int index{0}; index != state.aircraft().size(); ++index) if(state.aircraft()[index]) {
        bool const collision{argc == 3 && std::string_view{argv[2]} == "--collision"};
        auto const random{state.random_stream()};
        auto death{state.fatal_damage(index,collision ? tfx::actors::air_loss_cause::collision : tfx::actors::air_loss_cause::combat)};
        if(collision && (random.low != state.random_stream().low || random.high != state.random_stream().high)) throw std::runtime_error{"collision loss consumed combat randomness"};
        requests.insert(requests.end(),std::make_move_iterator(death.begin()),std::make_move_iterator(death.end()));
      }
      auto updates{state.advance(context)};
      requests.insert(requests.end(),std::make_move_iterator(updates.begin()),std::make_move_iterator(updates.end()));
      auto const sample{tfx::test::air_snapshot(program,state,requests,nullptr,true)};
      for(unsigned int shift{0}; shift != 64; shift += 8) digest = (digest ^ ((sample >> shift) & 255)) * 1099511628211ULL;
    }
    if(!expected.matches(digest)) throw std::runtime_error{"native aircraft combat differs in case " + std::to_string(cases)};
    ++cases;
  }
  if(!input.eof() || !cases) throw std::runtime_error{"incomplete aircraft route fixture"};
  std::cout << cases << " native aircraft combat and shared-program sequences passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
