#include "fixture_check.h"
#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "weapons/loadout.h"

auto main(int const argc, char const *const argv[])->int try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  auto &input{fixture.is_open() ? fixture : std::cin};
  tfx::weapons::arming_tables tables;
  tables.next = {7,6,5,4,8,3,2,1,9,10,0};
  tables.empty_role = tfx::weapons::weapon_role::external_tank;
  tables.empty_category = 7;
  std::vector<tfx::weapons::definition> definitions(tfx::weapons::weapon_count);
  for(unsigned int index{0}; index != definitions.size(); ++index) {
    definitions[index].role = static_cast<tfx::weapons::weapon_role>(index % 3 == 2 ? 6 : index % 2);
    definitions[index].category = static_cast<uint8_t>((index * 7) % 15);
  }
  unsigned int plane,flags,guns;
  int selected;
  unsigned int cases{0};
  while(input >> plane >> flags >> selected >> guns) {
    tfx::weapons::armament armament{static_cast<tfx::flight::player_aircraft>(plane + 1),definitions,tables};
    armament.selected = static_cast<int16_t>(selected);
    armament.guns = guns != 0;
    for(auto &pylon : armament.pylons) {
      unsigned int weapon,count;
      input >> weapon >> count;
      pylon = {
        .weapon{static_cast<uint8_t>(weapon)},
        .count{static_cast<uint8_t>(count)},
        .visible{true},
      };
    }
    vec3<int32_t> position;
    vec3<uint16_t> angles;
    int32_t speed;
    double fuel,capacity;
    input >> position.x >> position.y >> position.z >> angles.x >> angles.y >> angles.z >> speed >> fuel >> capacity;
    std::array<vec3<int16_t>,tfx::flight::store_slots> mounts;
    for(unsigned int slot{0}; slot != mounts.size(); ++slot) mounts[slot] = {static_cast<int16_t>(-800 + static_cast<int>(slot) * 200),static_cast<int16_t>(200 + plane * 128),static_cast<int16_t>(slot * 64)};
    tfx::weapons::selection_change change;
    std::vector<int64_t> shots;
    if((flags & 16) && !(flags & 7)) {
      armament.jettison({
        .position{vec3<int32_t>{position}},
        .angles{vec3<uint16_t>{angles}},
        .speed{speed},
        .target{
          .actor{tfx::weapons::actor_identity{.pool{tfx::weapons::actor_pool::aircraft}}},
        },
        .unlimited{(flags & 8) != 0},
      },mounts,change,[&](auto const &shot) {
        shots.insert(shots.end(),{shot.position.x,shot.position.y,shot.position.z,shot.angles.x,shot.angles.y,shot.angles.z,shot.speed,shot.weapon,shot.target.actor.has_value(),shot.jettisoned});
      });
      if(fuel > capacity) fuel = capacity;
    }
    std::vector<int64_t> values{static_cast<int64_t>(shots.size() / 10)};
    values.insert(values.end(),shots.begin(),shots.end());
    values.insert(values.end(),{armament.selected,armament.guns,armament.last_released,change.clear_contact,change.clear_tracking});
    for(unsigned int slot{0}; slot != tfx::flight::store_slots; ++slot) values.insert(values.end(),{armament.pylons[slot].weapon,armament.pylons[slot].count,armament.pylons[slot].visible});
    auto const bits{std::bit_cast<uint64_t>(fuel)};
    values.insert(values.end(),{static_cast<uint32_t>(bits),static_cast<uint32_t>(bits >> 32)});
    uint64_t digest{14695981039346656037ULL};
    for(auto const value : values) for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
    if(fixture.is_open()) {
      if(!tfx::test::fixture_matches(input,digest)) throw std::runtime_error{"native jettison state differs"};
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!input.eof() || !cases) throw std::runtime_error{"invalid jettison input"};
  if(fixture.is_open()) std::cout << cases << " original stores jettison cases agree\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
