#include "fixture_check.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "models/shape.h"
#include "render/integer_model_polygon.h"
#include "render/model_transform.h"
#include "render/model_raster.h"

std::vector<uint8_t> model(std::array<vec3<int16_t>,3> const &vertices, uint16_t const scale, bool const shaded, bool const effects, bool const line_only) {
  std::vector<uint8_t> source(50);
  auto const store{[&](unsigned int const at,uint16_t const value) { source.at(at) = static_cast<uint8_t>(value >> 8);source.at(at + 1) = static_cast<uint8_t>(value); }};
  auto const emit{[&](std::initializer_list<uint16_t> const words) { for(auto const value : words) { source.push_back(static_cast<uint8_t>(value >> 8));source.push_back(static_cast<uint8_t>(value)); } }};
  store(0,32768);store(2,32767);store(34,14);store(48,65535);emit({48,scale});
  for(unsigned int i{0}; i != vertices.size(); ++i) emit({98,static_cast<uint16_t>(i),static_cast<uint16_t>(vertices[i].x),static_cast<uint16_t>(vertices[i].y),static_cast<uint16_t>(vertices[i].z)});
  emit({97,3,0});
  if(line_only) emit({1,203,0,2});
  else if(shaded) emit({76,0xc0cf,0xc700,77,0,1,2});
  else emit({2,17,0,1,2});
  if(effects) emit({1,203,0,2,92,204,2,64,51,201,0,52,202,1});
  emit({0,65535});
  return source;
}

int main(int const argc, char const *const argv[]) try {
  bool pixels{false},shadow{false},shaded{false},effects{false},line_only{false};std::ifstream fixture;
  for(int i{1}; i != argc; ++i) {
    std::string_view const option{argv[i]};
    if(option == "--pixels") pixels = true;
    else if(option == "--shadow") shadow = true;
    else if(option == "--shaded") shaded = true;
    else if(option == "--effects") effects = true;
    else if(option == "--lines") line_only = true;
    else { if(fixture.is_open()) throw std::runtime_error{"expected [FILE] [--pixels] [--shadow]"};fixture.open(argv[i]);if(!fixture) throw std::runtime_error{"cannot open model fixture"}; }
  }
  auto &input{fixture.is_open() ? fixture : std::cin};
  bool wide;uint16_t scale;unsigned int draw_scale,cases{0};
  while(input >> wide >> scale >> draw_scale) {
    vec3<tfx::math::angle> angles;input >> angles.x >> angles.y >> angles.z;
    mat3<int32_t> matrix;for(auto &value : matrix.data) input >> value;
    vec3<int32_t> origin;input >> origin.x >> origin.y >> origin.z;
    std::array<vec3<int16_t>,3> vertices;for(auto &point : vertices) input >> point.x >> point.y >> point.z;
    auto view{tfx::render::original_camera({0,0,0},matrix,{160,100},wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal)};
    view.far_distance = 100000;
    tfx::render::model_transform const transform{matrix,angles,tfx::render::scale_model_cache(origin,static_cast<uint8_t>(draw_scale)),shadow ? tfx::render::model_surface::ground_shadow : tfx::render::model_surface::body};
    auto const camera_vertices{[&](std::span<tfx::models::model_vertex const> const source) {
      std::vector<tfx::models::model_vertex> result;
      for(auto const &vertex : source) result.push_back({
        .position{vec3d{vertex.position}},
        .uv{0,0},
        .projection_scale{vertex.projection_scale},
      });
      return result;
    }};
    tfx::models::shape const shape{model(vertices,scale,shaded,effects,line_only)};
    tfx::models::shape_context context;
    context.camera.emplace(tfx::models::shape_camera{
      .basis{mat3<int32_t>{transform.matrix()}},
      .origin{tfx::render::scale_model_cache(origin,static_cast<uint8_t>(draw_scale))},
      .projection_shift{tfx::render::projection_shift(view.opening)},
    });
    context.face_visibility = [&](std::span<tfx::models::model_vertex const> const source,tfx::models::facing_test const purpose) { return tfx::render::model_facing(camera_vertices(source),view,purpose); };
    auto const geometry{tfx::models::interpret(shape,context)};
    if(!geometry.warnings.empty()) throw std::runtime_error{"transformed model emitted warnings"};
    tfx::render::framebuffer output;output.pixels.fill(99);
    bool defined{true};
    tfx::render::colour_map colours{};
    for(unsigned int index{0}; index != colours.size(); ++index) colours[index] = static_cast<uint8_t>(index);
    uint16_t seed{0x4321};
    for(auto const &face : geometry.primitives) {
      if(face.kind == tfx::models::primitive_kind::polygon) {
        if(!tfx::render::integer_model_polygon(output,camera_vertices(face.vertices),view,shadow ? 0 : 17)) defined = false;
      } else {
        auto primitive{face};
        bool const black{shadow && !(face.kind == tfx::models::primitive_kind::point && face.pixel_size)};
        if(black) primitive.colour = 0;
        if(!tfx::render::paint_model_primitive(output,primitive,camera_vertices(face.vertices),view,{},black ? nullptr : &colours,seed)) defined = false;
      }
    }
    uint64_t hash{14695981039346656037ull};for(auto const value : output.pixels) hash = (hash ^ value) * 1099511628211;
    if(pixels) std::cout.write(reinterpret_cast<char const*>(output.pixels.data()),output.pixels.size());
    else if(fixture.is_open()) {
      if(!defined) throw std::runtime_error{"native fixture enters projection overflow"};
      for(auto const value : transform.matrix().data) { if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native model basis mismatch"}; }
      if(!tfx::test::fixture_matches(input,hash)) throw std::runtime_error{"native transformed model mismatch at " + std::to_string(cases)};
    } else { for(auto const value : transform.matrix().data) std::cout << value << ' ';std::cout << hash;if(!defined) std::cout << " overflow";std::cout << '\n'; }
    ++cases;
  }
  return cases ? 0 : 1;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
