#include "ground_service.h"

namespace tfx::flight {

bool ground_service::supplied() const {
  return serviced;
}
void ground_service::reset() {
  serviced = true;
}
bool ground_service::begin_frame(ground_service_context const &context) {
  if(!context.landed || context.speed || serviced || context.resolved || context.crashed || context.disabled || context.violations) return false;
  serviced = true;
  return true;
}
void ground_service::finish_frame(bool const landed) {
  if(!landed) serviced = false;
}

} // namespace tfx::flight
