#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "resources/archive.h"
#include "resources/ra.h"

namespace {

void require(bool const result, char const *const message) {
  if(!result) throw std::runtime_error{message};
}

template<typename F>
void rejected(F const &action) {
  bool failed{false};
  try {
    action();
  } catch(std::runtime_error const &) {
    failed = true;
  }
  require(failed, "malformed input was accepted");
}

} // anonymous namespace

auto main()->int try {
  using tfx::resources::bytes;
  bytes const literal{0x52, 0x41, 0, 2, 3, 0, 0, 0, 0, 0x7d, 0, 0, 1, 1, 0, 1, 0x40, 1, 0xfe, 1, 5, 0, 0x61, 0x62, 0x63};
  require(tfx::resources::decode_ra(literal) == bytes{'a', 'b', 'c'}, "RA literal differs from independent C/Python reference");
  for(size_t size{0}; size != literal.size(); ++size) rejected([&]{tfx::resources::decode_ra(std::span{literal}.first(size));});
  rejected([&]{tfx::resources::decode_ra(literal, 2);});
  for(auto const size : {2, 4}) {
    auto changed{literal};
    changed[4] = static_cast<uint8_t>(size);
    rejected([&]{tfx::resources::decode_ra(changed);});
  }
  std::array const examples{
    std::pair{"Dintrob", 0x72de1438u},
    std::pair{"Dintroc", 0x73df1538u},
    std::pair{"Dintroe", 0x75e11338u},
    std::pair{"8miss830V", 0x955faaa1u},
    std::pair{"Lmig_29l", 0x80bf3e23u},
  };
  for(auto const &[name, id] : examples) require(tfx::resources::hash_short_name(name) == id, "native TFX filename hash differs");
  for(auto const size : {0, 3, 12}) rejected([&]{tfx::resources::archive{bytes(static_cast<size_t>(size))};});
  std::cout << "RA literals, truncation/size rejection and native filename hashes passed" << std::endl;
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
