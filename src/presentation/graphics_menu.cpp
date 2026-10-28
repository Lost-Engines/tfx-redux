#include "graphics_menu.h"
#include <array>
#include <format>

namespace tfx::presentation {
namespace {
using configuration::graphics_field;
std::array<std::string_view,5> constexpr labels{"RENDER SIZE","WINDOW SCALE","HORIZONTAL FOV","DRAW DISTANCE","LOD SCALE"};
std::array<std::string_view,5> constexpr hints{"AUTO FOLLOWS THE WINDOW PIXELS","FIT / 0 FITS THE DESKTOP","HORIZONTAL ANGLE: 10 TO 170 DEGREES","RADIUS IN CELLS; 0 / INFINITE = WHOLE MAP","0 = FULL DETAIL; 1 = ORIGINAL CUTOFFS"};
dialogue_widget field_widget(unsigned int row) {return {.position{16,static_cast<uint8_t>(3+row*2)},.size{14,1}};}
dialogue_widget arrow_widget(unsigned int row,bool right) {return {.position{static_cast<uint8_t>(right ? 32 : 14),static_cast<uint8_t>(3+row*2)},.size{1,1},.text{right ? ">" : "<"}};}
std::array<dialogue_widget,3> const actions{{
  {.position{2,17},.size{10,1},.text{"DEFAULTS"}},
  {.position{15,17},.size{6,1},.text{"CANCEL"}},
  {.position{27,17},.size{8,1},.text{"APPLY"}},
}};
}
graphics_menu::graphics_menu(widget_font const &font, configuration::graphics_settings const &settings) : font{font},pending{settings} {}
graphics_action graphics_menu::result() const {return action;}
configuration::graphics_settings const &graphics_menu::value() const {return pending;}
bool graphics_menu::window_resize_requested() const {return resize_window;}
void graphics_menu::failed(std::string message) {action=graphics_action::none;error=std::move(message);}
void graphics_menu::edit(unsigned int row) {
  selected=row;editing=true;replace_text=true;field_changed=false;error.clear();
  text=configuration::graphics_value(pending,static_cast<graphics_field>(row));
}
bool graphics_menu::accept_field() {
  if(!editing) return true;
  try {
    if(field_changed) configuration::set_graphics_value(pending,static_cast<graphics_field>(selected),text);
    if(selected==1) resize_window=true;
    editing=false;error.clear();return true;
  } catch(std::exception const &e) {error=e.what();return false;}
}
void graphics_menu::character(unsigned int c) {
  if(c<32 || c>126) return;
  if(!editing) edit(selected);
  if(replace_text) {text.clear();replace_text=false;}
  if(text.size()<22) {text.push_back(static_cast<char>(c));field_changed=true;}
  error.clear();
}
void graphics_menu::adjust(int direction) {
  if(!accept_field()) return;
  auto const field{static_cast<graphics_field>(selected)};
  auto const choose{[&](auto const &choices,double current) {
    if(direction>0) {
      for(auto value:choices) if(value>current) return static_cast<double>(value);
      return static_cast<double>(choices.back());
    }
    for(auto it=choices.rbegin();it!=choices.rend();++it) if(*it<current) return static_cast<double>(*it);
    return static_cast<double>(choices.front());
  }};
  if(field==graphics_field::resolution) {
    std::array<std::string_view,11> constexpr sizes{"AUTO","320x200","640x400","960x600","1280x720","1280x800","1600x900","1920x1080","2560x1440","3840x2160","7680x4320"};
    auto const current{configuration::graphics_value(pending,field)};auto const found{std::ranges::find(sizes,current)};
    auto const index{found==sizes.end() ? 0 : std::clamp(static_cast<int>(found-sizes.begin())+direction,0,static_cast<int>(sizes.size()-1))};
    configuration::set_graphics_value(pending,field,sizes[static_cast<size_t>(index)]);
  } else if(field==graphics_field::window_scale) {
    pending.window_scale=static_cast<unsigned int>(std::clamp(static_cast<int>(pending.window_scale)+direction,0,16));resize_window=true;
  } else if(field==graphics_field::field_of_view) pending.video.field_of_view=std::clamp(std::round(pending.video.field_of_view)+direction,10.,170.);
  else if(field==graphics_field::draw_distance) {
    std::array<unsigned int,12> constexpr distances{1,4,8,16,24,32,48,64,100,200,10000,10001};
    auto const distance{static_cast<unsigned int>(choose(distances,pending.video.draw_distance ? pending.video.draw_distance : 10001))};
    pending.video.draw_distance=distance==10001 ? 0 : distance;
  } else {
    std::array<double,11> constexpr scales{0,.01,.05,.1,.25,.5,1,2,5,10,100};
    pending.video.lod_scale=choose(scales,pending.video.lod_scale);
  }
}
void graphics_menu::press(graphics_key key,bool shift) {
  switch(key) {
    case graphics_key::escape: if(editing) {editing=false;error.clear();} else action=graphics_action::cancel;break;
    case graphics_key::enter: if(editing) accept_field();else action=graphics_action::apply;break;
    case graphics_key::backspace:
      if(!editing) edit(selected);
      if(replace_text) text.clear();else if(!text.empty()) text.pop_back();replace_text=false;field_changed=true;error.clear();break;
    case graphics_key::erase: if(!editing) edit(selected);text.clear();replace_text=false;field_changed=true;error.clear();break;
    case graphics_key::left: adjust(-1);break;
    case graphics_key::right: adjust(1);break;
    case graphics_key::tab:case graphics_key::up:case graphics_key::down:
      if(accept_field()) {int const direction{key==graphics_key::up || (key==graphics_key::tab && shift) ? -1 : 1};selected=static_cast<unsigned int>((static_cast<int>(selected)+direction+5)%5);}break;
  }
}
void graphics_menu::click(vec2<int16_t> const &point) {
  // Cancel remains usable even if an edited value is invalid.
  if(dialogue_contains(form,actions[1],point)) {action=graphics_action::cancel;return;}
  if(dialogue_contains(form,actions[0],point)) {pending={};editing=false;error.clear();resize_window=true;return;}
  if(editing && dialogue_contains(form,field_widget(selected),point)) {replace_text=true;return;}
  if(!accept_field()) return;
  if(dialogue_contains(form,actions[2],point)) {action=graphics_action::apply;return;}
  for(unsigned int row=0;row<labels.size();++row) {
    if(dialogue_contains(form,field_widget(row),point)) {edit(row);return;}
    for(bool right : {false,true}) if(dialogue_contains(form,arrow_widget(row,right),point)) {selected=row;adjust(right ? 1 : -1);return;}
  }
}
void graphics_menu::draw(render::framebuffer &image) const {
  draw_dialogue_form(image,form);
  font.draw(image,{160-font.measure("REDUX GRAPHICS")/2,33},"REDUX GRAPHICS",dialogue_palette_base+15);
  for(unsigned int row=0;row<labels.size();++row) {
    font.draw(image,{24,58+static_cast<int>(row)*16},labels[row],dialogue_palette_base+15);
    auto field{field_widget(row)};
    field.text=editing && selected==row ? text+(replace_text ? "" : "_") : configuration::graphics_value(pending,static_cast<graphics_field>(row));
    while(!field.text.empty() && font.measure(field.text)>field.size.x*8-4) field.text.erase(field.text.begin());
    field.background=static_cast<uint8_t>(selected==row ? 11 : 8);field.border=dialogue_border::sunken;
    draw_dialogue_widget(image,font,form,field);
    for(bool right : {false,true}) draw_dialogue_widget(image,font,form,arrow_widget(row,right));
  }
  auto const hint{error.empty() ? hints[selected] : std::string_view{error}};
  font.draw(image,{160-font.measure(hint)/2,140},hint,dialogue_palette_base+15);
  font.draw(image,{31,151},"CLICK VALUE TO TYPE; ARROWS TO ADJUST",dialogue_palette_base+15);
  for(auto const &button:actions) draw_dialogue_widget(image,font,form,button);
}
} // namespace tfx::presentation
