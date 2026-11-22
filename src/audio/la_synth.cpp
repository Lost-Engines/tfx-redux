#include "la_synth.h"
#include <algorithm>
#include <stdexcept>
#include <vector>
#define MT32EMU_API_TYPE 1
#include <mt32emu/mt32emu.h>

namespace tfx::audio {

struct la_synth::implementation {
  mt32emu_context context{mt32emu_create_context({}, nullptr)};
  ~implementation() {
    mt32emu_free_context(context);
  }
};
la_synth::la_synth(std::filesystem::path const &directory, unsigned int const sample_rate) : state{std::make_unique<implementation>()} {
  if(!state->context) throw std::runtime_error{"cannot create Munt synthesiser"};
  if(!std::filesystem::is_directory(directory)) throw std::runtime_error{"Roland ROM directory does not exist: " + directory.string()};
  std::vector<std::filesystem::path> files;
  for(auto const &entry : std::filesystem::directory_iterator{directory}) {
    if(entry.is_regular_file() && entry.file_size() <= 2 * 1024 * 1024) files.push_back(entry.path());
  }
  std::ranges::sort(files);
  bool found{false};
  // Same content-based identification as Darker. Prefer LAPC-I-compatible
  // CM-32L ROMs; no General MIDI conversion or percussion workaround is used.
  for(char const *machine : {"cm32l_1_02", "cm32l_1_00", "mt32_2_07", "mt32_2_06", "mt32_2_04", "mt32_2_03", "mt32_1_07", "mt32_1_06", "mt32_1_05", "mt32_1_04"}) {
    mt32emu_free_context(state->context);
    state->context = mt32emu_create_context({}, nullptr);
    if(!state->context) throw std::runtime_error{"cannot create Munt ROM identification context"};
    for(auto const &file : files) mt32emu_add_machine_rom_file(state->context, machine, file.string().c_str());
    mt32emu_rom_info info{};
    mt32emu_get_rom_info(state->context, &info);
    if(info.control_rom_id && info.pcm_rom_id) {
      found = true;
      break;
    }
  }
  if(!found) throw std::runtime_error{"no compatible Roland control/PCM ROM pair found in " + directory.string()};
  mt32emu_set_stereo_output_samplerate(state->context, sample_rate);
  mt32emu_set_analog_output_mode(state->context, MT32EMU_AOM_ACCURATE);
  if(mt32emu_open_synth(state->context) != MT32EMU_RC_OK) throw std::runtime_error{"Munt could not open the Roland ROM pair"};
}
la_synth::~la_synth() = default;
void la_synth::short_message(uint32_t const packed) {
  mt32emu_play_msg_now(state->context, packed);
}
void la_synth::sysex(std::span<uint8_t const> const message) {
  mt32emu_play_sysex_now(state->context, message.data(), static_cast<uint32_t>(message.size()));
}
void la_synth::render(std::span<float> const stereo) {
  if(stereo.size() % 2) throw std::invalid_argument{"Munt output requires complete stereo frames"};
  mt32emu_render_float(state->context, stereo.data(), static_cast<uint32_t>(stereo.size() / 2));
}
void la_synth::silence() {
  mt32emu_flush_midi_queue(state->context);
  for(uint32_t channel{0}; channel != 16; ++channel) {
    for(uint32_t control : {64u, 120u, 123u}) mt32emu_play_msg_now(state->context, (0xb0 | channel) | (control << 8));
  }
}

} // namespace tfx::audio
