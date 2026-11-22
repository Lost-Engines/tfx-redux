#include <cstdlib>
#include <iostream>
#include <utility>
#include "cockpit/hud.h"

int main(int const argc, char const *const argv[]) try {
  if(argc < 3 || argc > 4) throw std::runtime_error{"usage: hud_probe DATA_DIRECTORY MAP_NAME"};
  auto const archive{tfx::resources::find_game_archive(argv[1])};
  tfx::resources::archive const game{archive};
  tfx::resources::executable_tables const tables{archive.parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  tfx::cockpit::hud hud{tables.instruments(), game, tables.weapons, argv[2]};
  if(argc == 4 && std::string_view{argv[3]} == "--controls") {
    unsigned int centre{0};
    unsigned int action{0};
    unsigned int aircraft{0};
    unsigned int category{0};
    bool targets{false};
    tfx::cockpit::hud_selection selected;
    while(std::cin >> selected.left >> centre >> selected.right >> selected.horizontal_range >> selected.bvr_range >> action >> aircraft >> targets >> category) {
      selected.centre = static_cast<tfx::cockpit::centre_display>(centre);
      auto const plane{static_cast<tfx::flight::player_aircraft>(aircraft + 1)};
      switch(action) {
      case 0:
        selected.cycle_left(targets);
        break;
      case 1:
        selected.cycle_centre(plane);
        break;
      case 2:
        selected.cycle_right(plane, static_cast<uint8_t>(category));
        break;
      case 3:
        selected.cycle_range();
        break;
      case 4:
        selected.normalise();
        break;
      default:
        throw std::runtime_error{"unknown display-control probe action"};
      }
      std::cout << selected.left << ' ' << std::to_underlying(selected.centre) << ' ' << selected.right << ' ' << selected.horizontal_range << ' ' << selected.bvr_range << '\n';
    }
    return EXIT_SUCCESS;
  }
  unsigned int index{0};
  while(std::cin >> index) {
    uint16_t constexpr left_pages[]{0, 1, 2, 3, 4, 5, 8};
    uint16_t constexpr right_pages[]{0, 1, 2, 3, 4, 5, 7, 8};
    tfx::cockpit::hud_selection selection{
      .left{left_pages[(index / 3) % 7]},
      .centre{static_cast<tfx::cockpit::centre_display>(index % 3)},
      .right{right_pages[(index / 21) % 8]},
      .horizontal_range{static_cast<uint16_t>(index % 4)},
      .bvr_range{static_cast<uint16_t>(index % 3)},
    };
    auto const aircraft{static_cast<tfx::flight::player_aircraft>((index / 168) % 3 + 1)};
    tfx::weapons::armament inventory{aircraft, tables.weapons, tables.arming};
    for(auto &pylon : inventory.pylons) {
      pylon.weapon = 1;
      pylon.count = 1;
    }
    inventory.selected = 0;
    inventory.guns = index % 2 != 0;
    tfx::cockpit::hud_state state;
    auto &instruments{state.instruments};
    instruments.aircraft = aircraft;
    instruments.systems.fill(tfx::flight::system_condition::normal);
    for(unsigned int slot{0}; slot != instruments.systems.size(); ++slot) {
      if((index + slot) % 9 == 0) instruments.systems[slot] = tfx::flight::system_condition::intermittent;
    }
    instruments.speed = static_cast<int32_t>(index + 100);
    instruments.height = -static_cast<float>(index * 2);
    instruments.heading = static_cast<uint16_t>(index * 182);
    instruments.fuel = std::array<double, 4>{0, 999.5, 2499.5, 5000}[index % 4];
    instruments.fuel_gauge = static_cast<int16_t>((index * 137) % 10000);
    instruments.smoothed_fuel_burn = 4;
    instruments.throttle = 0.75f;
    instruments.engine_load = {.65, .75};
    instruments.inventory = &inventory;
    instruments.confirmed_target = index % 2 != 0;
    instruments.controls.trailing_edge_flap = .3;
    instruments.controls.leading_edge_flap = -.7;
    instruments.controls.thrust_vector = {-.2, .1};
    instruments.controls.configuration = static_cast<tfx::cockpit::flight_configuration>(index % 5);
    instruments.controls.autopilot = {
      .enabled{index % 2 != 0},
      .mode{static_cast<tfx::cockpit::autopilot_mode>(index % 5)},
      .heading{16384},
      .waypoint{3},
      .height{-1000},
      .speed{250},
    };
    instruments.offset = {static_cast<int16_t>(index % 4), static_cast<int16_t>((index / 4) % 2 * 8)};
    state.position = {100000, -300, 110000};
    state.ticks = index * 54;
    state.clock_offset = 540;
    state.elapsed = static_cast<uint16_t>(index % 55);
    state.ecm = index % 3 == 0;
    state.stealth = index % 5 == 0;
    state.chaff = static_cast<uint8_t>(index % 16);
    state.flares = static_cast<uint8_t>((index / 2) % 16);
    state.hook = index % 5 == 0;
    state.fire_permission_notice = index % 7 == 0;
    if(index % 2) state.tracked_weapon_category = tables.weapons[1].category;
    state.stall_recovery = index % 11 == 0;
    state.threat = index % 13 == 0;
    state.redraw_phase = static_cast<uint8_t>(index % 4);
    state.afterburner_pips = index % 5;
    if(index % 2) {
      state.approach_cue = static_cast<uint8_t>(index % 3);
      state.landing_cell.emplace(50, 60);
    }
    state.approach_suppressed = index % 3 == 0;
    state.low_speed_ground_contact = index % 4 == 0;
    state.landed = index % 8 == 0;
    std::array<tfx::cockpit::radar_aircraft, 1> const enemy{{
      {
        .position_q8{103000 * 256, -100 * 256, 112000 * 256},
        .selected{false}
      },
    }};
    std::array<tfx::cockpit::radar_aircraft, 1> const allied{{
      {
        .position_q8{99000 * 256, -100 * 256, 111000 * 256},
        .selected{false}
      },
    }};
    std::array<tfx::cockpit::radar_surface, 1> const enemy_surface{{
      {
        .position_q8{100000 * 256, 0, 111000 * 256},
        .type{56}
      },
    }};
    std::array<tfx::cockpit::radar_surface, 1> const allied_surface{{
      {
        .position_q8{100000 * 256, 0, 109000 * 256},
        .type{3}
      },
    }};
    state.contacts = {
      .enemy_aircraft{enemy},
      .allied_aircraft{allied},
      .enemy_surface{enemy_surface},
      .allied_surface{allied_surface},
      .projectiles{},
    };
    std::array<vec3<int32_t>, 1> const targets{{{102000, 0, 112000}}};
    state.targets = targets;
    tfx::math::random_state random;
    renderer.output.pixels.fill(99);
    tfx::render::framebuffer coverage;
    auto const effects{hud.draw(renderer, selection, state, random, &coverage)};
    auto const animation{hud.animation()};
    auto const &clip{renderer.clipping};
    for(uint32_t const value : {static_cast<uint32_t>(animation.warnings.countdown), static_cast<uint32_t>(animation.warnings.phase), static_cast<uint32_t>(effects.threat_sound), static_cast<uint32_t>(animation.auxiliary.engine_refresh), static_cast<uint32_t>(animation.auxiliary.afterburner_refresh), animation.map.ticks, static_cast<uint32_t>(animation.map.target_colour), static_cast<uint32_t>(animation.map.landing_colour), static_cast<uint32_t>(animation.radar_pulse), static_cast<uint32_t>(animation.redraw_phase), static_cast<uint32_t>(random.low), static_cast<uint32_t>(random.high), static_cast<uint32_t>(clip.glyph_minimum.x), static_cast<uint32_t>(clip.glyph_minimum.y), static_cast<uint32_t>(clip.glyph_limit.x), static_cast<uint32_t>(clip.glyph_limit.y), static_cast<uint32_t>(clip.sprite_minimum.x), static_cast<uint32_t>(clip.sprite_minimum.y), static_cast<uint32_t>(clip.sprite_limit.x), static_cast<uint32_t>(clip.sprite_limit.y)}) {
      for(unsigned int byte{0}; byte != 4; ++byte) std::cout.put(static_cast<char>(value >> (byte * 8)));
    }
    std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
