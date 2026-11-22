#include <cstdlib>
#include <iostream>
#include "presentation/scores.h"

namespace {
void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}
} // anonymous namespace
int main() try {
  tfx::presentation::score_table table;
  for(unsigned int index{0}; index != table.size(); ++index) {
    table[index] = {
      .firstname{"ACE"},
      .surname{"PILOT"},
      .callsign{"ONE"},
      .score{100 - static_cast<int32_t>(index) * 10},
    };
  }
  auto const encoded{tfx::presentation::encode_scores(table)};
  require(encoded.size() == 674 && tfx::presentation::decode_scores(encoded) == table, "native score codec did not roundtrip");
  for(unsigned int index{0}; index != encoded.size(); ++index) {
    auto corrupt{encoded};
    corrupt[index] ^= 1;
    require(!tfx::presentation::decode_scores(corrupt), "altered score/checksum byte was accepted");
    require(!tfx::presentation::decode_scores(std::span{encoded}.first(index)), "truncated score file was accepted");
  }
  auto extra{encoded};
  extra.push_back(0);
  require(!tfx::presentation::decode_scores(extra), "extra score bytes were accepted");
  require(tfx::presentation::insert_score(table, {"NEW", "PILOT", "ACE", 100}) == 1, "tied score displaced its existing equal");
  require(table[0].score == 100 && table[1].firstname == "NEW" && table[9].score == 20, "score insertion moved the wrong records");
  require(!tfx::presentation::insert_score(table, {"LAST", "PILOT", "ACE", 20}), "equal bottom score unexpectedly qualified");
  table[0].firstname.assign(21, 'X');
  bool rejected{false};
  try {
    tfx::presentation::encode_scores(table);
  } catch(std::runtime_error const &) {
    rejected = true;
  }
  require(rejected, "oversized high-score name was accepted");
  std::cout << "Native high-score file bounds/checksum, signed ranking and stable ties passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
