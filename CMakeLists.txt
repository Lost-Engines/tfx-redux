cmake_minimum_required(VERSION 3.30)
project(tfx VERSION 0.1.0 LANGUAGES C CXX)
# No target uses C++ modules, including the isolated hardware backends.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
if(APPLE)
  enable_language(OBJC)
endif()
# Release packages carry the compiler runtime, not a MinGW/Homebrew dependency.
option(TFX_STATIC_RUNTIME "Link compiler runtimes statically" ${MINGW})
if(TFX_STATIC_RUNTIME)
  if(MINGW)
    add_link_options(-static)
  else()
    add_link_options("$<$<LINK_LANG_AND_ID:CXX,GNU>:-static-libgcc;-static-libstdc++>")
  endif()
endif()

include(CTest)
include(cmake/dependencies.cmake)
include(cmake/dbopl.cmake)
include(cmake/gf1.cmake)
include(cmake/sc55.cmake)

function(configure_first_party target)
  target_compile_features(${target} PUBLIC cxx_std_26)
  target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}")
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF CXX_SCAN_FOR_MODULES OFF)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-ffp-contract=off>
    $<$<AND:$<CONFIG:RelWithDebInfo>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-UNDEBUG>
  )
endfunction()

add_library(tfx_audio
  src/audio/cd_player.cpp src/audio/cd_player.h
  src/audio/radar_voice.cpp src/audio/radar_voice.h
  src/audio/sample_scheduler.cpp src/audio/sample_scheduler.h
  src/audio/gus_scheduler.cpp src/audio/gus_scheduler.h
  src/audio/gus_memory.cpp src/audio/gus_memory.h
  src/audio/gus_driver.cpp src/audio/gus_driver.h
  src/audio/gus_samples.cpp src/audio/gus_samples.h
  src/audio/sample_bank.cpp src/audio/sample_bank.h
  src/audio/sample_voice.cpp src/audio/sample_voice.h
  src/audio/qsound_effect.cpp src/audio/qsound_effect.h
  ${QSOUND_SOURCES}
  src/audio/runtime.cpp src/audio/runtime.h src/audio/requests.h
  src/audio/osl_resource.cpp src/audio/osl_resource.h
  src/audio/osl_channel.h src/audio/osl_device.h
  src/audio/osl_la_driver.cpp src/audio/osl_la_driver.h
  src/audio/osl_scc_driver.cpp src/audio/osl_scc_driver.h
  src/audio/sc55_synth.cpp src/audio/sc55_synth.h
  src/audio/osl_fm_driver.cpp src/audio/osl_fm_driver.h
  src/audio/fm_synth.cpp src/audio/fm_synth.h
  src/audio/osl_sequencer.cpp src/audio/osl_sequencer.h
  src/audio/midi_receiver.h src/audio/midi_byte_sink.h src/audio/midi_stream.cpp src/audio/midi_stream.h
  src/audio/la_synth.cpp src/audio/la_synth.h
  src/audio/music_catalogue.cpp src/audio/music_catalogue.h
  src/audio/music_control.cpp src/audio/music_control.h
  src/audio/music_device.cpp src/audio/music_device.h
  src/audio/music_clock.h src/audio/music_player.cpp src/audio/music_player.h
)
configure_first_party(tfx_audio)
target_include_directories(tfx_audio PRIVATE "${QSOUND_INCLUDE_DIR}")

add_library(tfx_core
  src/recording/video.cpp
  src/recording/video.h
  src/recording/file.cpp
  src/recording/file.h
  src/recording/recorder.cpp
  src/recording/recorder.h
  src/recording/playback.cpp
  src/recording/playback.h
  src/flight/cheat.cpp
  src/flight/cheat.h
  src/flight/keyboard.cpp
  src/flight/keyboard.h
  src/resources/sample_definitions.cpp
  src/render/g_effects.cpp
  src/render/g_effects.h
  src/flight/mouse_input.cpp
  src/flight/mouse_input.h
  src/flight/joystick.cpp
  src/flight/joystick.h
  src/configuration/save.cpp
  src/configuration/save.h
  src/configuration/graphics.cpp
  src/configuration/graphics.h
  src/configuration/settings.cpp
  src/configuration/settings.h
  src/flight/player_flight.cpp
  src/flight/player_flight.h
  src/flight/player_shared.cpp
  src/flight/player_shared.h
  src/flight/gear_controls.cpp src/flight/gear_controls.h
  src/flight/engine_controls.cpp src/flight/engine_controls.h
  src/flight/ground_service.cpp src/flight/ground_service.h
  src/flight/player_frame.cpp
  src/flight/player_frame.h
  src/flight/player_dispatch.cpp
  src/flight/player_dispatch.h
  src/flight/aerodynamic_launch.cpp
  src/flight/aerodynamic_launch.h
  src/flight/aerodynamic_pilot_keys.cpp
  src/flight/aerodynamic_pilot_keys.h
  src/flight/automatic_landing.cpp
  src/flight/automatic_landing.h
  src/flight/automatic_approach.cpp
  src/flight/automatic_approach.h
  src/flight/aerodynamic_approach_reference.cpp
  src/flight/aerodynamic_approach_reference.h
  src/flight/aerodynamic_recovery_trigger.cpp
  src/flight/aerodynamic_recovery_trigger.h
  src/flight/aerodynamic_autopilot.cpp
  src/flight/aerodynamic_autopilot.h
  src/flight/aerodynamic_navigation.cpp
  src/flight/aerodynamic_navigation.h
  src/flight/aerodynamic_guidance.cpp
  src/flight/aerodynamic_guidance.h
  src/flight/aerodynamic_rotation_internal.h
  src/flight/aerodynamic_recovery.cpp
  src/flight/aerodynamic_recovery.h
  src/flight/aerodynamic_step.cpp
  src/flight/aerodynamic_step.h
  src/flight/aerodynamic.cpp
  src/flight/aerodynamic.h
  src/flight/aerodynamic_wind.cpp
  src/flight/aerodynamic_wind.h
  src/flight/aerodynamic_start.cpp
  src/flight/aerodynamic_start.h
  src/flight/aerodynamic_commands.cpp
  src/flight/aerodynamic_commands.h
  src/flight/aerodynamic_realistic_ground.cpp
  src/flight/aerodynamic_realistic_ground.h
  src/flight/aerodynamic_ground_internal.h
  src/flight/aerodynamic_ground.cpp
  src/flight/aerodynamic_ground.h
  src/flight/aerodynamic_contact.cpp
  src/flight/aerodynamic_contact.h
  src/flight/aerodynamic_pose.cpp
  src/flight/aerodynamic_pose.h
  src/flight/aerodynamic_airframes.h
  src/flight/aerodynamic_control_law_internal.h
  src/flight/aerodynamic_manual.cpp
  src/flight/aerodynamic_manual.h
  src/flight/aerodynamic_speed.cpp
  src/flight/aerodynamic_speed.h
  src/flight/aerodynamic_controls.cpp
  src/flight/aerodynamic_controls.h
  src/flight/control_input.h
  src/flight/aerodynamic_surfaces.cpp
  src/flight/aerodynamic_surfaces.h
  src/flight/aerodynamic_powerplant.cpp
  src/flight/aerodynamic_powerplant.h
  src/flight/aerodynamic_coefficients.cpp
  src/flight/aerodynamic_coefficients.h
  src/flight/aerodynamic_airdata.cpp
  src/flight/aerodynamic_airdata.h
  src/flight/aerodynamic_response.cpp
  src/flight/aerodynamic_response.h
  src/flight/aerodynamic_forces.cpp
  src/flight/aerodynamic_forces.h
  src/flight/aerodynamic_motion.cpp
  src/flight/aerodynamic_motion.h
  src/flight/atmosphere.cpp
  src/flight/atmosphere.h
  src/flight/simple.cpp
  src/flight/simple.h
  src/flight/basic_parameters.h
  src/flight/landing_gear.cpp
  src/flight/landing_gear.h
  src/flight/simple_ground.cpp
  src/flight/simple_ground.h
  src/flight/simple_turn.cpp
  src/flight/simple_turn.h
  src/flight/simple_controls.cpp
  src/flight/simple_controls.h
  src/missions/recovery.cpp
  src/missions/recovery.h
  src/flight/ejection_sequence.cpp
  src/flight/ejection_sequence.h
  src/flight/ejection.cpp
  src/flight/ejection.h
  src/flight/landing.cpp
  src/flight/landing.h
  src/cockpit/landing_display.cpp
  src/cockpit/landing_display.h
  src/flight/refuelling.cpp
  src/flight/refuelling.h
  src/cockpit/hud_alerts.cpp
  src/cockpit/hud_alerts.h
  src/render/palette_lighting.cpp
  src/render/glare.cpp
  src/render/palette_lighting.h
  src/render/palette_selection.cpp
  src/render/palette_selection.h
  src/cockpit/external_display.cpp
  src/cockpit/external_display.h
  src/render/point_projection.cpp
  src/render/point_projection.h
  src/cockpit/view_camera.cpp
  src/cockpit/view_camera.h
  src/cockpit/mouse_orbit.cpp
  src/cockpit/mouse_orbit.h
  src/cockpit/view_control.cpp
  src/cockpit/view_control.h
  src/cockpit/tactical_map.cpp
  src/cockpit/tactical_map.h
  src/cockpit/target_display.cpp
  src/cockpit/target_display.h
  src/cockpit/format.h
  src/cockpit/radio_ticker.cpp
  src/cockpit/radio_ticker.h
  src/weapons/impact_prediction.cpp
  src/weapons/impact_prediction.h
  src/cockpit/weapon_display.cpp
  src/cockpit/weapon_display.h
  src/weapons/cannon_prediction.cpp
  src/weapons/cannon_prediction.h
  src/cockpit/attitude_display.cpp
  src/cockpit/attitude_display.h
  src/cockpit/hud_text.cpp
  src/cockpit/hud_text.h
  src/cockpit/frame.cpp
  src/cockpit/frame.h
  src/cockpit/viewport_layout.h
  src/cockpit/initialisation.cpp
  src/cockpit/initialisation.h
  src/flight/fuel_gauge.h
  src/cockpit/designator.cpp src/cockpit/designator.h
  src/cockpit/designator_mouse.cpp src/cockpit/designator_mouse.h
  src/cockpit/scene_display.cpp
  src/cockpit/scene_display.h
  src/cockpit/virtual_displays.cpp src/cockpit/virtual_displays.h
  src/cockpit/hud.cpp
  src/cockpit/hud.h
  src/cockpit/control_state.h
  src/cockpit/radar.cpp
  src/cockpit/radar.h
  src/cockpit/moving_map.cpp
  src/cockpit/moving_map.h
  src/cockpit/auxiliary.cpp
  src/cockpit/auxiliary.h
  src/cockpit/warnings.cpp
  src/cockpit/warnings.h
  src/cockpit/mfd.cpp
  src/cockpit/mfd.h
  src/cockpit/instrument_assets.cpp
  src/cockpit/instrument_assets.h
  src/cockpit/readouts.cpp
  src/cockpit/readouts.h
  src/presentation/drawing.cpp
  src/presentation/drawing.h
  src/planning/review.cpp
  src/planning/review.h
  src/planning/notes.cpp
  src/planning/notes.h
  src/planning/map_view.cpp
  src/planning/map_view.h
  src/presentation/graphics_menu.cpp
  src/presentation/graphics_menu.h
  src/presentation/configuration_menu.cpp
  src/presentation/configuration_menu.h
  src/presentation/dialogue.cpp
  src/presentation/dialogue.h
  src/presentation/widget_font.cpp
  src/presentation/widget_font.h
  src/resources/compiled_overlay.cpp
  src/resources/compiled_overlay.h
  src/resources/compiled_panel.cpp
  src/resources/compiled_panel.h
  src/planning/document.cpp
  src/planning/document.h
  src/presentation/score_player.cpp
  src/presentation/score_player.h
  src/presentation/scores.cpp
  src/presentation/scores.h
  src/presentation/arming_preview.cpp
  src/presentation/arming_preview.h
  src/presentation/arming_screen.cpp
  src/presentation/arming_screen.h
  src/presentation/arming_widgets.cpp
  src/presentation/arming_widgets.h
  src/presentation/arming_controller.cpp
  src/presentation/arming_controller.h
  src/presentation/arming_assets.cpp
  src/presentation/arming_assets.h
  src/presentation/frontend.cpp
  src/presentation/frontend.h
  src/presentation/pilot_panel.cpp
  src/presentation/pilot_panel.h
  src/presentation/pilots.cpp
  src/presentation/pilots.h
  src/presentation/save.cpp
  src/presentation/save.h
  src/presentation/menu_player.cpp
  src/presentation/menu_player.h
  src/presentation/cursor.cpp
  src/presentation/cursor.h
  src/presentation/menu.cpp
  src/presentation/menu.h
  src/presentation/session.cpp
  src/presentation/session.h
  src/presentation/player.cpp
  src/presentation/player.h
  src/presentation/timing.cpp
  src/presentation/timing.h
  src/presentation/text.cpp
  src/presentation/text.h
  src/presentation/fonts.cpp
  src/presentation/fonts.h
  src/presentation/program.cpp
  src/presentation/program.h
  src/presentation/runtime.cpp
  src/presentation/runtime.h
  src/resources/media.cpp
  src/resources/media.h
  src/actors/surface_combat.cpp
  src/actors/surface_combat.h
  src/sensors/ground.cpp
  src/sensors/ground.h
  src/actors/visibility.cpp
  src/actors/visibility.h
  src/actors/contact_region.cpp
  src/actors/contact_region.h
  src/actors/scenery.cpp
  src/actors/scenery.h
  src/missions/engagement.cpp src/missions/engagement.h
  src/missions/progress.cpp
  src/missions/progress.h
  src/missions/target_binding.cpp
  src/missions/target_binding.h
  src/flight/defence.cpp src/flight/defence.h
  src/flight/damage.cpp
  src/flight/damage.h
  src/sensors/air.cpp
  src/sensors/air.h
  src/weapons/loadout.cpp
  src/weapons/loadout.h
  src/weapons/cannon.cpp
  src/weapons/cannon.h
  src/weapons/runtime.cpp
  src/weapons/runtime.h
  src/weapons/warnings.cpp src/weapons/warnings.h
  src/weapons/projectile.cpp
  src/weapons/projectile.h
  src/weapons/definition.cpp
  src/weapons/definition.h
  src/actors/manoeuvre.cpp
  src/actors/manoeuvre.h
  src/actors/air.cpp
  src/actors/air.h
  src/actors/air_motion.cpp
  src/actors/air_motion.h
  src/actors/program.cpp
  src/actors/program.h
  src/actors/surface.cpp
  src/actors/surface.h
  src/flight/arcade.cpp
  src/flight/arcade.h
  src/math/attitude.cpp
  src/math/attitude.h
  src/flight/aircraft.h
  src/flight/airframes.cpp
  src/flight/airframes.h
  src/flight/stores.cpp
  src/flight/stores.h
  src/missions/timeline.cpp
  src/missions/timeline.h
  src/missions/rules.cpp
  src/missions/rules.h
  src/missions/initialisation.cpp
  src/missions/initialisation.h
  src/simulation/timing.cpp
  src/simulation/timing.h
  src/simulation/acceleration.h
  src/resources/executable_tables.cpp
  src/resources/executable_tables.h
  src/cockpit/vector_hud.cpp
  src/cockpit/vector_hud.h
  src/render/framebuffer.cpp
  src/render/framebuffer.h
  src/render/display_pages.cpp src/render/display_pages.h
  src/render/ilbm.cpp
  src/render/ilbm.h
  src/render/aperture.h
  src/render/camera.cpp
  src/render/camera.h
  src/render/view_settings.h
  src/render/markers.cpp
  src/render/markers.h
  src/render/clipping.h
  src/render/sprites.cpp
  src/render/sprites.h
  src/render/precipitation.cpp
  src/render/precipitation.h
  src/render/horizon.cpp src/render/horizon.h
  src/render/model_queue.cpp
  src/render/model_queue.h
  src/render/model_point.cpp
  src/render/model_point.h
  src/render/model_jitter.cpp src/render/model_jitter.h
  src/render/rounded_model_polygon.cpp src/render/rounded_model_polygon.h
  src/render/integer_texture_polygon.h
  src/render/integer_model_texture.cpp src/render/integer_model_texture.h
  src/render/model_polygon.cpp src/render/model_polygon.h src/render/rounded_curve.h
  src/render/model_raster.cpp src/render/model_raster.h
  src/render/model_projection.cpp src/render/model_projection.h
  src/render/model_transform.cpp src/render/model_transform.h
  src/world/model_orientation.h
  src/render/model_metrics.cpp
  src/render/model_metrics.h
  src/render/colour_remap.cpp
  src/render/colour_remap.h
  src/render/near_clip.cpp
  src/render/near_clip.h
  src/render/integer_model_polygon.cpp
  src/render/integer_model_polygon.h
  src/render/integer_polygon.cpp src/render/integer_polygon.h
  src/render/pattern_raster.cpp src/render/pattern_raster.h
  src/render/point_visibility.cpp
  src/render/point_size.h
  src/render/point_visibility.h
  src/render/preview_state.h
  src/render/software_renderer.cpp
  src/render/software_renderer.h
  src/render/texture_overlay.h
  src/math/native.cpp
  src/math/native.h
  src/models/shape.cpp
  src/models/shape.h
  src/world/instance.cpp
  src/world/instance.h
  src/world/impact.cpp src/world/impact.h
  src/world/effects.cpp
  src/world/effects.h
  src/world/trails.cpp src/world/trails.h
  src/world/sky.cpp src/world/sky.h
  src/world/flak.cpp src/world/flak.h
  src/world/collision.cpp
  src/world/collision.h
  src/world/model_placement.cpp
  src/world/model_placement.h
  src/world/animation.cpp
  src/world/animation.h
  src/world/scene.cpp
  src/world/scene.h
  src/world/environment.cpp
  src/world/environment.h
  src/world/definition.cpp
  src/world/definition.h
  src/world/assets.cpp
  src/world/assets.h
  src/resources/cd_image.cpp src/resources/cd_image.h
  src/resources/archive.cpp
  src/resources/archive.h
  src/resources/installation.cpp
  src/resources/installation.h
  src/resources/binary.h
  src/resources/ra.cpp
  src/resources/ra.h
)
target_compile_definitions(tfx_core PUBLIC VECTORSTORM_NO_BOOST)
configure_first_party(tfx_core)

target_link_libraries(tfx_audio PUBLIC tfx_core PRIVATE MT32Emu::mt32emu PRIVATE dosbox_opl_core PRIVATE sc55_backend PRIVATE gf1_backend)

if(APPLE)
  set_source_files_properties(src/platform/miniaudio_implementation.c PROPERTIES LANGUAGE OBJC)
endif()
add_library(pcm_device STATIC src/platform/miniaudio_implementation.c)
target_include_directories(pcm_device SYSTEM PUBLIC "${miniaudio_SOURCE_DIR}")
target_compile_definitions(pcm_device PUBLIC MA_NO_ENGINE MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH MA_NO_ENCODING MA_NO_GENERATION MA_NO_NULL)
target_link_libraries(pcm_device PRIVATE Threads::Threads PRIVATE ${CMAKE_DL_LIBS})
target_link_libraries(tfx_audio PUBLIC pcm_device)
if(UNIX)
  target_link_libraries(pcm_device PRIVATE m)
endif()

add_executable(tfx
  src/platform/audio_output.cpp
  src/platform/audio_output.h
  src/main.cpp
  src/platform/tactical_map.cpp
  src/platform/tactical_map.h
  src/platform/screenshot.cpp
  src/platform/screenshot.h
  src/platform/configuration.cpp
  src/platform/configuration.h
  src/platform/keyboard.cpp
  src/platform/keyboard.h
  src/platform/arming.cpp
  src/platform/arming.h
  src/platform/capture_scene.cpp
  src/platform/capture_scene.h
  src/platform/flight.cpp
  src/platform/flight.h
  src/platform/presentations.cpp
  src/platform/presentations.h
  src/platform/recording.cpp
  src/platform/recording.h
  src/platform/window.cpp
  src/platform/window.h
)
target_link_libraries(tfx PRIVATE tfx_audio PRIVATE pcm_device PRIVATE tfx_core PRIVATE glfw PRIVATE OpenGL::GL)
configure_first_party(tfx)
include(cmake/install.cmake)

if(BUILD_TESTING)
  add_executable(capture_scene_tests tests/capture_scene_test.cpp src/platform/capture_scene.cpp)
  target_link_libraries(capture_scene_tests PRIVATE tfx_core)
  configure_first_party(capture_scene_tests)
  add_test(NAME redux.capture_scene COMMAND capture_scene_tests)

  add_executable(graphics_tests tests/graphics_test.cpp)
  target_link_libraries(graphics_tests PRIVATE tfx_core)
  configure_first_party(graphics_tests)
  add_test(NAME configuration.redux_graphics COMMAND graphics_tests)
  add_executable(mouse_flight_input_tests tests/mouse_flight_input_test.cpp)
  target_link_libraries(mouse_flight_input_tests PRIVATE tfx_core)
  configure_first_party(mouse_flight_input_tests)
  add_test(NAME flight.redux_mouse_bounds COMMAND mouse_flight_input_tests)
  add_executable(designator_mouse_tests tests/designator_mouse_test.cpp)
  target_link_libraries(designator_mouse_tests PRIVATE tfx_core)
  configure_first_party(designator_mouse_tests)
  add_test(NAME render.redux_designator_mouse COMMAND designator_mouse_tests)
  add_executable(mouse_orbit_tests tests/mouse_orbit_test.cpp)
  target_link_libraries(mouse_orbit_tests PRIVATE tfx_core)
  configure_first_party(mouse_orbit_tests)
  add_test(NAME render.redux_mouse_orbit COMMAND mouse_orbit_tests)
  add_executable(redux_video_tests tests/redux_video_test.cpp)
  target_link_libraries(redux_video_tests PRIVATE tfx_core)
  configure_first_party(redux_video_tests)
  add_test(NAME recording.redux_lossless_video COMMAND redux_video_tests)
  add_executable(virtual_cockpit_probe tests/virtual_cockpit_probe.cpp)
  target_link_libraries(virtual_cockpit_probe PRIVATE tfx_core)
  configure_first_party(virtual_cockpit_probe)
  add_executable(redux_hud_probe tests/redux_hud_probe.cpp)
  target_link_libraries(redux_hud_probe PRIVATE tfx_core)
  configure_first_party(redux_hud_probe)
  add_executable(redux_effects_probe tests/redux_effects_probe.cpp)
  target_link_libraries(redux_effects_probe PRIVATE tfx_core)
  configure_first_party(redux_effects_probe)
  add_executable(redux_render_benchmark tests/redux_render_benchmark.cpp)
  target_link_libraries(redux_render_benchmark PRIVATE tfx_core)
  configure_first_party(redux_render_benchmark)
  add_executable(redux_view_test tests/redux_view_test.cpp)
  target_link_libraries(redux_view_test PRIVATE tfx_core)
  configure_first_party(redux_view_test)
  add_test(NAME render.redux_resolution_fov_and_ui COMMAND redux_view_test)
  add_executable(redux_arming_probe tests/redux_arming_probe.cpp)
  target_link_libraries(redux_arming_probe PRIVATE tfx_core)
  configure_first_party(redux_arming_probe)
  add_executable(campaign_journey_probe tests/campaign_journey_probe.cpp)
  target_link_libraries(campaign_journey_probe PRIVATE tfx_core)
  configure_first_party(campaign_journey_probe)
  add_executable(fm_synth_test tests/fm_synth_test.cpp)
  target_link_libraries(fm_synth_test PRIVATE tfx_audio)
  configure_first_party(fm_synth_test)
  add_test(NAME audio.fm_initialization_and_pcm COMMAND fm_synth_test)
  add_executable(cd_probe tests/cd_probe.cpp)
  target_link_libraries(cd_probe PRIVATE tfx_audio)
  configure_first_party(cd_probe)
  add_test(NAME audio.cd_image_and_local_streaming COMMAND cd_probe self-test)
  add_executable(music_render tools/render_music.cpp)
  target_link_libraries(music_render PRIVATE tfx_audio)
  configure_first_party(music_render)
  add_executable(installation_tests tests/installation_test.cpp)
  target_link_libraries(installation_tests PRIVATE tfx_core)
  configure_first_party(installation_tests)
  add_test(NAME resources.automatic_installation_and_rom_paths COMMAND installation_tests)
  add_library(gf1_reference SHARED tests/gf1_reference_bridge.cpp)
  target_link_libraries(gf1_reference PRIVATE gf1_backend)
  configure_first_party(gf1_reference)
  set_target_properties(gf1_reference PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  add_executable(pause_input_tests tests/pause_input_test.cpp)
  target_link_libraries(pause_input_tests PRIVATE tfx_core glfw)
  configure_first_party(pause_input_tests)
  add_test(NAME platform.pause_resume_input COMMAND pause_input_tests)
  add_executable(recording_tests tests/recording_test.cpp)
  target_link_libraries(recording_tests PRIVATE tfx_core)
  configure_first_party(recording_tests)
  add_test(NAME recording.original_file_format COMMAND recording_tests)
  add_executable(recording_stream_tests tests/recording_stream_test.cpp)
  target_link_libraries(recording_stream_tests PRIVATE tfx_core)
  configure_first_party(recording_stream_tests)
  add_test(NAME recording.streaming_capture_playback COMMAND recording_stream_tests)
  add_executable(recording_probe tests/recording_probe.cpp)
  target_link_libraries(recording_probe PRIVATE tfx_core)
  configure_first_party(recording_probe)
  add_executable(recording_playback_probe tests/recording_playback_probe.cpp)
  target_link_libraries(recording_playback_probe PRIVATE tfx_core)
  configure_first_party(recording_playback_probe)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i.86)$" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_library(native_x87_backend SHARED tools/native_x87_backend.cpp)
    configure_first_party(native_x87_backend)
    # Loaded by the Python capture process, which does not host ASan's runtime.
    target_compile_options(native_x87_backend PRIVATE -fno-sanitize=all)
    target_link_options(native_x87_backend PRIVATE -fno-sanitize=all)
  endif()
  add_executable(floating_environment_tests tests/floating_environment_test.cpp)
  configure_first_party(floating_environment_tests)
  add_test(NAME math.native_fpu_precision_and_restoration COMMAND floating_environment_tests)
  set_tests_properties(math.native_fpu_precision_and_restoration PROPERTIES SKIP_RETURN_CODE 77)

  add_executable(hud_font_probe tests/hud_font_probe.cpp)
  target_link_libraries(hud_font_probe PRIVATE tfx_core)
  configure_first_party(hud_font_probe)

  add_executable(screenshot_probe tests/screenshot_probe.cpp)
  target_link_libraries(screenshot_probe PRIVATE tfx_core)
  configure_first_party(screenshot_probe)

  add_executable(screenshot_tests tests/screenshot_test.cpp)
  target_link_libraries(screenshot_tests PRIVATE tfx_core)
  configure_first_party(screenshot_tests)
  add_test(NAME render.ilbm_capture COMMAND screenshot_tests)

  add_executable(cheat_probe tests/cheat_probe.cpp)
  target_link_libraries(cheat_probe PRIVATE tfx_core)
  configure_first_party(cheat_probe)

  add_executable(cheat_tests tests/cheat_test.cpp)
  target_link_libraries(cheat_tests PRIVATE tfx_core)
  configure_first_party(cheat_tests)
  add_test(NAME flight.original_cheat_dispatch COMMAND cheat_tests)

  add_executable(keyboard_probe tests/keyboard_probe.cpp)
  target_link_libraries(keyboard_probe PRIVATE tfx_core)
  configure_first_party(keyboard_probe)

  add_executable(keyboard_tests tests/keyboard_test.cpp)
  target_link_libraries(keyboard_tests PRIVATE tfx_core)
  configure_first_party(keyboard_tests)
  add_test(NAME flight.original_held_keyboard COMMAND keyboard_tests)

  add_executable(current_stores_tests tests/current_stores_test.cpp)
  target_link_libraries(current_stores_tests PRIVATE tfx_core)
  configure_first_party(current_stores_tests)
  add_test(NAME weapons.current_inventory_flight_load COMMAND current_stores_tests)

  add_executable(widget_font_probe tests/widget_font_probe.cpp)
  target_link_libraries(widget_font_probe PRIVATE tfx_core)
  configure_first_party(widget_font_probe)

  add_executable(planning_tests tests/planning_test.cpp)
  target_link_libraries(planning_tests PRIVATE tfx_core)
  configure_first_party(planning_tests)
  add_test(NAME planning.native_document_layout_and_bounds COMMAND planning_tests)

  add_executable(planning_probe tests/planning_probe.cpp)
  target_link_libraries(planning_probe PRIVATE tfx_core)
  configure_first_party(planning_probe)

  add_executable(score_probe tests/score_probe.cpp)
  target_link_libraries(score_probe PRIVATE tfx_core)
  configure_first_party(score_probe)

  add_executable(score_tests tests/score_test.cpp)
  target_link_libraries(score_tests PRIVATE tfx_core)
  configure_first_party(score_tests)
  add_test(NAME presentation.native_high_score_storage COMMAND score_tests)

  add_executable(pilot_probe tests/pilot_probe.cpp)
  target_link_libraries(pilot_probe PRIVATE tfx_core)
  configure_first_party(pilot_probe)

  add_executable(pilot_panel_probe tests/pilot_panel_probe.cpp)
  target_link_libraries(pilot_panel_probe PRIVATE tfx_core)
  configure_first_party(pilot_panel_probe)

  add_executable(name_probe tests/name_probe.cpp)
  target_link_libraries(name_probe PRIVATE tfx_core)
  configure_first_party(name_probe)

  add_executable(save_tests tests/save_test.cpp)
  target_link_libraries(save_tests PRIVATE tfx_core)
  configure_first_party(save_tests)
  add_test(NAME presentation.native_save_layout_and_bounds COMMAND save_tests)

  add_executable(save_probe tests/save_probe.cpp)
  target_link_libraries(save_probe PRIVATE tfx_core)
  configure_first_party(save_probe)

  add_executable(cursor_probe tests/cursor_probe.cpp)
  target_link_libraries(cursor_probe PRIVATE tfx_core)
  configure_first_party(cursor_probe)

  add_executable(menu_probe tests/menu_probe.cpp)
  target_link_libraries(menu_probe PRIVATE tfx_core)
  configure_first_party(menu_probe)

  add_executable(menu_tests tests/menu_test.cpp)
  target_link_libraries(menu_tests PRIVATE tfx_core)
  configure_first_party(menu_tests)
  add_test(NAME presentation.menu_scripts_and_clicks COMMAND menu_tests)

  add_executable(player_tests tests/player_test.cpp)
  target_link_libraries(player_tests PRIVATE tfx_core)
  configure_first_party(player_tests)
  add_test(NAME presentation.scene_phases_and_caption_input COMMAND player_tests)

  add_executable(player_probe tests/player_probe.cpp)
  target_link_libraries(player_probe PRIVATE tfx_core)
  configure_first_party(player_probe)

  add_executable(presentation_timing_probe tests/presentation_timing_probe.cpp)
  target_link_libraries(presentation_timing_probe PRIVATE tfx_core)
  configure_first_party(presentation_timing_probe)
  add_test(NAME presentation.native_input_waits_animation_fades COMMAND presentation_timing_probe ${PROJECT_SOURCE_DIR}/tests/reference/presentation_timing.txt)

  add_executable(text_clock_probe tests/text_clock_probe.cpp)
  target_link_libraries(text_clock_probe PRIVATE tfx_core)
  configure_first_party(text_clock_probe)
  add_test(NAME presentation.native_reveal_clock COMMAND text_clock_probe ${PROJECT_SOURCE_DIR}/tests/reference/text_clock.txt)

  add_executable(text_tests tests/text_test.cpp)
  target_link_libraries(text_tests PRIVATE tfx_core)
  configure_first_party(text_tests)
  add_test(NAME presentation.native_text_boundaries COMMAND text_tests)

  add_executable(text_probe tests/text_probe.cpp)
  target_link_libraries(text_probe PRIVATE tfx_core)
  configure_first_party(text_probe)

  add_executable(font_tests tests/font_test.cpp)
  target_link_libraries(font_tests PRIVATE tfx_core)
  configure_first_party(font_tests)
  add_test(NAME presentation.font_tables_and_coverage COMMAND font_tests)

  add_executable(font_probe tests/font_probe.cpp)
  target_link_libraries(font_probe PRIVATE tfx_core)
  configure_first_party(font_probe)

  add_executable(presentation_tests tests/presentation_test.cpp)
  target_link_libraries(presentation_tests PRIVATE tfx_core)
  configure_first_party(presentation_tests)
  add_test(NAME presentation.control_flow_and_boundaries COMMAND presentation_tests)

  add_executable(presentation_probe tests/presentation_probe.cpp)
  target_link_libraries(presentation_probe PRIVATE tfx_core)
  configure_first_party(presentation_probe)

  add_executable(media_tests tests/media_test.cpp)
  target_link_libraries(media_tests PRIVATE tfx_core)
  configure_first_party(media_tests)
  add_test(NAME resources.native_media_boundaries COMMAND media_tests)

  add_executable(media_probe tests/media_probe.cpp)
  target_link_libraries(media_probe PRIVATE tfx_core)
  configure_first_party(media_probe)

  add_executable(surface_combat_probe tests/surface_combat_probe.cpp)
  target_link_libraries(surface_combat_probe PRIVATE tfx_core)
  configure_first_party(surface_combat_probe)
  add_test(NAME actors.native_surface_combat COMMAND surface_combat_probe ${PROJECT_SOURCE_DIR}/tests/reference/surface_combat.txt)

  add_executable(ground_probe tests/ground_probe.cpp)
  target_link_libraries(ground_probe PRIVATE tfx_core)
  configure_first_party(ground_probe)
  add_test(NAME sensors.native_ground_geometry_and_selection COMMAND ground_probe ${PROJECT_SOURCE_DIR}/tests/reference/ground_sensors.txt)
  add_test(NAME sensors.native_cruise_target COMMAND ground_probe ${PROJECT_SOURCE_DIR}/tests/reference/cruise_target.txt)

  add_executable(world_contacts_probe tests/world_contacts_probe.cpp)
  target_link_libraries(world_contacts_probe PRIVATE tfx_core)
  configure_first_party(world_contacts_probe)
  add_test(NAME world.native_fixed_placements_and_surface_region COMMAND world_contacts_probe ${PROJECT_SOURCE_DIR}/tests/reference/world_contacts.txt)

  add_executable(defence_probe tests/defence_probe.cpp)
  target_link_libraries(defence_probe PRIVATE tfx_core)
  configure_first_party(defence_probe)
  add_test(NAME flight.native_ecm_stealth_and_countermeasure_keys COMMAND defence_probe ${PROJECT_SOURCE_DIR}/tests/reference/defence.txt)

  add_executable(texture_near_probe tests/texture_near_probe.cpp)
  target_link_libraries(texture_near_probe PRIVATE tfx_core)
  configure_first_party(texture_near_probe)
  add_test(NAME render.native_texture_near_intersection COMMAND texture_near_probe ${PROJECT_SOURCE_DIR}/tests/reference/texture_near.txt)

  add_executable(model_texture_probe tests/model_texture_probe.cpp)
  target_link_libraries(model_texture_probe PRIVATE tfx_core)
  configure_first_party(model_texture_probe)
  add_test(NAME render.native_model_textures COMMAND model_texture_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_textures.txt)
  add_test(NAME render.native_near_model_textures COMMAND model_texture_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_textures_near.txt)

  add_executable(texture_polygon_probe tests/texture_polygon_probe.cpp)
  target_link_libraries(texture_polygon_probe PRIVATE tfx_core)
  configure_first_party(texture_polygon_probe)
  add_test(NAME render.native_texture_polygons COMMAND texture_polygon_probe ${PROJECT_SOURCE_DIR}/tests/reference/texture_polygons.txt)
  add_test(NAME render.native_crossed_texture_polygons COMMAND texture_polygon_probe ${PROJECT_SOURCE_DIR}/tests/reference/texture_polygons_crossed.txt)

  add_executable(pattern_span_probe tests/pattern_span_probe.cpp)
  target_link_libraries(pattern_span_probe PRIVATE tfx_core)
  configure_first_party(pattern_span_probe)
  add_test(NAME render.native_pattern_span_consumers COMMAND pattern_span_probe ${PROJECT_SOURCE_DIR}/tests/reference/pattern_spans.txt)

  add_executable(model_frame_probe tests/model_frame_probe.cpp)
  target_link_libraries(model_frame_probe PRIVATE tfx_core)
  configure_first_party(model_frame_probe)
  add_test(NAME render.native_model_materials COMMAND model_frame_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_materials.txt)
  add_test(NAME render.native_far_model_faces COMMAND model_frame_probe "${PROJECT_SOURCE_DIR}/tests/reference/model_far_materials.txt")
  add_test(NAME render.native_inert_model_instructions COMMAND model_frame_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_inert_ops.txt)
  add_test(NAME render.native_rounded_model_programs COMMAND model_frame_probe ${PROJECT_SOURCE_DIR}/tests/reference/rounded_model_programs.txt)
  add_test(NAME render.native_authored_model_lines COMMAND model_frame_probe --viewport ${PROJECT_SOURCE_DIR}/tests/reference/model_lines.txt)
  add_test(NAME render.native_authored_model_dots COMMAND model_frame_probe --viewport ${PROJECT_SOURCE_DIR}/tests/reference/model_dots.txt)
  add_test(NAME render.native_authored_model_discs COMMAND model_frame_probe --viewport ${PROJECT_SOURCE_DIR}/tests/reference/model_discs.txt)

  add_executable(model_depth_palette_probe tests/model_depth_palette_probe.cpp)
  target_link_libraries(model_depth_palette_probe PRIVATE tfx_core)
  configure_first_party(model_depth_palette_probe)
  add_test(NAME render.native_model_depth_palettes COMMAND model_depth_palette_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_depth_palettes.txt)

  add_executable(model_palette_probe tests/model_palette_probe.cpp)
  target_link_libraries(model_palette_probe PRIVATE tfx_core)
  configure_first_party(model_palette_probe)
  add_test(NAME render.native_model_palette_switches COMMAND model_palette_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_palettes.txt)

  add_executable(rounded_model_probe tests/rounded_model_probe.cpp)
  target_link_libraries(rounded_model_probe PRIVATE tfx_core)
  configure_first_party(rounded_model_probe)
  add_test(NAME render.native_rounded_model_polygons COMMAND rounded_model_probe ${PROJECT_SOURCE_DIR}/tests/reference/rounded_models.txt)

  add_executable(model_articulation_probe tests/model_articulation_probe.cpp)
  target_link_libraries(model_articulation_probe PRIVATE tfx_core)
  configure_first_party(model_articulation_probe)
  add_test(NAME render.native_model_articulation COMMAND model_articulation_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_articulation.txt)

  add_executable(model_jitter_probe tests/model_jitter_probe.cpp)
  target_link_libraries(model_jitter_probe PRIVATE tfx_core)
  configure_first_party(model_jitter_probe)
  add_test(NAME render.native_model_jitter_frames COMMAND model_jitter_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_jitter_frames.txt)
  add_executable(animation_policy_probe tests/animation_policy_probe.cpp)
  target_link_libraries(animation_policy_probe PRIVATE tfx_core)
  configure_first_party(animation_policy_probe)
  add_test(NAME world.native_animation_eligibility COMMAND animation_policy_probe ${PROJECT_SOURCE_DIR}/tests/reference/animation_policy.txt)
  add_executable(model_delivery_probe tests/model_delivery_probe.cpp)
  target_link_libraries(model_delivery_probe PRIVATE tfx_core)
  configure_first_party(model_delivery_probe)
  add_test(NAME render.native_model_delivery COMMAND model_delivery_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_delivery.txt)
  add_executable(model_queue_probe tests/model_queue_probe.cpp)
  target_link_libraries(model_queue_probe PRIVATE tfx_core)
  configure_first_party(model_queue_probe)
  add_test(NAME render.native_model_queue COMMAND model_queue_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_queue.txt)

  add_executable(model_lod_probe tests/model_lod_probe.cpp)
  target_link_libraries(model_lod_probe PRIVATE tfx_core)
  configure_first_party(model_lod_probe)
  add_test(NAME render.consistent_complete_detail_models COMMAND model_lod_probe ${PROJECT_SOURCE_DIR}/tests/reference/redux_model_lod.txt)

  add_executable(model_placement_probe tests/model_placement_probe.cpp)
  target_link_libraries(model_placement_probe PRIVATE tfx_core)
  configure_first_party(model_placement_probe)
  add_test(NAME render.native_model_placements COMMAND model_placement_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_placements.txt)
  add_executable(model_point_probe tests/model_point_probe.cpp)
  target_link_libraries(model_point_probe PRIVATE tfx_core)
  configure_first_party(model_point_probe)
  add_test(NAME render.consistent_distant_points COMMAND model_point_probe ${PROJECT_SOURCE_DIR}/tests/reference/redux_model_points.txt)

  add_executable(model_metrics_probe tests/model_metrics_probe.cpp)
  target_link_libraries(model_metrics_probe PRIVATE tfx_core)
  configure_first_party(model_metrics_probe)
  add_test(NAME render.native_model_distance_and_details COMMAND model_metrics_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_metrics.txt)

  add_executable(gouraud_polygon_probe tests/gouraud_polygon_probe.cpp)
  target_link_libraries(gouraud_polygon_probe PRIVATE tfx_core)
  configure_first_party(gouraud_polygon_probe)
  add_test(NAME render.native_gouraud_polygons COMMAND gouraud_polygon_probe ${PROJECT_SOURCE_DIR}/tests/reference/gouraud_polygons.txt)
  add_test(NAME render.native_thin_gouraud_polygons COMMAND gouraud_polygon_probe ${PROJECT_SOURCE_DIR}/tests/reference/gouraud_polygons_thin.txt)

  add_executable(remap_renderer_probe tests/remap_renderer_probe.cpp)
  target_link_libraries(remap_renderer_probe PRIVATE tfx_core)
  configure_first_party(remap_renderer_probe)
  add_executable(colour_remap_probe tests/colour_remap_probe.cpp)
  target_link_libraries(colour_remap_probe PRIVATE tfx_core)
  configure_first_party(colour_remap_probe)
  add_test(NAME render.native_placed_colour_rows COMMAND colour_remap_probe --placement ${PROJECT_SOURCE_DIR}/tests/reference/placed_colour_rows.txt)
  add_test(NAME render.native_distance_colour_row COMMAND colour_remap_probe ${PROJECT_SOURCE_DIR}/tests/reference/colour_rows.txt)
  add_executable(model_program_probe tests/model_program_probe.cpp)
  target_link_libraries(model_program_probe PRIVATE tfx_core)
  configure_first_party(model_program_probe)
  add_test(NAME render.native_gouraud_model_programs COMMAND model_program_probe --shaded ${PROJECT_SOURCE_DIR}/tests/reference/gouraud_model_programs.txt)
  add_test(NAME render.native_model_programs COMMAND model_program_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_programs.txt)
  add_test(NAME render.native_model_program_overflow COMMAND model_program_probe --native-camera ${PROJECT_SOURCE_DIR}/tests/reference/model_program_overflow.txt)
  add_test(NAME render.native_gouraud_model_program_overflow COMMAND model_program_probe --native-camera --shaded ${PROJECT_SOURCE_DIR}/tests/reference/gouraud_model_program_overflow.txt)
  add_executable(model_facing_probe tests/model_facing_probe.cpp)
  target_link_libraries(model_facing_probe PRIVATE tfx_core)
  configure_first_party(model_facing_probe)
  add_test(NAME render.native_model_facing COMMAND model_facing_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_facing.txt)
  add_executable(near_clip_probe tests/near_clip_probe.cpp)
  target_link_libraries(near_clip_probe PRIVATE tfx_core)
  configure_first_party(near_clip_probe)
  add_test(NAME render.native_near_plane_edges COMMAND near_clip_probe ${PROJECT_SOURCE_DIR}/tests/reference/near_clip.txt)
  add_test(NAME render.native_near_model_polygons COMMAND world_polygon_probe ${PROJECT_SOURCE_DIR}/tests/reference/world_polygons_near.txt)
  add_test(NAME render.native_near_gouraud COMMAND world_polygon_probe --shaded ${PROJECT_SOURCE_DIR}/tests/reference/world_gouraud_near.txt)
  add_test(NAME render.native_projected_gouraud COMMAND world_polygon_probe --shaded ${PROJECT_SOURCE_DIR}/tests/reference/world_gouraud.txt)
  add_executable(model_orientation_probe tests/model_orientation_probe.cpp)
  target_link_libraries(model_orientation_probe PRIVATE tfx_core)
  configure_first_party(model_orientation_probe)
  add_test(NAME render.native_ssd_model_orientations COMMAND model_orientation_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_orientations.txt)
  add_executable(model_shadow_probe tests/model_shadow_probe.cpp)
  target_link_libraries(model_shadow_probe PRIVATE tfx_core)
  configure_first_party(model_shadow_probe)
  add_test(NAME render.native_shadow_placements COMMAND model_shadow_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_shadows.txt)
  add_executable(model_transform_probe tests/model_transform_probe.cpp)
  target_link_libraries(model_transform_probe PRIVATE tfx_core)
  configure_first_party(model_transform_probe)
  add_test(NAME render.native_queued_model_transforms COMMAND model_transform_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_transforms.txt)
  add_test(NAME render.native_transformed_model_lines COMMAND model_transform_probe --lines ${PROJECT_SOURCE_DIR}/tests/reference/model_transformed_lines.txt)
  add_test(NAME render.native_ground_shadow_frames COMMAND model_transform_probe --shadow ${PROJECT_SOURCE_DIR}/tests/reference/model_shadow_frames.txt)
  add_test(NAME render.native_gouraud_shadow_frames COMMAND model_transform_probe --shadow --shaded ${PROJECT_SOURCE_DIR}/tests/reference/model_shadow_frames.txt)
  add_test(NAME render.native_shadow_detail_primitives COMMAND model_transform_probe --shadow --effects ${PROJECT_SOURCE_DIR}/tests/reference/model_shadow_details.txt)
  add_executable(world_polygon_probe tests/world_polygon_probe.cpp)
  target_link_libraries(world_polygon_probe PRIVATE tfx_core)
  configure_first_party(world_polygon_probe)
  add_test(NAME render.native_flat_model_polygons COMMAND world_polygon_probe ${PROJECT_SOURCE_DIR}/tests/reference/world_polygons.txt)
  add_test(NAME render.native_warped_flat_model_polygons COMMAND world_polygon_probe ${PROJECT_SOURCE_DIR}/tests/reference/world_polygons_warped.txt)
  add_test(NAME render.native_warped_gouraud_model_polygons COMMAND world_polygon_probe --shaded ${PROJECT_SOURCE_DIR}/tests/reference/world_gouraud_warped.txt)
  add_test(NAME render.native_patterned_model_polygons COMMAND world_polygon_probe --pattern ${PROJECT_SOURCE_DIR}/tests/reference/world_patterns.txt)
  add_test(NAME render.native_warped_patterned_model_polygons COMMAND world_polygon_probe --pattern ${PROJECT_SOURCE_DIR}/tests/reference/world_patterns_warped.txt)
  add_test(NAME render.native_near_patterned_model_polygons COMMAND world_polygon_probe --pattern ${PROJECT_SOURCE_DIR}/tests/reference/world_patterns_near.txt)
  add_executable(aperture_probe tests/aperture_probe.cpp)
  target_link_libraries(aperture_probe PRIVATE tfx_core)
  configure_first_party(aperture_probe)
  add_test(NAME render.native_model_aperture COMMAND aperture_probe ${PROJECT_SOURCE_DIR}/tests/reference/model_aperture.txt)
  add_executable(horizon_pixels_probe tests/horizon_pixels_probe.cpp)
  target_link_libraries(horizon_pixels_probe PRIVATE tfx_core)
  configure_first_party(horizon_pixels_probe)
  add_test(NAME render.native_horizon_pixels COMMAND horizon_pixels_probe ${PROJECT_SOURCE_DIR}/tests/reference/horizon_pixels.txt)
  add_test(NAME render.native_wide_horizon_pixels COMMAND horizon_pixels_probe ${PROJECT_SOURCE_DIR}/tests/reference/horizon_wide_pixels.txt)
  add_executable(horizon_phase_probe tests/horizon_phase_probe.cpp)
  target_link_libraries(horizon_phase_probe PRIVATE tfx_core)
  configure_first_party(horizon_phase_probe)
  add_test(NAME render.native_horizon_phase COMMAND horizon_phase_probe ${PROJECT_SOURCE_DIR}/tests/reference/horizon_phase.txt)
  add_executable(precipitation_probe tests/precipitation_probe.cpp)
  target_link_libraries(precipitation_probe PRIVATE tfx_core)
  configure_first_party(precipitation_probe)
  add_test(NAME render.native_precipitation COMMAND precipitation_probe ${PROJECT_SOURCE_DIR}/tests/reference/precipitation.txt)
  add_test(NAME render.native_wide_spheres COMMAND visibility_probe --wide ${PROJECT_SOURCE_DIR}/tests/reference/sphere_visibility_wide.txt)
  add_test(NAME render.native_precipitation_gate COMMAND precipitation_probe --gate ${PROJECT_SOURCE_DIR}/tests/reference/precipitation_gate.txt)
  add_executable(horizon_probe tests/horizon_probe.cpp)
  target_link_libraries(horizon_probe PRIVATE tfx_core)
  configure_first_party(horizon_probe)
  add_test(NAME render.native_horizon_surfaces COMMAND horizon_probe ${PROJECT_SOURCE_DIR}/tests/reference/horizon.txt)
  add_executable(sky_scene_probe tests/sky_scene_probe.cpp)
  target_link_libraries(sky_scene_probe PRIVATE tfx_core)
  configure_first_party(sky_scene_probe)
  add_executable(sky_probe tests/sky_probe.cpp)
  target_link_libraries(sky_probe PRIVATE tfx_core)
  configure_first_party(sky_probe)
  add_test(NAME world.native_sky_and_layers COMMAND sky_probe ${PROJECT_SOURCE_DIR}/tests/reference/sky.txt)
  add_executable(trails_probe tests/trails_probe.cpp)
  target_link_libraries(trails_probe PRIVATE tfx_core)
  configure_first_party(trails_probe)
  add_test(NAME world.native_smoke_trail_pool COMMAND trails_probe ${PROJECT_SOURCE_DIR}/tests/reference/trails.txt)
  add_test(NAME world.native_smoke_trail_segments COMMAND trails_probe ${PROJECT_SOURCE_DIR}/tests/reference/trail_segments.txt)
  add_test(NAME weapons.native_smoke_trail_dispatch COMMAND trails_probe ${PROJECT_SOURCE_DIR}/tests/reference/trail_dispatch.txt)

  add_executable(flak_probe tests/flak_probe.cpp)
  target_link_libraries(flak_probe PRIVATE tfx_core)
  configure_first_party(flak_probe)
  add_test(NAME world.native_area_flak_controller COMMAND flak_probe ${PROJECT_SOURCE_DIR}/tests/reference/flak.txt)

  add_executable(effect_scene_probe tests/effect_scene_probe.cpp)
  target_link_libraries(effect_scene_probe PRIVATE tfx_core)
  configure_first_party(effect_scene_probe)

  add_executable(effects_probe tests/effects_probe.cpp)
  target_link_libraries(effects_probe PRIVATE tfx_core)
  configure_first_party(effects_probe)
  add_test(NAME world.native_temporary_effect_pool COMMAND effects_probe ${PROJECT_SOURCE_DIR}/tests/reference/effects.txt)
  add_test(NAME world.native_ssd_effect_placement COMMAND effects_probe --place ${PROJECT_SOURCE_DIR}/tests/reference/effect_placement.txt)
  add_test(NAME world.native_ssd_request_delivery_order COMMAND effects_probe --request-order)

  add_executable(projectile_impact_probe tests/projectile_impact_probe.cpp)
  target_link_libraries(projectile_impact_probe PRIVATE tfx_core)
  configure_first_party(projectile_impact_probe)
  add_test(NAME world.native_projectile_impact_outputs COMMAND projectile_impact_probe ${PROJECT_SOURCE_DIR}/tests/reference/projectile_impact.txt)
  add_test(NAME weapons.native_player_projectile_contact_gates COMMAND projectile_impact_probe --player-contacts ${PROJECT_SOURCE_DIR}/tests/reference/player_projectile_contacts.txt)

  add_executable(impact_probe tests/impact_probe.cpp)
  target_link_libraries(impact_probe PRIVATE tfx_core)
  configure_first_party(impact_probe)
  add_test(NAME world.native_impact_palette_and_crash_choice COMMAND impact_probe ${PROJECT_SOURCE_DIR}/tests/reference/impact.txt)

  add_executable(player_contact_probe tests/player_contact_probe.cpp)
  target_link_libraries(player_contact_probe PRIVATE tfx_core)
  configure_first_party(player_contact_probe)
  add_test(NAME world.native_player_contact_transition COMMAND player_contact_probe ${PROJECT_SOURCE_DIR}/tests/reference/player_contact.txt)

  add_executable(death_probe tests/death_probe.cpp)
  target_link_libraries(death_probe PRIVATE tfx_core)
  configure_first_party(death_probe)
  add_test(NAME missions.native_player_loss_transition COMMAND death_probe ${PROJECT_SOURCE_DIR}/tests/reference/death.txt)

  add_executable(engagement_probe tests/engagement_probe.cpp)
  target_link_libraries(engagement_probe PRIVATE tfx_core)
  configure_first_party(engagement_probe)
  add_test(NAME missions.native_permission_to_fire COMMAND engagement_probe ${PROJECT_SOURCE_DIR}/tests/reference/engagement.txt)

  add_executable(terrain_credit_probe tests/terrain_credit_probe.cpp)
  target_link_libraries(terrain_credit_probe PRIVATE tfx_core)
  configure_first_party(terrain_credit_probe)
  add_test(NAME missions.native_terrain_target_credit COMMAND terrain_credit_probe ${PROJECT_SOURCE_DIR}/tests/reference/terrain_credit.txt)

  add_executable(credit_probe tests/credit_probe.cpp)
  target_link_libraries(credit_probe PRIVATE tfx_core)
  configure_first_party(credit_probe)
  add_test(NAME missions.native_air_and_mobile_credit COMMAND credit_probe ${PROJECT_SOURCE_DIR}/tests/reference/credit.txt)

  add_executable(progress_probe tests/progress_probe.cpp)
  target_link_libraries(progress_probe PRIVATE tfx_core)
  configure_first_party(progress_probe)
  add_test(NAME missions.native_target_progress_and_termination COMMAND progress_probe ${PROJECT_SOURCE_DIR}/tests/reference/progress.txt)
  add_executable(target_binding_probe tests/target_binding_probe.cpp)
  target_link_libraries(target_binding_probe PRIVATE tfx_core)
  configure_first_party(target_binding_probe)
  add_test(NAME missions.native_target_binding COMMAND target_binding_probe ${PROJECT_SOURCE_DIR}/tests/reference/target_binding.txt)

  add_executable(damage_probe tests/damage_probe.cpp)
  target_link_libraries(damage_probe PRIVATE tfx_core)
  configure_first_party(damage_probe)
  add_test(NAME flight.native_system_damage_and_repair COMMAND damage_probe ${PROJECT_SOURCE_DIR}/tests/reference/damage.txt)

  add_executable(radar_voice_probe tests/radar_voice_probe.cpp)
  target_link_libraries(radar_voice_probe PRIVATE tfx_audio)
  configure_first_party(radar_voice_probe)
  add_test(NAME audio.native_radar_speech COMMAND radar_voice_probe ${PROJECT_SOURCE_DIR}/tests/reference/radar_voice.txt)

  add_executable(sensors_probe tests/sensors_probe.cpp)
  target_link_libraries(sensors_probe PRIVATE tfx_core)
  configure_first_party(sensors_probe)
  add_test(NAME sensors.native_air_radar_and_seeker COMMAND sensors_probe --announcements ${PROJECT_SOURCE_DIR}/tests/reference/sensors.txt)

  add_executable(attachment_tests tests/attachment_test.cpp)
  target_link_libraries(attachment_tests PRIVATE tfx_core)
  configure_first_party(attachment_tests)
  add_test(NAME models.carried_store_context_and_bounds COMMAND attachment_tests)

  add_executable(automatic_approach_probe tests/automatic_approach_probe.cpp)
  target_link_libraries(automatic_approach_probe PRIVATE tfx_core)
  configure_first_party(automatic_approach_probe)
  add_test(NAME flight.native_automatic_approach COMMAND automatic_approach_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/automatic_approach.txt")

  add_executable(aerodynamic_approach_reference_probe tests/aerodynamic_approach_reference_probe.cpp)
  target_link_libraries(aerodynamic_approach_reference_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_approach_reference_probe)
  add_test(NAME flight.native_aerodynamic_approach_reference COMMAND aerodynamic_approach_reference_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_approach_reference.txt")

  add_executable(aerodynamic_navigation_probe tests/aerodynamic_navigation_probe.cpp)
  target_link_libraries(aerodynamic_navigation_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_navigation_probe)
  add_test(NAME flight.native_aerodynamic_navigation COMMAND aerodynamic_navigation_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_navigation.txt")

  add_executable(aerodynamic_guidance_probe tests/aerodynamic_guidance_probe.cpp)
  target_link_libraries(aerodynamic_guidance_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_guidance_probe)
  add_test(NAME flight.native_aerodynamic_guidance COMMAND aerodynamic_guidance_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_guidance.txt")
  add_test(NAME flight.native_aerodynamic_autopilot_controls COMMAND aerodynamic_guidance_probe --controls "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_autopilot_controls.txt")

  add_executable(aerodynamic_recovery_probe tests/aerodynamic_recovery_probe.cpp)
  target_link_libraries(aerodynamic_recovery_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_recovery_probe)
  add_test(NAME flight.native_aerodynamic_recovery_toggle COMMAND aerodynamic_recovery_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_recovery.txt")
  add_test(NAME flight.native_aerodynamic_recovery_trigger COMMAND aerodynamic_recovery_probe --descent "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_recovery_trigger.txt")

  add_executable(resource_files_test tests/resource_files_test.cpp)
  target_link_libraries(resource_files_test PRIVATE tfx_core)
  configure_first_party(resource_files_test)
  add_test(NAME resources.archive_then_loose_dos_files COMMAND resource_files_test)
  add_executable(music_session_probe tests/music_session_probe.cpp)
  target_link_libraries(music_session_probe PRIVATE tfx_audio)
  configure_first_party(music_session_probe)
  add_executable(music_player_probe tests/music_player_probe.cpp)
  target_link_libraries(music_player_probe PRIVATE tfx_audio)
  configure_first_party(music_player_probe)
  add_executable(music_clock_test tests/music_clock_test.cpp)
  target_link_libraries(music_clock_test PRIVATE tfx_audio)
  configure_first_party(music_clock_test)
  add_test(NAME audio.shared_music_irq_phase COMMAND music_clock_test)
  add_executable(midi_stream_test tests/midi_stream_test.cpp)
  target_link_libraries(midi_stream_test PRIVATE tfx_audio)
  configure_first_party(midi_stream_test)
  add_test(NAME audio.midi_uart_message_assembly COMMAND midi_stream_test)
  add_executable(osl_parameters_test tests/osl_parameters_test.cpp)
  target_link_libraries(osl_parameters_test PRIVATE tfx_audio)
  configure_first_party(osl_parameters_test)
  add_test(NAME audio.native_osl_parameter_alias COMMAND osl_parameters_test)
  add_executable(osl_lifetime_test tests/osl_lifetime_test.cpp)
  target_link_libraries(osl_lifetime_test PRIVATE tfx_audio)
  configure_first_party(osl_lifetime_test)
  add_test(NAME audio.native_osl_playback_lifetime COMMAND osl_lifetime_test "${PROJECT_SOURCE_DIR}/tests/fixtures/osl_lifetime.txt")
  add_executable(music_poll_probe tests/music_poll_probe.cpp)
  target_link_libraries(music_poll_probe PRIVATE tfx_audio)
  configure_first_party(music_poll_probe)
  add_test(NAME audio.native_flight_music_frames COMMAND music_poll_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/music_poll.txt")
  add_executable(music_control_probe tests/music_control_probe.cpp)
  target_link_libraries(music_control_probe PRIVATE tfx_audio)
  configure_first_party(music_control_probe)
  add_test(NAME audio.native_flight_music_selection COMMAND music_control_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/music_control.txt")
  add_executable(osl_scc_probe tests/osl_scc_probe.cpp)
  target_link_libraries(osl_scc_probe PRIVATE tfx_audio)
  configure_first_party(osl_scc_probe)
  add_executable(osl_fm_probe tests/osl_fm_probe.cpp)
  target_link_libraries(osl_fm_probe PRIVATE tfx_audio)
  configure_first_party(osl_fm_probe)
  add_executable(osl_la_probe tests/osl_la_probe.cpp)
  target_link_libraries(osl_la_probe PRIVATE tfx_audio)
  configure_first_party(osl_la_probe)
  add_executable(osl_sequencer_probe tests/osl_sequencer_probe.cpp)
  target_link_libraries(osl_sequencer_probe PRIVATE tfx_audio)
  configure_first_party(osl_sequencer_probe)
  add_executable(osl_resource_probe tests/osl_resource_probe.cpp)
  target_link_libraries(osl_resource_probe PRIVATE tfx_audio)
  configure_first_party(osl_resource_probe)
  add_executable(osl_pattern_test tests/osl_pattern_test.cpp)
  target_link_libraries(osl_pattern_test PRIVATE tfx_audio)
  configure_first_party(osl_pattern_test)
  add_test(NAME audio.native_osl_patterns COMMAND osl_pattern_test "${PROJECT_SOURCE_DIR}/tests/fixtures/osl_patterns.txt")
  add_executable(qsound_playback_test tests/qsound_playback_test.cpp)
  target_link_libraries(qsound_playback_test PRIVATE tfx_audio)
  target_include_directories(qsound_playback_test PRIVATE "${QSOUND_INCLUDE_DIR}")
  configure_first_party(qsound_playback_test)
  add_test(NAME audio.qsound_dac_and_sweeps COMMAND qsound_playback_test)

  add_executable(sample_bank_probe tests/sample_bank_probe.cpp)
  target_link_libraries(sample_bank_probe PRIVATE tfx_audio)
  configure_first_party(sample_bank_probe)
  add_test(NAME audio.sample_clock_and_pcm COMMAND sample_bank_probe)
  add_executable(sample_scheduler_probe tests/sample_scheduler_probe.cpp)
  target_link_libraries(sample_scheduler_probe PRIVATE tfx_audio)
  configure_first_party(sample_scheduler_probe)
  add_test(NAME audio.native_sample_scheduler COMMAND sample_scheduler_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/sample_scheduler.txt")
  add_executable(gus_scheduler_probe tests/gus_scheduler_probe.cpp)
  target_link_libraries(gus_scheduler_probe PRIVATE tfx_audio)
  configure_first_party(gus_scheduler_probe)
  add_test(NAME audio.native_gus_scheduler COMMAND gus_scheduler_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/gus_scheduler.txt")
  add_executable(gus_memory_probe tests/gus_memory_probe.cpp)
  target_link_libraries(gus_memory_probe PRIVATE tfx_audio)
  configure_first_party(gus_memory_probe)
  add_test(NAME audio.native_gus_sample_ram COMMAND gus_memory_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/gus_memory.txt")
  add_executable(gus_driver_probe tests/gus_driver_probe.cpp)
  target_link_libraries(gus_driver_probe PRIVATE tfx_audio)
  configure_first_party(gus_driver_probe)
  add_test(NAME audio.native_gus_registers COMMAND gus_driver_probe --synthetic "${PROJECT_SOURCE_DIR}/tests/fixtures/gus_driver.txt")
  add_executable(gus_samples_probe tests/gus_samples_probe.cpp)
  target_link_libraries(gus_samples_probe PRIVATE tfx_audio)
  configure_first_party(gus_samples_probe)
  add_test(NAME audio.native_gus_pcm COMMAND gus_samples_probe --synthetic "${PROJECT_SOURCE_DIR}/tests/fixtures/gus_samples.txt" "${PROJECT_SOURCE_DIR}/tests/fixtures/gus_samples.expected")
  add_executable(gear_key_probe tests/gear_key_probe.cpp)
  target_link_libraries(gear_key_probe PRIVATE tfx_core)
  configure_first_party(gear_key_probe)
  add_test(NAME flight.native_gear_key COMMAND gear_key_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/gear_key.txt")

  add_executable(engine_key_probe tests/engine_key_probe.cpp)
  target_link_libraries(engine_key_probe PRIVATE tfx_core)
  configure_first_party(engine_key_probe)
  add_test(NAME flight.native_engine_keys COMMAND engine_key_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/engine_key.txt")

  add_executable(ground_service_probe tests/ground_service_probe.cpp)
  target_link_libraries(ground_service_probe PRIVATE tfx_core)
  configure_first_party(ground_service_probe)
  add_test(NAME flight.native_ground_service COMMAND ground_service_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/ground_service.txt")

  add_executable(display_pages_probe tests/display_pages_probe.cpp)
  target_link_libraries(display_pages_probe PRIVATE tfx_core)
  configure_first_party(display_pages_probe)
  add_test(NAME render.native_display_pages COMMAND display_pages_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/display_pages.txt")

  add_executable(detail_status_probe tests/detail_status_probe.cpp)
  target_link_libraries(detail_status_probe PRIVATE tfx_core)
  configure_first_party(detail_status_probe)
  add_test(NAME world.native_detail_status COMMAND detail_status_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/detail_status.txt")

  add_executable(configuration_menu_probe tests/configuration_menu_probe.cpp)
  add_executable(recording_name_probe tests/recording_name_probe.cpp)
  target_link_libraries(recording_name_probe PRIVATE tfx_core)
  configure_first_party(recording_name_probe)
  target_link_libraries(configuration_menu_probe PRIVATE tfx_core)
  configure_first_party(configuration_menu_probe)

  add_executable(dialogue_probe tests/dialogue_probe.cpp)
  target_link_libraries(dialogue_probe PRIVATE tfx_core)
  configure_first_party(dialogue_probe)

  add_executable(tactical_map_probe tests/tactical_map_probe.cpp)
  target_link_libraries(tactical_map_probe PRIVATE tfx_core)
  configure_first_party(tactical_map_probe)

  add_executable(flight_configuration_probe tests/flight_configuration_probe.cpp)
  target_link_libraries(flight_configuration_probe PRIVATE tfx_core)
  configure_first_party(flight_configuration_probe)
  add_test(NAME flight.native_weapon_configuration COMMAND flight_configuration_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/flight_configuration.txt")

  add_executable(configuration_save_test tests/configuration_save_test.cpp)
  target_link_libraries(configuration_save_test PRIVATE tfx_core)
  configure_first_party(configuration_save_test)
  add_test(NAME configuration.preference_file_roundtrip COMMAND configuration_save_test)

  add_executable(configuration_save_probe tests/configuration_save_probe.cpp)
  target_link_libraries(configuration_save_probe PRIVATE tfx_core)
  configure_first_party(configuration_save_probe)
  add_test(NAME configuration.native_preference_writer COMMAND configuration_save_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/configuration_save.txt")

  add_executable(weather_probe tests/weather_probe.cpp)
  target_link_libraries(weather_probe PRIVATE tfx_core)
  configure_first_party(weather_probe)

  add_executable(g_effects_probe tests/g_effects_probe.cpp)
  target_link_libraries(g_effects_probe PRIVATE tfx_core)
  configure_first_party(g_effects_probe)
  add_test(NAME render.native_g_effects COMMAND g_effects_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/g_effects.txt")

  add_executable(mouse_input_probe tests/mouse_input_probe.cpp)
  target_link_libraries(mouse_input_probe PRIVATE tfx_core)
  configure_first_party(mouse_input_probe)
  add_test(NAME flight.native_mouse_input COMMAND mouse_input_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/mouse_input.txt")

  add_executable(joystick_probe tests/joystick_probe.cpp)
  target_link_libraries(joystick_probe PRIVATE tfx_core)
  configure_first_party(joystick_probe)
  add_test(NAME flight.native_joystick_input COMMAND joystick_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/joystick.txt")

  add_executable(configuration_probe tests/configuration_probe.cpp)
  target_link_libraries(configuration_probe PRIVATE tfx_core)
  configure_first_party(configuration_probe)
  add_test(NAME configuration.native_readers COMMAND configuration_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/configuration.txt")

  add_test(NAME configuration.native_documents COMMAND configuration_probe --document "${PROJECT_SOURCE_DIR}/tests/fixtures/configuration_documents.txt")

  add_executable(player_flight_probe tests/player_flight_probe.cpp)
  target_link_libraries(player_flight_probe PRIVATE tfx_core)
  configure_first_party(player_flight_probe)

  add_executable(player_frame_probe tests/player_frame_probe.cpp)
  target_link_libraries(player_frame_probe PRIVATE tfx_core)
  configure_first_party(player_frame_probe)
  add_test(NAME flight.native_player_frame COMMAND player_frame_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/player_frame.txt")

  add_executable(player_dispatch_probe tests/player_dispatch_probe.cpp)
  target_link_libraries(player_dispatch_probe PRIVATE tfx_core)
  configure_first_party(player_dispatch_probe)
  add_test(NAME flight.native_player_dispatch COMMAND player_dispatch_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/player_dispatch.txt")

  add_executable(aerodynamic_launch_probe tests/aerodynamic_launch_probe.cpp)
  target_link_libraries(aerodynamic_launch_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_launch_probe)

  add_executable(aerodynamic_probe tests/aerodynamic_probe.cpp)
  target_link_libraries(aerodynamic_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_probe)

  add_executable(aerodynamic_wind_probe tests/aerodynamic_wind_probe.cpp)
  target_link_libraries(aerodynamic_wind_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_wind_probe)
  add_test(NAME flight.native_aerodynamic_wind COMMAND aerodynamic_wind_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_wind.txt")

  add_executable(aerodynamic_start_probe tests/aerodynamic_start_probe.cpp)
  target_link_libraries(aerodynamic_start_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_start_probe)
  add_test(NAME flight.native_aerodynamic_start COMMAND aerodynamic_start_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_start.txt")

  add_executable(aerodynamic_commands_probe tests/aerodynamic_commands_probe.cpp)
  target_link_libraries(aerodynamic_commands_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_commands_probe)
  add_test(NAME flight.native_aerodynamic_commands COMMAND aerodynamic_commands_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_commands.txt")

  add_executable(aerodynamic_realistic_ground_probe tests/aerodynamic_realistic_ground_probe.cpp)
  target_link_libraries(aerodynamic_realistic_ground_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_realistic_ground_probe)
  add_test(NAME flight.native_aerodynamic_realistic_ground COMMAND aerodynamic_realistic_ground_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_realistic_ground.txt")

  add_executable(aerodynamic_ground_probe tests/aerodynamic_ground_probe.cpp)
  target_link_libraries(aerodynamic_ground_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_ground_probe)
  add_test(NAME flight.native_aerodynamic_ground COMMAND aerodynamic_ground_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_ground.txt")

  add_executable(aerodynamic_contact_probe tests/aerodynamic_contact_probe.cpp)
  target_link_libraries(aerodynamic_contact_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_contact_probe)
  add_test(NAME flight.native_aerodynamic_contact COMMAND aerodynamic_contact_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_contact.txt")

  add_executable(aerodynamic_airframe_probe tests/aerodynamic_airframe_probe.cpp)
  target_link_libraries(aerodynamic_airframe_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_airframe_probe)
  add_test(NAME flight.native_airframe_pitch_calibration COMMAND aerodynamic_airframe_probe --calibration "${PROJECT_SOURCE_DIR}/tests/fixtures/airframe_calibration.txt")

  add_executable(aerodynamic_manual_probe tests/aerodynamic_manual_probe.cpp)
  target_link_libraries(aerodynamic_manual_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_manual_probe)
  add_test(NAME flight.native_aerodynamic_manual_control COMMAND aerodynamic_manual_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_manual.txt")

  add_executable(aerodynamic_speed_probe tests/aerodynamic_speed_probe.cpp)
  target_link_libraries(aerodynamic_speed_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_speed_probe)
  add_test(NAME flight.native_aerodynamic_speed COMMAND aerodynamic_speed_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_speed.txt")

  add_executable(aerodynamic_controls_probe tests/aerodynamic_controls_probe.cpp)
  target_link_libraries(aerodynamic_controls_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_controls_probe)
  add_test(NAME flight.native_aerodynamic_controls COMMAND aerodynamic_controls_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_controls.txt")

  add_executable(aerodynamic_surfaces_probe tests/aerodynamic_surfaces_probe.cpp)
  target_link_libraries(aerodynamic_surfaces_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_surfaces_probe)
  add_test(NAME flight.native_aerodynamic_surfaces COMMAND aerodynamic_surfaces_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_surfaces.txt")

  add_executable(aerodynamic_powerplant_probe tests/aerodynamic_powerplant_probe.cpp)
  target_link_libraries(aerodynamic_powerplant_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_powerplant_probe)
  add_test(NAME flight.native_aerodynamic_powerplant COMMAND aerodynamic_powerplant_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_powerplant.txt")

  add_executable(aerodynamic_coefficients_probe tests/aerodynamic_coefficients_probe.cpp)
  target_link_libraries(aerodynamic_coefficients_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_coefficients_probe)
  add_test(NAME flight.native_aerodynamic_coefficients COMMAND aerodynamic_coefficients_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_coefficients.txt")

  add_executable(aerodynamic_airdata_probe tests/aerodynamic_airdata_probe.cpp)
  target_link_libraries(aerodynamic_airdata_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_airdata_probe)
  add_test(NAME flight.native_aerodynamic_airdata COMMAND aerodynamic_airdata_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_airdata.txt")

  add_executable(aerodynamic_response_probe tests/aerodynamic_response_probe.cpp)
  target_link_libraries(aerodynamic_response_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_response_probe)
  add_test(NAME flight.native_aerodynamic_response COMMAND aerodynamic_response_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_response.txt")

  add_executable(aerodynamic_forces_probe tests/aerodynamic_forces_probe.cpp)
  target_link_libraries(aerodynamic_forces_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_forces_probe)
  add_test(NAME flight.native_aerodynamic_forces COMMAND aerodynamic_forces_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_forces.txt")

  add_executable(aerodynamic_motion_probe tests/aerodynamic_motion_probe.cpp)
  target_link_libraries(aerodynamic_motion_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_motion_probe)
  add_test(NAME flight.native_aerodynamic_motion COMMAND aerodynamic_motion_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_motion.txt")

  add_executable(atmosphere_probe tests/atmosphere_probe.cpp)
  target_link_libraries(atmosphere_probe PRIVATE tfx_core)
  configure_first_party(atmosphere_probe)
  add_test(NAME flight.native_atmosphere COMMAND atmosphere_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/atmosphere.txt")

  add_executable(aerodynamic_pose_probe tests/aerodynamic_pose_probe.cpp)
  target_link_libraries(aerodynamic_pose_probe PRIVATE tfx_core)
  configure_first_party(aerodynamic_pose_probe)
  add_test(NAME flight.native_aerodynamic_world_pose COMMAND aerodynamic_pose_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/aerodynamic_pose.txt")

  add_executable(simple_probe tests/simple_probe.cpp)
  target_link_libraries(simple_probe PRIVATE tfx_core)
  configure_first_party(simple_probe)
  add_test(NAME flight.native_simple_sequences COMMAND simple_probe --check "${PROJECT_SOURCE_DIR}/tests/fixtures/simple.txt")
  add_test(NAME flight.native_simple_disabled_sequences COMMAND simple_probe --check-disabled "${PROJECT_SOURCE_DIR}/tests/fixtures/simple_disabled.txt")

  add_executable(landing_gear_probe tests/landing_gear_probe.cpp)
  target_link_libraries(landing_gear_probe PRIVATE tfx_core)
  configure_first_party(landing_gear_probe)
  add_test(NAME flight.native_landing_gear_damage COMMAND landing_gear_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/landing_gear.txt")

  add_executable(simple_ground_probe tests/simple_ground_probe.cpp)
  target_link_libraries(simple_ground_probe PRIVATE tfx_core)
  configure_first_party(simple_ground_probe)
  add_test(NAME flight.native_simple_ground_contact COMMAND simple_ground_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/simple_ground.txt")

  add_executable(simple_turn_probe tests/simple_turn_probe.cpp)
  target_link_libraries(simple_turn_probe PRIVATE tfx_core)
  configure_first_party(simple_turn_probe)
  add_test(NAME flight.native_simple_coordinated_turn COMMAND simple_turn_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/simple_turn.txt")

  add_executable(simple_controls_probe tests/simple_controls_probe.cpp)
  target_link_libraries(simple_controls_probe PRIVATE tfx_core)
  configure_first_party(simple_controls_probe)
  add_test(NAME flight.native_simple_control_response COMMAND simple_controls_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/simple_controls.txt")

  add_executable(recovery_probe tests/recovery_probe.cpp)
  target_link_libraries(recovery_probe PRIVATE tfx_core)
  configure_first_party(recovery_probe)

  add_executable(ejection_sequence_probe tests/ejection_sequence_probe.cpp)
  target_link_libraries(ejection_sequence_probe PRIVATE tfx_core)
  configure_first_party(ejection_sequence_probe)
  add_executable(ejection_probe tests/ejection_probe.cpp)
  target_link_libraries(ejection_probe PRIVATE tfx_core)
  configure_first_party(ejection_probe)
  add_test(NAME flight.native_ejected_pilot_motion COMMAND ejection_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/ejection.txt")
  add_executable(down_alignment_probe tests/down_alignment_probe.cpp)
  target_link_libraries(down_alignment_probe PRIVATE tfx_core)
  configure_first_party(down_alignment_probe)
  add_test(NAME math.native_down_axis_alignment COMMAND down_alignment_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/down_alignment.txt")
  add_executable(landing_tests tests/landing_tests.cpp)
  target_link_libraries(landing_tests PRIVATE tfx_core)
  configure_first_party(landing_tests)
  add_test(NAME flight.native_ils_guidance COMMAND landing_tests "${PROJECT_SOURCE_DIR}/tests/fixtures/landing.txt")
  add_executable(landing_probe tests/landing_probe.cpp)
  target_link_libraries(landing_probe PRIVATE tfx_core)
  configure_first_party(landing_probe)
  add_executable(refuelling_probe tests/refuelling_probe.cpp)
  target_link_libraries(refuelling_probe PRIVATE tfx_core)
  configure_first_party(refuelling_probe)
  add_executable(hud_alert_probe tests/hud_alert_probe.cpp)
  target_link_libraries(hud_alert_probe PRIVATE tfx_core)
  configure_first_party(hud_alert_probe)
  add_executable(dac_probe tests/dac_probe.cpp)
  target_link_libraries(dac_probe PRIVATE tfx_core)
  configure_first_party(dac_probe)
  add_test(NAME render.native_vga_dac COMMAND dac_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/dac.txt")
  add_executable(glare_probe tests/glare_probe.cpp)
  target_link_libraries(glare_probe PRIVATE tfx_core)
  configure_first_party(glare_probe)
  add_test(NAME render.native_sunlight_glare COMMAND glare_probe ${PROJECT_SOURCE_DIR}/tests/reference/glare.txt)

  add_executable(palette_lighting_probe tests/palette_lighting_probe.cpp)
  target_link_libraries(palette_lighting_probe PRIVATE tfx_core)
  configure_first_party(palette_lighting_probe)
  add_executable(terrain_cells_probe tests/terrain_cells_probe.cpp)
  target_link_libraries(terrain_cells_probe PRIVATE tfx_core)
  configure_first_party(terrain_cells_probe)
  add_test(NAME world.native_terrain_traversal COMMAND terrain_cells_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/terrain_cells.txt")
  add_executable(environment_palette_probe tests/environment_palette_probe.cpp)
  target_link_libraries(environment_palette_probe PRIVATE tfx_core)
  configure_first_party(environment_palette_probe)
  add_executable(palette_selection_probe tests/palette_selection_probe.cpp)
  target_link_libraries(palette_selection_probe PRIVATE tfx_core)
  configure_first_party(palette_selection_probe)
  add_test(NAME render.native_palette_selection COMMAND palette_selection_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/palette_selection.txt")
  add_executable(external_display_probe tests/external_display_probe.cpp)
  target_link_libraries(external_display_probe PRIVATE tfx_core)
  configure_first_party(external_display_probe)

  add_executable(visibility_probe tests/visibility_probe.cpp)
  target_link_libraries(visibility_probe PRIVATE tfx_core)
  configure_first_party(visibility_probe)
  add_test(NAME render.native_sphere_visibility COMMAND visibility_probe ${PROJECT_SOURCE_DIR}/tests/reference/sphere_visibility.txt)

  add_executable(point_projection_probe tests/point_projection_probe.cpp)
  target_link_libraries(point_projection_probe PRIVATE tfx_core)
  configure_first_party(point_projection_probe)
  add_test(NAME render.native_world_point_projection COMMAND point_projection_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/point_projection.txt")

  add_executable(scaled_projection_probe tests/scaled_projection_probe.cpp)
  target_link_libraries(scaled_projection_probe PRIVATE tfx_core)
  configure_first_party(scaled_projection_probe)

  add_executable(view_camera_tests tests/view_camera_test.cpp)
  target_link_libraries(view_camera_tests PRIVATE tfx_core)
  configure_first_party(view_camera_tests)
  add_test(NAME render.missing_missile_camera COMMAND view_camera_tests)

  add_executable(view_camera_probe tests/view_camera_probe.cpp)
  target_link_libraries(view_camera_probe PRIVATE tfx_core)
  configure_first_party(view_camera_probe)

  add_executable(view_control_probe tests/view_control_probe.cpp)
  target_link_libraries(view_control_probe PRIVATE tfx_core)
  configure_first_party(view_control_probe)

  add_executable(target_tracking_probe tests/target_tracking_probe.cpp)
  target_link_libraries(target_tracking_probe PRIVATE tfx_core)
  configure_first_party(target_tracking_probe)

  add_executable(target_display_probe tests/target_display_probe.cpp)
  target_link_libraries(target_display_probe PRIVATE tfx_core)
  configure_first_party(target_display_probe)

  add_executable(radio_probe tests/radio_probe.cpp)
  target_link_libraries(radio_probe PRIVATE tfx_core)
  configure_first_party(radio_probe)

  add_executable(weapon_display_probe tests/weapon_display_probe.cpp)
  target_link_libraries(weapon_display_probe PRIVATE tfx_core)
  configure_first_party(weapon_display_probe)

  add_executable(cannon_prediction_probe tests/cannon_prediction_probe.cpp)
  target_link_libraries(cannon_prediction_probe PRIVATE tfx_core)
  configure_first_party(cannon_prediction_probe)

  add_executable(attitude_display_probe tests/attitude_display_probe.cpp)
  target_link_libraries(attitude_display_probe PRIVATE tfx_core)
  configure_first_party(attitude_display_probe)

  add_executable(actor_visibility_probe tests/actor_visibility_probe.cpp)
  target_link_libraries(actor_visibility_probe PRIVATE tfx_core)
  configure_first_party(actor_visibility_probe)
  add_test(NAME actors.native_draw_regions COMMAND actor_visibility_probe ${PROJECT_SOURCE_DIR}/tests/reference/actor_visibility.txt)

  add_executable(hud_text_probe tests/hud_text_probe.cpp)
  target_link_libraries(hud_text_probe PRIVATE tfx_core)
  configure_first_party(hud_text_probe)

  add_executable(cockpit_frame_probe tests/cockpit_frame_probe.cpp)
  target_link_libraries(cockpit_frame_probe PRIVATE tfx_core)
  configure_first_party(cockpit_frame_probe)

  add_executable(fuel_gauge_probe tests/fuel_gauge_probe.cpp)
  target_link_libraries(fuel_gauge_probe PRIVATE tfx_core)
  configure_first_party(fuel_gauge_probe)

  add_executable(designator_probe tests/designator_probe.cpp)
  target_link_libraries(designator_probe PRIVATE tfx_core)
  configure_first_party(designator_probe)
  add_test(NAME cockpit.native_designator_pose COMMAND designator_probe --camera ${PROJECT_SOURCE_DIR}/tests/reference/designator_pose.txt)

  add_executable(scene_display_probe tests/scene_display_probe.cpp)
  target_link_libraries(scene_display_probe PRIVATE tfx_core)
  configure_first_party(scene_display_probe)

  add_executable(hud_probe tests/hud_probe.cpp)
  target_link_libraries(hud_probe PRIVATE tfx_core)
  configure_first_party(hud_probe)

  add_executable(radar_probe tests/radar_probe.cpp)
  target_link_libraries(radar_probe PRIVATE tfx_core)
  configure_first_party(radar_probe)

  add_executable(map_display_probe tests/map_display_probe.cpp)
  target_link_libraries(map_display_probe PRIVATE tfx_core)
  configure_first_party(map_display_probe)

  add_executable(auxiliary_probe tests/auxiliary_probe.cpp)
  target_link_libraries(auxiliary_probe PRIVATE tfx_core)
  configure_first_party(auxiliary_probe)

  add_executable(warning_probe tests/warning_probe.cpp)
  target_link_libraries(warning_probe PRIVATE tfx_core)
  configure_first_party(warning_probe)

  add_executable(overlay_tests tests/overlay_test.cpp)
  target_link_libraries(overlay_tests PRIVATE tfx_core)
  configure_first_party(overlay_tests)
  add_test(NAME resources.compiled_overlay_bounds COMMAND overlay_tests)

  add_executable(overlay_probe tests/overlay_probe.cpp)
  target_link_libraries(overlay_probe PRIVATE tfx_core)
  configure_first_party(overlay_probe)

  add_executable(mfd_probe tests/mfd_probe.cpp)
  target_link_libraries(mfd_probe PRIVATE tfx_core)
  configure_first_party(mfd_probe)

  add_executable(readout_probe tests/readout_probe.cpp)
  target_link_libraries(readout_probe PRIVATE tfx_core)
  configure_first_party(readout_probe)

  add_executable(sprite_probe tests/sprite_probe.cpp)
  target_link_libraries(sprite_probe PRIVATE tfx_core)
  configure_first_party(sprite_probe)

  add_executable(arming_preview_probe tests/arming_preview_probe.cpp)
  target_link_libraries(arming_preview_probe PRIVATE tfx_core)
  configure_first_party(arming_preview_probe)

  add_executable(arming_controls_probe tests/arming_controls_probe.cpp)
  target_link_libraries(arming_controls_probe PRIVATE tfx_core)
  configure_first_party(arming_controls_probe)

  add_executable(arming_screen_probe tests/arming_screen_probe.cpp)
  target_link_libraries(arming_screen_probe PRIVATE tfx_core)
  configure_first_party(arming_screen_probe)

  add_executable(airframe_probe tests/airframe_probe.cpp)
  target_link_libraries(airframe_probe PRIVATE tfx_core)
  configure_first_party(airframe_probe)

  add_executable(arming_probe tests/arming_probe.cpp)
  target_link_libraries(arming_probe PRIVATE tfx_core)
  configure_first_party(arming_probe)
  add_test(NAME weapons.native_mount_permissions_and_assignment COMMAND arming_probe --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/arming.txt)

  add_executable(loadout_probe tests/loadout_probe.cpp)
  target_link_libraries(loadout_probe PRIVATE tfx_core)
  configure_first_party(loadout_probe)
  add_test(NAME weapons.native_pylons COMMAND loadout_probe --check ${PROJECT_SOURCE_DIR}/tests/reference/loadout.txt)
  add_test(NAME weapons.native_store_feedback COMMAND loadout_probe --feedback ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/store_feedback.txt)

  add_executable(jettison_probe tests/jettison_probe.cpp)
  target_link_libraries(jettison_probe PRIVATE tfx_core)
  configure_first_party(jettison_probe)
  add_test(NAME weapons.native_stores_jettison COMMAND jettison_probe ${PROJECT_SOURCE_DIR}/tests/reference/jettison.txt)

  add_executable(cannon_probe tests/cannon_probe.cpp)
  target_link_libraries(cannon_probe PRIVATE tfx_core)
  configure_first_party(cannon_probe)
  add_test(NAME weapons.native_keyboard_cannon COMMAND cannon_probe --check ${PROJECT_SOURCE_DIR}/tests/reference/cannon.txt)

  add_executable(anchor_probe tests/anchor_probe.cpp)
  target_link_libraries(anchor_probe PRIVATE tfx_core)
  configure_first_party(anchor_probe)
  add_test(NAME weapons.native_anchor_transform COMMAND anchor_probe ${PROJECT_SOURCE_DIR}/tests/reference/anchor.txt)

  add_executable(projectile_warnings_probe tests/projectile_warnings_probe.cpp)
  target_link_libraries(projectile_warnings_probe PRIVATE tfx_core)
  configure_first_party(projectile_warnings_probe)
  add_test(NAME weapons.native_incoming_warning_state COMMAND projectile_warnings_probe ${PROJECT_SOURCE_DIR}/tests/reference/projectile_warnings.txt)

  add_executable(projectile_runtime_probe tests/projectile_runtime_probe.cpp)
  target_link_libraries(projectile_runtime_probe PRIVATE tfx_core)
  configure_first_party(projectile_runtime_probe)

  add_executable(projectile_tail_tests tests/projectile_tail_test.cpp)
  target_link_libraries(projectile_tail_tests PRIVATE tfx_core)
  configure_first_party(projectile_tail_tests)
  add_test(NAME weapons.frame_tail_lifecycle COMMAND projectile_tail_tests)

  add_executable(contact_tests tests/contact_test.cpp)
  target_link_libraries(contact_tests PRIVATE tfx_core)
  configure_first_party(contact_tests)
  add_test(NAME weapons.native_contact_damage COMMAND contact_tests ${PROJECT_SOURCE_DIR}/tests/reference/contact.txt)

  add_executable(projectile_probe tests/projectile_probe.cpp)
  target_link_libraries(projectile_probe PRIVATE tfx_core)
  configure_first_party(projectile_probe)
  add_test(NAME weapons.native_projectile_motion COMMAND projectile_probe --check ${PROJECT_SOURCE_DIR}/tests/reference/projectile.txt)

  add_executable(air_combat_tests tests/air_combat_test.cpp)
  target_link_libraries(air_combat_tests PRIVATE tfx_core)
  configure_first_party(air_combat_tests)
  add_test(NAME actors.native_air_combat COMMAND air_combat_tests ${PROJECT_SOURCE_DIR}/tests/reference/air_combat.txt)
  add_test(NAME actors.native_air_crossfire_notices COMMAND air_combat_tests ${PROJECT_SOURCE_DIR}/tests/reference/air_crossfire.txt)
  add_test(NAME actors.native_air_collision_fall COMMAND air_combat_tests ${PROJECT_SOURCE_DIR}/tests/reference/air_collision.txt --collision)
  add_test(NAME actors.native_air_death COMMAND air_combat_tests ${PROJECT_SOURCE_DIR}/tests/reference/air_death.txt --death)

  add_executable(air_tests tests/air_test.cpp)
  target_link_libraries(air_tests PRIVATE tfx_core)
  configure_first_party(air_tests)
  add_test(NAME actors.native_air_sequences COMMAND air_tests ${PROJECT_SOURCE_DIR}/tests/reference/air.txt)

  add_executable(air_probe tests/air_probe.cpp)
  target_link_libraries(air_probe PRIVATE tfx_core)
  configure_first_party(air_probe)

  add_executable(air_motion_probe tests/air_motion_probe.cpp)
  target_link_libraries(air_motion_probe PRIVATE tfx_core)
  configure_first_party(air_motion_probe)
  add_test(NAME actors.native_air_motion COMMAND air_motion_probe --check ${PROJECT_SOURCE_DIR}/tests/reference/air_motion.txt)

  add_executable(timeline_probe tests/timeline_probe.cpp)
  target_link_libraries(timeline_probe PRIVATE tfx_core)
  configure_first_party(timeline_probe)
  add_test(NAME missions.native_timeline_sequences COMMAND timeline_probe --check ${PROJECT_SOURCE_DIR}/tests/reference/timeline.txt)

  add_executable(world_probe tests/world_probe.cpp)
  target_link_libraries(world_probe PRIVATE tfx_core)
  configure_first_party(world_probe)

  add_executable(instance_tests tests/instance_test.cpp)
  target_link_libraries(instance_tests PRIVATE tfx_core)
  configure_first_party(instance_tests)
  add_test(NAME world.native_instance_events COMMAND instance_tests)

  add_executable(instance_probe tests/instance_probe.cpp)
  target_link_libraries(instance_probe PRIVATE tfx_core)
  configure_first_party(instance_probe)

  add_executable(collision_tests tests/collision_test.cpp)
  target_link_libraries(collision_tests PRIVATE tfx_core)
  configure_first_party(collision_tests)
  add_test(NAME world.native_collision_boundaries COMMAND collision_tests)

  add_executable(tfx_tests tests/resource_test.cpp)
  target_link_libraries(tfx_tests PRIVATE tfx_core)
  configure_first_party(tfx_tests)
  add_test(NAME resources.malformed_and_literal COMMAND tfx_tests)

  add_executable(animation_tests tests/animation_test.cpp)
  target_link_libraries(animation_tests PRIVATE tfx_core)
  configure_first_party(animation_tests)
  add_test(NAME actors.native_animation_boundaries COMMAND animation_tests)

  add_executable(timing_tests tests/timing_test.cpp)
  target_link_libraries(timing_tests PRIVATE tfx_core)
  configure_first_party(timing_tests)
  add_test(NAME simulation.native_timing_boundaries COMMAND timing_tests)

  add_executable(acceleration_probe tests/acceleration_probe.cpp)
  target_link_libraries(acceleration_probe PRIVATE tfx_core)
  configure_first_party(acceleration_probe)
  add_test(NAME simulation.native_acceleration_controls COMMAND acceleration_probe ${PROJECT_SOURCE_DIR}/tests/reference/acceleration.txt)

  add_executable(airspace_probe tests/airspace_probe.cpp)
  target_link_libraries(airspace_probe PRIVATE tfx_core)
  configure_first_party(airspace_probe)
  add_test(NAME missions.native_airspace_boundaries COMMAND airspace_probe ${PROJECT_SOURCE_DIR}/tests/reference/airspace.txt)

  add_executable(tfx_replay tests/replay.cpp)
  target_link_libraries(tfx_replay PRIVATE tfx_core)
  configure_first_party(tfx_replay)

  add_executable(mission_tests tests/mission_test.cpp)
  target_link_libraries(mission_tests PRIVATE tfx_core)
  configure_first_party(mission_tests)
  add_test(NAME missions.native_initialisation_boundaries COMMAND mission_tests)

  add_executable(rules_tests tests/rules_test.cpp)
  target_link_libraries(rules_tests PRIVATE tfx_core)
  configure_first_party(rules_tests)
  add_test(NAME missions.native_result_boundaries COMMAND rules_tests)

  add_executable(stores_tests tests/stores_test.cpp)
  target_link_libraries(stores_tests PRIVATE tfx_core)
  configure_first_party(stores_tests)
  add_test(NAME flight.native_store_contributions COMMAND stores_tests)
  set_tests_properties(flight.native_store_contributions PROPERTIES SKIP_RETURN_CODE 77)

  add_executable(stores_probe tests/stores_probe.cpp)
  target_link_libraries(stores_probe PRIVATE tfx_core)
  configure_first_party(stores_probe)

  add_executable(rules_probe tests/rules_probe.cpp)
  target_link_libraries(rules_probe PRIVATE tfx_core)
  configure_first_party(rules_probe)

  add_executable(mission_probe tests/mission_probe.cpp)
  target_link_libraries(mission_probe PRIVATE tfx_core)
  configure_first_party(mission_probe)

  add_executable(surface_tests tests/surface_test.cpp)
  target_link_libraries(surface_tests PRIVATE tfx_core)
  configure_first_party(surface_tests)
  add_test(NAME actors.native_surface_sequences COMMAND surface_tests ${PROJECT_SOURCE_DIR}/tests/reference/surface.txt)

  add_executable(actor_probe tests/actor_probe.cpp)
  target_link_libraries(actor_probe PRIVATE tfx_core)
  configure_first_party(actor_probe)

  add_executable(attitude_probe tests/attitude_probe.cpp)
  target_link_libraries(attitude_probe PRIVATE tfx_core)
  configure_first_party(attitude_probe)

  add_executable(arcade_probe tests/arcade_probe.cpp)
  target_link_libraries(arcade_probe PRIVATE tfx_core)
  configure_first_party(arcade_probe)
  add_executable(basic_engine_probe tests/basic_engine_probe.cpp)
  target_link_libraries(basic_engine_probe PRIVATE tfx_core)
  configure_first_party(basic_engine_probe)
  add_test(NAME flight.native_basic_engine_indicators COMMAND basic_engine_probe "${PROJECT_SOURCE_DIR}/tests/fixtures/basic_engine_indicators.txt")
  set_tests_properties(flight.native_basic_engine_indicators PROPERTIES SKIP_RETURN_CODE 77)
  add_test(NAME flight.native_arcade_sequences COMMAND arcade_probe --check ${PROJECT_SOURCE_DIR}/tests/reference/arcade.txt)
  add_test(NAME flight.native_disabled_sequences COMMAND arcade_probe --check-disabled ${PROJECT_SOURCE_DIR}/tests/reference/disabled.txt)
  add_test(NAME flight.native_ground_sequences COMMAND arcade_probe --check-ground ${PROJECT_SOURCE_DIR}/tests/reference/ground_flight.txt)
  set_tests_properties(flight.native_arcade_sequences flight.native_ground_sequences PROPERTIES SKIP_RETURN_CODE 77)

  add_executable(tfx_probe tests/probe.cpp)
  target_link_libraries(tfx_probe PRIVATE tfx_core)
  configure_first_party(tfx_probe)
endif()
