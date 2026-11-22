#include <cstdlib>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <utility>
#include "resources/executable_tables.h"
#include "weapons/runtime.h"

int main(int const argc, char const *const argv[]) try {
  if(argc < 2 || argc > 4) throw std::runtime_error{"projectile_runtime_probe DATA_DIRECTORY [--frame-tail] [--recycle]"};
  bool frame_tail{false};
  bool recycle{false};
  for(int index{2}; index != argc; ++index) {
    if(std::string_view{argv[index]} == "--frame-tail") {
      frame_tail = true;
    } else if(std::string_view{argv[index]} == "--recycle") {
      recycle = true;
    } else {
      throw std::runtime_error{"unknown projectile runtime probe option"};
    }
  }
  tfx::resources::executable_tables const tables{std::filesystem::path{argv[1]} / "TFX.DAT"};
  unsigned int frames{0};
  unsigned int elapsed{0};
  unsigned int dispenser{0};
  unsigned int capacity{0};
  while(std::cin >> frames >> elapsed >> dispenser >> capacity) {
    std::vector<int32_t> events;
    unsigned int frame_index{0};
    vec3<int32_t> designation{2000000, -200000, 3000000};
    bool const countermeasures{(dispenser >= 2 && dispenser <= 3) || (dispenser >= 6 && dispenser <= 8)};
    auto const target_pool{dispenser >= 6 && dispenser <= 8 ? tfx::weapons::actor_pool::player : tfx::weapons::actor_pool::aircraft};
    tfx::weapons::runtime state{tables.weapons, {
      .mount_state{[]{
        return tfx::weapons::launch_mount{
        .angles{0, 12000, 0},
        .selected{4}
      };
      }},
      .actor_position{[](tfx::weapons::actor_identity){return vec3<int32_t>{1000000, -300000, 1000000};}},
      .designation_position{[&]{return vec3<int32_t>{designation};}},
      .motion_inputs{[](tfx::weapons::active_projectile const &shot, uint16_t const ticks){
        return tfx::weapons::motion_context{
          .elapsed{ticks},
          .target{0, 0, 0},
          .player_owned{shot.owner.pool == tfx::weapons::actor_pool::player},
          .launch_anchor{1001000, -300500, 1002000},
          .player_angles{0, 12000, 0},
          .player_speed{400},
          .target_is_player{shot.target.actor && shot.target.actor->pool == tfx::weapons::actor_pool::player},
        };
      }},
      .reset_visual{[&](unsigned int const weapon, bool const secondary){events.insert(events.end(), {0, static_cast<int32_t>(weapon), secondary});}},
      .launched{[&](tfx::weapons::active_projectile const &shot){events.insert(events.end(), {1, static_cast<int32_t>(shot.id)});}},
      .target_cleared{[&](tfx::weapons::active_projectile const &shot){events.insert(events.end(), {2, static_cast<int32_t>(shot.id)});}},
      .contacts{[&](tfx::weapons::active_projectile &shot, uint16_t const ticks){
        events.insert(events.end(), {3, static_cast<int32_t>(shot.id), ticks});
        if(frame_tail && (frame_index + shot.id) % 7 == 0) shot.spent = true;
      }},
    }, capacity};
    tfx::math::random_state random;
    tfx::weapons::active_projectile const *warning{nullptr};
    auto const launch_batch{[&]{
      for(unsigned int index{0}; index != (dispenser == 1 ? 3u : 27u); ++index) {
        state.launch({
          .weapon{dispenser == 1 ? index == 0 ? 15u : index == 1 ? 16u : 0u : index},
          .position{1000000, -300000, 1000000},
          .angles{0, 12000, 0},
          .speed{400},
          .owner{
            .pool{tfx::weapons::actor_pool::aircraft},
          },
          .target{
            .position{2000000, -200000, 3000000},
            .actor{countermeasures ? std::optional{tfx::weapons::actor_identity{.pool{target_pool}}} : std::nullopt},
            .untargeted{dispenser == 4},
            .designated{dispenser == 5},
          },
          .jettisoned{dispenser == 9},
        });
      }
    }};
    launch_batch();
    for(unsigned int frame{0}; frame != frames; ++frame) {
      frame_index = frame;
      events.clear();
      if(recycle && frame && frame % 9 == 0) launch_batch();
      if(dispenser == 5) designation = vec3<int32_t>{2000000 + static_cast<int32_t>(frame) * 10000, -200000, 3000000 - static_cast<int32_t>(frame) * 7000};
      if(countermeasures && frame == 2) state.countermeasure({.pool{target_pool}}, {1000000, -300000, 1000000}, dispenser == 2 || dispenser == 6, dispenser < 6 ? 64 : dispenser == 6 ? 60 : dispenser == 7 ? 40 : 5, random);
      state.advance(static_cast<uint16_t>(elapsed), random, false);
      if(frame_tail) state.finish_frame();
      uint64_t digest{14695981039346656037ULL};
      auto const word{[&](auto const value){
        for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
      }};
      word(state.projectiles().size());
      for(auto const &[id, shot] : state.projectiles() | std::views::reverse) {
        auto const &body{shot->state};
        word(id);
        word(body.weapon);
        word(body.next_stage);
        word(std::to_underlying(body.motion));
        word(body.ticks);
        for(auto const value : {body.position.x, body.position.y, body.position.z, body.previous.x, body.previous.y, body.previous.z}) word(value);
        for(auto const value : {body.angles.x, body.angles.y, body.angles.z}) word(value);
        word(body.speed);
        word(body.fall_speed);
        word(body.acceleration);
        word(body.range);
        word(body.secondary);
        word(shot->spent);
        word(shot->mount);
      }
      word(random.low);
      word(random.high);
      // The HUD consumes the prior draw's borrowed record, even after retirement/recycling.
      word(warning ? warning->state.weapon : -1);
      warning = state.first_targeting_player();
      word(warning ? warning->id : 0);
      word(events.size());
      for(auto const value : events) word(value);
      std::cout << digest << '\n';
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
