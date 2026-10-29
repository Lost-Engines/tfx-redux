#include "fixture_check.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <utility>
#include "world/sky.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"expected [FILE]"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int cases{0};char command;
  while(input >> command) {
    std::vector<int64_t> values;
    if(command == 'S') {
      tfx::world::sky_state state;tfx::world::sky_context context;unsigned int pending;
      input >> state.delay >> state.phase >> state.scatter >> state.lightning_position.x >> state.lightning_position.y >> pending
        >> context.suppressed >> context.cloud_layer >> context.storm >> context.animate >> context.layer_y
        >> context.player.x >> context.player.y >> context.player.z >> context.camera_anchor.x >> context.camera_anchor.y >> context.camera_anchor.z;
      state.pending_flash = static_cast<uint8_t>(pending);
      auto const request{tfx::world::advance_sky(state,context)};
      values = {state.delay,state.phase,state.scatter,state.lightning_position.x,state.lightning_position.y,state.pending_flash,request.has_value()};
      if(request) values.insert(values.end(),{std::to_underlying(request->model),request->position.x,request->position.y,request->position.z});
    } else if(command == 'L') {
      bool cloud;int16_t height,offset;int32_t camera;
      input >> cloud >> height >> offset >> camera;
      auto const layer{tfx::world::select_terrain_layer(cloud,height,offset,camera)};
      values = {layer.upper,layer.height};
    } else if(command == 'F') {
      tfx::world::sky_state state;tfx::render::point_projection projection;unsigned int pending;
      input >> pending >> state.lightning_position.x >> state.lightning_position.y;
      state.pending_flash = static_cast<uint8_t>(pending);
      for(auto &v : projection.matrix.data) input >> v;
      input >> projection.position_q16.x >> projection.position_q16.y >> projection.position_q16.z >> projection.origin.x >> projection.origin.y;
      bool const flash{tfx::world::consume_lightning_flash(state,projection)};
      values = {state.pending_flash,flash};
    } else throw std::runtime_error{"unknown sky probe command"};
    if(!input) throw std::runtime_error{"incomplete sky input"};
    uint64_t hash{14695981039346656037ULL};
    for(auto const v : values) for(unsigned int shift{0}; shift != 32; shift += 8) hash = (hash ^ ((static_cast<uint32_t>(v) >> shift) & 255)) * 1099511628211ULL;
    if(argc == 2) { if(!tfx::test::fixture_matches(input,hash)) throw std::runtime_error{"native sky mismatch at case " + std::to_string(cases)}; }
    else std::cout << hash << '\n';
    ++cases;
  }
  return cases ? 0 : 1;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
