#pragma once

#include "planning/map_view.h"
#include "planning/notes.h"

namespace tfx::planning {

enum class review_phase : uint8_t {
  introduction,
  waypoint,
  segment,
  leaving,
  complete
};
struct review_status {
  review_phase phase;
  unsigned int points;
  unsigned int extent;
};

// The retail Tour of Duty mode runs 0xaceb4, then returns to its parser.
// Mission authoring and editable route review are separate planner modes.
class review {
private:
  document plan;
  presentation::widget_font const &font;
  map_view map;
  notes annotations;
  render::framebuffer display;
  review_phase stage{review_phase::introduction};
  unsigned int points{0};
  unsigned int extent{0};
  unsigned int fade_level{40};
  bool next{false};
  annotation const *current_note() const;
  void compose();
  void first_point();

public:
  review(resources::archive const &game, resources::planner_artwork const &artwork,
    presentation::widget_font const &font, std::string const &name, std::string const &language = "english");
  review(review const &) = delete;
  review &operator=(review const &) = delete;
  void press(uint8_t scan);
  bool advance(uint8_t buttons);
  bool finished() const;
  review_status status() const;
  render::framebuffer const &image() const;
  render::palette colours() const;
  document const &source() const;
};

} // namespace tfx::planning
