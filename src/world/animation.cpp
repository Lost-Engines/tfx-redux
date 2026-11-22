#include "animation.h"
#include <stdexcept>
#include "math/native.h"

namespace tfx::world {

parameter_player::parameter_player(std::span<uint16_t const> const source) : words{source.begin(), source.end()} {
}

stream_step parameter_player::tick(uint8_t &parameter) {
  /// Original interpreter at 2.05 file 0xb6ce0: one value-changing operation per actor tick
  if(finished) {
    return {
      .action{stream_action::finished},
    };
  }
  unsigned int constexpr instruction_limit{1024};
  for(unsigned int step{0}; step != instruction_limit; ++step) {
    auto const word{words.at(cursor)};
    auto const opcode{word & 255};
    auto const value{static_cast<uint8_t>(word >> 8)};
    switch(opcode) {
    case 0:
      parameter = value;
      ++cursor;
      return {
        .action{stream_action::value_written},
      };
    case 1:
    case 2:
      parameter = static_cast<uint8_t>(parameter + (opcode == 1 ? 1 : -1));
      ++cursor;
      return {
        .action{stream_action::value_written},
      };
    case 3:
      if(parameter == value) {
        cursor += 2;
        continue;
      }
      [[fallthrough]];
    case 4:
      {
      auto const displacement{math::signed_word(words.at(cursor + 1))};
      int64_t const target{static_cast<int64_t>(cursor) + 1 + displacement};
      if(target < 0 || static_cast<uint64_t>(target) >= words.size()) throw std::runtime_error{"animation jump leaves its program"};
      cursor = static_cast<unsigned int>(target);
    }
      continue;
    case 5:
      finished = true;
      return {
        .action{stream_action::finished},
      };
    case 6:
      cursor += 2;
      return {
        .action{stream_action::event_requested},
        .event{math::signed_word(words.at(cursor - 1))},
      };
    default:
      throw std::runtime_error{"unknown actor animation command " + std::to_string(opcode)};
    }
  }
  throw std::runtime_error{"animation program makes no value-changing progress"};
}

unsigned int parameter_player::position() const {
  return cursor;
}

bool parameter_player::stopped() const {
  return finished;
}

bool animation_clock::advance(timer_ticks const elapsed) {
  /// Signed word comparisons and wrap match the native gate at 2.05 file 0x7205c
  accumulated = static_cast<timer_ticks>(accumulated + elapsed);
  if(math::signed_word(accumulated) <= animation_interval) return false;
  accumulated = static_cast<timer_ticks>(accumulated - animation_interval);
  if(math::signed_word(accumulated) > maximum_animation_backlog) accumulated = 0;
  return true;
}
int16_t animation_clock::remainder() const {
  return math::signed_word(accumulated);
}

} // namespace tfx::world
