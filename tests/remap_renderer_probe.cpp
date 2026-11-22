#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "render/software_renderer.h"
#include "world/assets.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"expected DATA_DIRECTORY"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::world::assets const assets{game};
  mat3<int32_t> const identity{32767,0,0,0,32767,0,0,0,32767};
  auto const view{tfx::render::original_camera({0,0,0},identity,{160,100})};
  unsigned int kind,row,colour,override_map,gradient;
  while(std::cin >> kind >> row >> colour >> override_map >> gradient) {
    tfx::render::colour_map remap{assets.model_colours(static_cast<uint8_t>(row))};
    if(override_map == 1) remap[0] = 77;
    if(override_map == 2) remap[19] = 0;
    tfx::render::software_renderer renderer{tables};renderer.output.pixels.fill(199);
    if(kind <= 1) {
      tfx::models::primitive source{
        .kind{kind == 0 ? tfx::models::primitive_kind::polygon : tfx::models::primitive_kind::point},
        .vertices{},
        .colour{colour},
        .radius{20},
        .attachment_slot{},
      };
      if(kind == 0) for(auto const &point : std::array<vec3d,4>{{{-64,-34,128},{-16,-34,128},{-16,14,128},{-64,14,128}}}) source.vertices.push_back({
        .position{vec3d{point}},
        .uv{0,0},
      });
      else source.vertices.push_back({
        .position{-40,-10,128},
        .uv{0,0},
      });
      renderer.primitive(source,source.vertices,view,{},&remap);
    } else {
      std::array<tfx::render::screen_vertex,4> vertices{{
        {
          .position{20,20},
          .uv{0,0},
          .shade{-1},
        },
        {
          .position{100,20},
          .uv{0,0},
          .shade{-1},
        },
        {
          .position{100,100},
          .uv{0,0},
          .shade{-1},
        },
        {
          .position{20,100},
          .uv{0,0},
          .shade{-1},
        },
      }};
      if(kind == 2) {
        std::array<std::array<unsigned int,4>,4> constexpr shades{{{{192,192,192,192}},{{192,207,207,192}},{{192,192,207,207}},{{192,207,200,197}}}};
        for(unsigned int i{0}; i != vertices.size(); ++i) vertices[i].shade = shades.at(gradient)[i];
        renderer.polygon(vertices,200,-1,{},&remap);
      } else {
        std::vector<uint8_t> texture(256 * 192,static_cast<uint8_t>(colour));
        renderer.polygon(vertices,0,-1,texture,&remap);
      }
    }
    std::cout.write(reinterpret_cast<char const*>(renderer.output.pixels.data()),renderer.output.pixels.size());
  }
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
