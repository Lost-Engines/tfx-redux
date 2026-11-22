#include <iostream>
#include <stdexcept>
#include "render/software_renderer.h"
#include "resources/archive.h"

int main(int const argc, char const *const argv[]) {
  if(argc != 2) throw std::runtime_error{"scaled projection probe requires original data directory"};
  tfx::resources::executable_tables const tables{tfx::resources::find_game_archive(argv[1]).parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  tfx::render::camera const camera{
    .position{0, 0, 0},
    .right{1, 0, 0},
    .down{0, 1, 0},
    .forward{0, 0, 1},
    .projection_origin{160, 100},
    .focal_distance{256},
    .projection{tfx::render::projection_mode::original_model},
    .far_distance{100000},
  };
  unsigned int kind;
  unsigned int count;
  double scale;
  while(std::cin >> kind >> count >> scale) {
    tfx::models::primitive primitive{
      .kind{static_cast<tfx::models::primitive_kind>(kind)},
      .vertices{},
      .colour{17},
      .pixel_size{kind == 2 ? 1u : 0u},
      .attachment_slot{},
    };
    for(unsigned int vertex{0}; vertex != count; ++vertex) {
      vec3<int32_t> point;
      std::cin >> point.x >> point.y >> point.z;
      primitive.vertices.push_back({
        .position{vec3d{point} / 65536 * scale},
        .uv{0, 0},
        .projection_scale{scale},
      });
    }
    renderer.output.pixels.fill(99);
    renderer.primitive(primitive, primitive.vertices, camera, {});
    uint64_t hash{14695981039346656037ull};
    unsigned int written{0};
    for(auto const pixel : renderer.output.pixels) {
      hash = (hash ^ pixel) * 1099511628211ull;
      written += pixel != 99;
    }
    std::cout << hash << ' ' << written << '\n';
  }
}
