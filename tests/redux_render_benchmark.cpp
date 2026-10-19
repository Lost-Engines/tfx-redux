#include <chrono>
#include <iostream>
#include <stdexcept>
#include "missions/initialisation.h"
#include "render/horizon.h"
#include "render/view_settings.h"
#include "world/scene.h"
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#else
#define CALLGRIND_START_INSTRUMENTATION
#define CALLGRIND_STOP_INSTRUMENTATION
#define CALLGRIND_ZERO_STATS
#endif

int main(int const argc, char const *const argv[]) try {
  if(argc < 2 || argc > 7 || argc == 5) throw std::runtime_error{"usage: redux_render_benchmark DATA_DIR [DISTANCE [FRAMES [WIDTH HEIGHT [OUTPUT.ppm]]]]"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::executable_tables const tables{std::filesystem::path{argv[1]} / "TFX.DAT"};
  tfx::world::mission const mission{game,"miss501"};
  auto const setup{tfx::missions::initialise(mission.script)};
  tfx::world::environment const environment{game,mission.grid_name(),setup.environment};
  tfx::world::assets assets{game};tfx::world::scene scene{environment,assets};
  tfx::render::view_settings settings;
  settings.draw_distance = argc > 2 ? static_cast<unsigned int>(std::stoul(argv[2])) : 0;
  unsigned int const frames{argc > 3 ? static_cast<unsigned int>(std::stoul(argv[3])) : 8};
  if(!frames) throw std::runtime_error{"benchmark needs at least one frame"};
  if(argc > 5) settings.resolution = {std::stoi(argv[4]),std::stoi(argv[5])};
  tfx::render::software_renderer renderer{tables};renderer.output.resize(settings.resolution);
  renderer.clipping = tfx::render::raster_clip::viewport({0,0},{settings.resolution.x - 1,settings.resolution.y - 1});
  auto const source{tfx::render::inspection_camera(vec3d{setup.start.position} + vec3d{0,-700,0},.7,-.24)};
  auto const view{tfx::render::scene_camera(source,settings)};
  scene.set_visual_lod_scale(settings.lod_scale);
  tfx::render::framebuffer ui;ui.track_writes();
  for(unsigned int y{120}; y != 200; ++y) for(unsigned int x{0}; x != 320; ++x) ui.write(y * 320 + x,static_cast<uint8_t>(x / 8 + y / 8));
  for(unsigned int warmup{0}; warmup != 2; ++warmup) { tfx::render::paint_horizon(renderer.output,view,0);scene.draw(renderer,view,settings.draw_distance); }
  std::chrono::duration<double,std::milli> horizon{}, world{}, overlay{};
  CALLGRIND_START_INSTRUMENTATION;CALLGRIND_ZERO_STATS;
  for(unsigned int frame{0}; frame != frames; ++frame) {
    auto const begin{std::chrono::steady_clock::now()};
    tfx::render::paint_horizon(renderer.output,view,0);
    auto const sky{std::chrono::steady_clock::now()};
    scene.draw(renderer,view,settings.draw_distance);
    auto const geometry{std::chrono::steady_clock::now()};
    tfx::render::composite_interface(renderer.output,ui);
    auto const end{std::chrono::steady_clock::now()};
    horizon += sky - begin;world += geometry - sky;overlay += end - geometry;
  }
  CALLGRIND_STOP_INSTRUMENTATION;
  std::cout << "Mean ms: horizon " << horizon.count() / frames << ", scene " << world.count() / frames << ", UI " << overlay.count() / frames << "; " << scene.parts_drawn << " model parts, " << scene.primitives_drawn << " primitives\n";
  if(!scene.warnings.empty()) throw std::runtime_error{*scene.warnings.begin()};
  uint64_t hash{14695981039346656037ull};for(auto const pixel : renderer.output.pixels) hash = (hash ^ pixel) * 1099511628211ull;
  std::cout << "Indexed image FNV-1a: " << hash << '\n';
  if(argc == 7) renderer.output.screenshot(argv[6],tfx::render::decode_palette(game.read("colours/" + environment.lighting.palette_name + ".col")));
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
