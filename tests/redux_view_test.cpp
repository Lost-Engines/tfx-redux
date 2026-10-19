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
  std::cout << "Variable targets, square-pixel FOV, explicit UI opacity and wide horizon passed\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
