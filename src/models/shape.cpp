#include "shape.h"
#include "render/continuous_pose.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace tfx::models {
namespace {

enum class colour_load { polygon, inline_lookup };

unsigned int operand_count(uint16_t const opcode, resources::binary_view const &source, size_t const at) {
  /// Original load converter layouts (2.05 file 0xba004), before pointer/vertex fixups
  if(opcode >= 23 && opcode <= 24) return 2;
  if(opcode >= 32 && opcode <= 41) return 2;
  if(opcode >= 58 && opcode <= 61) return 5;
  if(opcode >= 63 && opcode <= 70) return 2;
  switch(opcode) {
  case 0: case 84: case 88: case 89: case 65535: return 0;
  case 7: case 8: case 48: case 53: case 54: case 55: case 72: case 74:
  case 86: case 87: case 100: case 101: case 102: case 116: case 117: case 118: return 1;
  case 45: case 51: case 52: case 76: case 78: case 81: case 94: case 97:
  case 103: case 104: case 105: case 107: case 108: case 109: return 2;
  case 1: case 17: case 77: case 80: case 82: case 92: case 99:
  case 110: case 111: case 112: case 113: return 3;
  case 2: case 21: case 25: case 57: case 79: case 90: case 93: case 98:
  case 106: case 114: return 4;
  case 3: case 26: case 56: case 91: return 5;
  case 6: case 27: case 62: return 6;
  case 30: return 7;
  case 31: return 8;
  case 119: return 9;
  case 120: return 10;
  case 73: return 11;
  case 4: return 2 + source.be16(at + 2);
  case 28: return 3 + source.be16(at + 4);
  case 42: return 4 + source.be16(at + 6);
  case 43: return 5 + source.be16(at + 8);
  case 46: case 71: return 2 + 2 * source.be16(at + 2);
  case 47: case 49: case 50: case 115: return 1 + source.be16(at);
  case 83: return 2 + 3 * source.be16(at);
  case 95: return 1 + 3 * source.be16(at);
  case 96: return 1 + 4 * source.be16(at);
  default: throw std::runtime_error{"unknown shape opcode " + std::to_string(opcode)};
  }
}

bool is_condition(uint16_t const opcode) {
  return (opcode >= 23 && opcode <= 24) || (opcode >= 32 && opcode <= 41) || (opcode >= 63 && opcode <= 70);
}
bool condition_calls(uint16_t const opcode) {
  return opcode == 24 || (opcode >= 35 && opcode <= 37) || opcode == 40 || opcode == 41 || opcode == 64 || (opcode >= 68 && opcode <= 70);
}

} // anonymous namespace

shape::shape(std::span<uint8_t const> const source) {
  /// Retain all polygon detail programs and distant representations; follow header-relative pointers
  resources::binary_view const view{source};
  if(source.size() < 52 || source.size() % 2 || view.be16(18) != 0 || view.be16(34) < 14 || view.be16(34) > 256) throw std::runtime_error{"invalid shape header"};
  sort_depth_bias = view.signed_be16(16);
  minimum_draw_distance = view.signed_be16(0);
  std::set<unsigned int> seen;
  bool terminated{false};
  for(unsigned int slot{1}; slot <= 7; ++slot) {
    auto const distance{view.be16(slot * 2)};
    auto const kind{static_cast<detail_kind>(view.be16((8 + slot) * 2))};
    if(kind == detail_kind::reserved || kind > detail_kind::pixel) throw std::runtime_error{"unknown shape detail representation"};
    unsigned int const field{32 + slot * 2};
    unsigned int const offset{field + view.be16(field)};
    details.push_back({
      .distance{distance},
      .kind{kind},
      .offset{offset},
      .colour{kind >= detail_kind::pixel_block ? static_cast<uint8_t>(view.be16(offset)) : uint8_t{0}},
    });
    if(kind == detail_kind::polygon && seen.insert(offset).second) {
      if(offset < 48 || view.be16(offset) != 65535) throw std::runtime_error{"shape entry lacks its program marker"};
      levels.push_back(offset + 2);
      unsigned int at{offset + 2};
      for(;;) {
        uint16_t const opcode{view.be16(at)};
        auto const count{operand_count(opcode, view, at + 2)};
        view.slice(at + 2, count * 2);
        shape_instruction instruction{
          .address{at},
          .opcode{opcode},
          .operands{},
        };
        for(unsigned int i{0}; i != count; ++i) instruction.operands.push_back(view.be16(at + 2 + i * 2));
        if(opcode == 2 || opcode == 3 || opcode == 4 || opcode == 26 || opcode == 27 || opcode == 56 || opcode == 62 || opcode == 77 || opcode == 79 || opcode == 50 || opcode == 90 || opcode == 91 || opcode == 113 || opcode == 114 || opcode == 115 || opcode == 47) has_surfaces = true;
        instructions.insert_or_assign(at, std::move(instruction));
        at += 2 + count * 2;
        if(opcode == 65535) break;
      }
    }
    if(distance == 32767) {
      terminated = true;
      break;
    }
  }
  if(!terminated || levels.empty()) throw std::runtime_error{"shape has no complete detail table"};
  if(instructions.contains(50) && instructions.at(50).opcode == 46) {
    for(size_t at{56}; at < source.size() && source[at]; ++at) texture_name += static_cast<char>(source[at]);
  }
}

shape_detail const *shape::detail_for_distance(int16_t const distance) const {
  if(distance < minimum_draw_distance) return nullptr;
  auto const found{std::ranges::find_if(details,[&](auto const &detail) { return distance <= math::signed_word(static_cast<uint16_t>(detail.distance)); })};
  return found == details.end() ? nullptr : &*found;
}

bool condition(uint16_t const opcode, uint16_t const operand, uint8_t const state, int16_t const distance, uint16_t const status) {
  /// Predicates captured from native handlers, including signed distance/status and byte-only state operands
  auto const value{static_cast<uint8_t>(operand)};
  auto const signed_operand{math::signed_word(operand)};
  auto const signed_status{math::signed_word(status)};
  switch(opcode) {
  case 23: case 24: return (value & state) != 0;
  case 32: case 35: return value > state;
  case 33: case 36: return value == state;
  case 34: case 37: return value < state;
  case 38: case 40: return signed_operand > distance;
  case 39: case 41: return signed_operand < distance;
  case 63: case 64: return (operand & status) != 0;
  case 65: case 68: return signed_operand > signed_status;
  case 66: case 69: return signed_operand == signed_status;
  case 67: case 70: return signed_operand < signed_status;
  default: throw std::logic_error{"not a shape condition opcode"};
  }
}

shape_geometry interpret(shape const &program, shape_context const &context) {
  /// Execute original branch/call order; projection is supplied by the caller, independently of resource decoding
  unsigned int constexpr vertex_slots{256}, maximum_steps{50000}, maximum_depth{64};
  std::array<std::optional<vec3d>, vertex_slots> local, world;
  std::array<std::optional<vec3<int32_t>>, vertex_slots> camera_vertices;
  auto camera_basis{context.camera ? mat3<int32_t>{context.camera->basis} : mat3<int32_t>{}};
  auto camera_origin{context.camera ? vec3<int32_t>{context.camera->origin} : vec3<int32_t>{}};
  std::array<double, vertex_slots> vertex_scales{0};
  std::array<unsigned int, 3> jitter_shifts{14,14,14};
  unsigned int cursor{0}, address{0};
  // The native loaded ink and its raw re-shading source are separate stores.
  unsigned int colour{112}, source_colour{112};
  uint8_t state{context.initial_state};
  int16_t distance{context.distance}, colour_depth{0};
  bool face_depth_palette{false}, shaded_flat{false}, textured_flat{false};
  std::optional<uint8_t> constant_ink;
  int drawing_mode{-1};
  palette_source palette{palette_source::distance}, colour_palette{palette};
  double camera_scale{1};
  vec3d origin;
  mat3d matrix;
  std::vector<unsigned int> shade_colours;
  std::optional<texture_binding> texture;
  std::vector<vec2d> texture_uvs;
  shape_geometry result;
  std::optional<unsigned int> attachment_slot;
  auto const project{[&context](model_vertex const &v){ return context.project ? context.project(v.position,v.projection_scale) : vec2d{-v.position.x,v.position.y}; }};
  auto const determinant{[&project](std::span<model_vertex const> const vertices){
    auto const a{project(vertices[0])}, b{project(vertices[1])}, c{project(vertices[2])};
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
  }};
  auto const facing{[&](std::span<model_vertex const> const vertices, facing_test const purpose) {
    if(context.face_visibility) if(auto const native{context.face_visibility(vertices,purpose)}) return *native;
    return determinant(vertices) >= 0;
  }};
  auto const wrap{[](double const value) { return std::bit_cast<int32_t>(static_cast<uint32_t>(static_cast<int64_t>(value))); }};
  auto const transform_vertex{[&](vec3d const &value) {
    if(!context.camera) return matrix * value;
    vec3<int32_t> result;
    for(unsigned int row{0}; row != 3; ++row) {
      uint32_t sum{0};
      for(unsigned int axis{0}; axis != 3; ++axis) sum += static_cast<uint32_t>(camera_basis.data[row * 3 + axis]) * static_cast<uint32_t>(math::signed_word(static_cast<uint16_t>(static_cast<int64_t>(value[axis]))));
      result[row] = std::bit_cast<int32_t>(sum);
    }
    return vec3d{result};
  }};
  auto const emit{[&](vec3d value){
    if(cursor >= vertex_slots) throw std::runtime_error{"shape vertex slot exceeds native table"};
    local[cursor++] = std::move(value);
  }};
  auto const project_range{[&](unsigned int const count, unsigned int const start){
    if(start > vertex_slots || count > vertex_slots - start) throw std::runtime_error{"shape projection range exceeds native table"};
    bool near{false};
    for(unsigned int i{start}; i != start + count; ++i) {
      if(!local[i]) continue;
      auto const scale{context.reference_scale / camera_scale};
      if(context.camera) {
        vec3<int32_t> point;
        for(unsigned int axis{0}; axis != 3; ++axis) point[axis] = wrap((*local[i])[axis] + camera_origin[axis]);
        camera_vertices[i].emplace(point);
        int32_t constexpr near_projection_depth{16};
        int32_t const depth{point.z >> 16};
        if(depth > near_projection_depth &&
           (!std::in_range<int16_t>((point.x >> context.camera->projection_shift) / depth) ||
            !std::in_range<int16_t>((point.y >> context.camera->projection_shift) / depth))) {
          // Original B9434's signed-word IDIV faults unwind the entire model
          // through B91AC -> B9383, rather than using a continuous fallback.
          result.projection_aborted = true;
          return;
        }
        near |= point.z >> 16 <= 16;
        world[i] = vec3d{point} * (2 * std::abs(scale) / 65536);
      } else world[i] = *local[i] * scale + origin;
      vertex_scales[i] = scale;
    }
    // B9434 changes the current shaded/texture consumers only when a near
    // vertex occurs. Changing the base consumers with 86/88 is not immediate.
    if(near) { shaded_flat = constant_ink.has_value();textured_flat = constant_ink.has_value(); }
    cursor = start + count;
  }};
  auto const load_colour{[&](unsigned int const ink, std::span<uint16_t const> const indices, colour_load const loader = colour_load::polygon) {
    if(loader == colour_load::polygon) source_colour = ink & 255;
    if(constant_ink) return;
    colour = ink & 255;
    colour_palette = palette;
    if(!face_depth_palette || loader == colour_load::inline_lookup || !context.camera || indices.empty()) return;
    int32_t sum{0};
    for(auto const index : indices) {
      if(index >= vertex_slots || !camera_vertices[index]) throw std::runtime_error{"face palette references an unavailable camera vertex"};
      sum += (*camera_vertices[index]).z >> 16;
    }
    // BBB6C averages signed high words before selecting a colour-table row.
    // Its reused-ink handlers recompute this depth for each new face too.
    colour_depth = static_cast<int16_t>(sum / static_cast<int32_t>(indices.size()));
    colour_palette = palette_source::face_depth;
  }};
  auto const surface{[&](std::span<uint16_t const> const indices, unsigned int const ink, int const mode, bool const textured, bool const shaded, bool const rounded = false){
    bool const use_texture{textured && !textured_flat},use_shading{shaded && !shaded_flat};
    bool const flat{!use_texture && !use_shading};
    primitive p{
      .kind{primitive_kind::polygon},
      .vertices{},
      .colour{flat ? colour : ink & 255},
      .mode{flat ? (mode == -1 ? drawing_mode : mode) : -1},
      .palette{flat ? colour_palette : palette},
      .palette_depth{colour_depth},
      .texture{use_texture ? texture : std::nullopt},
      .rounded{rounded},
      .source_address{address},
      .attachment_slot{attachment_slot},
    };
    for(size_t i{0}; i != indices.size(); ++i) {
      auto const index{indices[i]};
      if(index >= vertex_slots || !world[index]) {
        result.warnings.insert("unavailable vertex at " + std::to_string(address));
        return;
      }
      p.vertices.push_back({
        .position{vec3d{*world[index]}},
        .uv{i < texture_uvs.size() ? vec2d{texture_uvs[i]} : vec2d{}},
        .shade{use_shading && i < shade_colours.size() ? static_cast<double>(shade_colours[i]) : -1},
        .projection_scale{2 * std::abs(vertex_scales[index])},
      });
    }
    bool const rounded_native{context.camera && rounded};
    if(context.facing && !rounded_native && p.vertices.size() >= 3 && !facing(p.vertices,facing_test::polygon)) return;
    result.primitives.push_back(std::move(p));
  }};
  auto const run{[&](auto const &self, shape const &active, unsigned int at, unsigned int const depth)->void {
    if(depth > maximum_depth) throw std::runtime_error{"shape helper nesting exceeds limit"};
    for(; !result.projection_aborted;) {
      if(++result.steps > maximum_steps) throw std::runtime_error{"shape instruction limit exceeded"};
      auto const found{active.instructions.find(at)};
      if(found == active.instructions.end()) throw std::runtime_error{"shape branch misses instruction at " + std::to_string(at)};
      auto const &instruction{found->second};
      auto const opcode{instruction.opcode};
      std::span<uint16_t const> const a{instruction.operands};
      address = at;
      at += 2 + static_cast<unsigned int>(a.size()) * 2;
      auto const target{a.empty() ? 0 : static_cast<unsigned int>(static_cast<int>(at - 2) + math::signed_word(a.back()))};
      if(opcode == 0 || opcode == 65535) return;
      // Native BBFA6/BC8D9 consume their operands without drawing or changing
      // state. These retained instructions are intentional no-ops, not faces.
      if(opcode == 6 || opcode == 80) continue;
      if(opcode == 7) { at = target; continue; }
      if(opcode == 8) { self(self, active, target, depth + 1); continue; }
      if(opcode == 98) {
        cursor = a[0];
        emit(transform_vertex(vec3d{vec3<int16_t>{math::signed_word(a[1]), math::signed_word(a[2]), math::signed_word(a[3])}}));
        continue;
      }
      if(opcode >= 99 && opcode <= 112) {
        if(!cursor || !local[cursor - 1]) throw std::runtime_error{"delta vertex has no predecessor"};
        vec3d delta;
        if(opcode == 99) delta = vec3d{vec3<int16_t>{math::signed_word(a[0]), math::signed_word(a[1]), math::signed_word(a[2])}};
        else {
          unsigned int const mask{opcode == 100 || opcode == 107 ? 1u : opcode == 101 || opcode == 108 ? 2u : opcode == 102 || opcode == 109 ? 4u : opcode == 103 || opcode == 110 ? 3u : opcode == 104 || opcode == 111 ? 5u : opcode == 105 || opcode == 112 ? 6u : 7u};
          unsigned int index{0};
          for(unsigned int axis{0}; axis != 3; ++axis) if(mask & (1u << axis)) delta[axis] = math::signed_word(a[index++]);
        }
        auto value{*local[cursor - 1] + transform_vertex(delta)};
        if(context.camera) for(auto &axis : value) axis = wrap(axis);
        if(opcode >= 106) cursor = a.back();
        emit(std::move(value));
        continue;
      }
      if(opcode == 97 || opcode == 117) {
        project_range(opcode == 97 ? a[0] : 1, opcode == 97 ? a[1] : a[0]);
        continue;
      }
      if(opcode == 82) {
        for(unsigned int axis{0}; axis != 3; ++axis) jitter_shifts[axis] = a[axis] & 255;
        continue;
      }
      if(opcode == 83) {
        cursor = a[1];
        for(size_t i{2}; i < a.size(); i += 3) {
          vec3d value;
          for(unsigned int axis{0}; axis != 3; ++axis) {
            auto coordinate{math::signed_word(a[i + axis])};
            if(context.jitter) coordinate = context.jitter->perturb(coordinate,jitter_shifts[axis]);
            value[axis] = coordinate;
          }
          emit(transform_vertex(value));
        }
        if(!context.jitter) result.warnings.insert("procedural jitter held at zero");
        continue;
      }
      if(opcode == 72) { state = context.parameters.at(a[0]); continue; }
      if(opcode == 84) { palette = palette_source::fixed;continue; }
      if(opcode == 86) {
        constant_ink = context.parameters[a[0] & 255];
        colour = *constant_ink;
        colour_palette = palette_source::direct;
        continue;
      }
      if(opcode == 87) { drawing_mode = context.parameters[a[0] & 255];continue; }
      if(opcode == 88) { constant_ink.reset();face_depth_palette = false;continue; }
      if(opcode == 89) { drawing_mode = -1;continue; }
      if(opcode == 74) {
        if(a[0] >= vertex_slots) throw std::runtime_error{"distance selection references an unavailable camera vertex"};
        if(camera_vertices[a[0]]) distance = static_cast<int16_t>((*camera_vertices[a[0]]).z >> 16);
        else if(world[a[0]] && context.camera_depth) distance = static_cast<int16_t>(std::clamp(context.camera_depth(*world[a[0]]),-32768.0,32767.0));
        else throw std::runtime_error{"distance selection references an unavailable camera vertex"};
        continue;
      }
      if(opcode == 118) {
        bool const previous{face_depth_palette};
        face_depth_palette = true;
        self(self,active,target,depth + 1);
        face_depth_palette = previous;
        continue;
      }
      if(is_condition(opcode)) {
        if(condition(opcode, a[0], state, distance, context.status)) {
          if(condition_calls(opcode)) self(self, active, target, depth + 1);
          else at = target;
        }
        continue;
      }
      if(opcode == 21) {
        std::vector<model_vertex> vertices;
        for(unsigned int i{0}; i != 3; ++i) if(a[i] < vertex_slots && world[a[i]]) vertices.push_back({
          .position{vec3d{*world[a[i]]}},
          .uv{0,0},
          .projection_scale{2 * std::abs(vertex_scales[a[i]])},
        });
        if(context.facing && vertices.size() == 3 && facing(vertices,facing_test::branch)) at = target;
        continue;
      }
      if(opcode == 45 || opcode == 73) {
        auto const index{opcode == 45 ? a[0] : a[9]};
        if(index >= vertex_slots || !world[index]) { result.warnings.insert("unavailable helper anchor"); continue; }
        auto const old_origin{vec3d{origin}};
        auto const old_matrix{mat3d{matrix}};
        auto const old_basis{mat3<int32_t>{camera_basis}};
        auto const old_camera_origin{vec3<int32_t>{camera_origin}};
        auto const old_scale{camera_scale};
        origin = *world[index];
        if(context.camera) {
          camera_origin = *camera_vertices[index];
          if(opcode == 73) camera_basis = math::compose_model_basis(camera_basis,math::rotation_words({a[0],a[1],a[2]}));
        } else if(opcode == 73) matrix = matrix * render::continuous_rotation(vec3<math::angle>{a[0],a[1],a[2]});
        self(self, active, target, depth + 1);
        origin = old_origin;
        matrix = old_matrix;
        camera_basis = old_basis;
        camera_origin = old_camera_origin;
        camera_scale = old_scale;
        continue;
      }
      if(opcode == 81) {
        // The second disk operand is a byte offset into the native table of 16-bit model handles.
        if(a[1] % sizeof(uint16_t)) throw std::runtime_error{"unaligned carried-model slot"};
        unsigned int const slot{static_cast<unsigned int>(a[1] / sizeof(uint16_t))};
        if(slot >= context.attachments.size() || !context.attachments[slot]) continue;
        if(a[0] >= vertex_slots || !world[a[0]]) { result.warnings.insert("unavailable carried-model anchor"); continue; }
        auto const old_origin{vec3d{origin}};
        auto const old_camera_origin{vec3<int32_t>{camera_origin}};
        auto const old_scale{camera_scale};
        auto const old_state{state};
        auto const old_slot{attachment_slot};
        origin = *world[a[0]];
        if(context.camera) camera_origin = *camera_vertices[a[0]];
        state = 0xf9; // Native 0xbc94c seeds carried stores independently of the parent condition byte.
        if(!attachment_slot) attachment_slot = slot;
        auto const &child{*context.attachments[slot]};
        // Vertex storage and drawing state remain shared, as in the original. Only translation and condition are restored.
        self(self, child, child.levels.at(0), depth + 1);
        origin = old_origin;
        camera_origin = old_camera_origin;
        camera_scale = old_scale;
        state = old_state;
        attachment_slot = old_slot;
        continue;
      }
      if(opcode == 48 || opcode == 54 || opcode == 55) {
        auto const factor{static_cast<double>(math::signed_word(a[0]))};
        if(!factor) throw std::runtime_error{"zero shape camera scale"};
        if(context.camera) for(unsigned int axis{0}; axis != 3; ++axis) {
          if(opcode == 54) camera_origin[axis] = wrap(camera_origin[axis] * factor);
          else if(opcode == 48) camera_origin[axis] = wrap(context.camera->origin[axis] * factor);
          else camera_origin[axis] = static_cast<int32_t>(static_cast<int64_t>(context.camera->origin[axis]) / static_cast<int64_t>(factor));
        }
        if(opcode == 54) camera_scale *= factor;
        else { camera_scale = opcode == 48 ? factor : 1 / factor; origin = vec3d{}; }
        continue;
      }
      if(opcode == 1) {
        unsigned int const line_colour{constant_ink ? *constant_ink : a[0] & 255u};
        if(a[1] < vertex_slots && a[2] < vertex_slots && world[a[1]] && world[a[2]]) {
          result.primitives.push_back({
            .kind{primitive_kind::line},
            .vertices{
              {
                .position{vec3d{*world[a[1]]}},
                .uv{0,0},
                .projection_scale{2 * std::abs(vertex_scales[a[1]])},
              },
              {
                .position{vec3d{*world[a[2]]}},
                .uv{0,0},
                .projection_scale{2 * std::abs(vertex_scales[a[2]])},
              },
            },
            .colour{line_colour},
            .palette{constant_ink ? palette_source::direct : palette},
            .source_address{address},
            .attachment_slot{attachment_slot},
          });
        }
        continue;
      }
      if(opcode == 76 || opcode == 78) {
        shade_colours.clear();
        for(auto const word : a) { shade_colours.push_back(word >> 8); shade_colours.push_back(word & 255); }
        shade_colours.resize(opcode == 76 ? 3 : 4);
        continue;
      }
      if(opcode == 46 || opcode == 71) {
        texture = texture_binding{
          .source{opcode == 71 ? texture_source::global : texture_source::model},
          .slot{a[0]},
          .name{active.texture_name},
        };
        texture_uvs.clear();
        for(size_t i{2}; i < a.size(); i += 2) texture_uvs.emplace_back(a[i], a[i + 1]);
        continue;
      }
      if(opcode == 51 || opcode == 52 || opcode == 92 || opcode == 93 || opcode == 95 || opcode == 96) {
        unsigned int const count{opcode == 95 || opcode == 96 ? a[0] : 1u};
        size_t const stride{opcode == 96 ? 4 : opcode == 95 ? 3 : a.size()};
        for(unsigned int i{0}; i != count; ++i) {
          auto const record{opcode == 95 || opcode == 96 ? a.subspan(1 + i * stride, stride) : a};
          unsigned int const offset{opcode == 93 || opcode == 96 ? 1u : 0u};
          auto const index{record[offset + 1]};
          if(index >= vertex_slots || !world[index]) { result.warnings.insert("unavailable point vertex"); continue; }
          bool const loads_ink{opcode != 51 && opcode != 52};
          if(loads_ink) load_colour(record[offset],record.subspan(offset + 1,1));
          result.primitives.push_back({
            .kind{primitive_kind::point},
            .vertices{{
              .position{vec3d{*world[index]}},
              .uv{0,0},
              .projection_scale{2 * std::abs(vertex_scales[index])},
            }},
            .colour{loads_ink ? colour : static_cast<unsigned int>(record[offset] & 255)},
            .mode{offset ? record[0] : loads_ink ? drawing_mode : -1},
            .palette{loads_ink ? colour_palette : palette},
            .palette_depth{colour_depth},
            .radius{record.size() > offset + 2 ? record[offset + 2] * std::abs(vertex_scales[index]) : 0},
            .pixel_size{opcode == 51 ? 1u : opcode == 52 ? 2u : 0u},
            .source_address{address},
            .attachment_slot{attachment_slot},
          });
        }
        continue;
      }
      if(opcode == 2 || opcode == 3 || opcode == 56) {
        load_colour(a[0],a.subspan(1),opcode == 56 ? colour_load::inline_lookup : colour_load::polygon);
        surface(a.subspan(1),colour,-1,false,false,opcode == 56);
        continue;
      }
      if(opcode == 4) {
        if(face_depth_palette && context.camera) throw std::runtime_error{"depth-shaded variable polygon requires native count-word interpretation"};
        load_colour(a[0],a.subspan(2)); surface(a.subspan(2), colour, -1, false, false); continue;
      }
      if(opcode == 26 || opcode == 27 || opcode == 62 || opcode == 90 || opcode == 91) {
        unsigned int const offset{opcode == 90 ? 0u : 1u};
        load_colour(a[offset],a.subspan(offset + 1),opcode == 26 || opcode == 27 ? colour_load::polygon : colour_load::inline_lookup);
        surface(a.subspan(offset + 1), colour, opcode == 26 || opcode == 27 || opcode == 62 || opcode == 91 ? a[0] : -1, false, false, opcode == 62 || opcode == 90 || opcode == 91);
        continue;
      }
      if(opcode == 77 || opcode == 79 || opcode == 50) {
        surface(opcode == 50 ? a.subspan(1) : a, shade_colours.empty() ? colour : shade_colours[0], -1, false, true);
        continue;
      }
      if(opcode == 113 || opcode == 114 || opcode == 115 || opcode == 47) {
        if(face_depth_palette && opcode >= 113) load_colour(source_colour,opcode == 115 ? a.subspan(1) : a);
        surface(opcode == 115 || opcode == 47 ? a.subspan(1) : a, opcode == 47 ? 112 : colour, -1, opcode == 47, false);
        continue;
      }
      result.warnings.insert("shape opcode " + std::to_string(opcode) + " requires runtime context");
    }
  }};
  run(run, program, context.entry_address.value_or(program.levels.at(context.level)), 0);
  return result;
}

} // namespace tfx::models
