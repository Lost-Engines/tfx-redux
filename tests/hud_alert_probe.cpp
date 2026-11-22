#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include "cockpit/hud_alerts.h"
#include "resources/archive.h"

int main(int const argc, char const *const argv[]) {
  if(argc != 2) throw std::runtime_error{"HUD alert probe requires original data directory"};
  tfx::resources::executable_tables const tables{tfx::resources::find_game_archive(argv[1]).parent_path() / "TFX.DAT"};
  tfx::render::software_renderer renderer{tables};
  std::optional<tfx::cockpit::hud_alerts> alerts;
  int reset;
  int alert;
  int remaining;
  int request;
  int elapsed;
  int dx;
  int dy;
  int lx;
  int ly;
  int hx;
  int hy;
  int draws;
  int background;
  double fuel;
  while(std::cin >> reset >> alert >> remaining >> request >> fuel >> elapsed >> dx >> dy >> lx >> ly >> hx >> hy >> draws >> background) {
    if(reset) {
      alerts.emplace(tables.instruments(), tfx::cockpit::hud_alert_state{
        .alert{static_cast<tfx::cockpit::hud_alert>(alert)},
        .remaining{static_cast<int16_t>(remaining)},
      });
    }
    if(!alerts) throw std::runtime_error{"HUD alert sequence requires reset"};
    if(request >= 0) alerts->show(static_cast<tfx::cockpit::hud_alert>(request));
    alerts->check_fuel(fuel);
    renderer.output.pixels.fill(static_cast<uint8_t>(background));
    tfx::render::framebuffer coverage;
    for(int draw{0}; draw != draws; ++draw) alerts->draw(renderer, {static_cast<int16_t>(dx), static_cast<int16_t>(dy)}, static_cast<tfx::simulation::timer_ticks>(elapsed), &coverage);
    std::array<int32_t, 2> const state{static_cast<int32_t>(std::to_underlying(alerts->value().alert)), alerts->value().remaining};
    std::cout.write(reinterpret_cast<char const *>(state.data()), sizeof(state));
    std::cout.write(reinterpret_cast<char const *>(renderer.output.pixels.data()), renderer.output.pixels.size());
    std::cout.write(reinterpret_cast<char const *>(coverage.pixels.data()), coverage.pixels.size());
  }
}
