#include <cstdlib>
#include <iostream>
#include "flight/fuel_gauge.h"

int main() {
  tfx::flight::fuel_gauge state;
  int16_t reading{0};
  while(std::cin >> state.reading >> state.smoothed_burn >> reading) {
    state.sample(reading);
    std::cout << state.reading << ' ' << state.smoothed_burn << '\n';
  }
  return EXIT_SUCCESS;
}
