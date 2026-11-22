#include <cstdlib>
#include <iostream>
#include "cockpit/scene_display.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 3 && argc != 4) throw std::runtime_error{"usage: scene_display_probe DATA_DIRECTORY ORBIT_STEP"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::cockpit::instrument_assets assets{tables.instruments()};
  assets.camera_orbit_step = static_cast<int16_t>(std::stoi(argv[2]));
  tfx::cockpit::scene_displays displays{assets};
  tfx::render::software_renderer renderer{tables};
  unsigned int page{0};
  unsigned int side{0};
  unsigned int aircraft{0};
  while(std::cin >> page >> side >> aircraft) {
    tfx::cockpit::scene_camera_state state;
    if(argc == 4) state.height_bounds.emplace(-16384, 0);
    state.aircraft = static_cast<tfx::flight::player_aircraft>(aircraft + 1);
    vec2<int16_t> offset;
    vec2<int16_t> projected;
    bool tracking{false};
    vec3<int32_t> target;
    std::cin >> state.position.x >> state.position.y >> state.position.z >> state.angles.x >> state.angles.y >> state.angles.z >> state.free_angles.x >> state.free_angles.y >> tracking >> state.tracking_roll >> target.x >> target.y >> target.z >> state.ground_target.x >> state.ground_target.y >> state.ground_target.z >> offset.x >> offset.y >> projected.x >> projected.y;
    if(tracking) state.tracked_position_q8.emplace(vec3<int32_t>{target});
    std::array<int32_t, 10> request{};
    renderer.output.pixels.fill(99);
    tfx::render::framebuffer coverage;
    displays.draw(renderer, static_cast<tfx::cockpit::display_side>(side), static_cast<tfx::cockpit::display_page>(page), offset, state, [&](tfx::cockpit::scene_camera_request const &view){
      request = {view.position.x, view.position.y, view.position.z, tfx::math::signed_word(view.angles.x), tfx::math::signed_word(view.angles.y), tfx::math::signed_word(view.angles.z), view.minimum.x, view.minimum.y, view.maximum.x, view.maximum.y};
      return vec2<int16_t>{projected};
    }, &coverage);
    auto const &animation{displays.state()};
    auto const output{[](int32_t const value){
      for(unsigned int byte{0}; byte != 4; ++byte) std::cout.put(static_cast<char>(static_cast<uint32_t>(value) >> (byte * 8)));
    }};
    for(auto const value : request) output(value);
    for(int32_t const value : {static_cast<int32_t>(animation.reticle_countdown), static_cast<int32_t>(animation.orbit_angles.x), static_cast<int32_t>(animation.orbit_angles.y), static_cast<int32_t>(animation.orbit_angles.z), static_cast<int32_t>(animation.orbit_step), static_cast<int32_t>(state.free_angles.x), static_cast<int32_t>(state.free_angles.y)}) output(value);
    std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
