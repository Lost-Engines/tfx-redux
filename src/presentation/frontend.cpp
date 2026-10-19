#include "frontend.h"
#include <algorithm>
#include <stdexcept>

namespace tfx::presentation {

frontend::frontend(resources::archive const &game, resources::executable_tables const &tables,
  std::filesystem::path const &save_directory, std::string const &language, bool const skip_intro, std::function<void(audio_request const&)> play_audio) :
  game{game}, tables{tables}, language{language}, play_audio{std::move(play_audio)}, schema{game.read("tfx.par")}, store{save_directory},
  parser{game,tables.presentation_fonts(),tables.text_palette,state,"tfx",{},language} {
  if(skip_intro) seek_enrolment();
}
void frontend::seek_enrolment() {
  auto const &main{schema.functions.front().statements};
  auto const entry{std::ranges::find_if(main,[](statement const &node) {
    return node.kind == node_kind::launch && std::get<host_action>(node.data).resource == "enrol";
  })};
  if(entry == main.end()) throw std::runtime_error{"root presentation has no enrolment action"};
  parser.seek({0,static_cast<unsigned int>(entry - main.begin())});
}
void frontend::return_to_enrolment() {
  last_menu.reset();
  menu.reset();
  pilot.reset();
  scores.reset();
  planner.reset();
  selected_slot.reset();
  mouse_held = false;
  parser.restart("tfx");
  seek_enrolment();
}

void frontend::cancel_launch() {
  if(last_menu) { parser.restore(*last_menu);mouse_held = false; }
  else return_to_enrolment();
}

statement const *frontend::advance(scene_counters const counters, scene_input &input,
  std::optional<vec2<int16_t>> const &position) {
  bool const click{(input.mouse_buttons & 1) && !mouse_held};
  mouse_held = input.mouse_buttons & 1;
  if(planner) {
    bool const done{planner->advance(input.mouse_buttons)};
    display = planner->image();
    palette = planner->colours();
    input.key = scene_key::none;
    if(done) {
      planner.reset();
      parser.complete();
    }
    return nullptr;
  }
  if(pilot) {
    if(click && position) pilot->click(*position);
    bool const done{pilot->advance(position)};
    display = pilot->image();
    palette = pilot->colours();
    if(done) {
      selected_slot = pilot->slot();
      if(pilot->recalled()) parser.restore(*pilot->recalled());
      // Native enrolment writes the current (possibly restored) statement before its caller advances.
      store.write(*selected_slot,false,schema,parser.snapshot());
      pilot.reset();
      parser.complete();
    }
    return nullptr;
  }
  if(scores) {
    bool const done{scores->advance(input.mouse_buttons)};
    display = scores->image();
    palette = scores->colours();
    if(done) {
      scores.reset();
      parser.complete();
    }
    return nullptr;
  }
  if(menu) {
    bool const done{menu->advance(counters.bios,position ? *position : vec2<int16_t>{0,0},input.mouse_buttons & 1,position.has_value())};
    display = menu->image();
    palette = menu->colours();
    if(done) {
      menu.reset();
      parser.complete();
    }
    return nullptr;
  }
  unsigned int constexpr action_budget{256};
  for(unsigned int step{0}; step != action_budget; ++step) {
    auto const *action{parser.advance(counters,input)};
    display = parser.image();
    palette = parser.colours();
    if(!action) return nullptr;
    if(action->kind == node_kind::audio) {
      if(play_audio) play_audio(std::get<audio_request>(action->data));
      parser.complete();
    } else if(action->kind == node_kind::options) {
      last_menu = parser.snapshot();
      auto const bytes{game.read("lev/" + std::get<host_action>(action->data).resource + ".opt")};
      menu_program script{std::string_view{reinterpret_cast<char const*>(bytes.data()),bytes.size()}};
      auto const media{game.read(script.animation.empty() ? "iff/raw/" + script.backdrop : "anim/" + script.animation + ".256")};
      menu = std::make_unique<menu_player>(std::move(script),media,state,tables.pointers,counters.bios);
      return nullptr;
    } else if(action->kind == node_kind::planner) {
      auto const &request{std::get<host_action>(action->data)};
      if(request.mode != 1) return action;
      planner = std::make_unique<planning::review>(game,tables.planner_art(),tables.interface_font(),request.resource,language);
      display = planner->image();
      palette = planner->colours();
      return nullptr;
    } else if(action->kind == node_kind::launch) {
      auto const &command{std::get<host_action>(action->data).resource};
      if(command == "enrol") {
        pilot = std::make_unique<pilot_panel>(game,tables.presentation_fonts(),tables.pilots(),schema,state,store);
        return nullptr;
      }
      if(command == "sscore" || command == "vscore") {
        auto entries{store.load_scores(tables.scores().defaults)};
        std::optional<unsigned int> selected;
        if(command == "vscore") {
          selected = insert_score(entries,pilot_score(state));
          store.save_scores(entries);
        }
        scores = std::make_unique<score_player>(tables.presentation_fonts(),tables.scores(),std::move(entries),selected,game.read("iff/raw/hiscore.lbm"));
        return nullptr;
      }
      if(command == "slog" || command == "rlog" || command == "spos") {
        if(!selected_slot) throw std::runtime_error{"pilot storage action has no selected slot"};
        if(command == "rlog") parser.restore(store.read(*selected_slot,true,schema));
        else store.write(*selected_slot,command == "slog",schema,parser.snapshot(command == "slog" ? save_mode::without_resume : save_mode::current_statement));
        parser.complete();
      } else if(command == "strain") {
        store.complete_training();
        parser.complete();
      } else return action;
    } else return action;
  }
  throw std::runtime_error{"front-end host actions do not yield"};
}
void frontend::press(uint8_t const scan, bool const control) {
  if(pilot) pilot->press(scan,control);
  if(scores) scores->press(scan);
  if(planner) planner->press(scan);
}
void frontend::complete() {
  parser.complete();
}
variables &frontend::values() {
  return state;
}
variables const &frontend::values() const {
  return state;
}
bool frontend::finished() const {
  return parser.finished();
}
bool frontend::pointer_visible() const {
  return menu || pilot;
}
render::framebuffer const &frontend::image() const {
  return display;
}
render::palette const &frontend::colours() const {
  return palette;
}
std::string const &frontend::function_name() const {
  return parser.function_name();
}

} // namespace tfx::presentation
