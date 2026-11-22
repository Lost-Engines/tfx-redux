#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include "audio/gus_samples.h"
#include "audio/sample_bank.h"
#include "resources/binary.h"

int main(int const argc, char const *const argv[]) try {
  if(argc != 4) throw std::runtime_error{"expected GUS.COM PCM_OUTPUT RATE or --synthetic INPUT EXPECTED"};
  bool const fixture{std::string_view{argv[1]} == "--synthetic"};
  std::ifstream source,expected;
  std::ofstream pcm;
  if(fixture) { source.open(argv[2]);expected.open(argv[3]);if(!source || !expected) throw std::runtime_error{"cannot open Gravis PCM fixture"}; } else { pcm.open(argv[2],std::ios::binary);if(!pcm) throw std::runtime_error{"cannot create Gravis PCM capture"}; }
  auto &input{fixture ? source : std::cin};
  std::string header;
  if(!(input >> header)) throw std::runtime_error{"missing sample count"};
  bool const retail{header == "retail"};
  std::vector<tfx::resources::bytes> buffers(retail ? 0u : static_cast<unsigned int>(std::stoul(header)));
  std::unique_ptr<tfx::audio::sample_bank> bank;
  std::vector<tfx::audio::gus_sample_data> samples;
  if(retail) {
    auto const directory{std::filesystem::path{argv[1]}.parent_path()};
    auto const definitions{tfx::audio::load_sample_definitions(tfx::resources::read_file(directory / "TFX.DAT"))};
    tfx::resources::archive const game{directory / "DID.DAT"};
    bank = std::make_unique<tfx::audio::sample_bank>(definitions,game);
    for(unsigned int index{0}; index != bank->entries().size(); ++index) samples.push_back({
      .pcm{bank->pcm(index)},
      .rate{bank->entries()[index].rate},
      .retrigger_delay{bank->entries()[index].retrigger_delay},
    });
  }
  for(auto &buffer : buffers) {
    unsigned int rate,length,seed;
    int delay;
    if(!(input >> rate >> delay >> length >> seed)) throw std::runtime_error{"incomplete sample definition"};
    buffer.resize(length);
    for(unsigned int index{0}; index != length; ++index) buffer[index] = static_cast<uint8_t>(index * 37u + seed);
    samples.push_back({
      .pcm{buffer},
      .rate{static_cast<uint16_t>(rate)},
      .retrigger_delay{static_cast<int16_t>(delay)},
    });
  }
  tfx::audio::gus_parameters parameters{fixture ? tfx::audio::gus_parameters{} : tfx::audio::load_gus_parameters(tfx::resources::read_file(argv[1]))};
  if(fixture) {
    parameters.frequency_base = 19293;
    for(unsigned int index{0}; index != parameters.volumes.size(); ++index) parameters.volumes[index] = static_cast<uint16_t>((index * 13u + 17u) & 4095u);
  }
  tfx::audio::gus_samples device{samples,parameters,fixture ? 44100u : static_cast<unsigned int>(std::stoul(argv[3]))};
  unsigned int commands{0};
  char command;
  while(input >> command) {
    uint64_t hash{14695981039346656037ull};
    auto bytes{[&](uint32_t const bits){
      for(unsigned int shift{0}; shift != 32; shift += 8) { hash ^= (bits >> shift) & 255;hash *= 1099511628211ull; }
    }};
    if(command == 's') {
      unsigned int priority,size;input >> priority >> size;
      std::vector<unsigned int> sequence(size);
      for(auto &index : sequence) input >> index;
      device.submit(static_cast<tfx::audio::sample_priority>(priority),sequence);
    } else if(command == 'a') {
      unsigned int ticks;input >> ticks;device.advance(static_cast<tfx::simulation::timer_ticks>(ticks));
    } else if(command == 'e') {
      bool enabled;input >> enabled;device.enable(enabled);
    } else if(command == 'r') {
      unsigned int frames;input >> frames;
      std::vector<float> buffer(frames * 2);
      device.render(buffer);
      for(auto const value : buffer) bytes(std::bit_cast<uint32_t>(value));
      if(!fixture) pcm.write(reinterpret_cast<char const*>(buffer.data()),static_cast<std::streamsize>(buffer.size() * sizeof(float)));
    } else throw std::runtime_error{"unknown Gravis probe command"};
    if(!input) throw std::runtime_error{"incomplete Gravis probe command"};
    auto const state{device.value()};
    std::ostringstream row;
    row << state.scheduling.enabled << ' ' << static_cast<unsigned int>(state.scheduling.priority) << ' ' << static_cast<unsigned int>(state.scheduling.resident_cursor);
    for(auto const value : state.pending) row << ' ' << value;
    for(auto const value : state.started) row << ' ' << value;
    for(auto const value : state.cooldowns) row << ' ' << value;
    row << ' ' << hash;
    if(fixture) {
      std::string line;
      if(!std::getline(expected,line) || line != row.str()) throw std::runtime_error{"Gravis PCM/state mismatch at command " + std::to_string(commands)};
    } else std::cout << row.str() << '\n';
    ++commands;
  }
  if(!commands) throw std::runtime_error{"empty Gravis playback test"};
  return EXIT_SUCCESS;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return EXIT_FAILURE; }
