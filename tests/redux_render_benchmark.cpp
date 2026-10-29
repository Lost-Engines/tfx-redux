#include <chrono>
#include <iostream>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "missions/initialisation.h"
#include "cockpit/vector_hud.h"
#include "cockpit/weapon_display.h"
#include "render/continuous_pose.h"
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

int main(int const original_argc, char const *const original_argv[]) try {
  bool with_hud{false},virtual_hud{false};std::vector<char const*> arguments;
  for(int i=0;i<original_argc;++i) {
    if(std::string_view{original_argv[i]}=="--hud")with_hud=true;
    else if(std::string_view{original_argv[i]}=="--virtual-hud")with_hud=virtual_hud=true;
    else arguments.push_back(original_argv[i]);
  }
  int const argc{static_cast<int>(arguments.size())};auto const argv{arguments.data()};
  if(argc < 2 || argc > 7 || argc == 5) throw std::runtime_error{"usage: redux_render_benchmark DATA_DIR [DISTANCE [FRAMES [WIDTH HEIGHT [OUTPUT.ppm]]]] [--hud | --virtual-hud]"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::executable_tables const tables{std::filesystem::path{argv[1]} / "TFX.DAT"};
  tfx::world::mission const mission{game,"miss501"};
  auto const setup{tfx::missions::initialise(mission.script)};
  tfx::world::environment const environment{game,mission.grid_name(),setup.environment};
  tfx::world::assets assets{game};tfx::world::scene scene{environment,assets};
  tfx::render::view_settings settings;
  settings.lod_scale = 0; // Keep benchmark geometry comparable when game defaults change.
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
  std::optional<tfx::cockpit::weapon_display> instruments;
  std::optional<tfx::cockpit::vector_hud> hud;
  std::optional<tfx::render::software_renderer> native_ui;
  tfx::cockpit::weapon_display_state hud_state;
  if(with_hud) {
    instruments.emplace(tables.instruments(),game);hud.emplace(tables.instruments());native_ui.emplace(tables);
    hud_state.mode=tfx::cockpit::aiming_mode::air;hud_state.cannon=false;
    hud_state.weapon="AIM 9L";hud_state.range=100;hud_state.ammunition=4;hud_state.limited=true;
    hud_state.flight.speed=4100;hud_state.flight.height=-800;hud_state.flight.throttle=.8f;
    hud_state.flight.mach=.86;hud_state.flight.load=1.2f;
  }
  auto const paint_hud=[&](unsigned frame) {
    if(!with_hud)return;
    hud_state.mode=static_cast<tfx::cockpit::aiming_mode>((frame/8)%3);
    vec3d const attitude{.02*std::sin(frame*.1),.001*frame,.5+.002*frame};
    hud_state.attitude.display_angles=vec3d{attitude};
    hud_state.attitude.display_projection_angles=vec2d{-attitude.x,-attitude.z};
    hud->begin(settings.resolution);
    if(virtual_hud) {
      auto const head=tfx::render::scene_camera(tfx::render::inspection_camera({0,0,-.25},.15*std::sin(frame*.05),.02),settings);
      hud->attach_to_glass(head,{0,0,0},tfx::render::continuous_rotation(vec3d{0,0,0}));
      hud_state.attitude.projection_origin=vec2<int16_t>{tfx::cockpit::vector_hud::glass_origin};
      hud_state.attitude.focal_distance=tfx::cockpit::vector_hud::glass_focal_distance;
    }
    {tfx::cockpit::capture_hud capture{native_ui->output,&*hud};instruments->draw_forward(*native_ui,hud_state);}
    hud->compose(renderer.output,{});
  };
  for(unsigned int warmup{0}; warmup != 2; ++warmup) { tfx::render::paint_horizon(renderer.output,view,0);scene.draw(renderer,view,settings.draw_distance);paint_hud(warmup); }
  std::chrono::duration<double,std::milli> horizon{}, world{}, overlay{},hud_time{};
  CALLGRIND_START_INSTRUMENTATION;CALLGRIND_ZERO_STATS;
  for(unsigned int frame{0}; frame != frames; ++frame) {
    auto const begin{std::chrono::steady_clock::now()};
    tfx::render::paint_horizon(renderer.output,view,0);
    auto const sky{std::chrono::steady_clock::now()};
    scene.draw(renderer,view,settings.draw_distance);
    auto const geometry{std::chrono::steady_clock::now()};
    tfx::render::composite_interface(renderer.output,ui);
    auto const composited{std::chrono::steady_clock::now()};
    paint_hud(frame);
    auto const end{std::chrono::steady_clock::now()};
    horizon += sky - begin;world += geometry - sky;overlay += composited - geometry;hud_time += end - composited;
  }
  CALLGRIND_STOP_INSTRUMENTATION;
  std::cout << "Mean ms: horizon " << horizon.count() / frames << ", scene " << world.count() / frames << ", UI " << overlay.count() / frames << "; " << scene.parts_drawn << " model parts, " << scene.primitives_drawn << " primitives\n";
  if(with_hud)std::cout<<"HUD ms: "<<hud_time.count()/frames<<"; mode "<<(virtual_hud ? "virtual" : "2d")<<'\n';
  if(!scene.warnings.empty()) throw std::runtime_error{*scene.warnings.begin()};
  uint64_t hash{14695981039346656037ull};for(auto const pixel : renderer.output.pixels) hash = (hash ^ pixel) * 1099511628211ull;
  std::cout << "Indexed image FNV-1a: " << hash << '\n';
  if(with_hud) {
    uint64_t coverage_hash{14695981039346656037ull};
    for(auto value:renderer.output.hud_coverage)coverage_hash=(coverage_hash^value)*1099511628211ull;
    std::cout<<"HUD coverage FNV-1a: "<<coverage_hash<<'\n';
    if(argc==7) { std::ofstream file{std::string{argv[6]}+".coverage",std::ios::binary};file.exceptions(std::ios::badbit|std::ios::failbit);file.write(reinterpret_cast<char const*>(renderer.output.hud_coverage.data()),static_cast<std::streamsize>(renderer.output.hud_coverage.size())); }
  }
  if(argc == 7) renderer.output.screenshot(argv[6],tfx::render::decode_palette(game.read("colours/" + environment.lighting.palette_name + ".col")));
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
