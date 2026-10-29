#include "fixture_check.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include "actors/surface_combat.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  std::istream &input{argc == 2 ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot read surface combat fixture"};
  char command{0};
  while(input >> command) {
    unsigned int side{0},range{0},weapon{0},limit{0},shots{0},low{0},high{0},plane{0},steps{0},count{0};
    vec3<int32_t> position,player;
    int32_t speed{0};
    input >> side >> range >> weapon >> limit >> shots >> low >> high >> plane >> steps >> position.x >> position.y >> position.z >> speed >> player.x >> player.y >> player.z >> count;
    tfx::actors::profile profile{
      .id{0},
      .kind{tfx::actors::profile_kind::surface},
      .side{static_cast<tfx::actors::allegiance>(side)},
      .name{},
      .definition{},
      .source{tfx::resources::bytes(46)},
    };
    profile.source[42] = static_cast<uint8_t>(range);
    profile.source[43] = static_cast<uint8_t>(range >> 8);
    profile.source[44] = static_cast<uint8_t>(weapon);
    profile.source[45] = static_cast<uint8_t>(limit);
    tfx::actors::surface_state state{
      .pose{
        .kind{tfx::actors::record_kind::surface},
        .record{0},
        .profile_id{0},
        .slot{0},
        .side{profile.side},
        .position{vec3<int32_t>{position}},
        .angles{0,0,0},
        .speed{speed},
        .flags{0},
      },
      .commands{},
      .player_shots{static_cast<uint8_t>(shots)},
    };
    tfx::math::random_state random{static_cast<uint16_t>(low),static_cast<uint16_t>(high)};
    std::vector<tfx::actors::surface_air_target> targets;
    for(unsigned int i{0}; i != count; ++i) {
      unsigned int actor{0},flags{0};vec3<int32_t> target;
      input >> actor >> target.x >> target.y >> target.z >> flags;
      targets.push_back({
        .actor{actor},
        .position{vec3<int32_t>{target}},
        .flags{static_cast<uint16_t>(flags)},
      });
    }
    if(command == 'R') {
      std::string hex;input >> hex;
      for(unsigned int i{0}; i < hex.size(); i += 2) state.commands.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i,2),nullptr,16)));
    }
    uint64_t digest{14695981039346656037ULL};
    auto const add{[&](auto const value) { for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL; }};
    auto const emit{[&](tfx::weapons::release const &shot) {
      add(shot.weapon);
      for(auto const value : {shot.position.x,shot.position.y,shot.position.z}) add(value);
      for(auto const value : {shot.angles.x,shot.angles.y,shot.angles.z}) add(value);
      add(shot.speed);
      add(shot.target.actor ? shot.target.actor->pool == tfx::weapons::actor_pool::player ? tfx::actors::player_air_target : shot.target.actor->index : 101u);
      for(auto const value : {shot.target.position.x,shot.target.position.y,shot.target.position.z}) add(value);
    }};
    for(unsigned int step{0}; step != steps; ++step) {
      std::vector<tfx::weapons::release> launches;
      if(command == 'N') launches = tfx::actors::engage_surface_targets(state,profile,{
        .owner{0},
        .player{vec3<int32_t>{player}},
        .aircraft{static_cast<tfx::flight::player_aircraft>(plane + 1)},
        .aircraft_targets{targets},
      },random);
      else if(command == 'R') {
        tfx::actors::advance_surface(state,profile,[&](tfx::actors::surface_request const &request) {
          if(request.kind == tfx::actors::surface_request_kind::nearby_targets) {
            auto shots{tfx::actors::engage_surface_targets(state,profile,{
              .owner{0},
              .player{vec3<int32_t>{player}},
              .aircraft{static_cast<tfx::flight::player_aircraft>(plane + 1)},
              .aircraft_targets{targets},
            },random)};
            for(auto &shot : shots) launches.push_back(std::move(shot));
          } else if(request.kind == tfx::actors::surface_request_kind::ground_target) {
            if(auto shot{tfx::actors::surface_ground_attack(state,0,request,true)}) launches.push_back(std::move(*shot));
          }
        });
      } else if(command == 'G') {
        if(auto const shot{tfx::actors::surface_ground_attack(state,0,{
          .kind{tfx::actors::surface_request_kind::ground_target},
          .cell{static_cast<int16_t>(player.x),static_cast<int16_t>(player.z)},
          .weapon{static_cast<uint16_t>(weapon)},
        },plane == 2)}) launches.push_back(*shot);
      } else throw std::runtime_error{"unknown surface combat operation"};
      add(random.low);add(random.high);add(state.player_shots);add(launches.size());
      for(auto const &shot : launches) emit(shot);
    }
    if(argc == 2) { if(!tfx::test::fixture_matches(input,digest)) throw std::runtime_error{"surface combat differs from original"}; }
    else std::cout << digest << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
