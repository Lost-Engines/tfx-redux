#include "recording.h"
#include <algorithm>
#include <array>
#include <cmath>
#include "presentation/player.h"
#include "recording/playback.h"
#include "render/horizon.h"
#include "simulation/timing.h"

namespace tfx::platform {
namespace {
presentation::scene_counters counters(double const seconds) {
  double constexpr bios_divisor{65536}, text_divisor{21845}, pit_hz{1193182};
  auto const wrapped{[](double const value) { return static_cast<uint32_t>(std::fmod(std::floor(value),4294967296.0)); }};
  return {
    .bios{wrapped(seconds * pit_hz / bios_divisor)},
    .text{wrapped(seconds * pit_hz / text_divisor)},
  };
}
}

void replay_recording(window &window, resources::archive const &game,
  resources::executable_tables const &tables, world::scene &scene,
  render::software_renderer &renderer, std::filesystem::path const &path,
  int16_t const camera_y, int16_t const ground_phase, render::dac_palette const &infrared) {
  recording::playback movie{path};
  if(!movie.frame_count()) return;
  auto *const host{window.get()};
  glfwSetInputMode(host,GLFW_CURSOR,GLFW_CURSOR_HIDDEN);
  render::dac_palette palette{};
  // Native DAVE_FADE loads the first block without its own ramp. The
  // wrapper then performs one 40-retrace fade and one complete traversal.
  // Neither introduction polls the keyboard; the replay clears make codes.
  for(auto const *name : {"wwn","prtv2"}) {
    presentation::clip const clip{
      .resource{name},
      .fade_in{1},
      .frame_delay{1},
      .play_until_end{true},
    };
    presentation::scene_input input;
    presentation::sequence_controls sequence;
    double const started{glfwGetTime()};
    double next{started}, paused{0};
    presentation::scene_player introduction{presentation::node_kind::animation,clip,
      game.read("anim/" + clip.resource + ".256"),tables.presentation_fonts(),{},
      {},palette,tables.text_palette,counters(0),input};
    window.title("TFX Redux — WNN — " + clip.resource);
    while(window.poll()) {
      double const now{glfwGetTime()};
      if(!window.focused()) {
        double const before{now};
        glfwWaitEventsTimeout(.05);
        paused += glfwGetTime() - before;
        next = glfwGetTime();
        continue;
      }
      if(now < next) { glfwWaitEventsTimeout(std::min(.01,next - now));continue; }
      next = now + 1.0 / 70;
      bool const complete{introduction.advance(counters(now - started - paused),input,sequence)};
      window.present(introduction.image().colourise(introduction.colours()));
      if(complete) break;
    }
    if(glfwWindowShouldClose(host)) return;
    palette = introduction.palette();
  }
  window.title("TFX Redux — Flight recording");
  auto const colours{render::expand_dac_palette(infrared)};
  renderer.output.pixels.fill(0);
  std::array<bool,GLFW_KEY_LAST + 1> held{};
  auto sample_keys{[&] {
    bool pressed{false};
    for(int key{GLFW_KEY_SPACE}; key <= GLFW_KEY_LAST; ++key) {
      bool const down{glfwGetKey(host,key) == GLFW_PRESS};
      pressed |= down && !held[static_cast<unsigned int>(key)];
      held[static_cast<unsigned int>(key)] = down;
    }
    return pressed;
  }};
  sample_keys();
  unsigned int index{0};
  double next{glfwGetTime()};
  double constexpr frame_seconds{4 / simulation::timer_hz};
  while(window.poll()) {
    double const now{glfwGetTime()};
    if(!window.focused()) {
      sample_keys();
      next = now;
      glfwWaitEventsTimeout(.05);
      continue;
    }
    if(sample_keys()) break;
    if(now < next) { glfwWaitEventsTimeout(std::min(.01,next - now));continue; }
    auto const frame{movie.next()};
    if(!frame) break;
    auto const &angles{frame->data.camera_angles};
    auto const view{render::orbit_camera(angles,0,{159,99})};
    render::paint_horizon(renderer.output,{
      .pitch{angles.x},
      .roll{angles.z},
      .camera_y{camera_y},
      .ground_phase{ground_phase},
      .origin_y{99},
    });
    scene.replay(renderer,view,*frame);
    if(index % 2 == 0) {
      auto const &text{tables.configuration_widgets().recording_play};
      tables.presentation_fonts().draw_driver(renderer.output,{285,1},text,0);
      tables.presentation_fonts().draw_driver(renderer.output,{284,0},text,6);
    }
    window.present(renderer.output.colourise(colours));
    ++index;
    next = now + frame_seconds;
  }
}

} // namespace tfx::platform
