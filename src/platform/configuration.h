#pragma once

#include "audio/runtime.h"
#include "configuration/graphics.h"
#include "platform/window.h"
#include "presentation/configuration_menu.h"
#include "resources/executable_tables.h"

namespace tfx::platform {

presentation::configuration_action configure_flight(window &window, resources::executable_tables const &tables,
  configuration::settings &preferences, std::string &recording_name, std::filesystem::path const &directory,
  render::framebuffer const &background, render::palette const &colours, audio::runtime *sound = nullptr, bool quit_prompt = false, configuration::graphics_settings *graphics = nullptr);

} // namespace tfx::platform
