#include "fixture_check.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "render/horizon.h"

int main(int const argc, char const *const argv[]) try {
  bool const wide{argc == 2 && (std::string_view{argv[1]} == "--pixels-wide" || std::string_view{argv[1]} == "--surfaces-wide")};
  bool const pixels{argc == 2 && (std::string_view{argv[1]} == "--pixels" || std::string_view{argv[1]} == "--pixels-wide")};
  bool const dump{argc == 2 && (std::string_view{argv[1]} == "--surfaces" || std::string_view{argv[1]} == "--surfaces-wide")};
  std::ifstream fixture;if(argc == 2 && !dump && !pixels) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !dump && !pixels && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 && !dump && !pixels ? fixture : std::cin};unsigned int cases{0};tfx::render::horizon_context context;
  if(wide) context.projection_shift = {9u,9u};
  while(input >> context.pitch >> context.roll >> context.camera_y >> context.ground_phase >> context.minimum.x >> context.minimum.y >> context.maximum.x >> context.maximum.y >> context.origin_y) {
    if(pixels) {
      tfx::render::framebuffer output;output.pixels.fill(77);
      tfx::render::paint_horizon(output,context);
      std::cout.write(reinterpret_cast<char const*>(output.pixels.data()),output.pixels.size());++cases;continue;
    }
    auto const surfaces{tfx::render::horizon_surfaces(context)};
    uint64_t hash{14695981039346656037ULL};
    auto const add{[&](int32_t const value) { if(dump) std::cout << value << ' '; for(unsigned int shift{0}; shift != 32; shift += 8) hash = (hash ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL; }};
    add(static_cast<int32_t>(surfaces.size()));
    for(auto const &surface : surfaces) {
      add(surface.colour);add(surface.rectangle);add(static_cast<int32_t>(surface.vertices.size()));
      for(auto const &v : surface.vertices) { add(v.x);add(v.y); }
    }
    if(dump) std::cout << '\n';
    else if(argc == 2) { if(!tfx::test::fixture_matches(input,hash)) throw std::runtime_error{"native horizon mismatch at case " + std::to_string(cases)}; }
    else std::cout << hash << '\n';
    ++cases;
  }
  return cases ? 0 : 1;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
