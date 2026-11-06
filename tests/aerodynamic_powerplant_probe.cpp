#include "numeric_fixture.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <vector>
#include "flight/aerodynamic_powerplant.h"

namespace {

double read_double(std::istream &input) {
  uint64_t bits; input >> bits;
  return std::bit_cast<double>(bits);
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  if(argc > 2 || (argc == 2 && !fixture)) throw std::runtime_error{"cannot open aerodynamic powerplant fixtures"};
  auto &input{argc == 2 ? fixture : std::cin};
  unsigned int operation,aircraft,warp,afterburner,control,display,left,right,stages,cases{0};
  while(input >> operation >> aircraft >> warp >> afterburner >> control >> display >> left >> right >> stages) {
    tfx::math::random_state random;
    input >> random.low >> random.high;
    tfx::flight::aerodynamic_powerplant state{
      .engine_condition{static_cast<tfx::flight::system_condition>(left),static_cast<tfx::flight::system_condition>(right)},
      .afterburner{static_cast<uint8_t>(afterburner)},
      .afterburner_control{static_cast<uint8_t>(control)},
      .afterburner_display{static_cast<uint8_t>(display)},
    };
    uint32_t throttle_bits; input >> throttle_bits;
    state.throttle = std::bit_cast<float>(throttle_bits);
    for(auto *vector : {&state.engine_factor,&state.nozzle_angles}) for(auto &value : *vector) value = read_double(input);
    for(auto *vector : {&state.engine_force,&state.engine_moment}) for(auto &value : *vector) value = read_double(input);
    for(auto *value : {&state.thrust,&state.fuel,&state.fuel_capacity,&state.extra_fuel_capacity,&state.fuel_flow}) *value = read_double(input);
    input >> state.fuel_readout;
    tfx::flight::aerodynamic_powerplant_parameters p{
      .aircraft{static_cast<tfx::flight::player_aircraft>(aircraft)},
      .dry_thrust{read_double(input)},
      .afterburner_thrust{read_double(input)},
      .afterburner_stages{static_cast<uint8_t>(stages)},
      .ceiling_km{read_double(input)},
      .dry_fuel_coefficient{read_double(input)},
      .afterburner_fuel_coefficient{read_double(input)},
      .roll_thrust_arm{read_double(input)},
      .pitch_thrust_arm{read_double(input)},
    };
    tfx::flight::aerodynamic_powerplant_context const context{
      .altitude_km{read_double(input)},
      .seconds{read_double(input)},
      .time_warp{warp != 0},
    };
    bool stopped{false};
    if(operation == 1 || operation == 3) stopped = tfx::flight::consume_aerodynamic_fuel(state,p,context);
    if(operation == 2 || operation == 3) tfx::flight::update_aerodynamic_engine_faults(state,random);
    if(operation == 0 || operation == 3) tfx::flight::update_aerodynamic_thrust(state,p,context);
    std::vector<tfx::test::numeric_fixture_value> values;
    for(auto const *vector : {&state.engine_factor,&state.nozzle_angles}) for(auto const value : *vector) values.emplace_back(value);
    for(auto const *vector : {&state.engine_force,&state.engine_moment}) for(auto const value : *vector) values.emplace_back(value);
    for(auto const value : {state.thrust,state.fuel,state.fuel_capacity,state.extra_fuel_capacity,state.fuel_flow}) values.emplace_back(value);
    for(auto const value : {static_cast<unsigned int>(state.engine_condition[0]),static_cast<unsigned int>(state.engine_condition[1]),static_cast<unsigned int>(state.afterburner),static_cast<unsigned int>(state.afterburner_control),static_cast<unsigned int>(state.afterburner_display),static_cast<unsigned int>(static_cast<uint16_t>(state.fuel_readout)),static_cast<unsigned int>(random.low),static_cast<unsigned int>(random.high),static_cast<unsigned int>(stopped)}) values.push_back(value);
    if(argc == 2) {
      for(auto const value : values) {
        if(!tfx::test::fixture_matches(input,value)) throw std::runtime_error{"native aerodynamic powerplant differs"};
      }
    } else {
      for(auto const value : values) std::cout << value << ' ';
      std::cout << '\n';
    }
    ++cases;
  }
  if(!cases) throw std::runtime_error{"no aerodynamic powerplant cases"};
  if(argc == 2) std::cout << cases << " native aerodynamic powerplant fixtures agree\n";
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n'; return 1; }
