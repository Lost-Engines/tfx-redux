#include "fixture_check.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <utility>
#include "sensors/ground.h"
#include "render/point_projection.h"
#include "world/instance.h"

int main(int const argc, char const *const argv[]) try {
  bool const check{argc == 2};std::ifstream fixture;
  if(check) fixture.open(argv[1]);
  std::istream &input{check ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot read ground sensor fixture"};
  char command{0};
  while(input >> command) {
    std::vector<int64_t> values;
    if(command == 'C') {
      vec3<int32_t> player;vec2<int32_t> destination;
      tfx::render::point_projection projection;
      input >> player.x >> player.y >> player.z >> destination.x >> destination.y;
      for(auto &value : projection.matrix.data) input >> value;
      input >> projection.position_q16.x >> projection.position_q16.y >> projection.position_q16.z >> projection.origin.x >> projection.origin.y;
      auto const target{tfx::sensors::acquire_cruise_target(player,destination,[&](vec3<int32_t> const &position) {
        if(auto const point{projection.integer(position)}) return vec2<int16_t>{*point};
        return vec2<int16_t>{-1,-1};
      })};
      values = {target.position_q8.x,target.position_q8.y,target.position_q8.z,target.reticle_q8.x,target.reticle_q8.y,target.projected.x,target.projected.y};
    } else if(command == 'L') {
      unsigned int zoom{0};vec3<int32_t> player,target;input >> zoom >> player.x >> player.y >> player.z >> target.x >> target.y >> target.z;
      tfx::sensors::designator state{.zoom{static_cast<uint8_t>(zoom)}};
      tfx::sensors::follow_designation(state,player,target);
      values = {state.pitch,state.heading,state.zoom,state.camera.x,state.camera.y,state.camera.z,state.point.x,state.point.y,state.point.z};
    } else if(command == 'G' || command == 'I') {
      unsigned int pitch{0},heading{0},zoom{0},plane{0},flags{0},ap{0},ah{0};vec3<int32_t> player;
      input >> pitch >> heading >> zoom >> player.x >> player.y >> player.z >> ap >> ah >> plane >> flags;
      tfx::sensors::designator state{
        .pitch{static_cast<uint16_t>(pitch)},
        .heading{static_cast<uint16_t>(heading)},
        .zoom{static_cast<uint8_t>(zoom)},
      };
      if(command == 'G') {
        tfx::sensors::aim_designator(state,player,{static_cast<uint16_t>(ap),static_cast<uint16_t>(ah),0},static_cast<tfx::flight::player_aircraft>(plane + 1),{
          .up{(flags & 1) != 0},
          .down{(flags & 2) != 0},
          .left{(flags & 4) != 0},
          .right{(flags & 8) != 0},
        });
        values = {state.pitch,state.heading,state.zoom,state.camera.x,state.camera.y,state.camera.z,state.point.x,state.point.y,state.point.z};
      } else {
        auto const point{tfx::sensors::ground_intersection(player,state.pitch,state.heading)};
        values = {point.x,point.y,point.z};
      }
    } else if(command == 'D') {
      vec3<int32_t> a,b;input >> a.x >> a.y >> a.z >> b.x >> b.y >> b.z;
      values.push_back(tfx::math::distance_estimate(a,b));
    } else if(command == 'T' || command == 'M') {
      unsigned int count{0},type_count{0};int cursor{0},type{0};unsigned int side{0};vec3<int32_t> player;
      input >> cursor >> type >> side >> player.x >> player.y >> player.z >> count >> type_count;
      std::vector<tfx::sensors::surface_contact> contacts;
      for(unsigned int i{0}; i != count; ++i) {
        unsigned int slot{0},heading{0},flags{0},allegiance{0};int kind{0};vec3<int32_t> position;
        input >> slot >> position.x >> position.y >> position.z >> heading >> allegiance >> flags >> kind;
        contacts.push_back({
          .slot{slot},
          .position{vec3<int32_t>{position}},
          .heading{static_cast<uint16_t>(heading)},
          .side{static_cast<tfx::actors::allegiance>(allegiance)},
          .flags{static_cast<uint8_t>(flags)},
          .type{static_cast<int16_t>(kind)},
        });
        if(command == 'M') {
          unsigned int anchors{0};input >> anchors;
          for(unsigned int j{0}; j != anchors; ++j) { int x{0},y{0},z{0};input >> x >> y >> z;contacts.back().damage_centres.emplace_back(static_cast<int16_t>(x),static_cast<int16_t>(y),static_cast<int16_t>(z)); }
        }
      }
      std::vector<int16_t> types;
      for(unsigned int i{0}; i != type_count; ++i) { int value{0};input >> value;types.push_back(static_cast<int16_t>(value)); }
      if(command == 'M') {
        auto const mobile{tfx::sensors::nearest_mobile_target(player,contacts,cursor,static_cast<int16_t>(type))};
        values = {mobile ? static_cast<int>(mobile->slot) : -1,mobile ? mobile->offset.x : 0,mobile ? mobile->offset.y : 0,mobile ? mobile->offset.z : 0};
      } else {
      int16_t next{static_cast<int16_t>(cursor)};
      auto const radiation{tfx::sensors::next_radiation_target(next,contacts,static_cast<int16_t>(type),static_cast<tfx::actors::allegiance>(side))};
      auto const ship{tfx::sensors::nearest_ship(player,contacts,types)};
      values = {radiation ? static_cast<int>(*radiation) : -1,next,ship ? static_cast<int>(*ship) : -1};
      }
    } else throw std::runtime_error{"unknown ground sensor operation"};
    uint64_t digest{14695981039346656037ULL};
    for(auto const value : values) for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
    if(check) { if(!tfx::test::fixture_matches(input,digest)) throw std::runtime_error{"native ground sensor state differs"}; }
    else { for(auto const value : values) std::cout << value << ' ';std::cout << '\n'; }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
