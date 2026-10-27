#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "missions/initialisation.h"
#include "render/view_settings.h"
#include "render/point_size.h"
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
  for(auto const &size : {vec2<int>{320,200},vec2<int>{1280,800},vec2<int>{1920,1080},vec2<int>{3840,2160}}) {
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
    pixel.point_role=tfx::models::point_role::object;renderer.output.pixels.fill(99);renderer.primitive(pixel,vertex,view,{});
    auto const object_area{std::ranges::count(renderer.output.pixels,7)};
    require(object_area>=(size.y>=1080 ? 4 : 1) && object_area<=(size.y>=2160 ? 9 : 4),"aircraft point minimum does not follow resolution");
    pixel.point_role=tfx::models::point_role::light;
    if(size.x>320) {
      auto distant{view};distant.far_distance=std::numeric_limits<double>::infinity();
      std::array<long,2> near_area{}, far_area{};
      for(unsigned size_index=0;size_index<2;++size_index) {
        pixel.pixel_size=size_index+1;
        long previous{std::numeric_limits<long>::max()};
        for(double range : {100.,800.,2000.,8000.,32000.,100000.}) {
          vertex[0].position={0,0,range};renderer.output.pixels.fill(99);
          renderer.primitive(pixel,vertex,distant,{});
          long const count{std::ranges::count(renderer.output.pixels,7)};
          require(count>0 && count<=previous,"lights grew with distance or disappeared");previous=count;
          if(range==100)near_area[size_index]=count;
          if(range==100000)far_area[size_index]=count;
        }
        require(far_area[size_index]<near_area[size_index],"very distant light retained its near-field size");
      }
      require(far_area[1]<=4,"remote lights remain too large at high resolution");
      vertex[0].position={0,0,100};pixel.pixel_size=2;
      // Far-aircraft dots stay readable even through a wide-angle lens.
      pixel.point_role=tfx::models::point_role::object;auto wide_object{view};wide_object.focal_distance/=2;
      renderer.output.pixels.fill(99);renderer.primitive(pixel,vertex,wide_object,{});
      require(std::ranges::count(renderer.output.pixels,7)==object_area,"wide FOV removed the aircraft visibility floor");
      pixel.point_role=tfx::models::point_role::light;
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
  // The minimum also applies to real aircraft whose full meshes have
  // shrunk below it: LOD 0 must not make them harder to see than native dots.
  for(auto const &name : {"efa","f_117a","f_22"}) for(double lod : {0.,.1,1.}) {
    tfx::render::software_renderer renderer{tables};renderer.output.resize({3840,2160});
    renderer.clipping=tfx::render::raster_clip::pixels({0,0},{3839,2159});
    tfx::render::view_settings settings{.resolution{3840,2160},.draw_distance{0}};
    auto const view{tfx::render::scene_camera(tfx::render::inspection_camera({500000,-1000,500000},0,0),settings)};
    tfx::world::scene scene{environment,assets};scene.set_visual_lod_scale(lod);
    auto const actor{scene.create_actor(name,{500000,-1000,500000},{0,0,0},true)};
    for(double distance : {8000.,16000.,32000.}) for(auto const &angles : {vec3<tfx::math::angle>{0,0,0},vec3<tfx::math::angle>{0,16384,0},vec3<tfx::math::angle>{0,32768,0},vec3<tfx::math::angle>{8192,8192,8192}}) {
      scene.place_actor(actor,{500000,-1000,500000+distance},angles);
      renderer.output.pixels.fill(255);scene.draw(renderer,view,1);
      auto const count{std::ranges::count_if(renderer.output.pixels,[](auto c){return c!=255;})};
      if(count<4)std::cerr<<name<<" LOD "<<lod<<" at "<<distance<<" heading "<<angles.y<<": "<<count<<" pixels\n";
      require(count>=4,"real aircraft vanished below its 4K minimum");
      require(scene.warnings.empty(),"aircraft sizing produced scene warnings");
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
  std::cout << "Smoke spacing is independent of update divisions; animated appearance and lifetime are independent of LOD; lights are smaller discs with correct depth visibility; aircraft have a resolution-aware minimum; lights shrink with distance\n";

} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
