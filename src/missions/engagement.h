#pragma once

#include <variant>
#include <vector>
#include "math/native.h"
#include "flight/aircraft.h"

namespace tfx::missions {

enum class engagement_warning {
  tracking,
  firing
};
struct engagement_context {
  flight::player_aircraft aircraft;
  bool arcade{false};
  bool training{false};
  bool inhibited{false};
};
struct permission_notice {};
struct engagement_sample {
  unsigned int sample;
  bool urgent;
};
using engagement_event = std::variant<permission_notice, engagement_sample>;

struct engagement_state {
  bool permitted{false};
  int16_t notice_frames{0};
  std::vector<engagement_event> observe(engagement_warning warning, bool targets_player, engagement_context const &context);
  bool authorised(engagement_context const &context) const;
  void finish_frame();
};

} // namespace tfx::missions
