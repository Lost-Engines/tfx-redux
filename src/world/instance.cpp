#include "instance.h"
#include <stdexcept>
#include <utility>
#include "resources/binary.h"

namespace tfx::world {
namespace {

unsigned int constexpr instruction_budget{16384}, maximum_call_depth{64};

enum class instruction : uint16_t {
  finish, jump, call, bind_model, draw_rotated, bind_rotated_model, draw_rotated_anchor, draw_origin, draw_anchor,
  animate_primary, set_custom_run, set_custom, test_box, collide_box, damage_box_branch, set_category, destroy, set_impact_damage, set_health, restore,
  animate_0, animate_1, animate_2, animate_3, animate_4, animate_5, animate_6, animate_7,
  zone_box, player_zone_box,
  damage_box_call = 31,
  eject_player = 33,
  destroy_box = 35,
  if_custom_equal = 37,
  if_primary_equal, if_primary_equal_alt, if_primary_equal_2, if_primary_equal_3, if_primary_equal_4, if_primary_equal_5, if_primary_equal_6, if_primary_equal_7,
  draw_special_anchor, test_diagonal_box, collide_diagonal_box, damage_diagonal_branch, damage_diagonal_call, destroy_diagonal_box, zone_diagonal_box, player_zone_diagonal_box, draw_special_origin,
  emitter_begin, emitter_end, emitter_draw, if_custom_unequal, message, draw_ground_shadow, if_status,
  set_custom_one, trail_update, damage_box_once, damage_box_stages, if_distance_near, if_distance_far, call_if_distance_near, call_if_distance_far,
  measure_anchor, update_offset, set_offset, surface_height, draw_damage_overlay,
  rotate_part = 78, emit_effect, damage_percentage, surface_box, player_box_call, decrement_custom,
};

} // anonymous namespace

vec3<int16_t> place_instance_point(vec3<int16_t> const &local, vec3<int16_t> const &origin, math::angle const heading) {
  vec3<int32_t> rotated{local};
  switch(heading >> 14) {
  case 1: rotated = {local.z,local.y,-static_cast<int32_t>(local.x)};break;
  case 2: rotated = {-static_cast<int32_t>(local.x),local.y,-static_cast<int32_t>(local.z)};break;
  case 3: rotated = {-static_cast<int32_t>(local.z),local.y,local.x};break;
  default: break;
  }
  return {
    math::signed_word(static_cast<uint16_t>(rotated.x + origin.x)),
    math::signed_word(static_cast<uint16_t>(rotated.y + origin.y)),
    math::signed_word(static_cast<uint16_t>(rotated.z + origin.z)),
  };
}

int16_t instance_point_distance(vec3<int16_t> const &local, vec3<int16_t> const &origin, math::angle const heading, vec3<int16_t> const &reference) {
  // Native 0xb6be8 wraps each placed anchor/reference subtraction before CBE94.
  auto const placed{place_instance_point(local,origin,heading)};
  return math::distance_word({
    math::signed_word(static_cast<uint16_t>(placed.x - reference.x)),
    math::signed_word(static_cast<uint16_t>(placed.y - reference.y)),
    math::signed_word(static_cast<uint16_t>(placed.z - reference.z)),
  });
}

instance::instance(definition const &definition) : source{definition}, streams(definition.parts.size()), parts(definition.parts.size()), custom((definition.custom_bytes + 1) / 2) {
  execute(0);
}

std::optional<unsigned int> instance::stream_cursor(unsigned int const part, unsigned int const parameter) const {
  auto const &stream{streams.at(part).at(parameter)};
  if(!stream || stream->player.stopped()) return {};
  return stream->source + stream->player.position() * 2;
}

void instance::apply_contact_damage(uint16_t const damage) {
  // Native 0xb79c0: the querying projectile also loses health, with a minimum contact cost of one.
  health = static_cast<uint16_t>(health - (damage ? damage : 1));
  if(math::signed_word(health) <= 0) destroyed = true;
}

int16_t instance::bounding_radius() const {
  return source.bounding_radius;
}

int16_t instance::type() const {
  // Native 0xb7a3c distinguishes an untyped SSD from type zero.
  return source.flags & 2 ? static_cast<int16_t>((source.flags & 255) >> 2) : -1;
}

std::vector<vec3<int16_t>> instance::damage_centres() const {
  // Native 0xb7b90 enumerates only the leading stage/once damage boxes in collision entry 2.
  // These are targetable sub-volumes, separate from the model draw anchors.
  std::vector<vec3<int16_t>> result;
  if(source.entries.size() <= 2) return result;
  resources::binary_view const view{source.bytecode};
  unsigned int at{source.entries[2]};
  while(at < source.bytecode.size()) {
    auto const opcode{static_cast<instruction>(view.le16(at))};
    if(opcode != instruction::damage_box_once && opcode != instruction::damage_box_stages) break;
    view.slice(at + 2,12);
    auto const middle{[&](unsigned int const axis){ return static_cast<int16_t>((static_cast<int>(view.signed_le16(at + 2 + axis * 2)) + view.signed_le16(at + 8 + axis * 2)) / 2); }};
    result.emplace_back(middle(0),middle(1),middle(2));
    at += opcode == instruction::damage_box_once ? 18 : 20;
  }
  return result;
}

instance_result instance::execute(unsigned int const entry, instance_context const &context) {
  instance_result result;
  if(entry >= source.entries.size()) return result;
  auto working{context};
  bool install_streams{true};
  unsigned int budget{instruction_budget};
  interpret(source.entries[entry],working,result,budget,0,install_streams);
  return result;
}

instance_result instance::animate(instance_context const &context) {
  instance_result result;
  auto working{context};
  // Events can replace the current stream or any later stream. Re-fetch the slot after each event, as native 0xb6ce0 does.
  for(unsigned int part{0}; part != parts.size(); ++part) {
    for(unsigned int parameter{0}; parameter != part_parameters; ++parameter) {
      if(parameter && !(source.flags & 0x0100)) continue;
      unsigned int budget{instruction_budget};
      while(auto &stream{streams[part][parameter]}) {
        if(!budget--) throw std::runtime_error{"SSD animation events make no progress"};
        auto const step{stream->player.tick(parts[part].parameters[parameter])};
        if(step.action != stream_action::event_requested) break;
        auto const entry{static_cast<uint16_t>(step.event)};
        if(entry < source.entries.size()) {
          bool install_streams{true};
          interpret(source.entries[entry],working,result,budget,0,install_streams);
        }
      }
    }
  }
  return result;
}

void instance::interpret(unsigned int at, instance_context &context, instance_result &result, unsigned int &budget, unsigned int const depth, bool &install_streams) {
  if(depth > maximum_call_depth) throw std::runtime_error{"SSD helper nesting exceeds limit"};
  resources::binary_view const view{source.bytecode};
  auto const field{[&](unsigned int const address, uint16_t const word){
    int64_t const target{static_cast<int64_t>(address) + math::signed_word(word) * 2};
    if(target < 0 || static_cast<uint64_t>(target) >= source.bytecode.size()) throw std::runtime_error{"SSD branch leaves its program"};
    return static_cast<unsigned int>(target);
  }};
  while(budget) {
    --budget;
    auto const opcode{static_cast<instruction>(view.le16(at))};
    unsigned int const args{at + 2}, count{instruction_operands(std::to_underlying(opcode),source.bytecode,args)};
    view.slice(args,count * 2);
    auto const a{[&](unsigned int const index){ return view.le16(args + index * 2); }};
    auto const branch{[&](unsigned int const index){ return field(args + index * 2,a(index)); }};
    auto const call{[&](unsigned int const address){ interpret(address,context,result,budget,depth + 1,install_streams); }};
    auto const hit{[&](bool const diagonal = false){
      return intersects({
        .minimum{math::signed_word(a(0)),math::signed_word(a(1)),math::signed_word(a(2))},
        .maximum{math::signed_word(a(3)),math::signed_word(a(4)),math::signed_word(a(5))},
      },context.movement,diagonal);
    }};
    auto const collision{[&]{
      ++result.collisions;
      if(context.collision_response) { context.collision_response(impact_damage); modified = true; }
    }};
    auto const damage{[&](uint16_t &lives){
      modified = true;
      lives = static_cast<uint16_t>(lives - context.impact_damage);
      collision();
    }};
    auto const request{[&]{
      scene_request value{
        .opcode{std::to_underlying(opcode)},
        .arguments{},
      };
      for(unsigned int i{0}; i != count; ++i) value.arguments.push_back(a(i));
      result.requests.push_back(std::move(value));
      if(context.request_output) (*context.request_output)(result.requests.back());
    }};
    switch(opcode) {
    case instruction::finish: return;
    case instruction::jump: at = branch(0); continue;
    case instruction::call: call(branch(0)); break;
    case instruction::bind_model: case instruction::bind_rotated_model:
      {
        auto &part{parts.at(a(0))};
        part.model_index = a(1);
        part.name = source.names->at(a(1));
        part.rotation_mode = opcode == instruction::bind_model ? 0 : static_cast<uint8_t>(a(2));
        if(opcode == instruction::bind_rotated_model) part.rotation = vec3<math::angle>{a(3),a(4),a(5)};
        modified = true;
      }
      break;
    case instruction::rotate_part:
      parts.at(a(0)).rotation = vec3<math::angle>{a(1),a(2),a(3)};
      break;
    case instruction::draw_rotated: case instruction::draw_origin: case instruction::draw_special_origin: case instruction::draw_ground_shadow:
    case instruction::draw_rotated_anchor: case instruction::draw_anchor: case instruction::draw_special_anchor: case instruction::draw_damage_overlay:
      {
        bool const anchored{opcode == instruction::draw_rotated_anchor || opcode == instruction::draw_anchor || opcode == instruction::draw_special_anchor || opcode == instruction::draw_damage_overlay};
        result.draws.push_back({
          .slot{a(0)},
          .anchor{anchored ? vec3d{source.anchors.at(a(1))} : vec3d{}},
          .kind{opcode == instruction::draw_ground_shadow ? part_draw_kind::ground_shadow : part_draw_kind::model},
          .placement{!anchored ? part_position::assembly_origin : opcode == instruction::draw_anchor || opcode == instruction::draw_damage_overlay ? part_position::fixed_anchor : part_position::integer_anchor},
          .orientation{opcode == instruction::draw_special_origin || opcode == instruction::draw_special_anchor ? part_orientation::world_axes : part_orientation::terrain_axes},
          .delivery{opcode == instruction::draw_rotated || opcode == instruction::draw_rotated_anchor ? part_delivery::immediate : part_delivery::queued},
          .part{parts.at(a(0))},
        });
        if(opcode == instruction::draw_damage_overlay && parts.at(a(0)).parameters[7]) result.draws.push_back({
          .slot{a(2)},
          .anchor{vec3d{source.anchors.at(a(1))}},
          .placement{part_position::fixed_anchor},
          .part{parts.at(a(2))},
        });
      }
      break;
    case instruction::animate_primary:
    case instruction::animate_0: case instruction::animate_1: case instruction::animate_2: case instruction::animate_3:
    case instruction::animate_4: case instruction::animate_5: case instruction::animate_6: case instruction::animate_7:
      if(install_streams) {
        unsigned int const parameter{opcode == instruction::animate_primary ? 0u : std::to_underlying(opcode) - std::to_underlying(instruction::animate_0)};
        std::vector<uint16_t> words;
        for(unsigned int i{2}; i != count; ++i) words.push_back(a(i));
        streams.at(a(0)).at(parameter) = part_stream{
          .source{args + 4},
          .player{words},
        };
        modified = true;
      }
      break;
    case instruction::set_custom_run:
      for(unsigned int i{0}; i != a(1); ++i) custom.at(a(0) + i) = a(i + 2);
      modified = true;
      break;
    case instruction::set_custom: custom.at(a(0)) = a(1); modified = true; break;
    case instruction::set_custom_one: custom.at(a(0)) = 1; break;
    case instruction::decrement_custom: --custom.at(a(0)); break;
    case instruction::if_custom_equal: case instruction::if_custom_unequal:
      if((custom.at(a(0)) == a(1)) == (opcode == instruction::if_custom_equal)) { at = branch(2); continue; }
      break;
    case instruction::if_primary_equal: case instruction::if_primary_equal_alt: case instruction::if_primary_equal_2: case instruction::if_primary_equal_3:
    case instruction::if_primary_equal_4: case instruction::if_primary_equal_5: case instruction::if_primary_equal_6: case instruction::if_primary_equal_7:
      // All eight shipped dispatch aliases compare parameter zero, even opcodes whose numbers might suggest other parameters.
      if(parts.at(a(0)).parameters[0] == a(1)) { at = branch(2); continue; }
      break;
    case instruction::if_status:
      if(!(context.status & a(0))) { at = branch(1); continue; }
      break;
    case instruction::test_box: case instruction::test_diagonal_box:
      if(hit(opcode == instruction::test_diagonal_box)) { result.volume_hit = true; return; }
      break;
    case instruction::collide_box: case instruction::collide_diagonal_box:
      if(hit(opcode == instruction::collide_diagonal_box)) collision();
      break;
    case instruction::damage_box_branch: case instruction::damage_diagonal_branch:
    case instruction::damage_box_call: case instruction::damage_diagonal_call:
      {
        auto &lives{custom.at(a(6))};
        if(math::signed_word(lives) < 0) {
          install_streams = false;
          if(opcode == instruction::damage_box_branch || opcode == instruction::damage_diagonal_branch) { at = branch(7); continue; }
          call(branch(7));
          break;
        }
        if(!hit(opcode == instruction::damage_diagonal_branch || opcode == instruction::damage_diagonal_call)) break;
        damage(lives);
        if(math::signed_word(lives) < 0) { install_streams = true; at = branch(7); continue; }
      }
      break;
    case instruction::damage_box_once: case instruction::damage_box_stages:
      {
        auto &lives{custom.at(a(6))};
        if(math::signed_word(lives) < 0) {
          if(opcode == instruction::damage_box_stages) call(branch(8));
          break;
        }
        if(!hit()) break;
        damage(lives);
        if(math::signed_word(lives) < 0) call(branch(7));
      }
      break;
    case instruction::damage_percentage:
      {
        auto &lives{custom.at(a(6))};
        if(math::signed_word(lives) < 0 || !hit()) break;
        if(lives) {
          damage(lives);
          if(math::signed_word(lives) < 0) lives = 0;
          int const total{math::signed_word(a(7))};
          if(!total) throw std::runtime_error{"SSD damage percentage has zero maximum health"};
          int const percentage{(total - math::signed_word(lives)) * 100 / total};
          result.damage_reports.push_back(percentage);
          if(context.damage_response) context.damage_response(percentage);
        }
        if(math::signed_word(lives) <= 0) call(branch(8));
      }
      break;
    case instruction::destroy_box: case instruction::destroy_diagonal_box:
      if(hit(opcode == instruction::destroy_diagonal_box)) {
        auto &lives{custom.at(a(6))};
        damage(lives);
        if(math::signed_word(lives) < 0) destroyed = true;
      }
      break;
    case instruction::zone_box: case instruction::zone_diagonal_box:
      if(hit(opcode == instruction::zone_diagonal_box)) { result.zone = math::signed_word(a(6)); return; }
      break;
    case instruction::player_zone_box: case instruction::player_zone_diagonal_box:
      if(context.player_query && hit(opcode == instruction::player_zone_diagonal_box)) { result.player_zone = math::signed_word(a(6)); return; }
      break;
    case instruction::surface_box:
      if(context.surface_enabled) {
        if(context.player_query) {
          if(hit()) { call(branch(7)); result.surface_present = true; }
        } else {
          auto &lives{custom.at(a(6))};
          if(math::signed_word(lives) >= 0 && hit()) {
            damage(lives);
            if(math::signed_word(lives) < 0) call(branch(8));
          }
        }
      }
      break;
    case instruction::player_box_call:
      if(context.player_query && hit()) call(branch(6));
      break;
    case instruction::surface_height:
      if(context.player_query) result.surface_height = math::signed_word(a(0));
      break;
    case instruction::destroy: destroyed = true; modified = true; break;
    case instruction::restore: destroyed = false; modified = true; break;
    case instruction::set_category: category = static_cast<uint8_t>(a(0) & 15); modified = true; break;
    case instruction::set_impact_damage: impact_damage = a(0); modified = true; break;
    case instruction::set_health: health = a(0); modified = true; break;
    case instruction::eject_player: result.ejection_requested = true; break;
    case instruction::message: result.message = a(0); break;
    case instruction::measure_anchor:
      if(!context.anchor_distance) throw std::runtime_error{"SSD distance test needs a world placement"};
      context.distance = context.anchor_distance(source.anchors.at(a(0)));
      break;
    case instruction::if_distance_near: case instruction::if_distance_far:
      if((context.distance <= math::signed_word(a(0))) == (opcode == instruction::if_distance_near)) { at = branch(1); continue; }
      break;
    case instruction::call_if_distance_near: case instruction::call_if_distance_far:
      if((context.distance <= math::signed_word(a(0))) == (opcode == instruction::call_if_distance_near)) call(branch(1));
      break;
    case instruction::set_offset: case instruction::update_offset: case instruction::emit_effect:
      request();
      break;
    default: throw std::runtime_error{"unimplemented SSD instruction " + std::to_string(std::to_underlying(opcode))};
    }
    at = args + count * 2;
  }
  throw std::runtime_error{"SSD instruction budget exceeded"};
}

} // namespace tfx::world
