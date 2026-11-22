#include <cstdlib>
#include <iostream>
#include <utility>
#include "missions/recovery.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 2) throw std::runtime_error{"recovery probe requires game directory"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  tfx::missions::recovery_maps const maps{game};
  unsigned int region;
  while(std::cin >> region) {
    tfx::missions::recovery_context context;
    int outcome;
    int status;
    tfx::missions::recovery_state previous;
    std::cin >> context.ejected >> context.fatal >> context.damaged >> context.violations >> context.start.x >> context.start.y >> context.position.x >> context.position.y >> outcome >> status >> previous.safe;
    context.region = static_cast<tfx::missions::recovery_region>(region);
    previous.outcome = static_cast<tfx::missions::session_outcome>(outcome);
    previous.status = static_cast<tfx::missions::session_status>(status);
    auto const result{maps.resolve(previous, context)};
    std::cout << std::to_underlying(result.outcome) << ' ' << std::to_underlying(result.status) << ' ' << result.safe << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
