#include "session.h"
#include <algorithm>
#include <stdexcept>

namespace tfx::presentation {
namespace {
location entry(program const &source, std::string_view const function) {
  if(function.empty()) return {0,0};
  for(unsigned int index{0}; index != source.functions.size(); ++index) if(source.functions[index].name == function) return {
    .function{index},
    .statement{0},
  };
  throw std::runtime_error{"presentation function not found: " + std::string{function}};
}
} // anonymous namespace

session::frame::frame(resources::bytes const &bytes, variables &state, std::string_view const function) : graph{bytes}, execution{graph,state,entry(graph,function)} {}

session::session(resources::archive const &game, fonts const &fonts, dac_palette const &text_palette,
  variables &state, std::string const &program, std::string_view const function, std::string language) :
  game{game}, font_data{fonts}, text_palette{text_palette}, state{state}, language{std::move(language)}, current_palette{text_palette} {
  load(program,function);
}
void session::load(std::string const &name, std::string_view const function) {
  unsigned int constexpr maximum_parsers{32};
  if(stack.size() == maximum_parsers) throw std::runtime_error{"nested presentation parsers exceed the safety limit"};
  stack.push_back(std::make_unique<frame>(game.read(name + (name.ends_with(".par") ? "" : ".par")),state,function));
}

void session::restart(std::string const &program) {
  stack.clear();
  scene.reset();
  sequence = {};
  node_start = true;
  load(program,{});
}

statement const *session::advance(scene_counters const counters, scene_input &input) {
  if(scene) {
    bool const done{scene->advance(counters,input,sequence)};
    display = scene->image();
    displayed_colours = scene->palette();
    if(!done) return nullptr;
    scene.reset();
    stack.back()->execution.complete();
    node_start = true;
  }
  unsigned int constexpr dispatch_budget{65536};
  for(unsigned int step{0}; step != dispatch_budget; ++step) {
    if(sequence.quit || stack.empty()) { stack.clear();return nullptr; }
    auto &execution{stack.back()->execution};
    if(node_start) { input.key = scene_key::none;node_start = false; }
    auto const *node{execution.run()};
    if(!node) {
      stack.pop_back();
      if(!stack.empty()) stack.back()->execution.complete();
      node_start = true;
      continue;
    }
    if(node->kind == node_kind::load_parser) {
      auto const name{std::get<host_action>(node->data).resource};
      load(name,{});
      node_start = true;
    } else if(node->kind == node_kind::reset_skip) {
      sequence.skipped = false;
      execution.complete();
      node_start = true;
    } else if(node->kind == node_kind::image || node->kind == node_kind::animation || node->kind == node_kind::text) {
      if(sequence.skipped) { execution.complete();node_start = true;continue; }
      auto const &clip{std::get<presentation::clip>(node->data)};
      resources::bytes media;
      if(node->kind == node_kind::image) media = game.read("iff/raw/" + clip.resource + ".lbm");
      else if(node->kind == node_kind::animation) media = game.read("anim/" + clip.resource + ".256");
      // Text-only scenes optionally reload standard.col; image/movie palettes occupy a separate native buffer.
      if(node->kind == node_kind::text) {
        if(game.contains("colours/standard.col")) {
          auto const colours{game.read("colours/standard.col")};
          if(colours.size() != current_palette.size()) throw std::runtime_error{"standard text palette has the wrong size"};
          std::ranges::copy(colours,current_palette.begin());
        }
        current_palette[0] = current_palette[1] = current_palette[2] = 0;
      }
      scene = std::make_unique<scene_player>(node->kind,clip,media,font_data,state,[&](std::string_view const filename){
        auto const bytes{game.read(language + "/" + std::string{filename})};return std::string{bytes.begin(),bytes.end()};
      },current_palette,text_palette,counters,input);
      display = scene->image();
      displayed_colours = scene->palette();
      return nullptr;
    } else return node;
  }
  throw std::runtime_error{"presentation session does not yield"};
}
statement const *session::pending() const {
  return scene || stack.empty() ? nullptr : stack.back()->execution.current();
}
void session::complete() {
  if(scene || stack.empty()) throw std::runtime_error{"no suspended presentation host action"};
  auto const *action{pending()};
  if(action && (action->kind == node_kind::launch || action->kind == node_kind::planner)) sequence.skipped = false;
  stack.back()->execution.complete();
  node_start = true;
}
saved_state session::snapshot(save_mode const mode) const {
  if(scene || stack.size() != 1) throw std::runtime_error{"pilot save requires a suspended root presentation"};
  return stack.front()->execution.snapshot(mode);
}
void session::restore(saved_state const &saved) {
  if(scene || stack.size() != 1) throw std::runtime_error{"pilot load requires a suspended root presentation"};
  stack.front()->execution.restore(saved);
  node_start = true;
}
void session::seek(location const position) {
  auto saved{snapshot()};
  saved.resume = position;
  saved.callers.clear();
  restore(saved);
}
bool session::finished() const {
  return stack.empty();
}
render::framebuffer const &session::image() const {
  return display;
}
render::palette session::colours() const {
  return render::decode_palette(displayed_colours);
}
std::string const &session::function_name() const {
  static std::string const empty;
  if(stack.empty() || !stack.back()->execution.position()) return empty;
  return stack.back()->graph.functions[stack.back()->execution.position()->function].name;
}

} // namespace tfx::presentation
