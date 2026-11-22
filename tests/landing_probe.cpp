#include <array>
#include <bit>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string_view>
#include <utility>
#include "cockpit/landing_display.h"
#include "resources/archive.h"

namespace {

tfx::flight::landing_state read_state() {
  tfx::flight::landing_state state;
  unsigned int height;
  std::cin >> state.enabled >> state.carrier >> state.beacon >> state.cell.x >> state.cell.y >> state.port.offset.x >> state.port.offset.y >> state.port.heading >> state.beacon_position.x >> state.beacon_position.y >> state.bearing.x >> state.bearing.y >> state.correction.x >> state.correction.y >> state.pitch_error >> height;
  state.height = static_cast<tfx::flight::approach_height>(height);
  return state;
}
std::array<int32_t, 16> words(tfx::flight::landing_state const &state) {
  return {state.enabled, state.carrier, state.beacon, state.cell.x, state.cell.y, state.port.offset.x, state.port.offset.y, state.port.heading, state.beacon_position.x, state.beacon_position.y, state.bearing.x, state.bearing.y, state.correction.x, state.correction.y, state.pitch_error, static_cast<int32_t>(std::to_underlying(state.height))};
}

} // anonymous namespace

int main(int const argc, char const *const argv[]) try {
  if(argc != 3) throw std::runtime_error{"landing probe requires data directory and guidance/selection mode"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  int command;
  while(std::cin >> command) {
    tfx::flight::landing_system controller{read_state()};
    if(std::string_view{argv[2]} == "guidance") {
      vec3<int32_t> player;
      vec2<int16_t> offset;
      bool speed;
      tfx::flight::landing_projection projection;
      std::cin >> player.x >> player.y >> player.z >> speed >> projection.valid >> projection.point.x >> projection.point.y >> offset.x >> offset.y;
      vec3<int32_t> requested;
      auto const cue{controller.advance(player, speed, [&](auto const &point){
        for(unsigned int axis{0}; axis != 3; ++axis) requested[axis] = tfx::math::signed_word(static_cast<uint16_t>(point[axis]));
        return projection;
      })};
      renderer.output.pixels.fill(99);
      tfx::render::framebuffer coverage;
      renderer.clipping = tfx::render::raster_clip::viewport({0, 0}, {319, 199});
      tfx::cockpit::draw_landing_cue(renderer, tables.instruments(), cue, offset, &coverage);
      auto const state{words(controller.value())};
      std::array<int32_t, 4> const extra{cue.speed_request, requested.x, requested.y, requested.z};
      std::cout.write(reinterpret_cast<char const *>(state.data()), sizeof(state));
      std::cout.write(reinterpret_cast<char const *>(extra.data()), sizeof(extra));
      std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
      std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
    } else if(std::string_view{argv[2]} == "selection" || std::string_view{argv[2]} == "destination") {
      tfx::flight::landing_input input;
      unsigned int craft;
      unsigned int route_count;
      unsigned int sites_count;
      unsigned int carrier_count;
      std::cin >> craft >> input.inhibited >> input.crashed >> input.ejected >> input.damaged >> input.realistic >> route_count;
      input.aircraft = static_cast<tfx::flight::player_aircraft>(craft + 1);
      std::vector<vec3<int32_t>> route;
      for(unsigned int index{0}; index != route_count; ++index) {
        int32_t x;
        int32_t z;
        std::cin >> x >> z;
        route.emplace_back(x, 0, z);
      }
      std::map<std::pair<int16_t, int16_t>, tfx::flight::landing_port> sites;
      std::cin >> sites_count;
      for(unsigned int index{0}; index != sites_count; ++index) {
        int16_t x;
        int16_t z;
        tfx::flight::landing_port port;
        std::cin >> x >> z >> port.offset.x >> port.offset.y >> port.heading;
        sites.emplace(std::pair{x, z}, port);
      }
      std::cin >> carrier_count;
      std::vector<tfx::flight::landing_surface> carriers;
      for(unsigned int index{0}; index != carrier_count; ++index) {
        tfx::flight::landing_surface surface;
        unsigned int flags;
        unsigned int side;
        int type;
        std::cin >> surface.position_q8.x >> surface.position_q8.y >> surface.position_q8.z >> surface.port.offset.x >> surface.port.offset.y >> surface.port.heading >> surface.destroyed >> flags >> side >> type;
        if(!(flags & 24) && side == 1 && type == 50) carriers.push_back(std::move(surface));
      }
      auto const terrain{[&](vec2<int16_t> const &cell)->std::optional<tfx::flight::landing_port> {
        auto const found{sites.find({cell.x, cell.y})};
        return found == sites.end() ? std::nullopt : std::optional{found->second};
      }};
      auto const result{std::string_view{argv[2]} == "destination" ? controller.select_runway(route, terrain, carriers) : controller.toggle(input, route, terrain, carriers)};
      for(auto const value : words(controller.value())) std::cout << value << ' ';
      std::cout << result.navigation << ' ' << result.afterburner_stop_sample << ' ' << (result.music_cue ? static_cast<int>(*result.music_cue) : -1) << ' ' << (result.waypoint ? static_cast<int>(*result.waypoint) : -1) << ' ' << result.controls.has_value();
      if(result.controls) {
        auto const &control{*result.controls};
        std::cout << ' ' << control.configuration << ' ' << std::bit_cast<uint64_t>(control.response) << ' ' << std::bit_cast<uint64_t>(control.pitch_response) << ' ' << std::bit_cast<uint64_t>(control.roll_response) << ' ' << std::bit_cast<uint64_t>(control.yaw_response);
      }
      std::cout << ' ' << result.messages.size();
      for(auto const message : result.messages) std::cout << ' ' << std::to_underlying(message);
      std::cout << '\n';
    } else {
      throw std::runtime_error{"unknown landing probe mode"};
    }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << error.what() << '\n';
  return EXIT_FAILURE;
}
