#include "engagement.h"

namespace tfx::missions {

std::vector<engagement_event> engagement_state::observe(engagement_warning const warning, bool const targets_player, engagement_context const &context) {
  std::vector<engagement_event> events;
  if(warning == engagement_warning::tracking) {
    // Native 0x763fc -> 0x81994: only player tracking grants permission and refreshes the cockpit notice.
    if(!targets_player) return events;
    if(!permitted) {
      permitted = true;
      // Native aircraft 1 is the F-117; mission-script aircraft start at 1.
      if(context.aircraft != flight::player_aircraft::nighthawk && !context.arcade && !context.training) events.emplace_back(permission_notice{});
    }
    int16_t constexpr notice_duration{30};                                     // Rendered frames; independent of the simulation tick count.
    notice_frames = notice_duration;
  } else {
    // Native 0x76398: firing at another aircraft can also grant global permission.
    // Its player-only subroutine 0x819e8 is empty; this branch has no target gate.
    if(!permitted) {
      if(!context.arcade) {
        unsigned int constexpr permission_sample{45};
        events.emplace_back(engagement_sample{
          .sample{permission_sample},
          .urgent{true},
        });
        events.emplace_back(permission_notice{});
      }
      permitted = true;
    } else if(!context.inhibited) {
      unsigned int constexpr firing_sample{44};
      events.emplace_back(engagement_sample{
        .sample{firing_sample},
        .urgent{false},
      });
    }
  }
  return events;
}

bool engagement_state::authorised(engagement_context const &context) const {
  return permitted || context.arcade || context.training;
}

void engagement_state::finish_frame() {
  // Native 0x72372 decrements once after drawing, without a signed clamp.
  if(notice_frames) notice_frames = math::signed_word(static_cast<uint16_t>(notice_frames - 1));
}

} // namespace tfx::missions
