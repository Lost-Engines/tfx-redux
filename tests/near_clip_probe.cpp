#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "render/near_clip.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};vec3<int32_t> outside,inside;bool wide;vec2<int16_t> origin;
  while(input >> outside.x >> outside.y >> outside.z >> inside.x >> inside.y >> inside.z >> wide >> origin.x >> origin.y) {
    auto const raw{tfx::render::intersect_near_plane(outside,inside)};
    auto const screen{tfx::render::project_near_plane(raw,wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal,origin)};
    int32_t const values[]{raw.x,raw.y,screen.x,screen.y};
    if(argc == 2) {
      for(auto const value : values) { int32_t expected;if(!(input >> expected) || expected != value) throw std::runtime_error{"native near-plane edge mismatch at case " + std::to_string(cases)}; }
    } else { for(auto const value : values) std::cout << value << ' ';std::cout << '\n'; }
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
