#include <algorithm>
#include <iostream>
#include <stdexcept>
#include "missions/initialisation.h"
#include "render/view_settings.h"
#include "world/scene.h"

void require(bool const condition, char const *message) { if(!condition) throw std::runtime_error{message}; }
int main(int argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"usage: redux_effects_probe DATA_DIR"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::executable_tables const tables{std::filesystem::path{argv[1]} / "TFX.DAT"};
  tfx::world::mission const mission{game,"miss501"};
  auto const setup{tfx::missions::initialise(mission.script)};
  tfx::world::environment const environment{game,mission.grid_name(),setup.environment};
  tfx::world::assets assets{game};
  for(auto const &size : {vec2<int>{320,200},vec2<int>{1280,800},vec2<int>{1920,1080}}) {
    tfx::render::software_renderer renderer{tables};renderer.output.resize(size);
    renderer.clipping = tfx::render::raster_clip::viewport({0,0},{size.x-1,size.y-1});
    tfx::render::view_settings settings{.resolution{vec2<int>{size}}};
    auto const view{tfx::render::scene_camera(tfx::render::inspection_camera({500000,-1000,500000},0,0),settings)};
    for(auto const depth : {30.,100.,700.,1500.,17000.}) {
      tfx::render::pixel_buffer reference;
      unsigned int reference_count{0};
      for(auto const lod : {1.,.1,0.}) {
        tfx::world::scene scene{environment,assets};scene.set_visual_lod_scale(lod);
        renderer.output.pixels.fill(99);
        scene.queue_model("smokecld",{500000,-1000,500000+depth});
        scene.draw(renderer,view,1);
        require(scene.warnings.empty(),"smoke scene produced warnings");
        require(scene.primitives_drawn>0,"smoke scene did not draw");
        if(lod == 1) { reference = renderer.output.pixels;reference_count = scene.primitives_drawn; }
        else require(reference == renderer.output.pixels && reference_count == scene.primitives_drawn,"mesh LOD scale changed smoke density");
      }
    }
    tfx::models::primitive pixel{.kind{tfx::models::primitive_kind::point},.vertices{},.colour{7},.pixel_size{1},.attachment_slot{}};
    std::array<tfx::models::model_vertex,1> vertex{{{.position{0,0,100},.uv{0,0}}}};
    renderer.output.pixels.fill(99);renderer.primitive(pixel,vertex,view,{});
    int const extent{static_cast<int>(std::lround(std::min(size.x/320.,size.y/200.)))};
    require(std::ranges::count(renderer.output.pixels,7) == extent * extent,"point light did not retain its original screen size");
    pixel.pixel_size = 2;renderer.output.pixels.fill(99);renderer.primitive(pixel,vertex,view,{});
    int const double_extent{static_cast<int>(std::lround(2 * std::min(size.x/320.,size.y/200.)))};
    require(std::ranges::count(renderer.output.pixels,7) == double_extent * double_extent,"two-pixel detail did not scale");
  }
  std::cout << "Smoke transitions match across LOD scales 0, 0.1 and 1; pixel lights scale at three resolutions\n";
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
