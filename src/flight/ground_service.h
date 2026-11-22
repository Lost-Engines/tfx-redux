#pragma once

#include <cstdint>

namespace tfx::flight {

inline int32_t constexpr ground_service_reward{10000};
struct ground_service_context {
  bool landed;
  int32_t speed;
  bool resolved;
  bool crashed;
  bool disabled;
  int16_t violations;
};
class ground_service {
private:
  // Native mission entry 710F1 marks an initial ground start serviced.
  bool serviced{true};

public:
  bool supplied() const;
  void reset();
  bool begin_frame(ground_service_context const &context);
  // Native 71FBB tests the live landed flag after the reset callbacks too.
  void finish_frame(bool landed);
};

} // namespace tfx::flight
