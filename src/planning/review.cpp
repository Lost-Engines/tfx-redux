#include "review.h"
#include "presentation/timing.h"

namespace tfx::planning {
namespace {
document load(resources::archive const &game, std::string const &name) {
  auto const bytes{game.read("waytxt/" + name + ".way")};
  return document{std::string_view{reinterpret_cast<char const*>(bytes.data()),bytes.size()}};
}
} // anonymous namespace

review::review(resources::archive const &game, resources::planner_artwork const &artwork,
  presentation::widget_font const &font, std::string const &name, std::string const &language) :
  plan{load(game,name)}, font{font}, map{game,artwork,font,plan}, annotations{plan.notes} {
  // Native 0xa7fa8 overlays translated notes by key, retaining untranslated
  // waypoint notes from the base document when the language file only has an intro.
  std::string const translated{language + "/" + name + ".way"};
  if(language != "english" && game.contains(translated)) {
    auto const bytes{game.read(translated)};
    annotations.append(std::string_view{reinterpret_cast<char const*>(bytes.data()),bytes.size()});
  }
  if(plan.routes.empty() || plan.routes.front().points.empty()) stage = review_phase::complete;
  compose();
}
annotation const *review::current_note() const {
  if(stage != review_phase::introduction && stage != review_phase::waypoint) return nullptr;
  return annotations.find(stage == review_phase::introduction ? uint8_t{0} : static_cast<uint8_t>(points));
}
void review::first_point() {
  points = 1;
  extent = 0;
  stage = review_phase::waypoint;
}
void review::compose() {
  display = map.image();
  map.route(display,points);
  if(stage == review_phase::segment) map.extend(display,points,extent);
  if(auto const *note{current_note()}) draw_annotation(display,font,*note);
}
void review::press(uint8_t const scan) {
  // The original note wait accepts Space or the right mouse button, not Escape/Enter.
  if(scan == 0x39 && current_note()) next = true;
}
bool review::advance(uint8_t const buttons) {
  if(stage == review_phase::complete) return true;
  if(stage == review_phase::leaving) {
    if(fade_level > 1) --fade_level;
    else { fade_level = 0;stage = review_phase::complete;display.pixels.fill(0); }
    return stage == review_phase::complete;
  }
  if(stage == review_phase::introduction || stage == review_phase::waypoint) {
    if(current_note() && !next && !(buttons & 2)) return false;
    next = false;
    if(stage == review_phase::introduction) first_point();
    else {
      if(points == plan.routes.front().points.size()) stage = review_phase::leaving;
      else { stage = review_phase::segment;extent = 0; }
    }
    compose();
    return stage == review_phase::complete;
  }
  extent += 2;                                                                 // Original 0xaceb4 advances two pixels per display flip.
  display = map.image();
  map.route(display,points);
  if(!map.extend(display,points,extent)) {
    ++points;
    stage = review_phase::waypoint;
    extent = 0;
    compose();
  }
  return false;
}
review_status review::status() const {
  return {
    .phase{stage},
    .points{points},
    .extent{extent},
  };
}
bool review::finished() const {
  return stage == review_phase::complete;
}
render::framebuffer const &review::image() const {
  return display;
}
render::palette review::colours() const {
  unsigned int const level{stage == review_phase::complete ? 0u : fade_level};
  return render::decode_palette(presentation::fade_palette(map.palette(),level,40));
}
document const &review::source() const {
  return plan;
}

} // namespace tfx::planning
