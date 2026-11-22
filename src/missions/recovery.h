#pragma once

#include <array>
#include <string_view>
#include "missions/progress.h"
#include "render/framebuffer.h"
#include "resources/archive.h"

namespace tfx::missions {

enum class recovery_region : unsigned int { somalia, columbia, libya, yugoslavia, atlantic, unknown };
recovery_region pilot_recovery_region(std::string_view world);
struct recovery_state {
  session_outcome outcome{session_outcome::normal};
  session_status status{session_status::normal};
  bool safe{false};
};
struct recovery_context {
  bool ejected{false};
  bool fatal{false};
  bool damaged{false};
  int16_t violations{0};
  recovery_region region{recovery_region::unknown};
  vec2<int32_t> start{0,0};
  vec2<int32_t> position{0,0};
};
class recovery_maps {
private:
  std::array<std::optional<render::framebuffer>,6> maps;

public:
  explicit recovery_maps(resources::archive const &game);
  recovery_state resolve(recovery_state previous, recovery_context const &context) const;
};

} // namespace tfx::missions
