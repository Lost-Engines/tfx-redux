#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "resources/executable_tables.h"
#include "world/scene.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"expected DATA_DIRECTORY"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::world::assets assets{game};
  tfx::world::environment const environment{game, "yugoslav", "yugos_d"};
  tfx::world::scene scene{environment, assets};
  unsigned int constexpr x{112};
  unsigned int constexpr z{57};
  unsigned int constexpr animation_frames{32};
  auto const name{environment.variant(x, z)};
  auto const &definition{assets.object(name)};
  tfx::resources::binary_view const source{definition.bytecode};
  unsigned int const at{definition.entries.at(2)};
  if(source.le16(at) != 64 && source.le16(at) != 65) throw std::runtime_error{"reference target has no leading damage volume"};
  vec3<int32_t> local;
  for(unsigned int axis{0}; axis != 3; ++axis) local[axis] = (static_cast<int32_t>(source.signed_le16(at + 2 + axis * 2)) + source.signed_le16(at + 8 + axis * 2)) / 2;
  vec3d const origin{(x + .5) * tfx::world::cell_units, 0, (z + .5) * tfx::world::cell_units};
  auto const heading{environment.tile(x, z).rotation};
  vec3<int32_t> const reference{origin + vec3d{500, -1000, 3000}};
  scene.set_query_reference(&reference);
  vec3<int32_t> const point{origin + tfx::math::rotate_tile(vec3d{local}, heading)};
  std::cout << name << ' ' << heading << ' ' << static_cast<int32_t>(origin.x) << ' ' << static_cast<int32_t>(origin.y) << ' ' << static_cast<int32_t>(origin.z) << ' ' << local.x << ' ' << local.y << ' ' << local.z << ' ' << reference.x << ' ' << reference.y << ' ' << reference.z << '\n';
  tfx::world::effect_pool effects;
  std::array<std::array<tfx::world::scene_handle, tfx::world::effect_kind_count>, tfx::world::effect_capacity> models;
  for(auto &slot : models) {
    for(unsigned int kind{0}; kind != slot.size(); ++kind) {
      slot[kind] = scene.create_actor(std::string{tfx::world::effect_definitions[kind]}, {0, 0, 0}, {0, 0, 0});
      scene.show_actor(slot[kind], false);
    }
  }
  int phase{-1};
  unsigned int emitted{0};
  unsigned int queued{0};
  unsigned int drawn{0};
  scene.set_effect_sink([&](tfx::world::effect_record const &request){
    ++emitted;
    std::cout << phase << ' ' << std::to_underlying(request.kind) << ' ' << request.position.x << ' ' << request.position.y << ' ' << request.position.z << '\n';
    auto const slot{effects.spawn(vec3<int32_t>{request.position}, request.kind)};
    scene.actor_event(models[slot][std::to_underlying(request.kind)], 0);
  });
  scene.collide_terrain(point, point, 32767, false);
  tfx::render::software_renderer renderer{tables};
  auto const camera{tfx::render::inspection_camera(origin + vec3d{0, -500, -1500}, 0, 0)};
  vec3<int32_t> const camera_origin{camera.position};
  for(phase = 0; phase != static_cast<int>(animation_frames); ++phase) {
    scene.advance(8);                                                          // One native animation advance, with bounded retained backlog.
    scene.draw(renderer, camera, 1, [&]{
      for(auto const slot : effects.active_slots()) {
        ++queued;
        auto const &effect{effects.record(slot)};
        scene.queue_actor(models[slot][std::to_underlying(effect.kind)], vec3d{tfx::world::effect_position(effect.position, camera_origin)}, {0, 0, 0});
      }
    });
    drawn += scene.primitives_drawn;
  }
  if(!emitted || !queued || !drawn) throw std::runtime_error{"authored scenery effect did not reach its same-frame draw queue"};
  std::cerr << "Controlled factory scene: " << emitted << " authored effects, " << queued << " queued pool instances, " << drawn << " drawn primitives over " << animation_frames << " frames\n";
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
