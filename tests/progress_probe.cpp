#include <cstdlib>
#include <fstream>
#include <iostream>
#include <utility>
#include "missions/progress.h"

int main(int const argc, char const *const argv[]) try {
  bool const check{argc == 2};
  std::ifstream fixture;
  if(check) fixture.open(argv[1]);
  std::istream &input{check ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot read mission-progress fixture"};
  unsigned int frames{0};
  unsigned int count{0};
  unsigned int kind{0};
  unsigned int landed_start{0};
  unsigned int critical{0};
  int value{0};
  int timer{0};
  int violations{0};
  int exit_ticks{0};
  while(input >> frames >> count >> kind >> value >> landed_start >> timer >> violations >> exit_ticks >> critical) {
    tfx::missions::setup setup;
    setup.start.landed = landed_start != 0;
    setup.timer = timer;
    if(kind != 255) {
      setup.completion = {
        .kind{static_cast<tfx::missions::completion_kind>(kind)},
        .value{value},
        .text{"Complete"},
      };
    }
    for(unsigned int index{0}; index != count; ++index) {
      setup.targets.push_back({
        .position{0, 0, 0},
        .time_limit{index % 2 ? -1 : 100 + static_cast<int>(index * 100)},
        .text{"Target"},
      });
    }
    tfx::missions::progress state{setup};
    state.violations = static_cast<int16_t>(violations);
    state.exit_ticks = static_cast<int16_t>(exit_ticks);
    state.critical_loss = critical != 0;
    uint64_t digest{14695981039346656037ULL};
    for(unsigned int frame{0}; frame != frames; ++frame) {
      uint32_t mission_time{0};
      unsigned int elapsed{0};
      unsigned int landed{0};
      unsigned int arcade{0};
      unsigned int mask{0};
      int speed{0};
      input >> mission_time >> elapsed >> landed >> speed >> arcade >> state.results.enemy_aircraft_down >> state.results.friendly_aircraft_down >> state.results.enemy_mobile_down >> mask;
      std::vector<tfx::missions::target_life> targets;
      for(unsigned int index{0}; index != count; ++index) targets.push_back(mask & (1u << index) ? tfx::missions::target_life::dead : tfx::missions::target_life::alive);
      auto const notices{state.begin_frame({
        .mission_time{mission_time},
        .targets{targets},
        .landed{landed != 0},
        .speed{speed},
        .arcade{arcade != 0},
      })};
      state.advance_return_countdown(static_cast<uint16_t>(elapsed), arcade != 0);
      bool const timer_notice{state.finish_frame(static_cast<uint16_t>(elapsed))};
      auto const returned_landed{state.force_landed_return ? 1u : landed};
      auto const returned_speed{state.force_landed_return ? 0 : speed};
      std::vector<int64_t> values{state.results.targets_destroyed, state.results.credited_targets, state.target_cursor, state.remaining_timer, state.results.completed, state.exit_ticks, state.resolved, state.ended, std::to_underlying(state.outcome), std::to_underlying(state.status), state.return_score, returned_landed, returned_speed, static_cast<int64_t>(notices.size()) + timer_notice};
      for(auto const &event : notices) {
        if(auto const *notice{std::get_if<tfx::missions::progress_notice>(&event)}) {
          values.insert(values.end(), {std::to_underlying(notice->kind), notice->target});
        } else {
          values.insert(values.end(), {4, 0});
        }
      }
      if(timer_notice) values.insert(values.end(), {std::to_underlying(tfx::missions::notice_kind::timer_expired), 0});
      if(check) {
        for(auto const val : values) {
          for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(val) >> shift) & 255)) * 1099511628211ULL;
        }
      } else {
        for(auto const val : values) std::cout << val << ' ';
        std::cout << '\n';
      }
    }
    if(check) {
      uint64_t expected{0};
      if(!(input >> expected) || digest != expected) throw std::runtime_error{"native mission progress differs"};
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
