#include <array>
#include <bit>
#include <cstdlib>
#include <iostream>
#include "audio/osl_resource.h"

int main(int const argc, char const *const argv[]) try {
  if(argc < 2) throw std::runtime_error{"expected OSL resources"};
  for(int argument{1}; argument != argc; ++argument) {
    tfx::audio::osl_resource const resource{tfx::resources::read_file(argv[argument])};
    std::cout << static_cast<unsigned int>(resource.device()) << ' ' << resource.song_count() << ' ' << resource.instrument_count() << '\n';
    for(unsigned int index{0}; index != resource.song_count(); ++index) {
      auto const song{resource.song(index)};
      std::cout << song.channels() << ' ' << song.order_count() << ' ' << song.loop_order() << ' ' << song.pattern_count() << ' ' << song.clock_rate() << ' ' << static_cast<unsigned int>(song.speed()) << ' ' << static_cast<unsigned int>(song.transpose()) << '\n';
      for(unsigned int channel{0}; channel != tfx::audio::osl_channels; ++channel) std::cout << static_cast<unsigned int>(song.channel_volume(channel)) << ' ';
      for(unsigned int order{0}; order != 256; ++order) std::cout << static_cast<unsigned int>(song.order(order)) << ' ';
      std::cout << '\n';
      for(unsigned int pattern{0}; pattern != song.pattern_count(); ++pattern) {
        // Null pointers are authoring gaps, and are not played as patterns.
        try {
          auto const data{song.pattern(pattern)};
          tfx::audio::osl_pattern_reader reader{data};
          uint64_t hash{14695981039346656037ull};
          auto const byte{[&](uint8_t const value){hash = (hash ^ value) * 1099511628211ull;}};
          for(unsigned int row{0}; row != data.rows; ++row) {
            auto const cells{reader.next(song.channels())};
            for(unsigned int channel{0}; channel != song.channels(); ++channel) {
              auto const &cell{cells[channel]};
              for(auto const note : cell.notes) byte(note);
              byte(cell.instrument);
              byte(cell.effect);
              byte(cell.argument);
              byte(cell.auxiliary);
            }
            auto const cursor{reader.offset()};
            byte(static_cast<uint8_t>(cursor));
            byte(static_cast<uint8_t>(cursor >> 8));
          }
          std::cout << pattern << ' ' << data.rows << ' ' << reader.offset() << ' ' << hash << '\n';
        } catch(std::runtime_error const &error) {
          if(std::string_view{error.what()} != "OSL pattern has no resource pointer") throw;
          std::cout << pattern << " 0 0 0\n";
        }
      }
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
