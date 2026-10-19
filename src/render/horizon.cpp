#include "horizon.h"
#include "integer_polygon.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <cmath>
#include <utility>

namespace tfx::render {
namespace {
int16_t word(int32_t const value) { return math::signed_word(static_cast<uint16_t>(value)); }
int32_t wrap(uint32_t const value) { return std::bit_cast<int32_t>(value); }

struct horizon_projection {
  vec2<int32_t> horizontal;
  vec2<int32_t> vertical;
  vec2<int32_t> depth;
};

class band_builder {
private:
  horizon_context const &context;
  horizon_projection projection;
  int32_t sine, cosine;
  bool vertical;
  bool reverse;
  std::array<vec2<int16_t>,4> quad;
  std::vector<horizon_surface> surfaces;

  void emit(uint8_t const colour, std::span<vec2<int16_t> const> const vertices) {
    horizon_surface surface{
      .colour{colour},
      .vertices{},
    };
    for(auto const &point : vertices) surface.vertices.emplace_back(point);
    surfaces.push_back(std::move(surface));
  }
  vec2<int16_t> edge(vec2<int16_t> const &point, bool const opposite) const {
    // Native horizontal/vertical strip constructors retain signed IDIV and
    // their five-unit bias. Their far edge is 320/200, although stored corners
    // on a horizontal strip use X=319.
    int const along{vertical ? point.y : point.x}, across{vertical ? point.x : point.y};
    int const extent{vertical ? screen_height : screen_width};
    int const distance{opposite ? extent - along : along};
    int const slope{distance * sine / cosine};
    int const coordinate{across + (opposite ? -slope : slope) - (cosine * 5 >> math::matrix_fraction_bits)};
    return vertical ? vec2<int16_t>{word(coordinate),static_cast<int16_t>(opposite ? screen_height : 0)} : vec2<int16_t>{static_cast<int16_t>(opposite ? screen_width - 1 : 0),word(coordinate)};
  }

public:
  band_builder(horizon_context const &source, math::angle const pitch, math::angle const roll) : context{source} {
    auto const &table{math::sine_table()};
    unsigned int const p{static_cast<unsigned int>(pitch) >> 6}, r{static_cast<unsigned int>(roll) >> 6};
    auto const sin{[&](unsigned int const a) { return static_cast<int32_t>(table[a & (math::angle_steps - 1)]); }};
    auto const cos{[&](unsigned int const a) { return sin(a + math::angle_steps / 4); }};
    int const positive_sine{sin(p + r) >> 1}, negative_sine{sin(p - r) >> 1};
    int const positive_cosine{cos(p + r) >> 1}, negative_cosine{cos(p - r) >> 1};
    // Native 0xc81e6 forms sum/difference identities with separately rounded
    // halves, rather than multiplying ideal sine and cosine values.
    projection = {
      .horizontal{positive_sine - negative_sine,negative_cosine - positive_cosine},
      .vertical{positive_cosine + negative_cosine,positive_sine + negative_sine},
      .depth{-sin(p),cos(p)},
    };
    sine = sin(r);cosine = cos(r);
    vertical = (roll >= 0x2000 && roll <= 0x6000) || (roll >= 0xa000 && roll <= 0xe000);
    if(vertical) std::swap(sine,cosine);
    reverse = word(roll + 0x6000) < 0;
  }
  vec2<int16_t> project(int const y, int const z) const {
    auto const product{[&](vec2<int32_t> const &row) {
      return wrap(static_cast<uint32_t>(y) * static_cast<uint32_t>(row.x) + static_cast<uint32_t>(z) * static_cast<uint32_t>(row.y));
    }};
    int32_t const depth{std::max(16,product(projection.depth) >> 16)};
    int32_t const x{std::clamp((product(projection.horizontal) >> context.projection_shift.x) / depth,-32000,32000)};
    int32_t const vertical_position{std::clamp((product(projection.vertical) >> context.projection_shift.y) / depth,-32000,32000)};
    int16_t const origin_x{word(word(context.minimum.x + context.maximum.x) >> 1)};
    return {word(x + origin_x),word(vertical_position + context.origin_y)};
  }
  void start(vec2<int16_t> const &near, vec2<int16_t> const &far) {
    quad[0] = edge(far,false);quad[1] = edge(far,true);
    quad[2] = edge(near,true);quad[3] = edge(near,false);
  }
  void next(vec2<int16_t> const &far) {
    quad[3] = vec2<int16_t>{quad[0]};quad[2] = vec2<int16_t>{quad[1]};
    quad[0] = edge(far,false);quad[1] = edge(far,true);
  }
  void paint(uint8_t const colour) {
    if(reverse) {
      std::array<vec2<int16_t>,4> const reversed{vec2<int16_t>{quad[3]},vec2<int16_t>{quad[2]},vec2<int16_t>{quad[1]},vec2<int16_t>{quad[0]}};
      emit(colour,reversed);
    } else emit(colour,quad);
  }
  void cap(uint8_t const colour) {
    paint(colour);
    if(!reverse) {
      if(std::min(quad[2].y,quad[3].y) >= screen_height) return;
      std::vector<vec2<int16_t>> points;
      points.emplace_back(quad[3]);points.emplace_back(quad[2]);
      points.emplace_back(std::min<int16_t>(quad[2].x,screen_width - 1),std::max<int16_t>(quad[2].y,screen_height - 1));
      points.emplace_back(std::min<int16_t>(quad[3].x,0),std::max<int16_t>(quad[3].y,screen_height - 1));
      if(quad[3].x > 0) points.emplace_back(0,std::min<int16_t>(quad[3].y,0));
      emit(colour,points);
    } else {
      if(std::max(quad[3].y,quad[2].y) <= 0) return;
      std::vector<vec2<int16_t>> points;
      points.emplace_back(quad[2]);points.emplace_back(quad[3]);
      if(vertical) {
        points.emplace_back(std::max<int16_t>(quad[3].x,screen_width - 1),std::min<int16_t>(quad[3].y,0));
        points.emplace_back(std::max<int16_t>(quad[2].x,screen_width - 1),std::min<int16_t>(quad[2].y,screen_height - 1));
      } else {
        points.emplace_back(std::min<int16_t>(quad[3].x,0),std::min<int16_t>(quad[3].y,screen_height - 1));
        points.emplace_back(std::min<int16_t>(quad[2].x,0),std::min<int16_t>(quad[2].y,0));
        if(quad[2].y > 0) points.emplace_back(screen_width - 1,0);
      }
      emit(colour,points);
    }
  }
  std::vector<horizon_surface> finish() { return std::move(surfaces); }
};

} // anonymous namespace

void horizon_phase::advance(int16_t const movement) {
  // Native 0xcac04 selects its period from the adjacent retained mode word,
  // adjusts at most once, and stores only the counter word.
  int32_t constexpr near_period{1375}, far_period{2750};
  int32_t const period{period_mode == 0 ? near_period : far_period};
  int32_t next{static_cast<int32_t>(sample()) + (movement >> 2)};
  if(word(next) > period) next -= period;
  if(word(next) < 0) next += period;
  counter = word(next);
}

std::vector<horizon_surface> horizon_surfaces(horizon_context const &context) {
  math::angle pitch{context.pitch}, roll{context.roll};
  if(std::abs(static_cast<int>(math::signed_word(pitch))) >= 0x4000) {
    pitch = static_cast<math::angle>(0x8000 - pitch);
    roll = static_cast<math::angle>(roll + 0x8000);
  }
  int const signed_pitch{math::signed_word(pitch)};
  uint8_t constexpr sky_near_colour{240}, ground_near_colour{224}, steep_ground_colour{253};
  if(signed_pitch > 0x3000) {
    horizon_surface surface{
      .colour{steep_ground_colour},
      .vertices{},
      .rectangle{true},
    };
    surface.vertices.emplace_back(context.minimum);
    surface.vertices.emplace_back(context.maximum.x,context.minimum.y);
    surface.vertices.emplace_back(context.maximum);
    surface.vertices.emplace_back(context.minimum.x,context.maximum.y);
    std::vector<horizon_surface> result;result.push_back(std::move(surface));return result;
  }
  band_builder bands{context,pitch,roll};
  int constexpr minimum_horizon_height{100};
  // The original NEG is narrowed before SAR: INT16_MIN stays negative and
  // takes the minimum-height path, rather than becoming a high-altitude view.
  int const height{std::max(minimum_horizon_height,static_cast<int>(word(-static_cast<int32_t>(context.camera_y))) >> 2)};
  bands.start(bands.project(height,16),bands.project(height,0x4b32 - context.ground_phase));
  bands.next(bands.project(word(height - 164),0x55ef));bands.paint(sky_near_colour);
  int const sky_height{height + 350};
  struct sky_band { int16_t height_offset, depth; };
  std::array<sky_band,10> constexpr sky_bands{{
    {
      .height_offset{698},
      .depth{21998},
    },
    {
      .height_offset{940},
      .depth{21979},
    },
    {
      .height_offset{1270},
      .depth{21963},
    },
    {
      .height_offset{1714},
      .depth{21933},
    },
    {
      .height_offset{2315},
      .depth{21878},
    },
    {
      .height_offset{3119},
      .depth{21778},
    },
    {
      .height_offset{4198},
      .depth{21595},
    },
    {
      .height_offset{5638},
      .depth{21265},
    },
    {
      .height_offset{7542},
      .depth{20666},
    },
    {
      .height_offset{10013},
      .depth{19509},
    },
  }};
  if(signed_pitch >= -10000) {
    uint8_t colour{sky_near_colour};
    for(auto const &band : sky_bands) {
      if(band.height_offset == 10013 && sky_height < -10000) break;
      bands.next(bands.project(word(sky_height - band.height_offset),band.depth));bands.paint(++colour);
    }
    if(sky_height >= -13000) {
      bands.next(bands.project(word(sky_height - 13098),17675));bands.paint(++colour);
    }
    if(sky_height >= -16000) {
      for(auto const &band : std::array<sky_band,3>{{
        {
          .height_offset{16686},
          .depth{14337},
        },
        {
          .height_offset{20189},
          .depth{8740},
        },
        {
          .height_offset{22000},
          .depth{32},
        },
      }}) {
        bands.next(bands.project(word(sky_height - band.height_offset),band.depth));bands.paint(colour);
      }
      if(signed_pitch > 0x2680) { bands.next(bands.project(word(sky_height - 22000),-8000));bands.paint(colour); }
    }
  }
  if(signed_pitch <= 0x1aa0) {
    int16_t constexpr ground_spacing{1375};
    bands.start(bands.project(height,16),bands.project(height,ground_spacing - context.ground_phase));bands.cap(ground_near_colour);
    for(unsigned int i{2}; i <= 16; ++i) {
      unsigned int const colour{std::min(i - 1,13u)};
      bands.next(bands.project(height,static_cast<int>(i) * ground_spacing - context.ground_phase));
      bands.paint(static_cast<uint8_t>(ground_near_colour + (i == 16 ? 14 : colour)));
    }
    bands.next(bands.project(word(height - 50),22000));bands.paint(239);
  }
  return bands.finish();
}

void paint_horizon(framebuffer &output, horizon_context const &context) {
  for(auto const &surface : horizon_surfaces(context)) {
    if(surface.rectangle) {
      int const left{std::max(0,static_cast<int>(context.minimum.x))},right{std::min(screen_width - 1,static_cast<int>(context.maximum.x))};
      int const top{std::max(0,static_cast<int>(context.minimum.y))},bottom{std::min(screen_height - 1,static_cast<int>(context.maximum.y))};
      if(left <= right) for(int y{top}; y <= bottom; ++y) std::fill_n(output.pixels.begin() + y * screen_width + left,right - left + 1,surface.colour);
    } else integer_polygon(output,surface.vertices,surface.colour,context.minimum,context.maximum);
  }
}

void paint_horizon(framebuffer &output, camera const &view, int16_t const ground_phase) {
  double const height{std::max(100.0,-view.position.y / 4)};
  std::array<vec2d,10> const sky_bands{{
    {698,21998},{940,21979},{1270,21963},{1714,21933},{2315,21878},
    {3119,21778},{4198,21595},{5638,21265},{7542,20666},{10013,19509},
  }};
  output.pixels.fill(0);
  int const bottom{std::min(output.size.y - 1,static_cast<int>(view.viewport_maximum.y))};
  for(int y{std::max(0,static_cast<int>(view.viewport_minimum.y))}; y <= bottom; ++y) {
    auto const row{view.forward + view.down * ((y + .5 - view.projection_origin.y) / view.focal_distance)};
    for(int x{0}; x != output.size.x; ++x) {
      auto const ray{row + view.right * ((x + .5 - view.projection_origin.x) / view.focal_distance)};
      double const horizontal{std::hypot(ray.x,ray.z)};
      uint8_t colour{240};
      if(ray.y > 0) {
        double const distance{height * horizontal / ray.y};
        int const band{static_cast<int>(std::min(16.0,(distance + ground_phase) / 1375))};
        colour = static_cast<uint8_t>(224 + std::clamp(band,0,15));
        if(distance < 300) colour = 253;
      } else {
        double const slope{-ray.y / std::max(1e-9,horizontal)};
        for(auto const &band : sky_bands) if(slope >= (band.x - height - 350) / band.y) ++colour;
      }
      output.pixels[static_cast<size_t>(y * output.size.x + x)] = colour;
    }
  }
}

} // namespace tfx::render
