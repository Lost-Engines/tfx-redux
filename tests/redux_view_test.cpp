#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include "render/clipping.h"
#include "render/horizon.h"
#include "render/model_point.h"
#include "render/view_settings.h"

namespace {
void require(bool const value, char const *message) {
  if(!value) throw std::runtime_error{message};
}
}

int main() try {
  using namespace tfx::render;
  auto const clip{raster_clip::pixels({0,0},{1000,602})};
  require(clip.glyph_limit == vec2<int>{1001,603},"last render column or row was excluded");
  framebuffer image;image.resize({1001,603});
  require(image.pixels.size() == 1001u * 603u,"non-integer render dimensions were lost");
  bool rejected{false};try { image.resize({0,100}); } catch(std::invalid_argument const&) { rejected = true; }
  require(rejected,"zero width was accepted");
  framebuffer ui;ui.pixels.fill(7);ui.track_writes();
  // Equal-colour and opaque-black writes must remain visible over a different
  // high-resolution background; comparing before/after colours cannot do this.
  ui.write(100 * screen_width + 160,7);
  ui.write(101 * screen_width + 160,0);
  image.pixels.fill(42);composite_interface(image,ui);
  require(std::ranges::count(image.pixels,7) > 0 && std::ranges::count(image.pixels,0) > 0,"UI opacity was inferred from colour differences");
  require(image.pixels.front() == 42,"unpainted UI obscured the scene");
  camera source{
    .position{0,-1000,0},
    .right{1,0,0},
    .down{0,1,0},
    .forward{0,0,1},
  };
  view_settings settings{
    .resolution{1600,900},
    .field_of_view{90},
    .draw_distance{0},
  };
  auto view{scene_camera(source,settings)};
  require(std::abs(view.focal_distance - 800) < 1e-8,"horizontal FOV does not describe the actual target width");
  auto const a{view.project({100,0,1000})}, b{view.project({0,100,1000})};
  require(std::abs((a.x - view.projection_origin.x) - (b.y - view.projection_origin.y)) < 1e-8,"projection stretches pixels on a widescreen target");
  require(std::isinf(view.far_distance),"whole-map rendering retained a far plane");
  source.opening = aperture::missile_nose;
  require(std::abs(scene_camera(source,settings).focal_distance - 400) < 1e-8,"missile camera did not retain its wider FOV");
  source.opening = aperture::normal;source.projection_origin = {44,150};source.focal_distance = 256;
  auto const region{instrument_camera(source,settings,{20,135},{70,169})};
  require(region && region->size.x > 51 && region->size.y > 35,"MFD kept its original small raster");
  require(std::isinf(region->view.far_distance),"MFD retained its local draw-distance limit");
  interface_layout const layout{settings.resolution};
  auto const local{region->view.project({2,3,100}) + vec2d{region->position}};
  auto const expected{layout.project(source.project({2,3,100}))};
  require((local - expected).length_sq() < 1e-12,"MFD raster scaling changed camera framing");
  require(!instrument_camera(source,settings,{20,240},{70,269}),"offscreen MFD allocated a visible target");
  image.resize(settings.resolution);paint_horizon(image,view,0);
  require(image.pixels[10] >= 240 && image.pixels.back() < 240,"wide-FOV horizon lost its sky/ground division");
  // Independent per-pixel reference for the span renderer, including banked,
  // inverted and vertical views and the old blue-circle area near the nadir.
  for(auto const &size : {vec2<int>{320,200},vec2<int>{1001,603}}) {
    settings.resolution = size;settings.field_of_view = 120;
    for(int pose{0}; pose != 12; ++pose) {
      auto source{inspection_camera({0,pose < 6 ? -1000.0 : -20000.0,0},.7,-1.5 + pose * .28)};
      double const bank{pose * .63};auto const right{source.right};
      source.right = right * std::cos(bank) + source.down * std::sin(bank);
      source.down = source.down * std::cos(bank) - right * std::sin(bank);
      auto const camera{scene_camera(source,settings)};
      image.resize(size);paint_horizon(image,camera,1374);
      double const height{std::max(100.0,-camera.position.y / 4)};
      std::array<vec2d,10> const bands{{{698,21998},{940,21979},{1270,21963},{1714,21933},{2315,21878},{3119,21778},{4198,21595},{5638,21265},{7542,20666},{10013,19509}}};
      for(int y{0}; y < size.y; ++y) for(int x{0}; x < size.x; ++x) {
        uint8_t expected{0};
        if(y <= camera.viewport_maximum.y) {
          auto const ray{camera.forward + camera.down * ((y + .5 - camera.projection_origin.y) / camera.focal_distance) + camera.right * ((x + .5 - camera.projection_origin.x) / camera.focal_distance)};
          double const horizontal{std::hypot(ray.x,ray.z)};
          if(ray.y > 0) expected = static_cast<uint8_t>(224 + std::clamp(static_cast<int>(std::min(16.0,(height * horizontal / ray.y + 1374) / 1375)),0,15));
          else {
            expected = 240;
            for(auto const &band : bands) if(-ray.y / std::max(1e-9,horizontal) >= (band.x - height - 350) / band.y) ++expected;
          }
        }
        require(image.pixels[static_cast<size_t>(y * size.x + x)] == expected,"horizon span differs from independent pixel ray");
      }
    }
  }
  std::cout << "Variable targets, square-pixel FOV, explicit UI opacity and wide horizon passed\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
