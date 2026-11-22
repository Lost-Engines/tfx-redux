#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string_view>
#include "sensors/air.h"

int main(int const argc, char const *const argv[]) try {
  bool const announcements{argc > 1 && std::string_view{argv[1]} == "--announcements"};
  bool const check{argc == (announcements ? 3 : 2)};
  std::ifstream fixture;
  if(check) fixture.open(argv[announcements ? 2 : 1]);
  std::istream &input{check ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot read sensor fixture"};
  tfx::sensors::air_contacts radar;
  char command{0};
  while(input >> command) {
    std::vector<int64_t> values;
    if(command == 'R') {
      unsigned int count{0},cycle{0};int32_t px{0},py{0},pz{0};
      input >> px >> py >> pz >> count >> cycle;
      std::vector<tfx::sensors::air_contact> contacts;
      for(unsigned int index{0}; index != count; ++index) {
        unsigned int actor{0},flags{0},enemy{0}; int32_t x{0},y{0},z{0}; input >> actor >> x >> y >> z >> flags >> enemy;
        contacts.push_back({
          .actor{actor},
          .position{x,y,z},
          .heading{0},
          .flags{static_cast<uint16_t>(flags)},
          .enemy_pool{enemy != 0},
        });
      }
      auto const report{radar.update({px,py,pz},contacts)};
      if(cycle) radar.next();
      values.insert(values.end(),{radar.cursor,radar.selected ? static_cast<int>(*radar.selected) : -1});
      for(auto const &slot : radar.slots) values.push_back(slot ? static_cast<int>(*slot) : -1);
      values.push_back(radar.hostile_nearby);
      if(announcements) values.insert(values.end(),{radar.nearby_hostile_count,report.more_hostiles,report.announced_actor ? static_cast<int>(*report.announced_actor) : -1});
    } else if(command == 'B') {
      vec2<int16_t> centre, projected;int16_t lock, acquisition;bool close;
      input >> centre.x >> centre.y >> projected.x >> projected.y >> lock >> acquisition >> close;
      tfx::sensors::air_lock state{
        .tracking{7},
        .guidance{7},
        .reticle{-12345,67890},
        .projected{vec2<int16_t>{projected}},
        .lock_ticks{lock},
        .acquisition_ticks{acquisition},
        .too_close{close},
      };
      state.begin_mission(centre);
      values = {state.tracking.has_value(),state.guidance.has_value(),state.reticle.x,state.reticle.y,state.projected.x,state.projected.y,state.lock_ticks,state.acquisition_ticks,state.too_close};
    } else if(command == 'L') {
      unsigned int flags{0},category{0},role{0},target_class{0},ticks{0},pitch{0},heading{0},target_heading{0};
      int minimum{0},range{0},lock_ticks{0},acquisition_ticks{0},project_x{0},project_y{0}; int32_t px{0},py{0},pz{0},x{0},y{0},z{0},ret_x{0},ret_y{0};
      input >> flags >> category >> role >> target_class >> ticks >> minimum >> range >> px >> py >> pz >> pitch >> heading >> x >> y >> z >> target_heading >> ret_x >> ret_y >> project_x >> project_y >> lock_ticks >> acquisition_ticks;
      tfx::sensors::air_contacts contacts;
      if(flags & 1) contacts.selected = 7;
      if(flags & 2) contacts.slots[0] = 7;
      contacts.cursor = 0;
      tfx::sensors::air_lock state{
        .tracking{flags & 4 ? std::optional<unsigned int>{7} : std::nullopt},
        .guidance{flags & 8 ? std::optional<unsigned int>{7} : std::nullopt},
        .reticle{ret_x,ret_y},
        .projected{11,12},
        .lock_ticks{static_cast<int16_t>(lock_ticks)},
        .acquisition_ticks{static_cast<int16_t>(acquisition_ticks)},
        .too_close{(flags & 64) != 0},
      };
      std::array<tfx::sensors::air_contact,1> const actors{{{
        .actor{7},
        .position{x,y,z},
        .heading{static_cast<uint16_t>(target_heading)},
        .flags{0},
        .side{static_cast<tfx::actors::allegiance>(target_class)},
      }}};
      tfx::sensors::advance_air_lock(state,contacts,actors,{
        .role{static_cast<tfx::weapons::weapon_role>(role)},
        .category{static_cast<uint8_t>(category)},
        .range{range},
        .minimum_range{static_cast<int16_t>(minimum)},
      },{
        .position{px,py,pz},
        .angles{static_cast<uint16_t>(pitch),static_cast<uint16_t>(heading),0},
        .elapsed{static_cast<uint16_t>(ticks)},
        .guns{(flags & 16) != 0},
        .sensor_suppressed{(flags & 32) != 0},
        .project{[&](auto const&){ return vec2<int16_t>{static_cast<int16_t>(project_x),static_cast<int16_t>(project_y)}; }},
      });
      values = {contacts.selected.has_value(),state.tracking.has_value(),state.guidance.has_value(),state.reticle.x,state.reticle.y,state.projected.x,state.projected.y,state.lock_ticks,state.acquisition_ticks,state.too_close};
    } else throw std::runtime_error{"unknown sensor operation"};
    uint64_t digest{14695981039346656037ULL};
    for(auto const value : values) for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
    if(check) { uint64_t expected{0}; if(!(input >> expected) || expected != digest) throw std::runtime_error{"native radar or seeker state differs"}; } else { for(auto const value : values) std::cout << value << ' '; std::cout << '\n'; }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
