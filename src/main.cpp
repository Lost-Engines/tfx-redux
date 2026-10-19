#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include "audio/runtime.h"
#include "audio/sc55_synth.h"
#include "platform/audio_output.h"
#include "platform/flight.h"
#include "platform/recording.h"
#include "platform/presentations.h"
#include "resources/installation.h"
#include "world/environment.h"

namespace {

// Native LANG values 1..5 index the directory table at DATA:AB1C.
std::array<std::string_view,5> constexpr language_names{"english","french","german","italian","spanish"};

unsigned int number(std::string_view const text) {
  unsigned int result{0};
  auto const [end,error]{std::from_chars(text.data(),text.data() + text.size(),result)};
  if(error != std::errc{} || end != text.data() + text.size()) throw std::runtime_error{"expected a non-negative integer: " + std::string{text}};
  return result;
}

} // anonymous namespace

auto main(int const argc, char const *const argv[])->int try {
  std::optional<std::filesystem::path> data_directory;
  std::optional<std::filesystem::path> rom_directory;
  std::optional<std::filesystem::path> cd_cue, cd_audio_directory;
  std::optional<uint8_t> music_override;
  tfx::audio::sound_canvas_model canvas_model{tfx::audio::sound_canvas_model::scc1a};
  std::string mission_name{"miss501"}, override_environment, override_grid;
  tfx::render::view_settings video;
  unsigned int scale{0}, scene_radius{6}, frame_limit{0}, flight_steps{0}, flight_frames{0};
  tfx::simulation::flight_model model{tfx::simulation::flight_model::arcade};
  bool inspect{false}, direct_mission{false}, skip_intro{false}, model_selected{false}, language_selected{false};
  bool career_rules{false};
  std::optional<tfx::flight::control_source> control_override;
  std::optional<unsigned int> joystick;
  std::string presentation, presentation_function, language{"english"};
  std::optional<vec2<unsigned int>> override_cell;
  std::optional<std::filesystem::path> screenshot, replay;
  for(int i{1}; i != argc; ++i) {
    std::string_view const argument{argv[i]};
    auto const equals{argument.find('=')};
    auto const key{argument.substr(0,equals)};
    auto const value{[&]()->std::string_view {
      if(equals != std::string_view::npos) return argument.substr(equals + 1);
      if(i + 1 == argc) throw std::runtime_error{"missing value for " + std::string{key}};
      return argv[++i];
    }};
    if(key == "--help") {
      std::cout << "TFX Redux — enhanced native flight simulation\n"
                   "Usage: tfx [--data-dir DIRECTORY] [--mission 501] [--scale N]\n"
                   "  --data-dir DIRECTORY  Original TFX 2.05 installation (DID.DAT and TFX.DAT)\n"
                   "  --mission N           Enter a mission directly instead of the original front end\n"
                   "  --replay FILE         Play a saved flight recording; --skip-intro omits the WNN prelude\n"
                   "  --skip-intro          Start at pilot enrolment without the opening sequence\n"
                   "  --language NAME       english, french, german, italian or spanish\n"
                   "  --flight-model NAME   arcade, simple, military-spec or realistic; overrides TFX.CFG\n"
                   "  --control NAME        keyboard, mouse or joystick; overrides TFX.CFG\n"
                   "  --joystick N          Select joystick 0..15 (default: first available); enables joystick input\n"
                   "  --music NAME          none, soundblaster-cd, soundblaster-fm, roland-lapc, roland-scc1a or roland-sc55\n"
                   "  --cd-cue FILE         Play CD music from a CUE/BIN image (default: fetched disc)\n"
                   "  --cd-audio-dir DIR    Play numbered local FLAC, Ogg, MP3 or WAV CD tracks\n"
                   "  --rom-dir DIRECTORY   Override automatic Roland ROM lookup\n"
                   "  --resolution WxH      Render size in square pixels (default: 1280x800)\n"
                   "  --fov DEGREES         Horizontal field of view, 10..170 (original/default: 64.01)\n"
                   "  --draw-distance N     Terrain radius in cells (default: 16; original: 8); 0/infinite draws the whole map\n"
                   "  --lod-scale N         Visual LOD distance multiplier; 0 disables cutoffs (default), 1 uses original thresholds\n"
                   "  --scale N             Final image pixel scale; 0 fits a floating window to the desktop (default)\n"
                   "\nGame files: current directory, then " << tfx::resources::default_game_directory().string() << "\n"
                   "ROMs: selected game directory, then the same default asset directory.\n"
                   "\nFlight: hold left/right mouse or =/- for thrust.\n"
                   "Arcade uses mouse steering; other models use the original arrow-key controls.\n"
                   "Keypad * raises afterburner; Alt+keypad * or keypad - lowers it; / switches it off.\n"
                   "[ and ] toggle engines; G gear; H arrestor hook; W wheel brakes; B air brakes; Alt+B brake chute; Shift+B cut away.\n"
                   "E/S toggle ECM/stealth on Eurofighter/F-22; Insert releases chaff; Delete releases a flare.\n"
                   "Enter selects air-to-air weapons; Backspace/O air-to-ground; F/Ctrl selects cannon/navigation.\n"
                   "C cycles radar/emitter targets; Z opens/locks laser designator; arrows aim; comma/period zoom.\n"
                   "X clears targets; F1 returns to flight view; Space fires; Alt+J jettisons ground stores/tanks; T toggles accelerated time; P pauses; backtick opens configuration; double Escape ejects; Shift+Q opens quit/enrol confirmation.\n"
                   "M opens the tactical map; any key returns. D toggles external flight information; Shift+D restores default stores and countermeasures.\n"
                   "1/2/3 cycle the MFDs; R changes radar range; N selects the ground-target preview.\n"
                   "Shift+N toggles infrared night vision; Y/U adjust HUD brightness; apostrophe opens/closes the refuelling hookup.\n"
                   "I or backslash toggles ILS; Alt+L requests automatic landing.\n"
                   "A toggles autopilot; 7/8/9/0 choose waypoint/heading/track/speed hold; arrows adjust targets.\n"
                   "5 toggles stick pressure; 6 selects cruise speed; Tab toggles recovery.\n"
                   "Page Up/Down or keypad 9/3 raise and lower the cockpit.\n"
                   "Right Shift+arrows turn the view; +Home/End change distance; +Page Up/Down roll supported views.\n"
                   "Ctrl+arrows/Page Up/Down move the chase camera. Keypad navigation keys also work.\n"
                   "4 saves a numbered GRAB_00.LBM screenshot in the game directory.\n"
                   "Alt+V starts/stops recording; choose its filename in configuration. WNN replay follows mission completion.\n"
                   "F1 cockpit; Shift+F1 full forward; F2/F3 look left/right; F4 rear; F5 chase.\n"
                   "F6 fly-past; F7 tactical; F8 fired-store camera (twice: nose view).\n"
                   "F9 target observer; F10 virtual cockpit/target tracking.\n";
#ifndef NDEBUG
      std::cout << "\nDeveloper options:\n"
                   "  --career-rules        Use career permissions/stores with a direct --mission launch\n"
                   "  --menu NAME            Run an original options menu\n"
                   "  --presentation NAME[:FUNCTION]  Play an original presentation graph\n"
                   "  --inspect             Free scene camera: WASD, R/F, arrows/right-drag, Shift\n"
                   "  --flight-steps N      Advance N four-tick physics steps without intermediate drawing\n"
                   "  --flight-frames N     Advance N complete four-tick flight frames before capture\n"
                   "  --environment NAME    Override mission lighting/weather\n"
                   "  --grid NAME           Inspect an alternate stored terrain grid\n"
                   "  --cell X,Z            Inspect a particular terrain cell\n"
                   "  --scene-radius N      Inspection neighbourhood in tiles (default 6)\n"
                   "  --screenshot FILE     Save the current scene as a PPM image without opening a window\n"
                   "  --frames N            Exit the window after N frames (0 is unlimited)\n";
#endif // NDEBUG
      return EXIT_SUCCESS;
    }
    if(key == "--data-dir") data_directory = value();
    else if(key == "--rom-dir") rom_directory = value();
    else if(key == "--cd-cue") cd_cue = value();
    else if(key == "--cd-audio-dir") cd_audio_directory = value();
    else if(key == "--music") {
      auto const selected{value()};
      if(selected == "none") music_override = 0;
      else if(selected == "soundblaster-cd" || selected == "cd") music_override = 1;
      else if(selected == "soundblaster-fm") music_override = 3;
      else if(selected == "roland-lapc") music_override = 2;
      else if(selected == "roland-scc1a") { music_override = 4;canvas_model = tfx::audio::sound_canvas_model::scc1a; }
      else if(selected == "roland-sc55") { music_override = 4;canvas_model = tfx::audio::sound_canvas_model::sc55; }
      else throw std::runtime_error{"music must be none, soundblaster-cd, soundblaster-fm, roland-lapc, roland-scc1a or roland-sc55"};
    }
    else if(key == "--language") {
      language = value();
      language_selected = true;
      if(std::ranges::find(language_names,language) == language_names.end()) throw std::runtime_error{"unknown TFX language: " + language};
    }
    else if(key == "--replay") replay = value();
    else if(key == "--skip-intro") skip_intro = true;
    else if(key == "--mission") {
      direct_mission = true;
      auto const selected{value()};
      mission_name = "miss" + std::to_string(number(selected));
    } else if(key == "--joystick") {
      joystick = number(value());
      if(*joystick > GLFW_JOYSTICK_LAST) throw std::runtime_error{"joystick index must be between 0 and 15"};
      control_override = tfx::flight::control_source::joystick;
    } else if(key == "--control") {
      auto const selected{value()};
      if(selected == "keyboard") control_override = tfx::flight::control_source::keyboard;
      else if(selected == "mouse") control_override = tfx::flight::control_source::mouse;
      else if(selected == "joystick") control_override = tfx::flight::control_source::joystick;
      else throw std::runtime_error{"control must be keyboard, mouse or joystick"};
    } else if(key == "--flight-model") {
      model_selected = true;
      auto const selected{value()};
      if(selected == "arcade") model = tfx::simulation::flight_model::arcade;
      else if(selected == "simple") model = tfx::simulation::flight_model::simple;
      else if(selected == "military-spec") model = tfx::simulation::flight_model::military_spec;
      else if(selected == "realistic") model = tfx::simulation::flight_model::realistic;
      else throw std::runtime_error{"flight model must be arcade, simple, military-spec or realistic"};
    } else if(key == "--resolution") {
      auto const text{value()};auto const separator{text.find_first_of("xX")};
      if(separator == text.npos) throw std::invalid_argument{"resolution must be WIDTHxHEIGHT"};
      auto const width{number(text.substr(0,separator))}, height{number(text.substr(separator + 1))};
      if(width > 16384 || height > 16384) throw std::invalid_argument{"resolution dimensions must be at most 16384"};
      video.resolution = {static_cast<int>(width),static_cast<int>(height)};
      tfx::render::framebuffer checked;checked.resize(video.resolution);
    } else if(key == "--fov" || key == "--lod-scale") {
      auto const text{value()};double parsed{0};
      auto const [end,error]{std::from_chars(text.data(),text.data() + text.size(),parsed)};
      if(error != std::errc{} || end != text.data() + text.size() || !std::isfinite(parsed)) throw std::invalid_argument{"expected a finite number for " + std::string{key}};
      if(key == "--fov") {
        if(parsed < 10 || parsed > 170) throw std::invalid_argument{"FOV must be between 10 and 170 degrees"};
        video.field_of_view = parsed;
      } else {
        if(parsed < 0 || parsed > 100) throw std::invalid_argument{"LOD scale must be between 0 and 100"};
        video.lod_scale = parsed;
      }
    } else if(key == "--draw-distance") {
      auto const text{value()};video.draw_distance = text == "infinite" ? 0 : number(text);
      if(video.draw_distance > 10000) throw std::invalid_argument{"draw distance must be 0..10000 cells, or infinite"};
    } else if(key == "--scale") {
      scale = number(value());
      if(scale > 16) throw std::runtime_error{"window scale must be between 0 (automatic) and 16"};
    }
#ifndef NDEBUG
    else if(key == "--career-rules") career_rules = true;
    else if(key == "--menu") presentation = "menu:" + std::string{value()};
    else if(key == "--presentation") {
      auto const selection{value()};
      auto const colon{selection.find(':')};
      presentation = selection.substr(0,colon);
      if(colon != std::string_view::npos) presentation_function = selection.substr(colon + 1);
    }
    else if(key == "--inspect") inspect = true;
    else if(key == "--flight-steps") flight_steps = number(value());
    else if(key == "--flight-frames") flight_frames = number(value());
    else if(key == "--environment") override_environment = value();
    else if(key == "--grid") override_grid = value();
    else if(key == "--cell") {
      auto const cell{value()};
      auto const comma{cell.find(',')};
      if(comma == std::string_view::npos) throw std::runtime_error{"--cell expects X,Z"};
      inspect = true;
      override_cell = vec2<unsigned int>{number(cell.substr(0,comma)),number(cell.substr(comma + 1))};
      if(override_cell->x >= tfx::world::map_width || override_cell->y >= tfx::world::map_height) throw std::runtime_error{"cell must lie within 0..199 on each axis"};
    } else if(key == "--scene-radius") {
      scene_radius = number(value());
      if(scene_radius < 1 || scene_radius > 20) throw std::runtime_error{"scene radius must be between 1 and 20 tiles"};
    } else if(key == "--screenshot") screenshot = value();
    else if(key == "--frames") frame_limit = number(value());
#endif // NDEBUG
    else throw std::runtime_error{"unrecognised option: " + std::string{key} + "; use --help"};
  }
  if(cd_cue && cd_audio_directory) throw std::runtime_error{"choose either --cd-cue or --cd-audio-dir"};
  if(career_rules && !direct_mission) throw std::runtime_error{"--career-rules requires --mission"};
  auto const archive_path{tfx::resources::select_game_archive(data_directory,cd_cue)};
  auto preferences{tfx::configuration::load(archive_path.parent_path())};
  if(model_selected) preferences.model = model;
  if(music_override) preferences.music = *music_override;
  else if(cd_cue || cd_audio_directory) preferences.music = 1;
  if(control_override) preferences.preferred_control = preferences.control = *control_override;
  if(control_override == tfx::flight::control_source::joystick && preferences.model == tfx::simulation::flight_model::arcade) throw std::runtime_error{"joystick steering requires Simple, Military-spec or Realistic flight; Arcade uses relative mouse input"};
  if(control_override == tfx::flight::control_source::mouse && preferences.model == tfx::simulation::flight_model::simple) throw std::runtime_error{"the original Simple model has no mouse steering; choose keyboard or joystick"};
  if(!language_selected && preferences.language >= 1 && preferences.language <= language_names.size()) language = language_names[preferences.language - 1];
  tfx::resources::archive const game{archive_path};
  tfx::resources::executable_tables const tables{archive_path.parent_path() / "TFX.DAT"};
  std::unique_ptr<tfx::audio::runtime> sound;
  std::unique_ptr<tfx::platform::audio_output> output;
  if(!screenshot && !inspect && !replay) {
    std::unique_ptr<tfx::audio::cd_player> cd;
    if(preferences.music == 1) {
      if(cd_cue || cd_audio_directory) cd = std::make_unique<tfx::audio::cd_player>(cd_cue ? *cd_cue : *cd_audio_directory,tfx::platform::audio_output::sample_rate);
      else {
        for(auto const &directory : {archive_path.parent_path(),tfx::resources::default_game_directory()}) {
          auto const cue{directory / "cd" / "tfx.cue"};
          if(std::filesystem::is_regular_file(cue)) {
            try { cd = std::make_unique<tfx::audio::cd_player>(cue,tfx::platform::audio_output::sample_rate); }
            catch(std::exception const &error) { std::cerr << "CD image unavailable: " << error.what() << "; rerun scripts/fetch-assets to repair the disc.\n"; }
            break;
          }
        }
      }
    }
    std::unique_ptr<tfx::audio::music_player> music;
    if(preferences.music >= 1 && preferences.music <= 4) {
      try {
        auto const launcher{tfx::resources::read_file(archive_path.parent_path() / "START.EXE")};
        if(preferences.music == 2 || preferences.music == 4) {
          std::string failure;
          for(auto const &directory : tfx::resources::rom_directories(archive_path.parent_path(),rom_directory)) {
            try {
              if(preferences.music == 2) music = std::make_unique<tfx::audio::music_player>(game,launcher,directory,tfx::platform::audio_output::sample_rate);
              else music = std::make_unique<tfx::audio::music_player>(game,launcher,
                tfx::audio::make_scc_music_device(launcher,std::make_unique<tfx::audio::sc55_synth>(directory,tfx::platform::audio_output::sample_rate,canvas_model)),tfx::platform::audio_output::sample_rate);
              break;
            } catch(std::exception const &error) { failure = error.what(); }
          }
          if(!music) throw std::runtime_error{failure};
        } else music = std::make_unique<tfx::audio::music_player>(game,launcher,
          tfx::audio::make_fm_music_device(launcher,tfx::platform::audio_output::sample_rate),tfx::platform::audio_output::sample_rate);
      } catch(std::exception const &error) {
        std::cerr << "Music unavailable: " << error.what();
        if(preferences.music == 2 || preferences.music == 4) std::cerr << "; run "
#ifdef _WIN32
          "scripts\\fetch-assets.bat"
#else
          "scripts/fetch-assets.sh"
#endif // _WIN32
          " to fetch the required Roland ROMs";
        std::cerr << ".\n";
      }
    }
    if(music) music->configure(static_cast<tfx::audio::music_mode>(preferences.music),preferences.sound_effects,tables.music_orders);
    std::optional<tfx::audio::gus_parameters> gravis;
    auto effects_device{static_cast<tfx::audio::sound_device>(preferences.sound_effects)};
    if(preferences.sound_effects == 4) {
      try { gravis = tfx::audio::load_gus_parameters(tfx::resources::read_file(archive_path.parent_path() / "GUS.COM")); }
      catch(std::exception const &error) { std::cerr << "Gravis effects unavailable: " << error.what() << '\n';effects_device = tfx::audio::sound_device::none; }
    }
    if(preferences.sound_effects || music || cd) {
      try {
        sound = std::make_unique<tfx::audio::runtime>(tables.samples,game,tfx::platform::audio_output::sample_rate,effects_device,std::move(music),gravis,std::move(cd),static_cast<tfx::audio::music_mode>(preferences.music),preferences.music != 0);
        output = std::make_unique<tfx::platform::audio_output>([](void *const user, std::span<float> const pcm) noexcept {
          static_cast<tfx::audio::runtime*>(user)->render(pcm);
        },sound.get());
      } catch(std::exception const &error) {
        std::cerr << "Sound output unavailable: " << error.what() << '\n';
        sound.reset();
      }
    }
  }
  if(!presentation.empty()) {
    tfx::platform::play_presentation(game,tables,presentation,presentation_function,language,scale,frame_limit,screenshot,sound.get());
    return EXIT_SUCCESS;
  }
  tfx::world::flak_state flak_state;
  tfx::world::sky_state sky_state;
  tfx::world::animation_clock scenery_clock;
  tfx::sensors::air_lock seeker;
  vec2<int16_t> projection_origin{tfx::render::centre_x - 1,tfx::render::centre_y - 1};
  tfx::render::horizon_phase ground_phase;
  vec3<int32_t> ground_reference{0,0,0};
  tfx::actors::air_contact player_contact{
    .position{0,0,0},
    .angles{0,0,0},
    .speed{0},
  };
  tfx::render::precipitation rain;
  tfx::math::random_state session_random;
  int16_t nearby_hostile_count{0};
  int16_t last_released_mount{0};
  bool flight_information{true};
  bool cannon_navigation{true};
  tfx::cockpit::tactical_map_animation map_animation;
  tfx::flight::cheat_code cheat;
  tfx::missions::inherited_setup mission_state;
  auto recording_name{tables.configuration_widgets().recording_name};
  tfx::weapons::pylon_inventory launch_inventory;
  launch_inventory.back() = {
    .weapon{std::to_underlying(tfx::weapons::weapon_id::cannon)},
  };
  tfx::platform::flight_options const flight{
    .mission{mission_name},
    .data_directory{archive_path.parent_path()},
    .environment{override_environment},
    .grid{override_grid},
    .cell{override_cell ? std::optional<vec2<unsigned int>>{std::in_place,*override_cell} : std::nullopt},
    .screenshot{screenshot},
    .joystick{joystick},
    .scale{scale},
    .scene_radius{scene_radius},
    .frame_limit{frame_limit},
    .flight_steps{flight_steps},
    .flight_frames{flight_frames},
    .preferences{preferences},
    .flak{flak_state},
    .sky{sky_state},
    .scenery_clock{scenery_clock},
    .seeker{seeker},
    .projection_origin{projection_origin},
    .ground_phase{ground_phase},
    .ground_reference{ground_reference},
    .player_contact{player_contact},
    .nearby_hostile_count{nearby_hostile_count},
    .last_released_mount{last_released_mount},
    .flight_information{flight_information},
    .cannon_navigation{cannon_navigation},
    .map_animation{map_animation},
    .cheat{cheat},
    .mission_state{mission_state},
    .recording_name{recording_name},
    .launch_inventory{launch_inventory},
    .rain{rain},
    .session_random{session_random},
    .sound{sound.get()},
    .inspect{inspect},
    .video{video},
  };
  if(replay) {
    tfx::world::mission const selected{game,mission_name};
    tfx::world::environment const environment{game,selected.grid_name(),selected.environment};
    tfx::world::assets assets{game};tfx::world::scene scene{environment,assets};
    tfx::render::software_renderer renderer{tables};tfx::platform::window window{scale,video.resolution};
    tfx::render::dac_palette infrared{};
    auto const colours{game.read("colours/" + environment.lighting.infrared_palette_name + ".col")};
    std::ranges::copy(colours,infrared.begin());
    tfx::platform::replay_recording(window,game,tables,scene,renderer,*replay,0,0,infrared,!skip_intro);
  } else if(direct_mission || inspect || flight_steps || flight_frames || !override_environment.empty() || !override_grid.empty()) {
    auto const mission_digits{std::string_view{mission_name}.substr(std::string_view{"miss"}.size())};
    tfx::missions::variables campaign{
      {"mission",static_cast<int32_t>(number(mission_digits))},
      {"arcade",career_rules ? 0 : 1},
    };
    tfx::platform::fly_mission(game,tables,flight,campaign);
  } else tfx::platform::play_game(game,tables,archive_path.parent_path(),language,skip_intro,flight);
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}
