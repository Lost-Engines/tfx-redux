#include <cstdlib>
#include <fstream>
#include <iostream>
#include "missions/progress.h"

int main(int const argc, char const *const argv[]) try {
  bool const check{argc == 2}; std::ifstream fixture;
  if(check) fixture.open(argv[1]);
  std::istream &input{check ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot read combat credit fixture"};
  unsigned int surface{0},hostile{0},player{0},authorised{0},low{0},high{0}; int type{0},filter{0},violations{0};
  while(input >> surface >> hostile >> player >> authorised >> type >> filter >> low >> high >> violations) {
    tfx::missions::setup setup; setup.aircraft_type_filter = static_cast<int16_t>(filter);setup.surface_type_filter = static_cast<int16_t>(filter);
    tfx::missions::progress state{setup}; state.violations = static_cast<int16_t>(violations);
    input >> state.results.personal_air_score >> state.results.credited_air_kills >> state.results.credited_ground_kills >> state.results.enemy_aircraft_down >> state.results.friendly_aircraft_down >> state.results.enemy_mobile_down;
    tfx::math::random_state random{static_cast<uint16_t>(low),static_cast<uint16_t>(high)};
    auto const effects{surface ? state.record_surface_loss(hostile != 0,static_cast<int16_t>(type)) : state.record_air_loss(hostile != 0,static_cast<int16_t>(type),player != 0,authorised != 0,random)};
    std::array<int64_t,12> const values{state.results.personal_air_score,state.results.credited_air_kills,state.results.credited_ground_kills,state.results.enemy_aircraft_down,state.results.friendly_aircraft_down,state.results.enemy_mobile_down,state.violations,random.low,random.high,effects.acknowledgement_sample.value_or(0),effects.friendly_target,effects.orders_violated};
    uint64_t digest{14695981039346656037ULL};
    for(auto const value : values) for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL;
    if(check) { uint64_t expected{0};if(!(input >> expected) || expected != digest) throw std::runtime_error{"native combat credit differs"}; } else { for(auto const val : values) std::cout << val << ' ';std::cout << '\n'; }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
