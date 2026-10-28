#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "weapons/runtime.h"

namespace {
void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}
}

int main() try {
  using namespace tfx::weapons;
  std::array<definition,weapon_count> definitions;
  definitions[1].stages = {{
    .ticks{100},
    .motion{flight_stage::finished},
  }};
  std::vector<projectile_id> removed;
  bool lose_remaining{false};
  runtime projectiles{definitions,{
    .motion_inputs{[](active_projectile const&, uint16_t) { return motion_context{}; }},
    .target_cleared{[&](active_projectile const &shot) { removed.push_back(shot.id); }},
    .contacts{[&](active_projectile &shot, uint16_t) {
      if(shot.id == 1 || lose_remaining) shot.spent = true;
      if(shot.id == 2) shot.state.ticks = 0;
    }},
  },3};
  for(unsigned int index{0}; index != 3; ++index) projectiles.launch({
    .weapon{1},
    .position{0,-1000,0},
    .angles{0,0,0},
    .speed{0},
    .owner{.pool{actor_pool::player}},
    .target{},
  });
  auto const *const retained{projectiles.projectiles().at(1)};
  tfx::math::random_state random;
  projectiles.advance(1,random,false);
  require(projectiles.projectiles().size() == 3,"contact losses must remain available to the final drawing pass");
  projectiles.finish_frame();
  require(projectiles.projectiles().size() == 2 && !projectiles.projectiles().contains(1),"frame tail must retire the spent record");
  require(projectiles.projectiles().contains(2),"an expired stage alone must wait for the next motion pass");
  require(removed == std::vector<projectile_id>{1},"frame tail must notify the target owner once");
  require(retained->id == 1,"borrowed warning records must remain allocated after retirement");
  projectiles.finish_frame();
  require(removed.size() == 1,"repeated frame cleanup must not repeat owner notifications");
  projectiles.advance(1,random,false);
  require(projectiles.projectiles().size() == 1 && projectiles.projectiles().contains(3),"the next motion pass must retire the expired stage");
  require(removed == std::vector<projectile_id>{1,2},"retirement must retain owner notification order");
  lose_remaining = true;
  projectiles.advance(1,random,false);
  projectiles.finish_frame();
  require(projectiles.projectiles().empty(),"the final spent record must leave an empty pool");
  definitions[1].stages={{100,flight_stage::coasting}};
  definitions[1].player_proximity=true;definitions[1].range=10000;
  unsigned emissions{0}, impacts{0};
  for(bool camera_trails:{false,true}) {
    runtime airborne{definitions,{
      .motion_inputs{[](active_projectile const&,uint16_t) { return motion_context{}; }},
      .player_distance{[](active_projectile const&) { return 12000; }},
      .trail{[&](active_projectile const&) { ++emissions; }},
      .camera_controls_trails{camera_trails},
    }};
    airborne.launch({.weapon{1},.position{0,-25600,0},.angles{0,0,0},.speed{400},.owner{},.target{}});
    airborne.advance(1,random,true);
    require(emissions==(camera_trails ? 1u : 0u),"camera trail policy retained the DOS distance cutoff");
  }
  runtime grounded{definitions,{
    .motion_inputs{[](active_projectile const&,uint16_t) { return motion_context{}; }},
    .ground_impact{[&](active_projectile const&,int16_t&) { ++impacts; }},
  }};
  auto const ground_shot{grounded.launch({.weapon{1},.position{0,0,0},.angles{0,0,0},.speed{400},.owner{},.target{.position{100,0,100}}})};
  grounded.advance(1,random,true);
  require(impacts==1 && grounded.projectiles().at(ground_shot)->spent,"ground contact did not destroy missile");
  grounded.finish_frame();require(grounded.projectiles().empty(),"ground impact left a circling missile alive");
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
