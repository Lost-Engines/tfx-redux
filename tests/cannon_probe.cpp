#include "fixture_check.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include "resources/executable_tables.h"
#include "weapons/cannon.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2 && argc != 3) throw std::runtime_error{"cannon_probe DATA_DIRECTORY [FIXTURE]"};
  bool const check{std::string_view{argv[1]} == "--check"};
  bool const synthetic{check || std::string_view{argv[1]} == "--synthetic"};
  std::unique_ptr<tfx::resources::executable_tables> tables;
  if(!synthetic) tables = std::make_unique<tfx::resources::executable_tables>(std::filesystem::path{argv[1]} / "TFX.DAT");
  std::array<vec3<int16_t>,2> const mounts{{{192,640,-2048},{-192,640,-2048}}};
  std::ifstream fixture;
  if(check) fixture.open(argv[2]);
  std::istream &input{check ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot open cannon fixture"};
  unsigned int plane{0},flags{0},barrel{0},pitch{0},heading{0},roll{0};
  int ammunition{0};
  int32_t x{0},y{0},z{0},speed{0};
  while(input >> plane >> flags >> ammunition >> barrel >> x >> y >> z >> pitch >> heading >> roll >> speed) {
    tfx::weapons::cannon_state state{
      .ammunition{static_cast<int16_t>(ammunition)},
      .barrel{static_cast<uint16_t>(barrel)},
    };
    tfx::weapons::cannon_context const context{
      .aircraft{static_cast<tfx::flight::player_aircraft>(plane + 1)},
      .position{x,y,z},
      .angles{static_cast<uint16_t>(pitch),static_cast<uint16_t>(heading),static_cast<uint16_t>(roll)},
      .speed{speed},
      .trigger{(flags & 1) != 0},
      .gun_mode{(flags & 2) != 0},
      .flaps_available{(flags & 4) != 0},
      .communications_available{(flags & 8) != 0},
      .inhibited{(flags & 16) != 0},
      .protected_player{(flags & 32) != 0},
      .crashed{(flags & 64) != 0},
      .ejected{(flags & 128) != 0},
      .landed{(flags & 256) != 0},
    };
    tfx::math::random_state random;
    auto const shot{tfx::weapons::fire_cannon(state,context,synthetic ? std::span<vec3<int16_t> const>{mounts} : std::span<vec3<int16_t> const>{tables->cannon_mounts},random)};
    std::vector<int64_t> values{state.ammunition,state.barrel,random.low,random.high,shot.has_value()};
    if(shot) values.insert(values.end(),{shot->position.x,shot->position.y,shot->position.z,shot->angles.x,shot->angles.y,shot->angles.z,shot->speed,shot->weapon});
    uint64_t digest{14695981039346656037ULL};
    for(auto const value : values) for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
    if(check) {
      if(!tfx::test::fixture_matches(input,digest)) throw std::runtime_error{"native cannon input differs"};
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
