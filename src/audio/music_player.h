#pragma once

#include <filesystem>
#include <mutex>
#include "audio/music_catalogue.h"
#include "audio/music_clock.h"
#include "audio/music_control.h"
#include "audio/music_device.h"
#include "audio/osl_sequencer.h"
#include "audio/requests.h"
#include "resources/archive.h"

namespace tfx::audio {

class music_player {
private:
  resources::archive const &game;
  music_catalogue catalogue;
  std::unique_ptr<music_device> device;
  osl_sequencer sequencer;
  music_clock clock;
  mutable std::mutex mutex;
  std::optional<osl_resource_id> resource;
  std::optional<osl_playing_id> playing;
  bool effect_bank{false};
  std::optional<uint8_t> preselected;
  bool enabled;
  bool ticks_enabled;
  bool flight_active{false};
  unsigned int profile{0};
  lapc_order_profiles profiles{};
  music_control_state control;
  music_control_context context;
  void stop_playing_locked();
  void stop_locked();
  void replace_locked(uint8_t index);
  void apply_locked(music_selection const &selection);
  void cue_locked(uint8_t cue);
  void context_locked(flight_audio_state const &state);

public:
  music_player(resources::archive const &game, std::span<uint8_t const> launcher,
    std::unique_ptr<music_device> device, unsigned int output_rate, bool enabled = true);
  music_player(resources::archive const &game, std::span<uint8_t const> launcher,
    std::filesystem::path const &rom_directory, unsigned int output_rate, bool enabled = true);
  void configure(music_mode mode, uint8_t sound_device, lapc_order_profiles const &orders);
  void set_cd_mode(bool active);
  std::optional<uint8_t> preselected_index() const;
  void begin_flight(flight_audio_state const &state);
  void end_flight();
  void update_flight(flight_audio_state const &state);
  void cue(uint8_t cue);
  void effect(lapc_effect_cue cue);
  void select(int16_t index);
  void stop();
  void enable(bool enabled);
  bool is_enabled() const;
  void render(std::span<float> stereo);
};

} // namespace tfx::audio
