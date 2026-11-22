#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include "actors/program.h"
#include "actors/surface.h"
#include "resources/archive.h"
#include "resources/executable_tables.h"

auto main(int const argc, char const *const argv[])->int try {
  if(argc == 2 && std::string{argv[1]} == "move") {
    vec3<int32_t> position;
    vec3<tfx::math::angle> angles;
    int32_t distance{0};
    while(std::cin >> position.x >> position.y >> position.z >> angles.x >> angles.y >> angles.z >> distance) {
      tfx::actors::move(position, angles, distance);
      std::cout << position.x << ' ' << position.y << ' ' << position.z << '\n';
    }
    return EXIT_SUCCESS;
  }
  bool const surface{argc == 3 && std::string{argv[2]} == "surface"};
  if(argc != 2 && !surface) throw std::runtime_error{"actor probe needs a data directory or move"};
  auto const archive_path{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive_path};
  tfx::resources::executable_tables const tables{archive_path.parent_path() / "TFX.DAT"};
  auto const profiles{tfx::actors::profiles(game.read("waybin/craftype.cfg"))};
  std::string name;
  while(std::cin >> name) {
    uint32_t identifier{0};
    auto const [end, error]{std::from_chars(name.data(), name.data() + name.size(), identifier, 16)};
    auto const bytes{error == std::errc{} && end == name.data() + name.size() ? game.read(identifier) : game.read(name)};
    tfx::actors::program const program{bytes};
    auto const actors{tfx::actors::initialise(program, profiles, tables.formations)};
    if(surface) {
      unsigned int ticks{0};
      std::cin >> ticks;
      unsigned int count{0};
      for(auto const &actor : actors) {
        if(actor.kind == tfx::actors::record_kind::surface) ++count;
      }
      std::cout << name << ' ' << count << '\n';
      for(auto const &actor : actors) {
        if(actor.kind != tfx::actors::record_kind::surface) continue;
        auto const profile{std::ranges::find(profiles, actor.profile_id, &tfx::actors::profile::id)};
        auto state{tfx::actors::create_surface(actor, program)};
        uint64_t digest{14695981039346656037ULL};
        for(unsigned int tick{0}; tick != ticks; ++tick) {
          tfx::actors::advance_surface(state, *profile);
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
        std::cout << program.records[actor.record].source_offset << ' ' << actor.profile_id << ' ' << actor.slot << ' ' << digest << '\n';
      }
      continue;
    }
    std::cout << name << ' ' << program.records.size() << ' ' << actors.size() << ' ' << program.player_route.size() << '\n';
    for(auto const &actor : actors) {
      std::cout << static_cast<unsigned int>(actor.kind) << ' ' << program.records[actor.record].source_offset << ' ' << actor.profile_id << ' ' << actor.slot << ' ' << static_cast<unsigned int>(actor.side) << ' ';
      for(unsigned int axis{0}; axis != 3; ++axis) std::cout << actor.position[axis] << ' ';
      for(unsigned int axis{0}; axis != 3; ++axis) std::cout << actor.angles[axis] << ' ';
      std::cout << actor.speed << ' ' << actor.flags << ' ' << static_cast<unsigned int>(actor.leader) << ' ' << static_cast<unsigned int>(actor.formation) << '\n';
    }
    for(auto const &point : program.player_route) std::cout << point.x << ' ' << point.y << ' ' << point.z << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
