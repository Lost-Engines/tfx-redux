#include "scores.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace tfx::presentation {
namespace {
unsigned int constexpr score_payload_bytes{score_entries * score_record_bytes};
void dword(resources::bytes &bytes, uint32_t const value) {
  for(unsigned int shift{0}; shift != 32; shift += 8) bytes.push_back(static_cast<uint8_t>(value >> shift));
}
} // anonymous namespace

score_assets::score_assets(std::span<uint8_t const> const executable) {
  resources::binary_view const view{executable};
  unsigned int constexpr data{973476};
  auto const text{[&](unsigned int const address){
    auto const bytes{view.slice(data + address, score_name_bytes)};
    auto const end{std::ranges::find(bytes, uint8_t{0})};
    if(end == bytes.end()) throw std::runtime_error{"unterminated original high-score name"};
    return std::string{bytes.begin(), end};
  }};
  for(unsigned int index{0}; index != defaults.size(); ++index) {
    unsigned int const record{data + 0xa518 + index * 16};
    defaults[index] = {
      .firstname{text(view.le32(record))},
      .surname{text(view.le32(record + 4))},
      .callsign{text(view.le32(record + 8))},
      .score{std::bit_cast<int32_t>(view.le32(record + 12))},
    };
  }
  for(unsigned int index{0}; index != highlight.size(); ++index) highlight[index] = view.byte(data + 0xa5b8 + index * 2);
}
resources::bytes pack_scores(score_table const &scores) {
  resources::bytes bytes;
  bytes.reserve(score_payload_bytes);
  for(auto const &entry : scores) {
    for(auto const *name : {&entry.firstname, &entry.surname, &entry.callsign}) {
      if(name->size() >= score_name_bytes || name->find('\0') != std::string::npos) throw std::runtime_error{"high-score name exceeds native storage"};
      bytes.insert(bytes.end(), name->begin(), name->end());
      bytes.insert(bytes.end(), score_name_bytes - name->size(), 0);
    }
    dword(bytes, std::bit_cast<uint32_t>(entry.score));
  }
  return bytes;
}
score_table unpack_scores(std::span<uint8_t const> const source) {
  if(source.size() != score_payload_bytes) throw std::runtime_error{"high-score payload has the wrong size"};
  resources::binary_view const view{source};
  auto const text{[&](unsigned int const at){
    auto const bytes{view.slice(at, score_name_bytes)};
    auto const end{std::ranges::find(bytes, uint8_t{0})};
    if(end == bytes.end()) throw std::runtime_error{"unterminated high-score name"};
    return std::string{bytes.begin(), end};
  }};
  score_table scores;
  for(unsigned int index{0}; index != scores.size(); ++index) {
    unsigned int const at{index * score_record_bytes};
    scores[index] = {
      .firstname{text(at)},
      .surname{text(at + score_name_bytes)},
      .callsign{text(at + score_name_bytes * 2)},
      .score{std::bit_cast<int32_t>(view.le32(at + score_name_bytes * 3))},
    };
  }
  return scores;
}
resources::bytes encode_scores(score_table const &scores) {
  auto bytes{pack_scores(scores)};
  math::random_state random;
  uint32_t checksum{0};
  for(auto &byte : bytes) {
    byte ^= static_cast<uint8_t>(random.next());
    checksum += static_cast<uint32_t>(byte) + random.next();
  }
  dword(bytes, checksum);
  return bytes;
}
std::optional<score_table> decode_scores(std::span<uint8_t const> const source) {
  if(source.size() != score_payload_bytes + 4) return {};
  resources::bytes bytes{source.begin(), source.begin() + score_payload_bytes};
  math::random_state random;
  uint32_t checksum{0};
  for(auto &byte : bytes) {
    auto const mask{static_cast<uint8_t>(random.next())};
    checksum += static_cast<uint32_t>(byte) + random.next();
    byte ^= mask;
  }
  if(checksum != resources::binary_view{source}.le32(score_payload_bytes)) return {};
  try {
    return unpack_scores(bytes);
  }
  catch(std::runtime_error const &) {
    return {};
  }
}
std::optional<unsigned int> insert_score(score_table &scores, score_entry entry) {
  auto const position{std::ranges::find_if(scores, [&](score_entry const &other){return entry.score > other.score;})};
  if(position == scores.end()) return {};
  std::move_backward(position, scores.end() - 1, scores.end());
  auto const index{static_cast<unsigned int>(position - scores.begin())};
  *position = std::move(entry);
  return index;
}
score_entry pilot_score(variables const &state) {
  return {
    .firstname{std::get<std::string>(state.at("firstname"))},
    .surname{std::get<std::string>(state.at("pilotname"))},
    .callsign{std::get<std::string>(state.at("callsign"))},
    .score{std::bit_cast<int32_t>(std::get<uint32_t>(state.at("score")))},
  };
}

} // namespace tfx::presentation
