#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include "resources/executable_tables.h"
#include "world/scene.h"
#include "world/sky.h"

int main(int const argc, char const *const argv[]) try {
  if(argc < 2 || argc > 3) throw std::runtime_error{"expected DATA_DIRECTORY [SCREENSHOT_DIRECTORY]"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::world::assets assets{game};
  for(auto const *const name : {"yugos_dc", "yugos_st", "yugos_nc"}) {
    tfx::world::environment const ground{game, "yugoslav", name};
    tfx::world::environment const clouds{game, ground.lighting.upper_grid, name};
    tfx::world::scene scene{ground, assets, &clouds};
    for(int const height : {-2600, -2400, -2200}) {
      auto const view{tfx::render::inspection_camera({60.5 * tfx::world::cell_units, static_cast<double>(height), 80.5 * tfx::world::cell_units}, 0, -.2)};
      tfx::render::software_renderer renderer{tables};
      renderer.background(view, 0, 32);
      scene.draw(renderer, view, 6, {}, height);
      if(!scene.parts_drawn || !scene.primitives_drawn || !scene.warnings.empty()) throw std::runtime_error{"cloud/base layer scene failed: " + std::string{name} + (scene.warnings.empty() ? "" : *scene.warnings.begin())};
      std::cout << name << ' ' << height << ' ' << scene.parts_drawn << ' ' << scene.primitives_drawn << '\n';
      if(argc == 3) renderer.output.screenshot(std::filesystem::path{argv[2]} / (std::string{name} + std::to_string(height) + ".ppm"), tfx::render::decode_palette(game.read("colours/" + ground.lighting.palette_name + ".col")));
    }
    for(auto const model : {tfx::world::sky_model::sun, tfx::world::sky_model::lightning}) {
      auto const view{tfx::render::inspection_camera({60.5 * tfx::world::cell_units, -500, 80.5 * tfx::world::cell_units}, std::numbers::pi, std::numbers::pi / 4)};
      tfx::render::software_renderer renderer{tables};
      renderer.background(view, 0, 32);
      scene.draw(renderer, view, 1);
      auto const baseline{renderer.output};
      renderer.background(view, 0, 32);
      auto const position{view.position + vec3d{0, -500, -500}};
      scene.queue_model(std::string{tfx::world::sky_model_name(model)}, position);
      scene.draw(renderer, view, 1);
      if(!scene.parts_drawn || !scene.primitives_drawn || !scene.warnings.empty() || baseline.pixels == renderer.output.pixels) throw std::runtime_error{"direct sky shape painted no visible pixels"};
      std::cout << name << ' ' << tfx::world::sky_model_name(model) << ' ' << scene.parts_drawn << ' ' << scene.primitives_drawn << '\n';
      if(argc == 3) renderer.output.screenshot(std::filesystem::path{argv[2]} / (std::string{name} + '-' + std::string{tfx::world::sky_model_name(model)} + ".ppm"), tfx::render::decode_palette(game.read("colours/" + ground.lighting.palette_name + ".col")));
    }
  }
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
