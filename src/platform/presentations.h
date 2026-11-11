#pragma once

#include "platform/flight.h"
#include "presentation/session.h"
#include "resources/executable_tables.h"

namespace tfx::platform {

bool show_loading_screen(window &window, resources::archive const &game);
void play_game(resources::archive const &game, resources::executable_tables const &tables,
  std::filesystem::path const &save_directory, std::string const &language, bool skip_intro,
  flight_options const &options);
void play_presentation(resources::archive const &game, resources::executable_tables const &tables,
  std::string const &name, std::string_view function, std::string const &language,
  unsigned int scale, unsigned int frame_limit, std::optional<std::filesystem::path> const &screenshot, audio::runtime *sound = nullptr, bool fullscreen = false);
} // namespace tfx::platform
