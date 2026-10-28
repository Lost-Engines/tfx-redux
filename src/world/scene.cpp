#include "scene.h"
#include "render/preview_state.h"
#include "render/point_size.h"
#include "terrain_cells.h"
#include "world/sky.h"
#include "world/model_placement.h"
#include "world/model_orientation.h"
#include "render/model_transform.h"
#include "render/model_metrics.h"
#include "render/model_queue.h"
#include "render/model_point.h"
#include "render/integer_model_polygon.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <vector>
#include <utility>

namespace tfx::world {

scene::scene(environment const &environment, assets &bank, world::environment const *const upper, animation_clock *const retained_clock) : terrain{environment}, upper_terrain{upper}, resources{bank}, clock{retained_clock ? *retained_clock : local_clock} {
}

void scene::set_render_paused(bool paused) {
  if(render_paused && !paused) for(auto &[id,actor] : actors) actor.preview_entry.reset();
  render_paused = paused;
}

scene_handle scene::create_actor(std::string const &name, vec3d const &position, vec3<math::angle> const &angles, bool const aircraft) {
  auto const &source{resources.object(name)};
  unsigned int const id{next_actor++};
  actors.emplace(id,moving_instance{
    .state{source},
    .position{vec3d{position}},
    .angles{vec3<math::angle>{angles}},
    .aircraft{aircraft},
  });
  actor_order.push_back({
    .value{id},
  });
  return {.value{id}};
}

void scene::set_actor_draw_order(std::span<scene_handle const> const order) {
  std::set<unsigned int> seen;
  std::vector<scene_handle> replacement;
  replacement.reserve(actors.size());
  for(auto const handle : order) {
    if(!actors.contains(handle.value) || !seen.insert(handle.value).second) throw std::invalid_argument{"actor drawing order contains an unknown or repeated handle"};
    replacement.push_back(handle);
  }
  for(auto const handle : actor_order) if(seen.insert(handle.value).second) replacement.push_back(handle);
  actor_order = std::move(replacement);
}

void scene::place_actor(scene_handle const handle, vec3d const &position, vec3<math::angle> const &angles) {
  auto &actor{actors.at(handle.value)};
  actor.position = position;
  actor.angles = angles;
}

void scene::show_actor(scene_handle const handle, bool const visible) {
  actors.at(handle.value).visible = visible;
}

bool scene::terrain_intact(unsigned int const x, unsigned int const z) {
  return x < map_width && z < map_height && !terrain_instance(x,z).destroyed;
}

namespace {

vec3<int16_t> word_position(vec3d const &position) {
  auto const component{[](double const value) { return math::signed_word(static_cast<uint16_t>(static_cast<int64_t>(value))); }};
  return {component(position.x),component(position.y),component(position.z)};
}

std::optional<flight::landing_port> landing_port(instance const &state) {
  int16_t constexpr landing_type{50};
  if(state.type() != landing_type) return std::nullopt;
  // Native 0xb6e48 reads the first three initialized custom words.
  return flight::landing_port{
    .offset{math::signed_word(state.custom.at(0)),math::signed_word(state.custom.at(1))},
    .heading{state.custom.at(2)},
  };
}

unsigned int wrapped_cell(int const value, unsigned int const dimension) {
  /// Native 0xb0cbc uses dimension - 1 for negative remainders, including its one-cell seam quirk.
  int const remainder{value % static_cast<int>(dimension)};
  return static_cast<unsigned int>(remainder < 0 ? remainder + static_cast<int>(dimension) - 1 : remainder);
}

} // anonymous namespace

void scene::set_effect_sink(std::function<void(effect_record const&)> sink) { effect_sink = std::move(sink); }
void scene::set_query_reference(vec3<int32_t> const *const position) { query_reference = position; }
void scene::set_carried_models(std::span<models::shape const *const> const models) { carried_models.assign(models.begin(),models.end()); }
vec3<int16_t> scene::reference_words(vec3d const &fallback) const {
  if(query_reference) return word_position(vec3d{*query_reference});
  return word_position({std::floor(fallback.x),std::floor(fallback.y),std::floor(fallback.z)});
}
void scene::emit_effect(scene_request const &request, vec3d const &origin, math::angle const heading) {
  unsigned int constexpr emit_effect_opcode{79};
  if(render_paused || !effect_sink || request.opcode != emit_effect_opcode) return;
  auto const &a{request.arguments};
  if(a.size() != 4 || a[3] >= effect_kind_count) throw std::runtime_error{"invalid authored SSD effect request"};
  auto const world_origin{word_position({std::floor(origin.x),std::floor(origin.y),std::floor(origin.z)})};
  effect_sink({
    .position{place_instance_point({math::signed_word(a[0]),math::signed_word(a[1]),math::signed_word(a[2])},world_origin,heading)},
    .kind{static_cast<effect_kind>(a[3])},
  });
}

instance &scene::terrain_instance(unsigned int const x, unsigned int const z) {
  auto const &source{resources.object(terrain.variant(x,z))};
  return instances.try_emplace(z * map_width + x,source).first->second;
}

bool scene::terrain_destroyed(vec3<int32_t> const &position) {
  int const cell_x{position.x >> cell_shift}, cell_z{position.z >> cell_shift};
  auto const x{wrapped_cell(cell_x,map_width)};
  auto const z{map_height - 1 - wrapped_cell(static_cast<int>(map_height) - 1 - cell_z,map_height)};
  return terrain_instance(x,z).destroyed;
}

int16_t scene::terrain_type(vec3<int32_t> const &position) {
  int const cell_x{position.x >> 11}, cell_z{position.z >> 11};
  auto const x{wrapped_cell(cell_x,map_width)};
  auto const z{map_height - 1 - wrapped_cell(static_cast<int>(map_height) - 1 - cell_z,map_height)};
  return terrain_instance(x,z).type();
}

void scene::prepare_terrain_cell(unsigned int const x, unsigned int const z) {
  terrain_instance(x,z);
}

instance_result scene::terrain_event(unsigned int const x, unsigned int const z, unsigned int const entry) {
  vec3d const origin{(x + .5) * cell_units,0,(z + .5) * cell_units};
  auto const output{[&](scene_request const &request) { emit_effect(request,origin,terrain.tile(x,z).rotation); }};
  return terrain_instance(x,z).execute(entry,{
    .status{terrain.lighting.status},
    .request_output{output},
  });
}

instance_result scene::collide_terrain(vec3<int32_t> const &previous, vec3<int32_t> const &current, uint16_t const damage, bool const player, std::function<void(uint16_t)> response) {
  int const cell_x{current.x >> 11}, cell_z{current.z >> 11};
  auto const x{wrapped_cell(cell_x,map_width)};
  auto const z{map_height - 1 - wrapped_cell(static_cast<int>(map_height) - 1 - cell_z,map_height)};
  auto const &tile{terrain.tile(x,z)};
  auto &state{terrain_instance(x,z)};
  vec3d const origin{(cell_x + .5) * cell_units,0,(cell_z + .5) * cell_units};
  auto const inverse_heading{static_cast<math::angle>(-tile.rotation)};
  auto const output{[&](scene_request const &request) { emit_effect(request,origin,tile.rotation); }};
  return state.execute(2,{
    .movement{
      .previous{word_position(math::rotate_tile(vec3d{previous} - origin,inverse_heading))},
      .current{word_position(math::rotate_tile(vec3d{current} - origin,inverse_heading))},
    },
    .player_query{player},
    .impact_damage{damage},
    .status{terrain.lighting.status},
    .collision_response{std::move(response)},
    .request_output{output},
  });
}

instance_result scene::collide_actor(scene_handle const handle, vec3<int32_t> const &previous, vec3<int32_t> const &current, uint16_t const damage, std::function<void(uint16_t)> response, std::function<void(int)> damage_response, bool const player) {
  auto &actor{actors.at(handle.value)};
  // Native moving assemblies retain axis-aligned SSD collision volumes; their full visual Euler rotation is separate.
  vec3d const origin{std::floor(actor.position.x),std::floor(actor.position.y),std::floor(actor.position.z)};
  auto const output{[&](scene_request const &request) { emit_effect(request,origin); }};
  return actor.state.execute(2,{
    .movement{
      .previous{word_position(vec3d{previous} - origin)},
      .current{word_position(vec3d{current} - origin)},
    },
    .player_query{player},
    .impact_damage{damage},
    .status{terrain.lighting.status},
    .collision_response{std::move(response)},
    .damage_response{std::move(damage_response)},
    .request_output{output},
  });
}

void scene::apply_actor_contact_damage(scene_handle const handle, uint16_t const damage) {
  actors.at(handle.value).state.apply_contact_damage(damage);
}

bool scene::actor_destroyed(scene_handle const handle) const {
  return actors.at(handle.value).state.destroyed;
}

int16_t scene::actor_type(scene_handle const handle) const {
  return actors.at(handle.value).state.type();
}

std::optional<flight::landing_port> scene::terrain_landing_port(vec2<int16_t> const &cell) {
  if(cell.x < 0 || cell.y < 0 || cell.x >= static_cast<int>(map_width) || cell.y >= static_cast<int>(map_height)) return std::nullopt;
  return landing_port(terrain_instance(static_cast<unsigned int>(cell.x),static_cast<unsigned int>(cell.y)));
}

std::optional<flight::landing_port> scene::actor_landing_port(scene_handle const handle) const {
  return landing_port(actors.at(handle.value).state);
}

// Target origins remain usable when an SSD declares no custom aim offset.
vec2<int16_t> scene::actor_target_offset(scene_handle const handle) const {
  auto const &custom{actors.at(handle.value).state.custom};
  return custom.size() >= 2 ? vec2<int16_t>{math::signed_word(custom[0]),math::signed_word(custom[1])} : vec2<int16_t>{0,0};
}

vec2<int16_t> scene::terrain_target_offset(vec2<int32_t> const &cell) {
  auto const &custom{terrain_instance(static_cast<unsigned int>(cell.x),static_cast<unsigned int>(cell.y)).custom};
  return custom.size() >= 2 ? vec2<int16_t>{math::signed_word(custom[0]),math::signed_word(custom[1])} : vec2<int16_t>{0,0};
}

std::vector<vec3<int16_t>> scene::actor_damage_centres(scene_handle const handle) const {
  return actors.at(handle.value).state.damage_centres();
}

uint16_t scene::actor_impact_damage(scene_handle const handle) const {
  return actors.at(handle.value).state.impact_damage;
}

void scene::queue_actor(scene_handle const handle, vec3d const &position, vec3<math::angle> const &angles, vec3<int32_t> const *const distance_reference, submission_phase const phase) {
  submissions.push_back({
    .handle{handle},
    .position{vec3d{position}},
    .angles{vec3<math::angle>{angles}},
    .distance_reference{distance_reference ? std::optional{vec3<int32_t>{*distance_reference}} : std::nullopt},
    .phase{phase},
  });
}

void scene::queue_model(std::string const &name, vec3d const &position) {
  model_submissions.push_back({
    .part{
      .name{name},
      .rotation{0,0,0},
    },
    .position{vec3d{position}},
  });
}

instance_result scene::actor_event(scene_handle const handle, unsigned int const entry) {
  auto &actor{actors.at(handle.value)};
  if(render_paused) { actor.preview_entry=entry;return {}; }
  auto const output{[&](scene_request const &request) { emit_effect(request,actor.position); }};
  return actor.state.execute(entry,{
    .status{terrain.lighting.status},
    .request_output{output},
  });
}

bool scene::advance_instance(instance &state, vec3d const &origin, math::angle const heading) {
  auto const output{[&](scene_request const &request) { emit_effect(request,origin,heading); }};
  auto const placed_origin{word_position({std::floor(origin.x),std::floor(origin.y),std::floor(origin.z)})};
  auto const reference{reference_words()};
  return state.animate({
    .status{terrain.lighting.status},
    .distance{instance_point_distance({0,0,0},placed_origin,heading,reference)},
    .anchor_distance{[&](vec3d const &anchor) { return instance_point_distance(word_position(anchor),placed_origin,heading,reference); }},
    .request_output{output},
  }).ejection_requested;
}

scene_events scene::advance(timer_ticks const elapsed) {
  animation_pending = clock.advance(elapsed);
  player_animation_pending = animation_pending;
  return {
    .animated{animation_pending},
  };
}

bool scene::animate_player_model(scene_handle const handle) {
  if(!std::exchange(player_animation_pending,false)) return false;
  auto &actor{actors.at(handle.value)};
  return advance_instance(actor.state,actor.position);
}
void scene::set_player_rendering(scene_handle const handle, vec3d const &position, vec3<math::angle> const &angles, animation_policy const policy) {
  player_model.emplace(player_rendering{
    .handle{handle},
    .position{vec3d{position}},
    .angles{vec3<math::angle>{angles}},
    .animation{policy},
  });
}

std::optional<render::camera> const &scene::last_camera() const { return installed_camera; }

void scene::replay(render::software_renderer &renderer, render::camera const &view, recording::scene_frame const &source) {
  draw(renderer,view,0,{},std::nullopt,0,scene_pass::instrument,nullptr,&source);
}

scene_events scene::draw(render::software_renderer &renderer, render::camera const &view, unsigned int const radius, std::function<void()> const &final_submissions, std::optional<int32_t> const camera_anchor_y, int16_t const shadow_ground_height, scene_pass const pass, recording::scene_frame *const capture, recording::scene_frame const *const playback) {
  installed_camera = view;
  /// Native cached model metrics own detail, distance predicates, remap and signed sort priority
  if(!render_paused && pass == scene_pass::primary) instrument_animation = std::exchange(animation_pending,false);
  bool const animate{!render_paused && instrument_animation};
  render::preview_state jitter_frame{jitter,render_paused};
  jitter_frame->begin(animate);
  scene_events events;
  struct queued_part {
    models::shape const *shape;
    std::string name;
    std::array<uint8_t,part_parameters> parameters;
    uint8_t initial_state;
    vec3d origin;
    mat3d rotation;
    math::angle heading;
    vec3<math::angle> angles;
    double depth;
    double sort_key;
    vec3<int32_t> camera_origin;
    render::model_metrics metrics;
    render::model_render_path path;
    uint8_t rotation_scale;
    bool minimum_size{false};
  };
  std::vector<queued_part> queue;
  render::model_queue native_queue;
  auto const projection{view.projector()};
  bool const native{view.projection == render::projection_mode::original_model};
  if(!native) renderer.begin_points();
  auto const fixed_position{[](vec3d const &position) { return vec3<int32_t>{vec3<uint32_t>{vec3<int64_t>{position * 65536}}}; }};
  auto const paint_part{[&](queued_part &item, render::model_render_path const path, part_delivery const delivery = part_delivery::queued) {
    try {
      if(native && delivery == part_delivery::queued) model_parameters = item.parameters;
      if(native) item.camera_origin = render::scale_model_cache(item.camera_origin,item.rotation_scale);
      item.metrics.distance = render::measure_model(item.camera_origin,item.shape->sort_depth_bias).distance;
      // Missile smoke changes from a small sphere to several pixel
      // blocks with distance. These compensate for shrinking particles; the
      // nearest representation is not a higher-detail mesh to hold forever.
      double const detail_scale{item.name == "smokecld" ? 1 : visual_lod_scale};
      int16_t const visual_distance{native && detail_scale == 1 ? item.metrics.distance : static_cast<int16_t>(std::clamp((native ? item.metrics.distance : item.depth) * detail_scale,0.0,32767.0))};
      auto const *detail{detail_scale == 0 ? &item.shape->details.front() : item.shape->detail_for_distance(visual_distance)};
      if(!detail) return;
      double const fog_distance{std::isfinite(view.far_distance) ? view.far_distance : std::hypot(static_cast<double>(map_width),static_cast<double>(map_height)) * cell_units};
      uint8_t const distance_row{native ? item.metrics.colour_row : static_cast<uint8_t>(std::clamp(item.depth * 15 / fog_distance,0.0,15.0))};
      bool const shadow{path == render::model_render_path::ground_shadow};
      // Native shadows retain coloured distant-point LOD. Ordinary
      // silhouettes take the forced-black drawing path below.
      if(detail && detail->kind >= models::detail_kind::pixel_block) {
        unsigned int const size{detail->kind == models::detail_kind::pixel_block ? 2u : 1u};
        uint8_t const colour{resources.model_colours(distance_row)[detail->colour]};
        if(native && renderer.output.size == vec2<int>{render::screen_width,render::screen_height}) render::model_point(renderer.output,item.camera_origin,view,size,colour);
        else {
          models::primitive dot{
            .kind{models::primitive_kind::point},
            .vertices{},
            .colour{colour},
            .pixel_size{size},
            .attachment_slot{},
            .point_role{item.shape->has_surfaces ? models::point_role::object : item.name=="smokecld" ? models::point_role::particle : models::point_role::light},
          };
          std::array<models::model_vertex,1> const point{{{
            .position{native ? vec3d{item.camera_origin} / 65536.0 : view.transform(item.origin)},
            .uv{0,0},
          }}};
          renderer.primitive(dot,point,view,{});
        }
        ++parts_drawn;++primitives_drawn;
        return;
      }
      auto const placed{[&item](vec3d const &vertex){ return item.origin + math::rotate_tile(item.rotation * vertex,item.heading); }};
      render::model_transform const transform{projection.matrix,path == render::model_render_path::unrotated ? vec3<math::angle>{0,0,0} : vec3<math::angle>{item.angles},item.camera_origin,shadow ? render::model_surface::ground_shadow : render::model_surface::body};
      auto const camera_point{[&](vec3d const &position, double const scale) { if(native) return transform.point(position,scale);auto point{placed(position)};if(shadow) point.y = item.origin.y;return view.transform(point); }};
      models::shape_context context;
      if(native) context.camera.emplace(models::shape_camera{
        .basis{mat3<int32_t>{transform.matrix()}},
        .origin{vec3<int32_t>{item.camera_origin}},
        .projection_shift{render::projection_shift(view.opening)},
      });
      auto const &parameters{native ? model_parameters : item.parameters};
      std::copy(parameters.begin(),parameters.end(),context.parameters.begin());
      // Native 0xcc001 queues parameter 0; 0xbac16 seeds the shape condition from it.
      context.initial_state = item.initial_state;
      context.attachments = carried_models;
      context.status = terrain.lighting.status;
      context.jitter = &jitter_frame->state();
      context.distance = visual_distance;
      if(detail) context.entry_address = detail->offset + 2;
      context.camera_depth = [&](vec3d const &point) { return camera_point(point,1).z * detail_scale; };
      context.project = [&view, &camera_point, native](vec3d const &v, double const scale){
        auto point{native ? vec3d{v} : camera_point(v,scale)};
        if(std::abs(point.z) < view.near_distance * scale) point.z = std::copysign(view.near_distance * scale,point.z);
        return view.project(point,scale);
      };
      context.face_visibility = [&view,&camera_point,native](std::span<models::model_vertex const> const vertices, models::facing_test const purpose) {
        if(!native) {
          if(vertices.size()<3) return std::optional<bool>{};
          std::array<models::model_vertex,3> camera;
          for(unsigned i=0;i<3;++i) camera[i].position=camera_point(vertices[i].position,vertices[i].projection_scale);
          return render::model_facing(camera,view,purpose);
        }
        std::vector<models::model_vertex> camera;
        camera.reserve(vertices.size());
        for(auto const &vertex : vertices) camera.push_back({
          .position{vec3d{vertex.position}},
          .uv{0,0},
          .projection_scale{vertex.projection_scale},
        });
        return render::model_facing(camera,view,purpose);
      };
      auto const geometry{models::interpret(*item.shape,context)};
      warnings.insert(geometry.warnings.begin(),geometry.warnings.end());
      ++parts_drawn;
      // Measure the projected surface area, not just its bounding box: thin
      // distant LOD meshes can span several pixels yet cover none of them.
      bool minimum_proxy{false};
      if(!native && item.minimum_size && item.shape->has_surfaces && !shadow) {
        double const minimum{render::object_point_diameter(renderer.output.size)};
        models::primitive const *body{nullptr};bool small{true};double area{0};
        for(auto const &primitive : geometry.primitives) {
          if(primitive.kind==models::primitive_kind::point) continue;
          if(!body) body=&primitive;
          vec2d first{0,0}, previous{0,0};double twice_area{0};unsigned count{0};
          for(auto const &vertex : primitive.vertices) {
            auto const camera{camera_point(vertex.position,vertex.projection_scale)};
            if(camera.z<=view.near_distance*vertex.projection_scale) { small=false;break; }
            auto const screen{view.project(camera,vertex.projection_scale)};
            if(!count++) first=vec2d{screen};
            else twice_area += previous.x*screen.y-previous.y*screen.x;
            previous=vec2d{screen};
          }
          if(!small) break;
          if(primitive.kind==models::primitive_kind::line) area+=(previous-first).length();
          else area+=std::abs(twice_area+previous.x*first.y-previous.y*first.x)/2;
          if(area>=minimum*minimum) {small=false;break;}
        }
        if(body && small) {
          models::primitive dot{.kind{models::primitive_kind::point},.vertices{},.colour{body->colour},.pixel_size{1},.attachment_slot{},.point_role{models::point_role::object}};
          std::array<models::model_vertex,1> const centre{{{.position{view.transform(item.origin)},.uv{0,0}}}};
          auto const *colours{body->palette==models::palette_source::direct ? nullptr : &resources.model_colours(body->palette==models::palette_source::fixed ? 0 : distance_row)};
          renderer.primitive(dot,centre,view,{},colours);++primitives_drawn;minimum_proxy=true;
        }
      }
      std::vector<models::model_vertex> vertices;
      for(auto const &primitive : geometry.primitives) {
        // The visibility floor replaces the body, preserving attached lights/effects.
        if(minimum_proxy && primitive.kind!=models::primitive_kind::point) continue;
        vertices.clear();vertices.reserve(primitive.vertices.size());
        for(auto const &vertex : primitive.vertices) vertices.push_back({
          .position{native ? vec3d{vertex.position} : camera_point(vertex.position,vertex.projection_scale)},
          .uv{vec2d{vertex.uv}},
          .shade{shadow ? -1 : vertex.shade},
          .projection_scale{vertex.projection_scale},
        });
        // Opcode 51/52 pixels bypass the shadow ink loader, like distant LOD.
        bool const authored_pixel{primitive.kind == models::primitive_kind::point && primitive.pixel_size};
        if(shadow && !authored_pixel) {
          auto silhouette{primitive};silhouette.colour = 0;silhouette.texture.reset();
          renderer.primitive(silhouette,vertices,view,{});
          ++primitives_drawn;continue;
        }
        auto const texture{primitive.texture ? resources.texture(*primitive.texture) : std::span<uint8_t const>{}};
        auto const overlay{primitive.texture && texture_overlay ? texture_overlay(item.name,*primitive.texture) : std::span<uint16_t const>{}};
        if(primitive.texture && texture.empty()) warnings.insert("unresolved runtime texture: " + item.name);
        uint8_t colour_row{distance_row};
        if(primitive.palette == models::palette_source::fixed) colour_row = 0;
        else if(native && primitive.palette == models::palette_source::face_depth) colour_row = render::distance_colour_row(static_cast<int32_t>(primitive.palette_depth) * 65536);
        auto const *remap{primitive.palette != models::palette_source::direct ? &resources.model_colours(colour_row) : nullptr};
        if(primitive.pixel_size && (item.name=="smokecld" || (item.shape->has_surfaces && geometry.primitives.size()==1))) {
          auto point{primitive};point.point_role=item.name=="smokecld" ? models::point_role::particle : models::point_role::object;
          renderer.primitive(point,vertices,view,texture,remap);
        } else renderer.primitive(primitive,vertices,view,texture,remap,overlay);
        ++primitives_drawn;
      }
    } catch(std::exception const &error) {
      warnings.insert(item.name + ": " + error.what());
    }
  }};
  auto const queue_part{[&](models::shape const &shape, model_part const &part, vec3d const &origin, mat3d const &rotation, math::angle const heading, vec3<math::angle> const &angles, vec3<int32_t> const &camera_origin, render::model_render_path const path = render::model_render_path::scaled, part_delivery const delivery = part_delivery::queued, bool const minimum_size = false) {
    auto const metrics{render::measure_model(camera_origin,shape.sort_depth_bias)};
    double const depth{view.transform(origin).z};
    queued_part item{
      .shape{&shape},
      .name{part.name},
      .parameters{part.parameters},
      .initial_state{part.parameters.front()},
      .origin{vec3d{origin}},
      .rotation{mat3d{rotation}},
      .heading{heading},
      .angles{vec3<math::angle>{angles}},
      .depth{depth},
      .sort_key{native ? metrics.order : depth + shape.sort_depth_bias},
      .camera_origin{vec3<int32_t>{camera_origin}},
      .metrics{metrics},
      .path{path},
      .rotation_scale{part.rotation_mode ? part.rotation_mode : uint8_t{1}},
      .minimum_size{minimum_size},
    };
    if(native && delivery == part_delivery::immediate) {
      auto const slot{native_queue.scratch_slot()};
      auto const state{model_slots.immediate(slot,item.rotation_scale,item.angles)};
      item.path = state.path;item.rotation_scale = state.scale;
      if(slot < queue.size()) queue[slot] = std::move(item);
      else queue.push_back(std::move(item));
      paint_part(queue[slot],path,part_delivery::immediate);
      return;
    }
    if(native) {
      auto const slot{native_queue.insert(metrics.order)};
      auto const state{model_slots.queued(slot,path,item.rotation_scale,item.angles)};
      item.path = state.path;item.rotation_scale = state.scale;
      item.angles = vec3<math::angle>{state.angles};
      if(slot < queue.size()) queue[slot] = std::move(item);
      else queue.push_back(std::move(item));
    } else queue.push_back(std::move(item));
  }};
  auto const flush_queue{[&] {
    if(!native) std::stable_sort(queue.begin(),queue.end(),[](auto const &a, auto const &b){ return a.sort_key > b.sort_key; });
    auto const order{native ? native_queue.draw_order() : std::vector<render::model_slot>{}};
    unsigned int const count{native ? native_queue.size() : static_cast<unsigned int>(queue.size())};
    for(unsigned int draw_index{0}; draw_index != count; ++draw_index) {
      auto &item{queue[native ? static_cast<unsigned int>(order[draw_index]) : draw_index]};
      paint_part(item,item.path);
    }
  }};
  parts_drawn = primitives_drawn = 0;
  if(playback) {
    if(capture || !native || playback->data.models.size() != playback->model_names.size() || playback->data.models.size() != playback->data.order.size()) throw std::runtime_error{"invalid recorded scene binding"};
    native_queue.restore(playback->data.order);
    for(unsigned int slot{0}; slot != playback->data.models.size(); ++slot) {
      auto const &source{playback->data.models[slot]};
      auto const &name{playback->model_names[slot]};
      auto const path{source.kind == recording::model_kind::unrotated ? render::model_render_path::unrotated : source.kind == recording::model_kind::ground_shadow ? render::model_render_path::ground_shadow : render::model_render_path::scaled};
      model_slots.restore(static_cast<render::model_slot>(slot),{
        .path{path},
        .scale{source.scale},
        .angles{vec3<math::angle>{source.angles}},
      });
      queue.push_back({
        .shape{&resources.shape(name)},
        .name{name},
        .parameters{source.parameters},
        .initial_state{source.initial_state},
        .origin{0,0,0},
        .rotation{math::rotation({0,0,0})},
        .heading{0},
        .angles{vec3<math::angle>{source.angles}},
        .depth{0},
        .sort_key{0},
        .camera_origin{vec3<int32_t>{source.camera_origin}},
        .metrics{
          .distance{0},
          .order{0},
          .colour_row{source.colour_row},
        },
        .path{path},
        .rotation_scale{source.scale},
      });
    }
    flush_queue();
    return events;
  }
  // Native 0x7589a binds SSD range queries to the player, independently of the
  // camera. Standalone inspection scenes use their camera as the reference.
  auto const reference{reference_words(view.position)};
  auto const visible{[&](vec3d const &position, double const radius) {
    auto const point{view.transform(position)};
    if(point.z + radius <= 0 || point.z - radius > view.far_distance) return false;
    double const depth{std::max(0.0,point.z)};
    double const horizontal{std::max(view.projection_origin.x,renderer.output.size.x - view.projection_origin.x) / view.focal_distance};
    double const vertical{std::max(view.projection_origin.y,renderer.output.size.y - view.projection_origin.y) / view.focal_distance};
    return std::abs(point.x) <= depth * horizontal + radius * (1 + horizontal) && std::abs(point.y) <= depth * vertical + radius * (1 + vertical);
  }};
  auto const lod_distance{[&](int16_t const distance) { return visual_lod_scale == 1 ? distance : static_cast<int16_t>(std::clamp(distance * visual_lod_scale,0.0,32767.0)); }};
  int const centre_x{static_cast<int>(std::floor(view.position.x / cell_units))}, centre_z{static_cast<int>(std::floor(view.position.z / cell_units))};
  int const first_x{radius ? std::max(0, centre_x - static_cast<int>(radius)) : 0}, last_x{radius ? std::min(static_cast<int>(map_width) - 1, centre_x + static_cast<int>(radius)) : static_cast<int>(map_width) - 1};
  int const first_z{radius ? std::max(0, centre_z - static_cast<int>(radius)) : 0}, last_z{radius ? std::min(static_cast<int>(map_height) - 1, centre_z + static_cast<int>(radius)) : static_cast<int>(map_height) - 1};
  auto const layer{select_terrain_layer(upper_terrain && terrain.lighting.cloud_layer,terrain.lighting.layer_y,terrain.lighting.layer_offset_y,camera_anchor_y.value_or(static_cast<int32_t>(std::floor(view.position.y))))};
  auto const &grid{layer.upper ? *upper_terrain : terrain};
  auto &states{layer.upper ? upper_instances : instances};
  auto const submit{[&](moving_instance &actor, vec3d const &position, vec3<math::angle> const &angles, animation_policy const policy = animation_policy::when_visible) {
    if(!native && !visible(position,std::max(256,static_cast<int>(actor.state.bounding_radius())))) return;
    auto const rotation{math::rotation(angles)};
    // Native moving SSD effect placement retains quarter-turn zero; its full
    // visual Euler transform is separate from the request callback.
    auto const assembly{fixed_position(position)};
    if(native && !projection.camera_sphere_visible(projection.camera_fixed(assembly),actor.state.bounding_radius())) return;
    if(animation_eligible(policy,animate,true)) events.eject_player |= advance_instance(actor.state,position);
    auto const requests{[&](scene_request const &request) { emit_effect(request,position); }};
    auto const origin{word_position({std::floor(position.x),std::floor(position.y),std::floor(position.z)})};
    auto const actor_reference{reference_words(view.position)};
    instance_context const context{
      .status{terrain.lighting.status},
      .distance{lod_distance(instance_point_distance({0,0,0},origin,0,actor_reference))},
      .anchor_distance{[&](vec3d const &anchor) { return lod_distance(instance_point_distance(word_position(anchor),origin,0,actor_reference)); }},
      .request_output{requests},
    };
    render::preview_state actor_frame{actor.state,render_paused};
    if(render_paused && actor.preview_entry) actor_frame->execute(*actor.preview_entry,{.status{terrain.lighting.status}});
    auto const output{actor_frame->execute(1,context)};
    for(auto const &draw : output.draws) {
      bool const shadow{draw.kind == part_draw_kind::ground_shadow};
      auto const shadow_placement{shadow ? place_model_shadow(projection,assembly,shadow_ground_height,actor.state.bounding_radius()) : std::optional<model_placement>{}};
      if(shadow && (!shadow_placement || !shadow_placement->visible)) continue;
      auto const &part{draw.part};
      if(part.name.empty()) continue;
      try {
        auto const &shape{resources.shape(part.name)};
        auto const placement{shadow ? *shadow_placement : place_model_part(projection,assembly,word_position(draw.anchor),0,draw.placement,actor.state.bounding_radius())};
        if(native && !placement.visible) continue;
        auto const origin{(shadow || (native && draw.placement != part_position::assembly_origin)) ? vec3d{effect_position(placement.position,vec3<int32_t>{position})} : position + rotation * draw.anchor};
        double const depth{view.transform(origin).z};
        if(!native && (depth < -cell_units || depth > view.far_distance + cell_units)) continue;
        queue_part(shape,part,origin,rotation * math::rotation(part.rotation),0,model_orientation(angles,part),placement.camera_origin,shadow ? render::model_render_path::ground_shadow : render::model_render_path::scaled,draw.delivery,actor.aircraft && !shadow);
      } catch(std::exception const &error) {
        warnings.insert(part.name + ": " + error.what());
      }
    }
  }};
  auto const submit_queued{[&](submission_phase const phase) {
    struct restore_reference {
      vec3<int32_t> const *&current;
      vec3<int32_t> const *previous;
      ~restore_reference() { current = previous; }
    } const restore{
      .current{query_reference},
      .previous{query_reference},
    };
    for(auto const &submission : submissions) if(submission.phase == phase) {
      // Native projectile drawing temporarily substitutes the current camera
      // for the player's SSD query origin, including animation/effect requests.
      query_reference = submission.distance_reference ? &*submission.distance_reference : restore.previous;
      submit(actors.at(submission.handle.value),submission.position,submission.angles);
    }
  }};
  // Native 72135/7213A submit the camera's player model and abandoned
  // aircraft before A26CC queues the sky, independently of their constructors.
  if(pass == scene_pass::primary && player_model) {
    auto &actor{actors.at(player_model->handle.value)};
    if(actor.visible) submit(actor,player_model->position,player_model->angles,player_model->animation);
  }
  submit_queued(submission_phase::before_sky);
  // Native sky models enter the bounded queue before world/actor models.
  // Even an offscreen sun changes the original quicksort's equal-depth order.
  for(auto const &submission : model_submissions) {
    auto const &shape{resources.shape(submission.part.name)};
    mat3d const identity{1.,0.,0.,0.,1.,0.,0.,0.,1.};
    queue_part(shape,submission.part,submission.position,identity,0,{0,0,0},projection.camera_integer(vec3<int32_t>{submission.position}),render::model_render_path::unrotated);
  }
  model_submissions.clear();
  std::vector<terrain_cell> cells;
  // 72149 passes the camera's integer anchor (134F0/13500), not its smoothed
  // Q16 projection translation. Chase offsets can cross a cell boundary while
  // the original terrain neighbourhood remains centred on that anchor.
  if(native) visit_terrain_cells({view.world_anchor.x,view.world_anchor.z},static_cast<int16_t>(radius * 2),[&](terrain_cell const &cell) { cells.push_back(cell); });
  else for(int x{first_x}; x <= last_x; ++x) for(int z{first_z}; z <= last_z; ++z) cells.push_back(terrain_cell{
    .source{static_cast<unsigned int>(x),static_cast<unsigned int>(z)},
    .world_cell{x,z},
  });
  for(auto const &cell : cells) {
    auto const x{cell.world_cell.x}, z{cell.world_cell.y};
    auto const col{cell.source.x}, row{cell.source.y};
    auto const &tile{grid.tile(col,row)};
    vec3d const centre{(x + .5) * cell_units,static_cast<double>(layer.height),(z + .5) * cell_units};
    if(!native && !visible(centre,cell_units * 1.5)) continue;
    auto const camera_centre{view.transform(centre)};
    if(!native && (camera_centre.z < -cell_units || camera_centre.z > view.far_distance + cell_units)) continue;
    try {
      auto &instance{states.try_emplace(row * map_width + col,resources.object(grid.variant(col,row))).first->second};
      auto const assembly{fixed_position(centre)};
      if(native && !projection.camera_sphere_visible(projection.camera_fixed(assembly),instance.bounding_radius())) continue;
      if(animation_eligible(animation_policy::when_visible,animate,true)) events.eject_player |= advance_instance(instance,centre,tile.rotation);
      auto const requests{[&](scene_request const &request) { emit_effect(request,centre,tile.rotation); }};
      auto const origin{word_position(centre)};
      instance_context const context{
        .status{terrain.lighting.status},
        .distance{lod_distance(instance_point_distance({0,0,0},origin,tile.rotation,reference))},
        .anchor_distance{[&](vec3d const &anchor) { return lod_distance(instance_point_distance(word_position(anchor),origin,tile.rotation,reference)); }},
        .request_output{requests},
      };
      render::preview_state terrain_frame{instance,render_paused};
      auto const output{terrain_frame->execute(1,context)};
      for(auto const &draw : output.draws) {
        bool const shadow{draw.kind == part_draw_kind::ground_shadow};
        auto const shadow_placement{shadow ? place_model_shadow(projection,assembly,shadow_ground_height,instance.bounding_radius()) : std::optional<model_placement>{}};
        if(shadow && (!shadow_placement || !shadow_placement->visible)) continue;
        auto const &part{draw.part};
        if(part.name.empty()) continue;
        auto const &shape{resources.shape(part.name)};
        auto const placement{shadow ? *shadow_placement : place_model_part(projection,assembly,word_position(draw.anchor),tile.rotation,draw.placement,instance.bounding_radius())};
        if(native && !placement.visible) continue;
        auto const origin{(shadow || (native && draw.placement != part_position::assembly_origin)) ? vec3d{effect_position(placement.position,vec3<int32_t>{centre})} : centre + math::rotate_tile(draw.anchor,tile.rotation)};
        queue_part(shape,part,origin,math::rotation(part.rotation),tile.rotation,model_orientation({0,0,0},part,tile.rotation,draw.orientation),placement.camera_origin,shadow ? render::model_render_path::ground_shadow : render::model_render_path::scaled,draw.delivery);
      }
    } catch(std::exception const &error) {
      warnings.insert(grid.variant(col,row) + ": " + error.what());
    }
  }
  // Native 0xa30cc submits weapons before 0x7572c submits moving actors.
  // Equal-depth insertion and retained model-slot metadata observe this order.
  submit_queued(submission_phase::before_actors);
  for(auto const handle : actor_order) {
    auto &actor{actors.at(handle.value)};
    if(actor.visible && (!player_model || player_model->handle != handle)) submit(actor,actor.position,actor.angles);
  }
  // Gather effects after world SSD execution so newly emitted/reused slots
  // join this frame with their current state, before the global depth sort.
  if(final_submissions) final_submissions();
  submit_queued(submission_phase::after_actors);
  submissions.clear();
  if(capture) {
    if(!native) throw std::runtime_error{"flight recording requires the original model renderer"};
    // C5BEC observes the unsorted queue before C48E1 mutates its order,
    // parameter caches and scaled origins. Immediate draws may already have
    // overwritten/re-scaled a retained payload slot; preserve those writes.
    auto const order{native_queue.pending()};
    capture->data.order.assign(order.begin(),order.end());
    capture->data.models.clear();capture->model_names.clear();
    for(unsigned int slot{0}; slot != native_queue.size(); ++slot) {
      auto const &item{queue[slot]};
      auto const kind{item.path == render::model_render_path::unrotated ? recording::model_kind::unrotated : item.path == render::model_render_path::ground_shadow ? recording::model_kind::ground_shadow : recording::model_kind::rotated};
      capture->data.models.push_back({
        .kind{kind},
        .initial_state{item.initial_state},
        .colour_row{item.metrics.colour_row},
        .scale{item.rotation_scale},
        .resource{0},
        .angles{vec3<math::angle>{item.angles}},
        .camera_origin{vec3<int32_t>{item.camera_origin}},
        .parameters{item.parameters},
      });
      capture->model_names.push_back(item.name);
    }
  }
  flush_queue();
  if(!native) renderer.finish_points();
  return events;
}

} // namespace tfx::world
