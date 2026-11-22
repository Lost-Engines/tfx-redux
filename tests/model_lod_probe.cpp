#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "models/shape.h"
#include "render/integer_model_polygon.h"
#include "render/model_metrics.h"
#include "render/model_point.h"

std::vector<uint8_t> source_model() {
  std::vector<uint8_t> source(48);
  auto const store{[&](unsigned int const at,uint16_t const value){ source.at(at) = static_cast<uint8_t>(value >> 8);source.at(at + 1) = static_cast<uint8_t>(value); }};
  auto const emit{[&](std::initializer_list<uint16_t> const words){ for(auto const value : words) { source.push_back(static_cast<uint8_t>(value >> 8));source.push_back(static_cast<uint8_t>(value)); } }};
  store(0,32768);
  for(unsigned int i{0}; i != 4; ++i) {
    store(2 + i * 2,std::array<uint16_t,4>{500,2000,8000,32767}[i]);
    store(18 + i * 2,i < 2 ? 0 : static_cast<uint16_t>(i));
    unsigned int const field{34 + i * 2};store(field,static_cast<uint16_t>(source.size() - field));
    if(i < 2) {
      uint16_t const size{i ? uint16_t{200} : uint16_t{40}};
      emit({65535,48,1,98,0,static_cast<uint16_t>(-size),static_cast<uint16_t>(-size),0,98,1,size,static_cast<uint16_t>(-size),0,98,2,0,size,0,97,3,0,2,static_cast<uint16_t>(17 + i * 2),0,1,2,0,65535});
    } else emit({static_cast<uint16_t>(200 + i - 2)});
  }
  return source;
}

int main(int const argc, char const *const argv[]) try {
  bool const pixels{argc == 2 && std::string_view{argv[1]} == "--pixels"};
  std::ifstream fixture;if(argc == 2 && !pixels) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !pixels && !fixture)) throw std::runtime_error{"expected [FILE] or --pixels"};
  auto &input{fixture.is_open() ? fixture : std::cin};bool wide;vec3<int32_t> origin;unsigned int cases{0};
  tfx::models::shape const model{source_model()};
  mat3<int32_t> const identity{32767,0,0,0,32767,0,0,0,32767};
  while(input >> wide >> origin.x >> origin.y >> origin.z) {
    auto view{tfx::render::original_camera({0,0,0},identity,{160,100},wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal)};
    view.far_distance = 100000;                                                // Whole-assembly range admission is a separate gate.
    auto const metrics{tfx::render::measure_model(origin,model.sort_depth_bias)};
    auto const *detail{model.detail_for_distance(metrics.distance)};
    tfx::render::framebuffer output;output.pixels.fill(99);
    if(detail && detail->kind >= tfx::models::detail_kind::pixel_block) {
      if(!tfx::render::model_point(output,origin,view,detail->kind == tfx::models::detail_kind::pixel_block ? 2 : 1,detail->colour)) throw std::runtime_error{"native distant-point IDIV overflow"};
    } else if(detail) {
      auto const transform{[&](std::span<tfx::models::model_vertex const> const source){
        std::vector<tfx::models::model_vertex> result;
        for(auto const &vertex : source) result.push_back({
          .position{vec3d{origin} / 32768 + vertex.position * (32767.0 / 32768)},
          .uv{0,0},
          .shade{vertex.shade},
          .projection_scale{vertex.projection_scale},
        });
        return result;
      }};
      tfx::models::shape_context context{
        .distance{metrics.distance},
        .entry_address{detail->offset + 2},
      };
      context.face_visibility = [&](std::span<tfx::models::model_vertex const> const vertices,tfx::models::facing_test const purpose){ return tfx::render::model_facing(transform(vertices),view,purpose); };
      auto const geometry{tfx::models::interpret(model,context)};
      if(!geometry.warnings.empty()) throw std::runtime_error{"synthetic LOD model warning"};
      for(auto const &face : geometry.primitives) if(!tfx::render::integer_model_polygon(output,transform(face.vertices),view,static_cast<uint8_t>(face.colour))) throw std::runtime_error{"unhandled LOD polygon at case " + std::to_string(cases)};
    }
    int const selected{detail ? static_cast<int>(detail - model.details.data()) : -1};
    uint64_t hash{14695981039346656037ull};for(auto const value : output.pixels) hash = (hash ^ value) * 1099511628211;
    if(pixels) std::cout.write(reinterpret_cast<char const*>(output.pixels.data()),output.pixels.size());
    else if(fixture.is_open()) { int expected;uint64_t digest;if(!(input >> expected >> digest) || expected != selected || digest != hash) throw std::runtime_error{"native complete LOD model mismatch at case " + std::to_string(cases)}; } else std::cout << selected << ' ' << hash << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
