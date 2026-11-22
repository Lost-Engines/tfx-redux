#include <cstdlib>
#include <iostream>
#include "cockpit/moving_map.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 3) throw std::runtime_error{"usage: map_display_probe DATA_DIRECTORY MAP_NAME"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  tfx::cockpit::moving_map map{game, argv[2]};
  tfx::cockpit::map_state state;
  while(std::cin >> state.player.x >> state.player.y >> state.player.z >> state.heading >> state.elapsed >> state.stealth) {
    bool navigation{false};
    vec2<int16_t> cell;
    vec2<int> minimum;
    vec2<int> maximum;
    std::cin >> navigation >> cell.x >> cell.y >> state.offset.x >> state.offset.y >> minimum.x >> minimum.y >> maximum.x >> maximum.y;
    state.landing_cell.reset();
    if(navigation) state.landing_cell.emplace(vec2<int16_t>{cell});
    std::array<std::vector<vec3<int32_t>>, 4> contacts;
    for(auto &list : contacts) {
      unsigned int count{0};
      std::cin >> count;
      list.resize(count);
      for(auto &position : list) std::cin >> position.x >> position.y >> position.z;
    }
    std::vector<vec3<int32_t>> targets;
    unsigned int count{0};
    std::cin >> count;
    targets.resize(count);
    for(auto &position : targets) std::cin >> position.x >> position.y >> position.z;
    state.contacts = {
      .enemy_aircraft{contacts[0]},
      .allied_aircraft{contacts[1]},
      .enemy_surface{contacts[2]},
      .allied_surface{contacts[3]},
    };
    state.targets = targets;
    renderer.output.pixels.fill(99);
    tfx::render::framebuffer coverage;
    map.draw(renderer, state, minimum, maximum, &coverage);
    auto const ticks{map.state().ticks};
    for(unsigned int byte{0}; byte != 4; ++byte) std::cout.put(static_cast<char>(ticks >> (byte * 8)));
    std::cout.put(static_cast<char>(map.state().target_colour));
    std::cout.put(static_cast<char>(map.state().landing_colour));
    auto const &clip{renderer.clipping};
    for(auto const value : {clip.glyph_minimum.x, clip.glyph_minimum.y, clip.glyph_limit.x, clip.glyph_limit.y, clip.sprite_minimum.x, clip.sprite_minimum.y, clip.sprite_limit.x, clip.sprite_limit.y}) {
      for(unsigned int byte{0}; byte != 4; ++byte) std::cout.put(static_cast<char>(static_cast<uint32_t>(value) >> (byte * 8)));
    }
    std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
