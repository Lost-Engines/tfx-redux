#pragma once

#include <array>
#include "presentation/cursor.h"
#include "presentation/save.h"
#include "presentation/scores.h"

namespace tfx::presentation {

unsigned int constexpr pilot_slots{10}, edited_name_limit{11};
struct enrolment_line {
  std::string text;
  uint8_t colour;
};
struct pilot_assets {
  std::array<uint8_t, 128> characters;
  std::array<std::string, 3> defaults;
  std::vector<enrolment_line> introduction;
  std::array<std::string, 7> ranks;
  std::array<std::string, 5> statuses;
  pointer_shape pointer;
  explicit pilot_assets(std::span<uint8_t const> executable);
};
class name_entry {
private:
  pilot_assets const &assets;
  std::string fallback;
  std::string text;
  bool finished{false};
  bool control_at_confirmation{false};

public:
  name_entry(pilot_assets const &assets, std::string fallback);
  bool press(uint8_t scan, bool control);
  std::string const &value() const;
  bool qualified() const;
};
class pilot_store {
private:
  std::filesystem::path directory;
  std::filesystem::path path(unsigned int slot, bool log) const;

public:
  explicit pilot_store(std::filesystem::path directory);
  std::array<std::optional<saved_state>, pilot_slots> list(program const &schema) const;
  void write(unsigned int slot, bool log, program const &schema, saved_state const &saved) const;
  saved_state read(unsigned int slot, bool log, program const &schema) const;
  score_table load_scores(score_table const &defaults) const;
  void save_scores(score_table const &scores) const;
  bool trained() const;
  void complete_training() const;
};
void reset_pilot(program const &schema, variables &values);

} // namespace tfx::presentation
