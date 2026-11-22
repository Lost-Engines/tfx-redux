#pragma once

#include <memory>
#include <optional>
#include "audio/osl_device.h"

namespace tfx::audio {

unsigned int constexpr osl_resource_slots{16}, osl_playing_slots{16}, osl_voice_channels{32};
using osl_resource_id = unsigned int;
using osl_playing_id = unsigned int;

enum class osl_effect : uint8_t {
  none,
  pitch_up,
  pitch_down,
  portamento,
  modulation,
  pan,
  silence,
  reverb_mode,
  reverb_parameters,
  speed,
  volume_slide,
  order_jump,
  volume,
  stop = 14,
  clock_rate,
  arpeggio = 30,
  retrigger,
};
struct osl_playing_state {
  osl_resource_id resource{0};
  unsigned int song{0};
  uint8_t flags{0};
  uint8_t channel_count{0};
  uint8_t speed{0};
  uint16_t clock_rate{0};
  uint8_t subtick{0};
  uint8_t order{0};
  uint8_t loop_order{0};
  uint8_t volume{0};
  uint16_t row{0};
  uint8_t order_count{0};
  std::array<uint8_t,3> reverb{};
  uint16_t mask{65535};
  std::array<unsigned int,osl_channels> channel_slots{};
};
class osl_sequencer {
private:
  struct playing {
    osl_playing_state state;
    // Original playback retains bank pointers after its public handle is
    // released. Keep those bytes alive until this playback slot is reused.
    std::shared_ptr<osl_resource const> source;
    std::optional<osl_pattern_reader> pattern;
    unsigned int pattern_rows{0};
  };
  std::array<std::shared_ptr<osl_resource const>,osl_resource_slots> resources;
  std::array<playing,osl_playing_slots> songs;
  std::array<osl_channel,osl_voice_channels> channels;
  std::vector<osl_device_driver*> devices;
  uint16_t clock{0};
  osl_device_driver *device(osl_device type) const;
  osl_song song(playing const &state) const;
  uint8_t transpose(playing const &state, osl_channel const &channel) const;
  void read_row(playing &state);
  void reset_pattern(playing &state);
  void send_volume(playing &state, osl_channel &channel, unsigned int physical, osl_device_driver &driver);
  void arpeggio(playing &state, osl_channel &channel, unsigned int physical, osl_device_driver &driver);
  void retrigger(playing &state, osl_channel &channel, unsigned int physical, osl_device_driver &driver, bool new_row);
  void row_effect(playing &state, osl_channel &channel, unsigned int physical, osl_device_driver &driver, bool &jump, bool &stop);
  void tick_effect(playing &state, osl_channel &channel, unsigned int physical, osl_device_driver &driver);
  void new_notes(playing &state, osl_channel &channel, unsigned int physical, osl_device_driver &driver, bool &jump, bool &stop);

public:
  osl_sequencer();
  // Device objects and their output sinks must outlive this controller.
  void register_device(osl_device_driver &driver);
  osl_resource_id load(resources::bytes source);
  void release(osl_resource_id resource);
  osl_playing_id play(osl_resource_id resource, unsigned int song = 0, uint8_t order = 0, uint16_t row = 0, uint8_t volume = 75);
  // Native play reports carry when its fixed playback/channel pools are full.
  // Direct sound-effect requests ignore that failure without altering a song.
  std::optional<osl_playing_id> try_play(osl_resource_id resource, unsigned int song = 0, uint8_t order = 0, uint16_t row = 0, uint8_t volume = 75);
  void stop(osl_playing_id id);
  void stop_all();
  void volume(osl_playing_id id, uint8_t value);
  void tick();
  uint16_t clock_rate() const;
  unsigned int pattern_offset(osl_playing_id id) const;
  osl_playing_state const &state(osl_playing_id id) const;
  std::span<osl_channel const> channel_states() const;
};

} // namespace tfx::audio
