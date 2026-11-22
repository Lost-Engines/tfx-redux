#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "flight/ground_service.h"

int main(int const argc, char const *const argv[]) try {
  if(argc > 2) throw std::runtime_error{"expected optional ground-service fixture"};
  std::ifstream fixture;
  if(argc == 2) { fixture.open(argv[1]);if(!fixture) throw std::runtime_error{"cannot open ground-service fixture"}; }
  auto &input{argc == 2 ? fixture : std::cin};
  tfx::flight::ground_service service;
  unsigned int cases{0};
  bool reset,landed,resolved,crashed,disabled,post_landed;
  int32_t speed;
  int16_t violations;
  while(input >> reset >> landed >> speed >> resolved >> crashed >> disabled >> violations >> post_landed) {
    if(reset) service.reset();
    bool const requested{service.begin_frame({
      .landed{landed},
      .speed{speed},
      .resolved{resolved},
      .crashed{crashed},
      .disabled{disabled},
      .violations{violations},
    })};
    service.finish_frame(requested ? post_landed : landed);
    for(bool const value : {requested,service.supplied()}) {
      if(argc == 2) {
        bool expected;
        if(!(input >> expected) || expected != value) throw std::runtime_error{"ground-service mismatch at case " + std::to_string(cases)};
      } else std::cout << value << ' ';
    }
    if(argc != 2) std::cout << '\n';
    ++cases;
  }
  return cases ? EXIT_SUCCESS : EXIT_FAILURE;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
