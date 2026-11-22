#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "render/near_clip.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{fixture.is_open() ? fixture : std::cin};bool wide;unsigned int cases{0};vec3<int32_t> outside,inside;vec2<uint16_t> outside_uv,inside_uv;
  while(input >> wide >> outside.x >> outside.y >> outside.z >> inside.x >> inside.y >> inside.z >> outside_uv.x >> outside_uv.y >> inside_uv.x >> inside_uv.y) {
    auto const result{tfx::render::intersect_textured_near_plane(outside,inside,outside_uv,inside_uv)};
    auto const projected{tfx::render::project_near_plane(result.position,wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal,{160,100})};
    std::array<int64_t,6> const values{result.position.x,result.position.y,result.uv_q7.x,result.uv_q7.y,projected.x,projected.y};
    if(fixture.is_open()) { for(auto const value : values) { int64_t expected;if(!(input >> expected) || value != expected) throw std::runtime_error{"native texture near-clip mismatch at " + std::to_string(cases)}; } } else { for(auto const value : values) std::cout << value << ' ';std::cout << '\n'; }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
