#include <chrono>
#include <fstream>
#include <iostream>
#include "configuration/graphics.h"
#include "presentation/graphics_menu.h"

void require(bool ok,char const *message) {if(!ok) throw std::runtime_error{message};}
int main() try {
  using namespace tfx::configuration;
  using namespace tfx::presentation;
  graphics_settings state;
  auto const unchanged{state};
  for(auto const &[field,text] : std::initializer_list<std::pair<graphics_field,std::string_view>>{
    {graphics_field::resolution,"0x600"},{graphics_field::resolution,"16385x200"},
    {graphics_field::resolution,"10000x10000"},{graphics_field::resolution,"640x400junk"},
    {graphics_field::field_of_view,"nan"},{graphics_field::field_of_view,"9"},{graphics_field::field_of_view,"171"},
    {graphics_field::draw_distance,"-1"},{graphics_field::draw_distance,"10001"},
    {graphics_field::lod_scale,"-0.1"},{graphics_field::lod_scale,"inf"},{graphics_field::window_scale,"17"}}) {
    bool refused{false};try {set_graphics_value(state,field,text);} catch(std::invalid_argument const&) {refused=true;}
    require(refused,"invalid graphics value accepted");
    require(state.video.resolution==unchanged.video.resolution && state.video.field_of_view==unchanged.video.field_of_view && state.video.lod_scale==unchanged.video.lod_scale && state.video.draw_distance==unchanged.video.draw_distance && state.window_scale==unchanged.window_scale,"rejected edit mutated settings");
  }
  state=parse_graphics("# test\r\nRESOLUTION=1001x603\r\nFOV=97.25\nDRAW_DISTANCE=INFINITE\nLOD_SCALE=0\nWINDOW_SCALE=2\nUNKNOWN=kept out\n");
  require(!state.video.automatic_resolution && state.video.resolution==vec2<int>{1001,603} && state.video.field_of_view==97.25 && state.video.draw_distance==0 && state.video.lod_scale==0 && state.window_scale==2,"saved graphics parsing changed values");
  // The editor logic needs no artwork. An empty synthetic font keeps this
  // cancellation/validation test independent of the original game assets.
  tfx::resources::bytes font_bytes(0x100000,0);
  widget_font font{font_bytes};
  graphics_menu menu{font,state};
  auto const edit{[](graphics_menu &menu,int row,std::string_view text) {
    menu.click({180,static_cast<int16_t>(60+16*row)});
    for(unsigned char c:text) menu.character(c);
  }};
  edit(menu,2,"999");menu.click({260,172});
  require(menu.result()==graphics_action::none && menu.value().video.field_of_view==97.25,"invalid edit applied");
  menu.click({160,172});require(menu.result()==graphics_action::cancel,"invalid edit prevented cancellation");
  graphics_menu apply{font,state};
  edit(apply,2,"105.5");edit(apply,0,"AUTO");edit(apply,1,"FIT");
  apply.click({260,172});
  require(apply.result()==graphics_action::apply && apply.value().video.field_of_view==105.5 && apply.value().video.automatic_resolution && apply.value().window_scale==0 && apply.window_resize_requested(),"arbitrary values/auto/fit did not apply");
  require(state.video.field_of_view==97.25,"menu changed its input before Apply");
  graphics_menu precision{font,graphics_settings{}};
  precision.click({180,92});precision.press(graphics_key::enter);precision.click({260,172});
  require(precision.value().video.field_of_view==graphics_settings{}.video.field_of_view,"opening and accepting FOV rounded its value");
  require(!precision.window_resize_requested(),"editing FOV resized the window");
  graphics_menu defaults{font,state};defaults.click({70,172});defaults.click({260,172});
  auto const &fresh{defaults.value()};
  require(fresh.video.automatic_resolution && fresh.video.draw_distance==16 && fresh.video.lod_scale==.1 && fresh.video.field_of_view==graphics_settings{}.video.field_of_view && fresh.window_scale==0,"Defaults did not restore all graphics choices");
  graphics_menu limits{font,state};limits.click({276,108});
  require(limits.value().video.draw_distance==0,"infinite distance wrapped incorrectly");
  limits.click({132,108});require(limits.value().video.draw_distance==10000,"cannot step back from infinite distance");
  limits.click({132,124});require(limits.value().video.lod_scale==0,"LOD went negative");
  auto const root{std::filesystem::temp_directory_path()/("tfx-graphics-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  std::filesystem::create_directory(root);
  struct cleanup {std::filesystem::path path;~cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}} const cleanup{root};
  require(load_graphics(root).video.draw_distance==16,"missing file did not use defaults");
  {std::ofstream original{root/"TFX.CFG"};original<<"REALISM=0,1,1,1,0,1,1\n";}
  state.video.field_of_view=graphics_settings{}.video.field_of_view;
  save_graphics(root,state);auto loaded{load_graphics(root)};
  require(loaded.video.field_of_view==state.video.field_of_view && loaded.video.resolution==state.video.resolution && loaded.window_scale==state.window_scale && loaded.video.draw_distance==0,"saved choices lost precision or values");
  state.video.automatic_resolution=true;state.video.lod_scale=.05;save_graphics(root,state);loaded=load_graphics(root);
  require(loaded.video.automatic_resolution && loaded.video.lod_scale==.05,"replacement save failed");
  std::ifstream original{root/"TFX.CFG"};std::string line;std::getline(original,line);
  require(line=="REALISM=0,1,1,1,0,1,1","Redux graphics changed original configuration");
  for(auto const &entry:std::filesystem::directory_iterator(root)) require(entry.path().filename()=="TFX.CFG" || entry.path().filename()==graphics_filename,"temporary save file leaked");
  std::cout<<"Graphics validation, menu Apply/Cancel/Defaults, arbitrary values, exact persistence and separate configuration passed\n";
} catch(std::exception const &e) {std::cerr<<e.what()<<'\n';return 1;}
