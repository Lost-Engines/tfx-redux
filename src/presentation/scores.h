#pragma once

#include "presentation/program.h"

namespace tfx::presentation {

unsigned int constexpr score_entries{10}, score_name_bytes{21}, score_record_bytes{3 * score_name_bytes + 4};
struct score_entry {
  std::string firstname;
  std::string surname;
  std::string callsign;
  int32_t score;
  bool operator==(score_entry const &) const = default;
};
using score_table = std::array<score_entry, score_entries>;
struct score_assets {
  score_table defaults;
  std::array<uint8_t, 30> highlight;
  explicit score_assets(std::span<uint8_t const> executable);
};
resources::bytes pack_scores(score_table const &scores);
score_table unpack_scores(std::span<uint8_t const> source);
resources::bytes encode_scores(score_table const &scores);
std::optional<score_table> decode_scores(std::span<uint8_t const> source);
std::optional<unsigned int> insert_score(score_table &scores, score_entry entry);
score_entry pilot_score(variables const &state);

} // namespace tfx::presentation
