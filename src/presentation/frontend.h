#pragma once

#include <functional>
#include "presentation/menu_player.h"
#include "planning/review.h"
#include "presentation/pilot_panel.h"
#include "presentation/session.h"
#include "presentation/score_player.h"
#include "resources/executable_tables.h"

namespace tfx::presentation {

// Owns the original front-end's shared pilot state and suspended parser/menu actions.
// Flight and non-retail authoring modes remain explicit host boundaries.
class frontend {
private:
  resources::archive const &game;
  resources::executable_tables const &tables;
  std::string language;
  std::function<void(audio_request const&)> play_audio;
  program schema;
  variables state;
  pilot_store store;
  session parser;
  std::unique_ptr<menu_player> menu;
  std::unique_ptr<pilot_panel> pilot;
  std::unique_ptr<score_player> scores;
  std::unique_ptr<planning::review> planner;
  std::optional<unsigned int> selected_slot;
  render::framebuffer display;
  render::palette palette{};
  bool mouse_held{false};
  std::optional<saved_state> last_menu;
  void seek_enrolment();

public:
  frontend(resources::archive const &game, resources::executable_tables const &tables,
    std::filesystem::path const &save_directory, std::string const &language, bool skip_intro = false, std::function<void(audio_request const&)> play_audio = {});
  statement const *advance(scene_counters counters, scene_input &input,
    std::optional<vec2<int16_t>> const &position = {});
  void press(uint8_t scan, bool control);
  void complete();
  void return_to_enrolment();
  void cancel_launch();
  variables &values();
  variables const &values() const;
  bool finished() const;
  bool pointer_visible() const;
  render::framebuffer const &image() const;
  render::palette const &colours() const;
  std::string const &function_name() const;
};

} // namespace tfx::presentation
