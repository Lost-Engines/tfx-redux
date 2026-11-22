#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include "presentation/menu.h"
#include "presentation/menu_player.h"

namespace {
void require(bool const condition, char const *const message) {
  if(!condition) throw std::runtime_error{message};
}
}
int main() try {
  tfx::presentation::menu_program const menu{R"(
    ; Test-authored overlapping menu and signed store.
    backdrop test.lbm
    clickbox 1,2,3,4,first
    clickbox 2,3,4,5,second
    mouse = 1
    nobox
    display optional_unused_name
first:
    score = -1
    return
second:
    score = 42
    return
    eof
  )"};
  require(menu.backdrop == "test.lbm" && menu.animation.empty() && menu.cursor_style == 1 && !menu.flash_box,"menu setup differs");
  require(menu.hit_test({3,4}) == 0 && menu.hit_test({4,5}) == 1 && !menu.hit_test({0,2}),"click bounds must be inclusive and overlapping choices keep declaration priority");
  tfx::presentation::variables state{{"score",uint32_t{0}}};
  menu.choose(0,state);require(std::get<uint32_t>(state.at("score")) == 0xffffffffu,"signed menu assignment must store its complete native dword");
  menu.choose(1,state);require(std::get<uint32_t>(state.at("score")) == 42,"menu choice assignment differs");
  bool rejected{false};
  try { tfx::presentation::menu_program bad{"backdrop test.lbm\nclickbox 1,2,3,4,missing\ndisplay\neof"}; } catch(std::runtime_error const&) { rejected = true; }
  require(rejected,"missing choice labels must fail explicitly");
  tfx::resources::bytes screen(772 + 64000,0);
  screen[0] = 'w';screen[1] = 'a';screen[2] = 'r';screen[3] = ' ';
  for(unsigned int index{4}; index != 772; ++index) screen[index] = 63;
  std::array<tfx::presentation::pointer_style,2> pointers;
  for(auto &style : pointers) for(auto *shape : {&style.idle,&style.active}) shape->image = {
    .size{4,1},
    .pixels{0,7,0,0},
  };
  state["score"] = uint32_t{0};
  tfx::presentation::menu_player player{menu,screen,state,pointers,0};
  for(unsigned int frame{0}; frame != 40; ++frame) require(!player.advance(frame,{3,4},true),"menu must not choose during its fade-in");
  require(!player.choice() && std::get<uint32_t>(state.at("score")) == 0,"held initial click must wait until the content loop");
  player.advance(40,{3,4},true);
  require(player.choice() == 0 && std::get<uint32_t>(state.at("score")) == 0xffffffffu,"menu must commit its choice on press");
  for(unsigned int frame{41}; frame != 80; ++frame) require(!player.advance(frame,{4,5},true),"selected menu must wait for release and must not choose twice");
  require(std::get<uint32_t>(state.at("score")) == 0xffffffffu,"held movement must not activate a second menu choice");
  player.advance(80,{4,5},false);
  bool done{false};
  for(unsigned int frame{81}; frame != 121; ++frame) done = player.advance(frame,{4,5},false);
  require(done && player.image().pixels[0] == 0,"released menu must finish its fade and clear its display");
  std::cout << "Menu setup, inclusive click bounds, declaration priority and signed numeric choices passed\n";
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
