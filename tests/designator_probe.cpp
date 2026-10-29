#include "fixture_check.h"
#include "cockpit/designator.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
auto pose_values(tfx::sensors::designator const &state, vec3<int32_t> const &point, vec2<int16_t> const &origin) {
  auto const pose{tfx::cockpit::designator_pose(state)};
  auto const projected{pose.camera(vec2d{origin}).projector().integer(point).value_or(vec2<int16_t>{-1,-1})};
  std::array<int32_t,17> result;
  unsigned int at{0};
  for(auto const value : {pose.position_q16.x,pose.position_q16.y,pose.position_q16.z}) result[at++] = value;
  for(auto const value : pose.matrix.data) result[at++] = value;
  result[at++] = projected.x;result[at++] = projected.y;
  result[at++] = pose.horizon_angles->x;result[at++] = pose.horizon_angles->y;result[at] = pose.horizon_camera_y;
  return result;
}
void write(int32_t const value) {
  for(unsigned int byte{0}; byte != 4; ++byte) std::cout.put(static_cast<char>(static_cast<uint32_t>(value) >> (byte * 8)));
}
}
int main(int const argc, char const *const argv[]) try {
  bool const pose_only{argc >= 2 && std::string_view{argv[1]} == "--camera"};
  if(pose_only) {
    std::ifstream fixture;
    if(argc == 3) { fixture.open(argv[2]);if(!fixture) throw std::runtime_error{"cannot open designator pose fixture"}; }
    auto &input{fixture.is_open() ? fixture : std::cin};
    tfx::sensors::designator state;vec3<int32_t> point;vec2<int16_t> origin;
    unsigned int count{0};
    while(input >> state.pitch >> state.heading >> state.camera.x >> state.camera.y >> state.camera.z >> point.x >> point.y >> point.z >> origin.x >> origin.y) {
      auto const values{pose_values(state,point,origin)};
      if(fixture.is_open()) {
        for(auto const value : values) { if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native designator pose mismatch at " + std::to_string(count)}; }
      } else { for(auto const value : values) std::cout << value << ' ';std::cout << '\n'; }
      ++count;
    }
    return count ? 0 : 1;
  }
  if(argc != 2) throw std::runtime_error{"usage: designator_probe DATA_DIRECTORY | --camera [FIXTURE]"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::executable_tables const tables{std::filesystem::path{argv[1]} / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  tfx::cockpit::designator_display display{tables.instruments(),game};
  bool tracking;unsigned int zoom,aircraft,flags;vec3<int32_t> player,target;vec3<tfx::math::angle> angles;vec2<int16_t> origin;tfx::sensors::designator state;
  while(std::cin >> tracking >> state.pitch >> state.heading >> zoom >> player.x >> player.y >> player.z >> angles.x >> angles.y >> aircraft >> flags >> target.x >> target.y >> target.z >> origin.x >> origin.y) {
    state.zoom = static_cast<uint8_t>(zoom);angles.z = 0;
    if(tracking) tfx::sensors::follow_designation(state,player,target);
    else tfx::sensors::aim_designator(state,player,angles,static_cast<tfx::flight::player_aircraft>(aircraft + 1),{
      .up{(flags & 1) != 0},
      .down{(flags & 2) != 0},
      .left{(flags & 4) != 0},
      .right{(flags & 8) != 0},
    });
    auto const pose{pose_values(state,state.point,origin)};
    renderer.output.pixels.fill(99);tfx::render::framebuffer coverage;
    display.draw(renderer,{static_cast<int16_t>(pose[12]),static_cast<int16_t>(pose[13])},tracking,&coverage);
    for(int32_t const value : {static_cast<int32_t>(state.pitch),static_cast<int32_t>(state.heading),static_cast<int32_t>(state.zoom),state.camera.x,state.camera.y,state.camera.z,state.point.x,state.point.y,state.point.z}) write(value);
    for(auto const value : pose) write(value);
    write(display.state().search);write(display.state().tracking);
    std::cout.write(reinterpret_cast<char const*>(renderer.output.pixels.data()),renderer.output.pixels.size());
    std::cout.write(reinterpret_cast<char const*>(coverage.pixels.data()),coverage.pixels.size());
  }
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
