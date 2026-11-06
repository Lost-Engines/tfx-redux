#include <fstream>
#include <iomanip>
#include <iostream>
#include "missions/initialisation.h"
#include "recording/playback.h"
#include "render/horizon.h"
#include "world/scene.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 4) throw std::runtime_error{"recording_playback_probe DATA_DIRECTORY RECORDING OUTPUT_DIRECTORY < CONTEXT"};

  unsigned int mission{0},seed{0},animation{0},status{0};int16_t camera_y{0},phase{0};
  std::cin >> mission >> camera_y >> phase >> seed >> animation >> status;
  std::array<std::string,10> carried_names;
  for(auto &name : carried_names) std::cin >> std::quoted(name);
  if(!std::cin) throw std::runtime_error{"incomplete replay context"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::world::mission const selected{game,"miss" + std::to_string(mission)};
  auto const setup{tfx::missions::initialise(selected.script)};
  tfx::world::environment environment{game,selected.grid_name(),setup.environment};
  environment.lighting.status = static_cast<uint16_t>(status);
  tfx::world::assets assets{game};
  tfx::world::scene scene{environment,assets};
  tfx::render::software_renderer renderer{tables,static_cast<uint16_t>(seed)};
  std::array<tfx::models::shape const*,10> carried{};
  for(unsigned int slot{0}; slot != carried.size(); ++slot) if(!carried_names[slot].empty()) carried[slot] = &assets.shape(carried_names[slot]);
  scene.set_carried_models(carried);
  std::filesystem::path const output{argv[3]};
  if(!std::filesystem::create_directory(output)) throw std::runtime_error{"replay output directory must be new"};
  auto const infrared{game.read("colours/" + environment.lighting.infrared_palette_name + ".col")};
  {
    std::ofstream target{output / "infrared.dac",std::ios::binary};
    target.write(reinterpret_cast<char const*>(infrared.data()),static_cast<std::streamsize>(infrared.size()));
  }
  tfx::recording::playback movie{argv[2]};
  unsigned int index{0};
  while(auto frame{movie.next()}) {
    auto const &angles{frame->data.camera_angles};
    auto const view{tfx::render::orbit_camera(angles,0,{159,99})};
    tfx::render::paint_horizon(renderer.output,{
      .pitch{angles.x},
      .roll{angles.z},
      .camera_y{camera_y},
      .ground_phase{phase},
      .origin_y{99},
    });
    // Supply the captured native animation gate through the existing empty
    // scene clock. No mission actor, contact or outcome is simulated here.
    scene.advance(animation ? tfx::world::animation_interval + 1 : 0);
    scene.draw(renderer,view,0,{},std::nullopt,0,tfx::world::scene_pass::primary,nullptr,&*frame);
    if(index % 2 == 0) {
      auto const &text{tables.configuration_widgets().recording_play};
      tables.presentation_fonts().draw_driver(renderer.output,{285,1},text,0);
      tables.presentation_fonts().draw_driver(renderer.output,{284,0},text,6);
    }
    std::ofstream target{output / ("replay-" + std::to_string(index) + ".indices"),std::ios::binary};
    if(!target.write(reinterpret_cast<char const*>(renderer.output.pixels.data()),renderer.output.pixels.size())) throw std::runtime_error{"cannot write replay comparison image"};
    ++index;
  }
  if(!scene.warnings.empty()) throw std::runtime_error{*scene.warnings.begin()};
  std::cout << index << " recorded frames rendered with original queue sorting, model geometry, horizon and driver-font PLAY caption\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';return EXIT_FAILURE;
}
