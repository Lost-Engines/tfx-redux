#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include "resources/compiled_overlay.h"

namespace {
void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}
template<typename Action> void rejects(Action const &action) {
  try {
    action();
  } catch(std::runtime_error const &) {
    return;
  }
  throw std::runtime_error{"malformed compiled overlay was accepted"};
}
} // anonymous namespace

int main() try {
  // A test-authored fixed-artwork header and one two-plane opaque black pixel group.
  tfx::resources::bytes source{
    0x55, 0x57, 0x56, 0x8b, 0xec, 0xba, 0xc4, 3, 0, 0, 0xb0, 2, 0xee, 0xfe, 0xc2,
    0x8b, 0x7d, 0x10, 0xc1, 0xe7, 4, 0x66, 0xbb, 0, 0, 0x66, 0xb9, 0, 0, 0xb4, 0,
    0xb0, 3, 0xee, 0xc6, 0x87, 0, 0, 0, 0, 0, 0x5e, 0x5f, 0x5d, 0xc3,
  };
  tfx::resources::compiled_overlay const overlay{source};
  auto const normal{overlay.image()};
  require(normal.image.pixels[0] == 0 && normal.coverage.pixels[0] == 255 && normal.coverage.pixels[1] == 255 && normal.coverage.pixels[2] == 0, "explicit black and untouched planes became interchangeable");
  auto const ignored{overlay.image(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())};
  require(ignored.image.pixels == normal.image.pixels && ignored.coverage.pixels == normal.coverage.pixels, "fixed compiled artwork unexpectedly followed canopy offsets");
  for(unsigned int length{0}; length != source.size(); ++length) {
    rejects([&]{
      tfx::resources::compiled_overlay bad{std::span<uint8_t const>{source}.first(length)};
    });
  }
  auto bad{source};
  bad[32] = 16;
  rejects([&]{
    tfx::resources::compiled_overlay invalid{bad};
  });
  bad = source;
  bad[36] = 0x80;
  bad[37] = 0x3e;
  rejects([&]{
    tfx::resources::compiled_overlay invalid{bad};
  });
  bad = source;
  bad[34] = 0x90;
  rejects([&]{
    tfx::resources::compiled_overlay invalid{bad};
  });
  bad = source;
  bad.push_back(0);
  rejects([&]{
    tfx::resources::compiled_overlay invalid{bad};
  });
  std::cout << "Compiled artwork bounds, transparency and fixed-position rules passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
