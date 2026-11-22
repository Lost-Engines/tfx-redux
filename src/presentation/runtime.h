#pragma once

#include "presentation/program.h"
#include "presentation/save.h"

namespace tfx::presentation {

class runtime {
private:
  program const &source;
  variables &state;
  std::optional<location> cursor;
  std::vector<location> callers;
  std::vector<uint32_t> reserved;
  std::optional<location> successor() const;

public:
  explicit runtime(program const &source, variables &state, location entry = {0, 0});
  std::optional<location> position() const;
  unsigned int call_depth() const;
  statement const *current() const;
  // Execute numeric control flow until an audiovisual/menu/session action needs its host.
  statement const *run();
  void complete();
  saved_state snapshot(save_mode mode = save_mode::current_statement) const;
  // Like native load: restore the saved statement; the host then completes its load action.
  void restore(saved_state const &saved);
};

} // namespace tfx::presentation
