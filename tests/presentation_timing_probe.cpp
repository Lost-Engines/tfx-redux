#include <cstdlib>
#include <fstream>
#include <iostream>
#include <utility>
#include "presentation/timing.h"

int main(int const argc, char const *const argv[]) try {
  std::ifstream fixture;
  if(argc == 2) fixture.open(argv[1]);
  std::istream &input{argc == 2 ? fixture : std::cin};
  if(!input) throw std::runtime_error{"cannot read presentation timing fixture"};
  char command{0};
  while(input >> command) {
    uint64_t digest{14695981039346656037ULL};
    auto const word{[&](auto const value){ for(unsigned int shift{0}; shift != 32; shift += 8) digest = (digest ^ ((static_cast<uint32_t>(value) >> shift) & 255)) * 1099511628211ULL; }};
    if(command == 'K') {
      unsigned int key{0},shift{0},buttons{0},caption{0},revealing{0},visible{0},characters{0},skipped{0},quit{0};
      input >> key >> shift >> buttons >> caption >> revealing >> visible >> characters >> skipped >> quit;
      tfx::presentation::scene_input state{
        .key{static_cast<tfx::presentation::scene_key>(key)},
        .shift{shift != 0},
        .mouse_buttons{static_cast<uint8_t>(buttons)},
      };
      tfx::presentation::sequence_controls sequence{
        .skipped{skipped != 0},
        .quit{quit != 0},
      };
      word(tfx::presentation::advance_requested(state,sequence,caption && revealing,static_cast<uint16_t>(visible),static_cast<uint16_t>(characters)));
      word(std::to_underlying(state.key));word(sequence.skipped);word(sequence.quit);
    } else if(command == 'W') {
      unsigned int kind{0},caption{0},revealing{0},visible{0},characters{0};int ticks{0};
      input >> kind >> ticks >> caption >> revealing >> visible >> characters;
      tfx::presentation::scene_wait wait{static_cast<tfx::presentation::node_kind>(kind),static_cast<int16_t>(ticks)};
      bool const finished{wait.advance(caption && revealing && tfx::math::signed_word(static_cast<uint16_t>(visible)) < tfx::math::signed_word(static_cast<uint16_t>(characters)))};
      word(wait.remaining());word(finished);
    } else if(command == 'A') {
      unsigned int frames{0},delay{0},steps{0};uint32_t counter{0};
      input >> frames >> delay >> counter >> steps;
      tfx::presentation::animation_clock clock{static_cast<uint16_t>(frames),static_cast<uint16_t>(delay),counter};
      for(unsigned int step{0}; step != steps; ++step) {
        uint32_t elapsed{0};input >> elapsed;counter += elapsed;
        auto const update{clock.advance(counter)};
        word(clock.last_counter());word(clock.next_ordinal());word(update.changed);word(update.traversal_complete);word(update.block);
      }
    } else if(command == 'F') {
      unsigned int stages{0},level{0};input >> stages >> level;
      tfx::presentation::dac_palette source;
      for(unsigned int index{0}; index != source.size(); ++index) source[index] = static_cast<uint8_t>(index % 64);
      for(auto const value : tfx::presentation::fade_palette(source,level,stages)) word(value);
    } else throw std::runtime_error{"unknown presentation timing operation"};
    if(argc == 2) { uint64_t expected{0};if(!(input >> expected) || digest != expected) throw std::runtime_error{"presentation timing differs from original"}; } else std::cout << digest << '\n';
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
