#include <algorithm>
#include <iostream>
#include <stdexcept>
#include "missions/initialisation.h"
#include "render/view_settings.h"
#include "world/scene.h"
#include "world/trails.h"

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
    auto const area{std::ranges::count(renderer.output.pixels,7)};
    if(size.x>320) {
      int const extent{std::max(1,static_cast<int>(std::lround(view.focal_distance/512)))};
      require(area>0 && area<=extent*extent,"point light too large");
      if(extent>=3) require(area<extent*extent,"light is still a square");
    } else require(area==1,"original single pixel changed");
    pixel.pixel_size = 2;renderer.output.pixels.fill(99);renderer.primitive(pixel,vertex,view,{});
    auto const double_area{std::ranges::count(renderer.output.pixels,7)};
    require(double_area>area,"two-pixel light lost its relative size");
    pixel.model_proxy=true;renderer.output.pixels.fill(99);renderer.primitive(pixel,vertex,view,{});
    require(std::ranges::count(renderer.output.pixels,7)==1,"distant aircraft became a larger light");
    pixel.model_proxy=false;
    if(size.x>320) {
      // An object's queue position must not decide whether a light is hidden.
      tfx::models::primitive wall{.kind{tfx::models::primitive_kind::polygon},.vertices{},.colour{5},.attachment_slot{}};
      for(double depth : {50.,100.,200.}) for(bool point_first : {false,true}) {
        std::array<tfx::models::model_vertex,4> face{{
          {.position{-40,-40,depth},.uv{0,0}}, {.position{40,-40,depth},.uv{0,0}},
          {.position{40,40,depth},.uv{0,0}}, {.position{-40,40,depth},.uv{0,0}},
        }};
        renderer.output.pixels.fill(99);renderer.begin_points();
        if(point_first) renderer.primitive(pixel,vertex,view,{});
        renderer.primitive(wall,face,view,{});
        if(!point_first) renderer.primitive(pixel,vertex,view,{});
        renderer.finish_points();
        require((std::ranges::count(renderer.output.pixels,7)>0)==(depth>=100),"point visibility followed painter order instead of actual depth");
      }
      // Texture transparency follows the lighting lookup, including a
      // nonzero source texel mapped to transparent and zero mapped opaque.
      std::array<tfx::models::model_vertex,4> const occluder{{
        {.position{-40,-40,50},.uv{0,0}}, {.position{40,-40,50},.uv{0,0}},
        {.position{40,40,50},.uv{0,0}}, {.position{-40,40,50},.uv{0,0}},
      }};
      tfx::render::point_visibility visibility;
      std::vector<uint8_t> texels(256*192,9);
      tfx::render::colour_map remap{};
      for(bool opaque : {false,true}) {
        remap[9]=opaque ? 5 : 0;
        visibility.begin(size);visibility.add(occluder,view,-1,texels,&remap);
        require(visibility.visible(view.projection_origin,100)!=opaque,"texture remap transparency lost during point occlusion");
      }
      std::ranges::fill(texels,0);remap[0]=5;
      visibility.begin(size);visibility.add(occluder,view,-1,texels,&remap);
      require(!visibility.visible(view.projection_origin,100),"source texel zero incorrectly treated as transparent before remapping");
      visibility.begin(size);visibility.add(occluder,view,0,{},nullptr);
      for(int dx : {0,1}) {
        vec2d const ray{view.projection_origin+vec2d{static_cast<double>(dx),0}};
        bool const gap{((static_cast<int>(ray.x)+static_cast<int>(ray.y))&1)==0};
        require(visibility.visible(ray,100)==gap,"stipple gaps incorrectly occlude point sprites");
      }
      auto wide{view};wide.focal_distance/=2;
      renderer.output.pixels.fill(99);renderer.primitive(pixel,vertex,wide,{});
      require(std::ranges::count(renderer.output.pixels,7)<double_area,"wider FOV did not reduce light's apparent size");
    }

  }
  // Full SSD animation, not just the initial bare smoke shape.
  std::vector<uint64_t> reference_frames;
  for(double lod : {1.,.1,0.}) {
    tfx::world::scene scene{environment,assets};scene.set_visual_lod_scale(lod);
    auto const smoke{scene.create_actor("smokecld",{500000,-1000,500700},{0,0,0})};
    tfx::render::software_renderer renderer{tables};renderer.output.resize({1280,800});renderer.clipping=tfx::render::raster_clip::pixels({0,0},{1279,799});
    auto const view{tfx::render::scene_camera(tfx::render::inspection_camera({500000,-1000,500000},0,0),{})};
    for(unsigned frame{0}; frame<20; ++frame) {
      scene.advance(8);renderer.output.pixels.fill(99);scene.draw(renderer,view,1);
      uint64_t hash{14695981039346656037ull};for(auto c : renderer.output.pixels) hash=(hash^c)*1099511628211ull;
      hash=(hash^scene.actor_destroyed(smoke))*1099511628211ull;
      if(lod==1) reference_frames.push_back(hash);else require(hash==reference_frames[frame],"LOD changed animated smoke lifetime or appearance");
    }
  }
  // The same continuous path divided into fast, slow and irregular updates
  // must yield precisely the same sample positions, including fractional Q8.
  std::vector<vec3<int32_t>> reference_samples;
  for(int step : {1,3,8,19,41,200}) {
    tfx::world::trail_sampler sampler;std::vector<vec3<int32_t>> samples;
    auto const at{[](int t) { return vec3<int32_t>{16000000+t*257,-256000+t*19,8000000+t*385}; }};
    for(int t{0}; t<200; t+=step) sampler.advance(at(t),at(std::min(200,t+step)),[&](auto const &p){samples.emplace_back(p);});
    if(step==1) reference_samples=samples;else require(samples==reference_samples,"smoke spacing depends on frame duration");
    require(samples.size()>30,"trail sampler emitted too few puffs");
  }
  auto narrow{tfx::render::scene_camera(tfx::render::inspection_camera({0,0,0},0,0),{})};
  auto wide{narrow};wide.focal_distance/=2;
  require(!narrow.sphere_visible({90,0,100},1) && wide.sphere_visible({90,0,100},1),"smoke visibility ignores camera FOV");
  std::cout << "Smoke spacing and animated lifetime are independent of frame divisions and LOD; lights are smaller discs with correct depth visibility; aircraft proxies remain single pixels\n";

} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
