#include <bit>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "audio/sample_voice.h"

namespace {
tfx::resources::archive fixture() {
  tfx::resources::bytes bytes(4);
  auto const word{[&](uint16_t const v) { bytes.push_back(static_cast<uint8_t>(v));bytes.push_back(static_cast<uint8_t>(v >> 8)); }};
  auto const dword{[&](uint32_t const v) { word(static_cast<uint16_t>(v));word(static_cast<uint16_t>(v >> 16)); }};
  std::array<tfx::resources::bytes,3> const pcm{{{0,128,255,1,254},{64,192,127},{255,255,0,0}}};
  std::array<unsigned int,3> offsets{};
  for(unsigned int i{0}; i != pcm.size(); ++i) { offsets[i] = static_cast<unsigned int>(bytes.size());bytes.insert(bytes.end(),pcm[i].begin(),pcm[i].end()); }
  auto const index{static_cast<uint32_t>(bytes.size())};
  for(unsigned int i{0}; i != 4; ++i) bytes[i] = static_cast<uint8_t>(index >> (8 * i));
  word(3);word(0);word(0);word(0);
  for(unsigned int i{0}; i != pcm.size(); ++i) { dword(tfx::resources::hash_short_name("samples\\" + std::to_string(i) + "sam"));dword(offsets[i]);dword(static_cast<uint32_t>(pcm[i].size())); }
  return {std::move(bytes)};
}
void verify() {
  auto const game{fixture()};
  std::array<tfx::audio::sample_definition,3> const definitions{{
    {.name{"0"},.rate{8363},.retrigger_delay{0},.qsound_parameters{}},
    {.name{"1"},.rate{7500},.retrigger_delay{0},.qsound_parameters{}},
    {.name{"2"},.rate{6500},.retrigger_delay{0},.qsound_parameters{}},
  }};
  tfx::audio::sample_bank const bank{definitions,game};
  tfx::audio::sample_voice contiguous{bank,48000}, fragmented{bank,48000};
  std::array<unsigned int,3> constexpr sequence{0,1,2};
  contiguous.submit(tfx::audio::sample_priority::normal,sequence);
  fragmented.submit(tfx::audio::sample_priority::normal,sequence);
  std::array<float,512> whole{}, split{};
  contiguous.render(whole);
  unsigned int frame{0};
  for(unsigned int const chunk : {1u,7u,2u,83u,4u,159u}) { fragmented.render(std::span{split}.subspan(frame * 2,chunk * 2));frame += chunk; }
  if(whole != split || contiguous.state().active || fragmented.state().active) throw std::runtime_error{"sample callback segmentation changed playback"};
  uint64_t now{0};
  unsigned int expected_frame{0};
  for(auto const id : sequence) for(auto const pcm : bank.pcm(id)) {
    auto const end{now + tfx::audio::sound_blaster_period(definitions[id].rate) * 48000ull};
    while(expected_frame * 1'000'000ull < end) {
      float const expected{(static_cast<int>(pcm) - 128) / 128.f};
      if(whole[expected_frame * 2] != expected || whole[expected_frame * 2 + 1] != expected) throw std::runtime_error{"Sound Blaster sample hold changed"};
      ++expected_frame;
    }
    now = end;
  }
  for(unsigned int i{expected_frame * 2}; i != whole.size(); ++i) if(whole[i] != 0) throw std::runtime_error{"completed stream was not silent"};
  if(tfx::audio::sound_blaster_period(7500) != 133 || tfx::audio::sound_blaster_period(8363) != 119) throw std::runtime_error{"incorrect Sound Blaster time constant"};
}
} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  if(argc == 1) { verify();std::cout << "PCM sample holds and callback boundaries agree\n";return 0; }
  if(argc == 2 && std::string_view{argv[1]} == "--rates") {
    unsigned int rate;
    while(std::cin >> rate) std::cout << tfx::audio::sound_blaster_period(static_cast<uint16_t>(rate)) << '\n';
    return 0;
  }
  if(argc != 2) throw std::runtime_error{"expected [DATA_DIRECTORY|--rates]"};
  std::filesystem::path const directory{argv[1]};
  auto const definitions{tfx::audio::load_sample_definitions(tfx::resources::read_file(directory / "TFX.DAT"))};
  tfx::resources::archive const game{directory / "DID.DAT"};
  tfx::audio::sample_bank const bank{definitions,game};
  for(unsigned int i{0}; i != definitions.size(); ++i) {
    auto const &d{definitions[i]};
    auto const &s{bank.scheduling()[i]};
    uint64_t hash{14695981039346656037ull};
    for(auto const byte : bank.pcm(i)) hash = (hash ^ byte) * 1099511628211ull;
    std::cout << i << ' ' << std::quoted(d.name) << ' ' << d.rate << ' ' << d.retrigger_delay;
    std::cout << ' ' << std::bit_cast<int16_t>(d.qsound_parameters.position)
      << ' ' << d.qsound_parameters.increment << ' ' << std::bit_cast<int16_t>(d.qsound_parameters.movement_enable);
    std::cout << ' ' << s.available << ' ' << s.samples << ' ' << hash << '\n';
  }
  return 0;
} catch(std::exception const &error) { std::cerr << error.what() << '\n';return 1; }
