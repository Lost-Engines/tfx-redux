#include "fixture_check.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include "resources/executable_tables.h"
#include "weapons/projectile.h"

namespace {

uint64_t snapshot(tfx::weapons::projectile const &state, tfx::weapons::motion_result const &result, std::ostream *const output) {
  uint64_t digest{14695981039346656037ULL};
  auto const word{[&](auto const value) {
    if(output) *output << static_cast<int64_t>(value) << ' ';
    for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
  }};
  word(state.next_stage); word(std::to_underlying(state.motion)); word(state.ticks);
  for(auto const value : {state.position.x,state.position.y,state.position.z,state.previous.x,state.previous.y,state.previous.z}) word(value);
  for(auto const value : {state.angles.x,state.angles.y,state.angles.z}) word(value);
  word(state.speed); word(state.fall_speed); word(state.acceleration); word(state.range); word(state.weapon); word(state.secondary);
  word(result.reset_visual); word(result.children.size());
  for(auto const &child : result.children) {
    for(auto const value : {child.position.x,child.position.y,child.position.z}) word(value);
    for(auto const value : {child.angles.x,child.angles.y,child.angles.z}) word(value);
    word(child.speed); word(child.weapon);
  }
  return digest;
}

} // anonymous namespace

auto main(int const argc, char const *const argv[])->int try {
  if(argc < 2) throw std::runtime_error{"projectile_probe DATA_DIRECTORY | --synthetic | --check FIXTURE"};
  bool const check{std::string_view{argv[1]} == "--check"};
  bool const synthetic{check || std::string_view{argv[1]} == "--synthetic"};
  bool const verbose{argc == 3 && std::string_view{argv[2]} == "--verbose"};
  std::ifstream fixture;
  if(check) { fixture.open(argv[2]); if(!fixture) throw std::runtime_error{"cannot open projectile fixture"}; }
  std::istream &input{check ? fixture : std::cin};
  std::unique_ptr<tfx::resources::executable_tables> tables;
  if(!synthetic) tables = std::make_unique<tfx::resources::executable_tables>(std::filesystem::path{argv[1]} / "TFX.DAT");
  unsigned int id{0},ticks{0},elapsed{0},owned{0},target_player{0},ejected{0},decoys{0},pitch{0},heading{0},roll{0},cases{0};
  int32_t distance{0},x{0},y{0},z{0},speed{0},tx{0},ty{0},tz{0},category{0},motion{0};
  while(input >> id >> ticks >> elapsed >> owned >> target_player >> ejected >> decoys >> distance >> x >> y >> z >> pitch >> heading >> roll >> speed >> tx >> ty >> tz >> category >> motion) {
    tfx::weapons::definition definition{
      .carried{},
      .fired{},
      .secondary{},
      .category{static_cast<uint8_t>(category)},
      .mass{0},
      .range{10000},
      .launch_speed{300},
      .stages{
        {.ticks{25},.motion{static_cast<tfx::weapons::flight_stage>(motion)}},
        {.ticks{80},.motion{tfx::weapons::flight_stage::accelerating}},
        {.ticks{200},.motion{tfx::weapons::flight_stage::guided}},
        {.ticks{0},.motion{tfx::weapons::flight_stage::finished}},
      },
    };
    if(!synthetic) definition = tables->weapons.at(id);
    auto state{tfx::weapons::launch(definition,static_cast<uint8_t>(id),{x,y,z},{static_cast<tfx::math::angle>(pitch),static_cast<tfx::math::angle>(heading),static_cast<tfx::math::angle>(roll)},speed)};
    tfx::weapons::motion_context const context{
      .elapsed{static_cast<uint16_t>(elapsed)},
      .target{tx,ty,tz},
      .player_owned{owned != 0},
      .launch_anchor{x + 1000,y - 500,z + 2000},
      .player_angles{static_cast<tfx::math::angle>(pitch),static_cast<tfx::math::angle>(heading),static_cast<tfx::math::angle>(roll)},
      .player_speed{speed},
      .target_is_player{target_player != 0},
      .player_ejected{ejected != 0},
      .player_decoys{decoys != 0},
      .player_distance{distance},
    };
    tfx::math::random_state random;
    uint64_t digest{14695981039346656037ULL};
    for(unsigned int tick{0}; tick != ticks; ++tick) {
      auto const result{tfx::weapons::advance_motion(state,definition,context,random)};
      auto const sample{snapshot(state,result,verbose ? &std::cout : nullptr)};
      if(verbose) std::cout << '\n';
      else if(!check) std::cout << sample << '\n';
      for(unsigned int shift{0}; shift != 64; shift += 8) digest = (digest ^ ((sample >> shift) & 255)) * 1099511628211ULL;
    }
    if(check) {
      if(!tfx::test::fixture_matches(input,digest)) throw std::runtime_error{"native projectile sequence differs in case " + std::to_string(cases)};
    }
    ++cases;
  }
  if(!input.eof() || !cases) throw std::runtime_error{"incomplete projectile input"};
  if(check) std::cout << cases << " native projectile motion sequences passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
