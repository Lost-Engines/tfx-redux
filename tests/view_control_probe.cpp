#include <iostream>
#include <utility>
#include "cockpit/view_control.h"

int main() {
  using tfx::cockpit::view_control;
  using tfx::cockpit::view_control_state;
  using tfx::cockpit::view_key;
  using tfx::cockpit::view_key_context;
  using tfx::cockpit::view_mode;
  view_control views;
  int operation;
  while(std::cin >> operation) {
    if(operation == 0) {
      view_control_state initial;
      int current;
      std::cin >> current >> initial.offset.x >> initial.offset.y;
      initial.current = static_cast<view_mode>(current);
      views = view_control{std::move(initial)};
    } else if(operation == 1) {
      int mode, speed;
      std::cin >> mode >> speed;
      views.enqueue(static_cast<view_mode>(mode),speed);
    } else if(operation == 2) views.consume();
    else if(operation == 3) views.clear_queue();
    else if(operation == 4) views.ready();
    else if(operation == 5) {
      view_key_context context;
      int key;
      std::cin >> key >> context.inhibited >> context.crashed >> context.ejected >> context.landing >> context.shift >> context.low_memory >> context.speed
        >> context.position.x >> context.position.y >> context.position.z >> context.angles.x >> context.angles.y >> context.angles.z
        >> context.missile_available >> context.target_available >> context.target_is_ground >> context.tracking_available;
      views.press(static_cast<view_key>(key),context);
    }
    auto const &state{views.value()};
    auto const put{[](auto const value){ std::cout << static_cast<int32_t>(value) << ' '; }};
    put(state.current);put(state.previous);put(state.written);put(state.read);
    for(auto const view : state.queue) put(view);
    put(state.offset.x);put(state.offset.y);put(state.pending_offset.x);put(state.pending_offset.y);put(state.saved_offset.x);put(state.saved_offset.y);
    put(state.distance);put(state.pending_distance);put(state.target_distance);put(state.recenter);put(state.departure_delay);put(state.tracking_roll);put(state.tracking_complete);put(state.redraw);put(state.ready);
    for(auto const *basis : {&state.orbit,&state.missile_orbit}) for(unsigned int row{0}; row != 3; ++row) { put(basis->right[row]);put(basis->down[row]);put(basis->forward[row]); }
    put(state.fixed_position.x);put(state.fixed_position.y);put(state.fixed_position.z);
    std::cout << '\n';
  }
}
