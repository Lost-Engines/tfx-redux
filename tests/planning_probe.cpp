#include <algorithm>
#include <cstdlib>
#include <iostream>
#include "planning/document.h"
#include "planning/map_view.h"
#include "planning/notes.h"
#include "planning/review.h"
#include "resources/archive.h"
#include "resources/compiled_panel.h"
#include "resources/executable_tables.h"

int main(int const argc, char const *const argv[]) try {
  if(argc == 3 && std::string_view{argv[2]} == "--panel") {
    auto const panel{tfx::resources::decode_planner_panel(tfx::resources::read_file(std::filesystem::path{argv[1]} / "TFX.DAT"))};
    for(auto const *frame : {&panel.image,&panel.coverage}) std::cout.write(reinterpret_cast<char const*>(frame->pixels.data()),frame->pixels.size());
    return EXIT_SUCCESS;
  }
  if(argc == 3 && std::string_view{argv[2]} == "--palette") {
    auto const executable{tfx::resources::read_file(std::filesystem::path{argv[1]} / "TFX.DAT")};
    tfx::resources::planner_artwork const artwork{executable};
    std::string hex;
    while(std::cin >> hex) {
      tfx::resources::bytes input;
      for(unsigned int at{0}; at < hex.size(); at += 2) input.push_back(static_cast<uint8_t>(std::stoul(hex.substr(at,2),nullptr,16)));
      auto const palette{artwork.colours(input)};
      std::cout.write(reinterpret_cast<char const*>(palette.data()),palette.size());
    }
    return EXIT_SUCCESS;
  }
  if(argc == 3 && std::string_view{argv[2]} == "--review-intro") {
    auto const path{tfx::resources::find_game_archive(argv[1])};
    tfx::resources::archive const game{path};
    tfx::resources::executable_tables const tables{path.parent_path() / "TFX.DAT"};
    std::string language, name;
    while(std::cin >> language >> name) {
      tfx::planning::review review{game,tables.planner_art(),tables.interface_font(),name,language};
      auto const initial{review.image()};
      for(uint8_t const scan : std::array<uint8_t,3>{0x1c,1,0x15}) {
        review.press(scan);review.advance(0);
        if(review.image().pixels != initial.pixels || review.status().phase != tfx::planning::review_phase::introduction) throw std::runtime_error{"unaccepted key dismissed a briefing note"};
      }
      std::cout.write(reinterpret_cast<char const*>(initial.pixels.data()),initial.pixels.size());
      review.press(0x39);review.advance(0);
      if(review.status().phase != tfx::planning::review_phase::waypoint || review.status().points != 1) throw std::runtime_error{"Space did not advance to the first waypoint"};
    }
    return EXIT_SUCCESS;
  }
  if(argc == 3 && std::string_view{argv[2]} == "--review-fade") {
    auto const path{tfx::resources::find_game_archive(argv[1])};
    tfx::resources::archive const game{path};
    tfx::resources::executable_tables const tables{path.parent_path() / "TFX.DAT"};
    std::string name;
    while(std::cin >> name) {
      tfx::planning::review review{game,tables.planner_art(),tables.interface_font(),name};
      for(unsigned int frame{0}; !review.finished() && frame != 40000; ++frame) {
        if(review.status().phase == tfx::planning::review_phase::leaving) {
          for(auto const &colour : review.colours()) for(unsigned int channel{0}; channel != 3; ++channel) std::cout.put(static_cast<char>(colour[channel]));
        }
        review.advance(2);
      }
      if(!review.finished() || std::ranges::any_of(review.image().pixels,[](uint8_t const value){ return value != 0; })) throw std::runtime_error{"review fade did not finish on a cleared screen"};
      for(auto const &colour : review.colours()) for(unsigned int channel{0}; channel != 3; ++channel) std::cout.put(static_cast<char>(colour[channel]));
    }
    return EXIT_SUCCESS;
  }
  if(argc == 3 && std::string_view{argv[2]} == "--review") {
    auto const path{tfx::resources::find_game_archive(argv[1])};
    tfx::resources::archive const game{path};
    tfx::resources::executable_tables const tables{path.parent_path() / "TFX.DAT"};
    std::string name;
    while(std::cin >> name) {
      tfx::planning::review review{game,tables.planner_art(),tables.interface_font(),name};
      if(!review.finished()) std::cout << "N0 ";
      for(unsigned int frame{0}; !review.finished() && frame != 40000; ++frame) {
        auto const before{review.status()};
        if(before.phase == tfx::planning::review_phase::segment) std::cout << 'S' << before.extent + 2 << ' ';
        review.advance(2);
        auto const after{review.status()};
        if(after.phase == tfx::planning::review_phase::waypoint && before.phase != after.phase) std::cout << 'N' << after.points << ' ';
      }
      if(!review.finished()) throw std::runtime_error{"route review did not complete"};
      std::cout << "done\n";
    }
    return EXIT_SUCCESS;
  }
  if(argc == 3 && std::string_view{argv[2]} == "--segments") {
    auto const path{tfx::resources::find_game_archive(argv[1])};
    tfx::resources::archive const game{path};
    tfx::resources::executable_tables const tables{path.parent_path() / "TFX.DAT"};
    uint32_t id{0};unsigned int count{0}, extent{0};
    while(std::cin >> id >> count >> extent) {
      auto const bytes{game.read(id)};
      tfx::planning::document const plan{std::string_view{reinterpret_cast<char const*>(bytes.data()),bytes.size()}};
      tfx::planning::map_view const view{game,tables.planner_art(),tables.interface_font(),plan};
      auto image{view.image()};
      view.route(image,count);
      uint32_t const remaining{view.extend(image,count,extent)};
      std::cout.write(reinterpret_cast<char const*>(&remaining),sizeof(remaining));
      std::cout.write(reinterpret_cast<char const*>(image.pixels.data()),image.pixels.size());
    }
    return EXIT_SUCCESS;
  }
  if(argc == 3 && std::string_view{argv[2]} == "--routes") {
    auto const path{tfx::resources::find_game_archive(argv[1])};
    tfx::resources::archive const game{path};
    tfx::resources::executable_tables const tables{path.parent_path() / "TFX.DAT"};
    uint32_t id{0};
    while(std::cin >> id) {
      auto const bytes{game.read(id)};
      tfx::planning::document const plan{std::string_view{reinterpret_cast<char const*>(bytes.data()),bytes.size()}};
      tfx::planning::map_view const view{game,tables.planner_art(),tables.interface_font(),plan};
      if(plan.routes.empty()) continue;
      for(unsigned int count{1}; count <= plan.routes.front().points.size(); ++count) {
        auto image{view.image()};
        view.route(image,count);
        std::cout.write(reinterpret_cast<char const*>(image.pixels.data()),image.pixels.size());
      }
    }
    return EXIT_SUCCESS;
  }
  if(argc == 3 && std::string_view{argv[2]} == "--notes") {
    auto const path{tfx::resources::find_game_archive(argv[1])};
    tfx::resources::archive const game{path};
    tfx::resources::executable_tables const tables{path.parent_path() / "TFX.DAT"};
    uint32_t id{0};
    while(std::cin >> id) {
      auto const bytes{game.read(id)};
      tfx::planning::document const plan{std::string_view{reinterpret_cast<char const*>(bytes.data()),bytes.size()}};
      tfx::planning::map_view const view{game,tables.planner_art(),tables.interface_font(),plan};
      tfx::planning::notes const notes{plan.notes};
      uint32_t const count{static_cast<uint32_t>(notes.all().size())};
      std::cout.write(reinterpret_cast<char const*>(&count),sizeof(count));
      for(auto const &[key,note] : notes.all()) {
        auto image{view.image()};
        tfx::planning::draw_annotation(image,tables.interface_font(),note);
        std::cout.write(reinterpret_cast<char const*>(image.pixels.data()),image.pixels.size());
      }
    }
    return EXIT_SUCCESS;
  }
  if(argc == 3 && std::string_view{argv[2]} == "--maps") {
    auto const path{tfx::resources::find_game_archive(argv[1])};
    tfx::resources::archive const game{path};
    tfx::resources::executable_tables const tables{path.parent_path() / "TFX.DAT"};
    uint32_t id{0};
    while(std::cin >> id) {
      auto const bytes{game.read(id)};
      tfx::planning::document const plan{std::string_view{reinterpret_cast<char const*>(bytes.data()),bytes.size()}};
      tfx::planning::map_view view{game,tables.planner_art(),tables.interface_font(),plan};
      for(unsigned int layer{0}; layer != 3; ++layer) {
        view.select(static_cast<tfx::planning::map_layer>(layer));
        auto const &pixels{view.image().pixels};
        auto const &palette{view.palette()};
        std::cout.write(reinterpret_cast<char const*>(pixels.data()),pixels.size());
        std::cout.write(reinterpret_cast<char const*>(palette.data()),palette.size());
      }
    }
    return EXIT_SUCCESS;
  }
  if(argc != 2) throw std::runtime_error{"planning_probe DATA_DIRECTORY"};
  tfx::resources::archive const game{tfx::resources::find_game_archive(argv[1])};
  auto const string{[](std::string_view const text){
    char constexpr digits[]{"0123456789abcdef"};
    for(uint8_t const byte : text) std::cout << digits[byte >> 4] << digits[byte & 15];
    std::cout << '|';
  }};
  uint32_t id;
  while(std::cin >> id) {
    auto const bytes{game.read(id)};
    try {
      tfx::planning::document const plan{std::string_view{reinterpret_cast<char const*>(bytes.data()),bytes.size()}};
      std::cout << plan.format << ' ';
      for(auto const word : plan.parameters.words()) std::cout << word << ' ';
      std::cout << '|';
      string(plan.map_images[0]);string(plan.world);string(plan.map_images[1]);string(plan.palette);string(plan.map_images[2]);
      std::cout << plan.routes.size() << '|';
      for(auto const &route : plan.routes) {
        string(route.name);
        for(auto const &point : route.points) std::cout << point.ordinal << ',' << point.cell.x << ',' << point.cell.y << ';';
        std::cout << '|';
      }
      string(plan.notes);
      std::cout << '\n';
    } catch(std::runtime_error const&) { std::cout << "invalid\n"; }
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';
  return EXIT_FAILURE;
}
