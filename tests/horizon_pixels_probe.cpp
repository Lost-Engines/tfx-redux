#include "fixture_check.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "render/horizon.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"expected FILE"};
  std::ifstream input{argv[1]};if(!input) throw std::runtime_error{"cannot open horizon pixel fixture"};
  unsigned int cases{0};bool wide;
  while(input >> wide) {
    tfx::render::horizon_context context;
    if(!(input >> context.pitch >> context.roll >> context.camera_y >> context.ground_phase >> context.minimum.x >> context.minimum.y >> context.maximum.x >> context.maximum.y >> context.origin_y)) throw std::runtime_error{"incomplete horizon pixel fixture"};
    context.projection_shift = wide ? vec2<unsigned int>{9u,9u} : vec2<unsigned int>{8u,8u};
    tfx::render::framebuffer output;output.pixels.fill(77);tfx::render::paint_horizon(output,context);
    uint64_t hash{14695981039346656037ULL};
    for(auto const pixel : output.pixels) hash = (hash ^ pixel) * 1099511628211ULL;
    tfx::test::fixture_expectation<uint64_t> const expected{input};
    if(!expected.matches(hash)) throw std::runtime_error{"native horizon pixels differ at case " + std::to_string(cases)};
    ++cases;
  }
  return cases ? 0 : 1;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
