#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif // _WIN32
#include "math/native.h"
#include "models/shape.h"
#include "render/software_renderer.h"
#include "resources/archive.h"
#include "resources/executable_tables.h"
#include "world/animation.h"
#include "world/collision.h"
#include "world/definition.h"
#include "world/environment.h"

namespace {

void word(uint32_t const value) {
  for(unsigned int shift{0}; shift != 32; shift += 8) std::cout.put(static_cast<char>(value >> shift));
}

} // anonymous namespace

auto main(int const argc, char const *const argv[])->int try {
  if(argc < 2) throw std::runtime_error{"usage: tfx_probe archive|shape DATA_DIRECTORY, or sine|rotation|conditions|random"};
  std::string_view const mode{argv[1]};
  std::cout << std::setprecision(17);
  if(mode == "collision") {
    int diagonal{0};
    while(std::cin >> diagonal) {
      tfx::world::collision_box box;
      tfx::world::collision_segment segment;
      for(auto *point : {&box.minimum,&box.maximum,&segment.previous,&segment.current}) {
        int x{0}, y{0}, z{0};
        std::cin >> x >> y >> z;
        *point = vec3<int16_t>{static_cast<int16_t>(x),static_cast<int16_t>(y),static_cast<int16_t>(z)};
      }
      std::cout << tfx::world::intersects(box,segment,diagonal != 0) << '\n';
    }
  } else if(mode == "sine") {
    for(unsigned angle=0;angle<tfx::math::angle_units_per_turn;++angle) std::cout << tfx::math::sine_q15(static_cast<int32_t>(angle)) << ' ';
    std::cout << std::endl;
  } else if(mode == "model_vertex") {
    unsigned int pitch{0}, yaw{0}, roll{0};int x{0}, y{0}, z{0};
    while(std::cin >> pitch >> yaw >> roll >> x >> y >> z) {
      auto const camera{tfx::render::orbit_camera(vec3<tfx::math::angle>{static_cast<uint16_t>(pitch),static_cast<uint16_t>(yaw),static_cast<uint16_t>(roll)},0,{105,75})};
      auto const point{camera.transform(vec3d{vec3<int>{x,y,z}} * .5)};
      for(unsigned int axis{0}; axis != 3; ++axis) std::cout << static_cast<int32_t>(point[axis] * 65536) << ' ';
      std::cout << '\n';
    }
  } else if(mode == "model_projection" || mode == "model_projection_scaled") {
    int32_t x{0}, y{0}, z{0};
    tfx::render::camera const camera{
      .position{0,0,0},
      .right{1,0,0},
      .down{0,1,0},
      .forward{0,0,1},
      .projection_origin{105,75},
      .focal_distance{256},
      .projection{tfx::render::projection_mode::original_model},
    };
    while(std::cin >> x >> y >> z) {
      double scale{1};
      if(mode == "model_projection_scaled") std::cin >> scale;
      auto const point{camera.project(vec3d{vec3<int32_t>{x,y,z}} / 65536 * scale,scale)};
      std::cout << static_cast<int>(point.x) << ' ' << static_cast<int>(point.y) << '\n';
    }
  } else if(mode == "orbit") {
    unsigned int pitch{0}, yaw{0}, roll{0};int32_t distance{0};
    while(std::cin >> pitch >> yaw >> roll >> distance) {
      auto const camera{tfx::render::orbit_camera(vec3<tfx::math::angle>{static_cast<uint16_t>(pitch),static_cast<uint16_t>(yaw),static_cast<uint16_t>(roll)},distance,{105,75})};
      for(auto const *axis : {&camera.right,&camera.down,&camera.forward}) for(unsigned int i{0}; i != 3; ++i) std::cout << static_cast<int>(std::lround((*axis)[i] * tfx::math::matrix_unit)) << ' ';
      for(unsigned int i{0}; i != 3; ++i) std::cout << static_cast<int32_t>(camera.position[i] * 65536) << ' ';
      std::cout << '\n';
    }
  } else if(mode == "rotation") {
    unsigned int x{0}, y{0}, z{0};
    while(std::cin >> x >> y >> z) {
      auto const matrix{tfx::math::rotation_words(vec3<tfx::math::angle>{static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint16_t>(z)})};
      for(auto const value : matrix.data) std::cout << value << ' ';
      std::cout << '\n';
    }
  } else if(mode == "rounded") {
    int32_t ax{0}, ay{0}, bx{0}, by{0}, cx{0}, cy{0};
    while(std::cin >> ax >> ay >> bx >> by >> cx >> cy) {
      for(auto const &v : tfx::render::rounded_corner(vec2<int32_t>{ax,ay},vec2<int32_t>{bx,by},vec2<int32_t>{cx,cy})) std::cout << v.x << ' ' << v.y << ' ';
      std::cout << '\n';
    }
  } else if(mode == "clock") {
    unsigned int count{0};
    while(std::cin >> count) {
      tfx::world::animation_clock clock;
      for(unsigned int i{0}; i != count; ++i) {
        unsigned int elapsed{0};
        std::cin >> elapsed;
        bool const enabled{clock.advance(static_cast<uint16_t>(elapsed))};
        std::cout << enabled << ' ' << clock.remainder() << ' ';
      }
      std::cout << '\n';
    }
  } else if(mode == "stream") {
    unsigned int count{0}, ticks{0};
    while(std::cin >> count >> ticks) {
      std::vector<uint16_t> words;
      for(unsigned int i{0}; i != count; ++i) {
        unsigned int word{0};
        std::cin >> word;
        words.push_back(static_cast<uint16_t>(word));
      }
      tfx::world::parameter_player player{words};
      uint8_t value{0};
      for(unsigned int i{0}; i != ticks; ++i) {
        auto const step{player.tick(value)};
        if(step.action == tfx::world::stream_action::event_requested) throw std::runtime_error{"native stream probe reached an event requiring an actor dispatcher"};
        std::cout << static_cast<unsigned int>(value) << ' ';
      }
      std::cout << '\n';
    }
  } else if(mode == "jitter") {
    unsigned int x{0}, y{0};
    while(std::cin >> x >> y) {
      std::array<unsigned int,3> shifts;
      for(auto &shift : shifts) std::cin >> shift;
      tfx::math::jitter_state state{
        .x{static_cast<uint16_t>(x)},
        .y{static_cast<uint16_t>(y)},
      };
      for(unsigned int vertex{0}; vertex != 3; ++vertex) {
        for(unsigned int axis{0}; axis != 3; ++axis) {
          int coordinate{0};
          std::cin >> coordinate;
          std::cout << state.perturb(static_cast<int16_t>(coordinate),shifts[axis]) << ' ';
        }
      }
      std::cout << state.x << ' ' << state.y << '\n';
    }
  } else if(mode == "conditions") {
    unsigned int opcode{0}, operand{0}, state{0};
    while(std::cin >> opcode >> operand >> state) std::cout << tfx::models::condition(static_cast<uint16_t>(opcode), static_cast<uint16_t>(operand), static_cast<uint8_t>(state), tfx::math::signed_word(static_cast<uint16_t>(state)), static_cast<uint16_t>(state)) << '\n';
  } else if(mode == "random") {
    tfx::math::random_state state;
    unsigned int count{0};
    while(std::cin >> count) {
      if(!count) throw std::runtime_error{"zero variant count in probe"};
      auto const value{state.next()};
      std::cout << value % count << ' ' << state.low << ' ' << state.high << '\n';
    }
  } else {
    if(argc != 3) throw std::runtime_error{"resource probe needs a data directory"};
    tfx::resources::archive const game{tfx::resources::find_game_archive(argv[2])};
    if(mode == "archive") {
#ifdef _WIN32
      _setmode(_fileno(stdout), _O_BINARY);
#endif // _WIN32
      word(static_cast<uint32_t>(game.entries().size()));
      for(auto const &entry : game.entries()) {
        auto const decoded{game.read(entry.id)};
        word(entry.id);
        word(static_cast<uint32_t>(decoded.size()));
        std::cout.write(reinterpret_cast<char const*>(decoded.data()), static_cast<std::streamsize>(decoded.size()));
      }
    } else if(mode == "raster") {
#ifdef _WIN32
      _setmode(_fileno(stdout), _O_BINARY);
#endif // _WIN32
      tfx::resources::executable_tables const tables{tfx::resources::find_game_archive(argv[2]).parent_path() / "TFX.DAT"};
      int drawing_mode{0};
      unsigned int seed{0};
      std::string kind;
      while(std::cin >> drawing_mode >> kind >> seed) {
        tfx::render::software_renderer renderer{tables,static_cast<uint16_t>(seed)};
        renderer.output.pixels.fill(77);
        if(kind == "gradient") {
          std::array<tfx::render::screen_vertex,4> vertices{{
            {
              .position{20,20},
              .uv{0,0},
              .shade{-1},
            },
            {
              .position{100,20},
              .uv{0,0},
              .shade{-1},
            },
            {
              .position{100,100},
              .uv{0,0},
              .shade{-1},
            },
            {
              .position{20,100},
              .uv{0,0},
              .shade{-1},
            },
          }};
          for(auto &v : vertices) std::cin >> v.shade;
          renderer.polygon(vertices,200);
        } else if(kind == "circle") renderer.circle(80,80,40,200,drawing_mode);
        else {
          double const left{kind == "quad" ? 32.0 : 35.0}, top{kind == "quad" ? 32.0 : 37.0};
          std::array<tfx::render::screen_vertex, 4> const vertices{{
            {
              .position{left,top},
              .uv{0,0},
              .shade{-1},
            },
            {
              .position{left + 96,top},
              .uv{0,0},
              .shade{-1},
            },
            {
              .position{left + 96,top + 96},
              .uv{0,0},
              .shade{-1},
            },
            {
              .position{left,top + 96},
              .uv{0,0},
              .shade{-1},
            },
          }};
          renderer.polygon(vertices,200,drawing_mode);
        }
        word(renderer.procedural_state());
        std::cout.write(reinterpret_cast<char const*>(renderer.output.pixels.data()),renderer.output.pixels.size());
      }
    } else if(mode == "missions") {
      std::string name;
      while(std::cin >> name) {
        tfx::world::mission const mission{game,name};
        auto const &p{mission.start.position};
        std::cout << '[' << std::quoted(mission.world_name) << ',' << std::quoted(mission.grid_name()) << ',' << std::quoted(mission.environment) << ',' << std::quoted(mission.waypoints) << ',' << (mission.aircraft ? std::to_string(std::to_underlying(*mission.aircraft)) : "null") << ",[" << p.x << ',' << p.y << ',' << p.z << "]," << mission.start.landed << "]\n";
      }
    } else if(mode == "definitions") {
      auto const names{tfx::world::model_names(game.read("ss/ssinfo.fn"))};
      uint32_t id{0};
      while(std::cin >> id) {
        tfx::world::definition const definition{game.read(id), names};
        std::cout << '[' << definition.flags << ",[";
        bool first{true};
        for(unsigned int slot{0}; slot != definition.parts.size(); ++slot) {
          auto const &part{definition.parts[slot]};
          if(part.name.empty()) continue;
          if(!std::exchange(first, false)) std::cout << ',';
          std::cout << '[' << slot << ',' << std::quoted(part.name) << ',' << part.rotation.x << ',' << part.rotation.y << ',' << part.rotation.z << ']';
        }
        std::cout << "],[";
        first = true;
        for(auto const &draw : definition.draws) {
          if(!std::exchange(first, false)) std::cout << ',';
          std::cout << '[' << draw.slot << ',' << draw.anchor.x << ',' << draw.anchor.y << ',' << draw.anchor.z << ']';
        }
        std::cout << "],[";
        first = true;
        for(auto const &stream : definition.streams) {
          if(!std::exchange(first, false)) std::cout << ',';
          std::cout << '[' << stream.part << ',' << stream.parameter << ",[";
          bool first_word{true};
          for(auto const value : stream.words) {
            if(!std::exchange(first_word, false)) std::cout << ',';
            std::cout << value;
          }
          std::cout << "]]";
        }
        std::cout << "]]\n";
      }
    } else if(mode == "environment" || mode == "environment_bindings" || mode == "environment_variants") {
      std::string specification;
      while(std::getline(std::cin,specification)) {
        std::istringstream fields{specification};
        std::string name, lighting;
        fields >> name >> lighting;
        if(name.empty()) continue;
        tfx::world::environment const environment{game,name,lighting};
        if(mode == "environment_variants") {
          uint64_t hash{14695981039346656037ull};
          auto const append{[&](uint8_t const byte) { hash = (hash ^ byte) * 1099511628211ull; }};
          for(unsigned int z{0}; z != tfx::world::map_height; ++z) for(unsigned int x{0}; x != tfx::world::map_width; ++x) {
            for(unsigned char const byte : environment.variant(x,z)) append(byte);
            append(0);
            auto const rotation{environment.tile(x,z).rotation};
            append(static_cast<uint8_t>(rotation));append(static_cast<uint8_t>(rotation >> 8));
          }
          auto const random{environment.variant_random_state()};
          std::cout << '[' << hash << ',' << random.low << ',' << random.high << "]\n";
          continue;
        }
        if(mode == "environment_bindings") {
          std::cout << '[' << environment.lighting.status << ',' << std::quoted(environment.lighting.palette_name) << ',' << std::quoted(environment.lighting.schematic_name) << ',' << environment.lighting.water_world << ',' << environment.lighting.cloud_layer << ',' << environment.lighting.layer_y << ',' << environment.lighting.layer_offset_y << ',' << std::quoted(environment.lighting.upper_grid) << ',' << environment.lighting.sun_suppressed << "," << environment.lighting.initial_cockpit_raised << "]\n";
          continue;
        }
        std::cout << '[' << environment.lighting.status << ',' << std::quoted(environment.lighting.palette_name) << ",[";
        bool first{true};
        for(unsigned int z{0}; z != tfx::world::map_height; ++z) {
          for(unsigned int x{0}; x != tfx::world::map_width; ++x) {
            if(!std::exchange(first, false)) std::cout << ',';
            std::cout << '[' << environment.tile_id(x,z) << ',' << std::quoted(environment.variant(x,z)) << ']';
          }
        }
        std::cout << "]]\n";
      }
    } else if(mode == "shape" || mode == "attachments") {
      uint32_t id{0};
      unsigned int status{0}, state{0}, level{0};
      while(std::cin >> id >> status >> state >> level) {
        tfx::models::shape const program{game.read(id)};
        tfx::models::shape_context context;
        context.status = static_cast<uint16_t>(status);
        context.parameters[7] = static_cast<uint8_t>(state);
        context.level = level;
        context.facing = false;
        std::optional<tfx::models::shape> child;
        std::array<tfx::models::shape const*,10> attachments{};
        if(mode == "attachments") {
          uint32_t child_id{0};
          unsigned int slot{0};
          std::cin >> child_id >> slot;
          if(child_id) child.emplace(game.read(child_id));
          if(slot == attachments.size()) attachments.fill(child ? &*child : nullptr);
          else attachments.at(slot) = child ? &*child : nullptr;
          context.attachments = attachments;
        }
        auto const geometry{tfx::models::interpret(program, context)};
        std::cout << '[' << geometry.steps << ",[";
        bool first{true};
        for(auto const &p : geometry.primitives) {
          if(!std::exchange(first, false)) std::cout << ',';
          std::cout << '[' << std::to_underlying(p.kind) << ',' << p.colour << ',' << p.mode << ',' << p.pixel_size << ',' << p.radius << ',' << p.source_address << ",[";
          bool first_vertex{true};
          for(auto const &v : p.vertices) {
            if(!std::exchange(first_vertex, false)) std::cout << ',';
            std::cout << '[' << v.position.x << ',' << v.position.y << ',' << v.position.z << ',' << v.shade << ',' << v.uv.x << ',' << v.uv.y << ']';
          }
          std::cout << "]]";
        }
        std::cout << "]]\n";
      }
    } else throw std::runtime_error{"unknown probe mode"};
  }
  if(!std::cout) throw std::runtime_error{"probe output failed"};
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
