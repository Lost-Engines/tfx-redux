#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include "models/shape.h"
#include "render/integer_model_polygon.h"
#include "render/model_transform.h"

namespace {
struct program_builder {
  std::vector<uint8_t> bytes{std::vector<uint8_t>(50)};
  program_builder() { store(0,32768);store(2,32767);store(34,14);store(48,65535); }
  void store(unsigned int const at, uint16_t const value) { bytes.at(at) = static_cast<uint8_t>(value >> 8);bytes.at(at + 1) = static_cast<uint8_t>(value); }
  void emit(std::initializer_list<uint16_t> const words) { for(auto const value : words) { bytes.push_back(static_cast<uint8_t>(value >> 8));bytes.push_back(static_cast<uint8_t>(value)); } }
  void vertex(unsigned int const slot, vec3<int16_t> const &v) { emit({98,static_cast<uint16_t>(slot),static_cast<uint16_t>(v.x),static_cast<uint16_t>(v.y),static_cast<uint16_t>(v.z)}); }
  unsigned int call(bool const rotated, unsigned int const slot, vec3<tfx::math::angle> const &angles) {
    if(rotated) emit({73,angles.x,angles.y,angles.z,0,0,0,0,0,0,static_cast<uint16_t>(slot),0});
    else emit({45,static_cast<uint16_t>(slot),0});
    return static_cast<unsigned int>(bytes.size() - 2);
  }
  void triangle(unsigned int const first, unsigned int const colour, std::array<vec3<int16_t>,3> const &v, bool const delta) {
    vertex(first,v[0]);
    for(unsigned int i{1}; i != 3; ++i) {
      if(delta) { auto const d{vec3<int32_t>{v[i]} - vec3<int32_t>{v[i - 1]}};emit({99,static_cast<uint16_t>(d.x),static_cast<uint16_t>(d.y),static_cast<uint16_t>(d.z)}); }
      else vertex(first + i,v[i]);
    }
    emit({97,3,static_cast<uint16_t>(first),2,static_cast<uint16_t>(colour),static_cast<uint16_t>(first),static_cast<uint16_t>(first + 1),static_cast<uint16_t>(first + 2)});
  }
};
}

int main(int const argc, char const *const argv[]) try {
  bool pixels{false};std::ifstream fixture;
  for(int i{1}; i != argc; ++i) if(std::string_view{argv[i]} == "--pixels") pixels = true;else { fixture.open(argv[i]);if(!fixture) throw std::runtime_error{"cannot open articulation fixture"}; }
  auto &input{fixture.is_open() ? fixture : std::cin};unsigned int mode,cases{0};bool wide,delta;uint16_t scale;
  while(input >> mode >> wide >> scale >> delta) {
    mat3<int32_t> matrix;for(auto &value : matrix.data) input >> value;
    vec3<tfx::math::angle> angles,a,b;vec3<int32_t> origin;vec3<int16_t> anchor,second;std::array<vec3<int16_t>,3> vertices;
    input >> angles.x >> angles.y >> angles.z >> origin.x >> origin.y >> origin.z >> a.x >> a.y >> a.z >> b.x >> b.y >> b.z >> anchor.x >> anchor.y >> anchor.z >> second.x >> second.y >> second.z;
    for(auto &v : vertices) input >> v.x >> v.y >> v.z;
    program_builder parent,child;
    unsigned int const kind{mode % 4};
    parent.emit({mode / 4 == 1 ? uint16_t{55} : uint16_t{48},scale});
    if(mode / 4 == 2) parent.emit({54,65533});
    parent.vertex(0,anchor);parent.emit({97,1,0});auto const first{parent.call(kind != 0,0,a)};
    parent.triangle(40,23,vertices,delta);parent.emit({0});parent.store(first,static_cast<uint16_t>(parent.bytes.size() - first));
    parent.vertex(1,second);parent.emit({97,1,1});
    unsigned int nested{0};
    if(kind == 3) parent.emit({81,1,2});else nested = parent.call(kind == 1,1,b);
    parent.triangle(10,19,vertices,delta);parent.emit({0});
    if(kind != 3) { parent.store(nested,static_cast<uint16_t>(parent.bytes.size() - nested));parent.triangle(20,17,vertices,delta);parent.emit({0}); }
    parent.emit({65535});child.triangle(20,17,vertices,delta);child.emit({0,65535});
    tfx::models::shape const source{parent.bytes},attachment{child.bytes};std::array<tfx::models::shape const*,2> const attachments{nullptr,&attachment};
    tfx::render::model_transform const transform{matrix,angles,origin};
    auto view{tfx::render::original_camera({0,0,0},matrix,{160,100},wide ? tfx::render::aperture::missile_nose : tfx::render::aperture::normal)};view.far_distance = 1e10;
    tfx::models::shape_context context;
    context.facing = false;
    context.camera.emplace(tfx::models::shape_camera{
      .basis{mat3<int32_t>{transform.matrix()}},
      .origin{vec3<int32_t>{origin}},
      .projection_shift{tfx::render::projection_shift(view.opening)},
    });
    context.attachments = attachments;
    auto const geometry{tfx::models::interpret(source,context)};
    if(!geometry.warnings.empty() || (!geometry.projection_aborted && geometry.primitives.size() != 3)) throw std::runtime_error{"articulation emitted incomplete geometry"};
    // Projection can deliberately stop where the original IDIV faults. Report
    // that case to the reference harness instead of aborting the whole batch.
    tfx::render::framebuffer output;output.pixels.fill(99);bool defined{!geometry.projection_aborted};std::vector<int32_t> positions;
    for(auto const &face : geometry.primitives) {
      for(auto const &v : face.vertices) for(auto const axis : v.position) positions.push_back(static_cast<int32_t>(std::nearbyint(axis / v.projection_scale * 65536)));
      if(!tfx::render::integer_model_polygon(output,face.vertices,view,static_cast<uint8_t>(face.colour))) defined = false;
    }
    uint64_t hash{14695981039346656037ull};for(auto const value : output.pixels) hash = (hash ^ value) * 1099511628211;
    if(pixels) std::cout.write(reinterpret_cast<char const*>(output.pixels.data()),output.pixels.size());
    else if(fixture.is_open()) {
      for(auto const value : positions) { int32_t expected;if(!(input >> expected) || value != expected) throw std::runtime_error{"native articulated vertex mismatch at " + std::to_string(cases)}; }
      uint64_t expected;if(!defined || !(input >> expected) || hash != expected) throw std::runtime_error{"native articulated frame mismatch at " + std::to_string(cases)};
    } else { for(auto const value : positions) std::cout << value << ' ';std::cout << hash;if(!defined) std::cout << " overflow";std::cout << '\n'; }
    ++cases;
  }
  return cases ? 0 : 1;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
