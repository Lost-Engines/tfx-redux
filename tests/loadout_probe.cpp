#include "fixture_check.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <utility>
#include "resources/executable_tables.h"
#include "audio/requests.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2 && argc != 3) throw std::runtime_error{"loadout_probe DATA_DIRECTORY|--synthetic|--check|--feedback [FIXTURE]"};
  if(std::string_view{argv[1]} == "--feedback") {
    std::ifstream fixture;
    if(argc == 3) fixture.open(argv[2]);
    std::istream &input{argc == 3 ? fixture : std::cin};
    if(!input) throw std::runtime_error{"cannot read release feedback fixture"};
    unsigned int category{0};
    while(input >> category) {
      auto const sounds{tfx::audio::store_release_sequence(static_cast<uint8_t>(category))};
      if(argc == 3) {
        unsigned int first{0},second{0};
        if(!(input >> first >> second) || sounds != std::array<unsigned int,2>{first,second}) throw std::runtime_error{"native store release sequence differs"};
      } else std::cout << sounds[0] << ' ' << sounds[1] << '\n';
    }
    return EXIT_SUCCESS;
  }
  bool const check{std::string_view{argv[1]} == "--check"};
  bool const synthetic{check || std::string_view{argv[1]} == "--synthetic"};
  std::unique_ptr<tfx::resources::executable_tables> tables;
  tfx::weapons::arming_tables arming;
  std::vector<tfx::weapons::definition> definitions(tfx::weapons::weapon_count);
  std::array<std::array<vec3<int16_t>,10>,3> mounts;
  if(!synthetic) {
    tables = std::make_unique<tfx::resources::executable_tables>(std::filesystem::path{argv[1]} / "TFX.DAT");
    arming = tables->arming;
    definitions = tables->weapons;
    mounts = tables->store_mounts;
  } else {
    arming.empty_role = tfx::weapons::weapon_role::external_tank;
    arming.empty_category = 7;
    arming.next = {7,6,5,4,8,3,2,1,9,10,0};
    for(unsigned int aircraft{0}; aircraft != 3; ++aircraft) {
      arming.defaults[aircraft].resize(6);
      for(unsigned int fit{0}; fit != 6; ++fit) for(unsigned int pair{0}; pair != 5; ++pair) arming.defaults[aircraft][fit][pair] = static_cast<int16_t>((aircraft * 7 + fit * 3 + pair) % 24) - 1;
      for(unsigned int slot{0}; slot != 10; ++slot) mounts[aircraft][slot] = {static_cast<int16_t>(-800 + static_cast<int>(slot) * 200),static_cast<int16_t>(200 + aircraft * 128),static_cast<int16_t>(slot * 64)};
    }
    for(unsigned int index{0}; index != definitions.size(); ++index) {
      definitions[index].role = static_cast<tfx::weapons::weapon_role>(index % 3 == 2 ? 6 : index % 2);
      definitions[index].category = static_cast<uint8_t>((index * 7) % 15);
    }
  }
  std::ifstream fixture;
  if(check) fixture.open(argv[2]);
  std::istream &input{check ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot read loadout fixture"};
  char command{0};
  unsigned int plane{0};
  while(input >> command >> plane) {
    tfx::weapons::armament state{static_cast<tfx::flight::player_aircraft>(plane + 1),definitions,arming};
    std::vector<int64_t> values;
    if(command == 'D') {
      unsigned int fit{0}; input >> fit;
      state.fit_default(fit);
      for(unsigned int slot{0}; slot != 10; ++slot) values.insert(values.end(),{state.pylons[slot].weapon,state.pylons[slot].count,state.pylons[slot].visible});
    } else {
      int selected{0}; unsigned int gun_role{0},guns{0};
      input >> selected >> gun_role >> guns;
      state.selected = static_cast<int16_t>(selected);
      state.guns = guns != 0;
      definitions[0].role = static_cast<tfx::weapons::weapon_role>(gun_role);
      // The cannon role is mutable native state, so reconstruct after supplying it.
      tfx::weapons::armament actual{static_cast<tfx::flight::player_aircraft>(plane + 1),definitions,arming};
      actual.selected = state.selected; actual.guns = state.guns;
      for(auto &slot : actual.pylons) { unsigned int weapon{0},count{0}; input >> weapon >> count; slot.weapon = static_cast<uint8_t>(weapon); slot.count = static_cast<uint8_t>(count); slot.visible = true; }
      tfx::weapons::selection_change change;
      if(command == 'S') {
        unsigned int role{0},different{0};input >> role >> different;
        change = actual.select(static_cast<tfx::weapons::weapon_role>(role),different != 0);
      } else if(command == 'F') {
        unsigned int flags{0},pitch{0},heading{0},roll{0}; int32_t x{0},y{0},z{0},speed{0};
        input >> flags >> x >> y >> z >> pitch >> heading >> roll >> speed;
        auto const shot{actual.fire({
          .position{x,y,z},
          .angles{static_cast<uint16_t>(pitch),static_cast<uint16_t>(heading),static_cast<uint16_t>(roll)},
          .speed{speed},
          .inhibited{(flags & 1) != 0},
          .ejected{(flags & 2) != 0},
          .protected_player{(flags & 4) != 0},
          .unlimited{(flags & 8) != 0},
        },mounts[plane],change)};
        values.push_back(shot.has_value());
        if(shot) values.insert(values.end(),{shot->position.x,shot->position.y,shot->position.z,shot->angles.x,shot->angles.y,shot->angles.z,shot->speed,shot->weapon});
      } else throw std::runtime_error{"unknown loadout probe operation"};
      values.insert(values.end(),{actual.selected,actual.guns,actual.last_released,change.clear_contact,change.clear_tracking});
      for(unsigned int slot{0}; slot != 10; ++slot) values.insert(values.end(),{actual.pylons[slot].weapon,actual.pylons[slot].count,actual.pylons[slot].visible});
    }
    uint64_t digest{14695981039346656037ULL};
    for(auto const value : values) for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
    if(check) {
      if(!tfx::test::fixture_matches(input,digest)) throw std::runtime_error{"native pylon state differs"};
    } else { for(auto const value : values) std::cout << value << ' '; std::cout << '\n'; }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
