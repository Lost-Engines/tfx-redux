#include "view_control.h"
#include <utility>

namespace tfx::cockpit {

view_control::view_control(view_control_state initial) : state{std::move(initial)} {}
view_control_state const &view_control::value() const {
  return state;
}
void view_control::offset(vec2<int16_t> const &position) {
  state.offset = vec2<int16_t>{position};
}
void view_control::redraw(uint8_t const remaining) {
  state.redraw = remaining;
}
void view_control::fixed_position(vec3<int32_t> const &position) {
  state.fixed_position = vec3<int32_t>{position};
}
void view_control::clear_queue() {
  state.written = state.read = 0;
}
void view_control::ready() {
  state.ready = true;
}
void view_control::enqueue(view_mode view, int32_t const speed) {
  // Native 0x8fd84 discards any pending transition when leaving a missile view.
  if(state.current == view_mode::missile || state.current == view_mode::missile_infrared) {
    state.recenter = 1;
    clear_queue();
  }
  int constexpr minimum_fly_past_speed{20};
  if(view == view_mode::fly_past && speed < minimum_fly_past_speed) {
    state.distance = 40;
    state.recenter = 1;
    view = view_mode::chase;
  }
  if(state.written < state.queue.size()) state.queue[state.written++] = view;
}
void view_control::start_impact_orbit(int32_t const speed) {
  // Native 7D349 queues the loss camera, then replaces its orbit basis with
  // a fresh 1/16-turn pitch. It must not inherit the previous chase orbit.
  math::angle constexpr initial_pitch{0x1000};
  enqueue(view_mode::impact_orbit, speed);
  state.orbit = {};
  state.orbit.rotate(math::rotation_axis::pitch, initial_pitch);
}
void view_control::consume() {
  // The flight loop calls this only after the active camera stage signals ready.
  if(state.read != state.written) {
    state.previous = state.current;
    state.current = state.queue[state.read++];
    state.redraw = 3;
    if(state.pending_offset.x != -1) state.offset.x = state.pending_offset.x;
    if(state.pending_offset.y != -1) state.offset.y = state.pending_offset.y;
    state.pending_offset = {-1, -1};
  } else {
    clear_queue();
  }
  if(state.pending_distance) state.distance = state.pending_distance;
  state.pending_distance = 0;
  state.ready = false;
}
void view_control::save_offset() {
  if(state.saved_offset.y == -1) state.saved_offset = vec2<int16_t>{state.offset};
}
void view_control::centre(view_key_context const &context) {
  if(context.inhibited || context.crashed || context.ejected) return;
  auto const queue{[&](view_mode const view){enqueue(view, context.speed);}};
  state.recenter = 0;
  switch(state.current) {
  case view_mode::look_left:
  case view_mode::look_right:
  case view_mode::look_far_left:
  case view_mode::look_far_right:
    queue(view_mode::centre_head);
    return;
  case view_mode::fly_past:
  case view_mode::approach_cockpit:
  case view_mode::virtual_cockpit:
  case view_mode::centre_head:
    return;
  case view_mode::chase:
    clear_queue();
    state.orbit = math::attitude{};
    state.orbit.rotate(math::rotation_axis::pitch, 0x2000);
    state.distance = 50;
    queue(view_mode::approach_cockpit);
    state.recenter = 0;
    break;
  case view_mode::target_tracking:
    break;
  case view_mode::tactical:
    state.orbit = math::attitude{};
    state.recenter = 1;
    break;
  default:
    state.orbit = math::attitude{};
    queue(view_mode::cockpit);
    queue(view_mode::leave_cockpit);
    queue(view_mode::leave_panel);
    break;
  }
  queue(view_mode::centre_head);
  queue(view_mode::virtual_cockpit);
}
view_key_events view_control::press(view_key const key, view_key_context const &context) {
  view_key_events result;
  if(context.inhibited || context.crashed || context.ejected) return result;
  // F5 intentionally lacks the automatic-landing gate used by the other keys.
  if(key != view_key::chase && context.landing) return result;
  auto const queue{[&](view_mode const view){enqueue(view, context.speed);}};
  switch(key) {
  case view_key::front:
    if(context.shift) {
      save_offset();
      state.pending_offset = {0, 50};
      queue(view_mode::full_forward);
      break;
    }
    if(context.low_memory) {
      queue(view_mode::cockpit);
      state.distance = 0;
      break;
    }
    if(state.saved_offset.y != -1) {
      state.offset = vec2<int16_t>{state.saved_offset};
      state.saved_offset.y = -1;
    }
    if(state.current == view_mode::chase) {
      centre(context);
      queue(view_mode::virtual_cockpit);
      queue(view_mode::enter_cockpit);
      queue(view_mode::leave_cockpit);
    }
    if(state.current == view_mode::virtual_cockpit || state.current == view_mode::look_left || state.current == view_mode::look_right || state.current == view_mode::centre_head) {
      queue(view_mode::centre_head);
      queue(view_mode::virtual_cockpit);
      queue(view_mode::enter_cockpit);
      queue(view_mode::leave_cockpit);
    }
    queue(view_mode::cockpit);
    break;
  case view_key::left:
    if(state.current == view_mode::look_left) {
      queue(view_mode::look_far_left);
    } else if(state.current == view_mode::look_far_left) {
      queue(view_mode::centre_head);
      queue(view_mode::virtual_cockpit);
    } else {
      if(state.current == view_mode::look_far_right) {
        queue(view_mode::centre_head);
        queue(view_mode::virtual_cockpit);
      } else {
        centre(context);
      }
      queue(view_mode::look_left);
    }
    break;
  case view_key::right:
    if(state.current == view_mode::look_right) {
      queue(view_mode::look_far_right);
    } else if(state.current == view_mode::look_far_right) {
      queue(view_mode::centre_head);
      queue(view_mode::virtual_cockpit);
    } else {
      if(state.current == view_mode::look_far_left) {
        queue(view_mode::centre_head);
        queue(view_mode::virtual_cockpit);
      } else {
        centre(context);
      }
      queue(view_mode::look_right);
    }
    break;
  case view_key::rear:
    queue(view_mode::rear);
    break;
  case view_key::chase:
    if(context.low_memory) {
      state.distance = 40;
      state.recenter = 1;
    } else {
      if(state.current == view_mode::cockpit) {
        state.orbit = math::attitude{};
        state.recenter = 1;
        queue(view_mode::leave_cockpit);
        queue(view_mode::leave_panel);
        queue(view_mode::virtual_cockpit);
      }
      if(state.current == view_mode::tactical || state.current == view_mode::chase) {
        state.distance = 40;
        state.recenter = 1;
      } else {
        state.recenter = 0;
        state.departure_delay = 20;
        queue(view_mode::depart_cockpit);
      }
    }
    queue(view_mode::chase);
    break;
  case view_key::fly_past:
    {
    if(context.speed < 20) break;
    state.fixed_position = vec3<int32_t>{context.position};
    vec3<math::angle> const angles{context.angles.x, static_cast<math::angle>(context.angles.y - 700), context.angles.z};
    math::advance_position(state.fixed_position, angles, math::signed_word(static_cast<uint16_t>(context.speed >> 1)), context.height_bounds.x, context.height_bounds.y);
    // Native also computes a bearing into an otherwise unused stack temporary.
    queue(view_mode::fly_past);
    result.fly_past_sound = true;
    break;
  }
  case view_key::tactical:
    queue(view_mode::tactical);
    state.tracking_complete = 0;
    break;
  case view_key::missile:
    if(!context.missile_available) break;
    if(state.current == view_mode::missile) {
      queue(view_mode::missile_infrared);
      save_offset();
      state.pending_offset = {0, 0};
    } else {
      state.recenter = 1;
      state.orbit = math::attitude{};
      state.missile_orbit = math::attitude{};
      state.orbit.rotate(math::rotation_axis::heading, 0x500);
      state.orbit.rotate(math::rotation_axis::pitch, 0x500);
      state.distance = 300;
      queue(view_mode::missile);
    }
    break;
  case view_key::target:
    state.recenter = 1;
    // Native compares the returned pointer with the ground pointer before
    // checking for null: two absent targets also select the ground distance.
    state.target_distance = !context.target_available || context.target_is_ground ? 2500 : 300;
    queue(view_mode::target);
    break;
  case view_key::virtual_cockpit:
    if(state.current == view_mode::virtual_cockpit) {
      int const roll{math::signed_word(context.angles.z)};
      if(context.tracking_available && roll > -0x3000 && roll < 0x3000) queue(view_mode::target_tracking);
    } else {
      centre(context);
      state.tracking_roll = 0;
    }
    break;
  }
  return result;
}

} // namespace tfx::cockpit
