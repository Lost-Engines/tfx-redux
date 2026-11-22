#include <chrono>
#include <iostream>
#include "recording/playback.h"
#include "recording/recorder.h"

namespace {
struct temporary_directory {
  std::filesystem::path path{std::filesystem::temp_directory_path() / ("tfx-recording-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
  temporary_directory() {
    if(!std::filesystem::create_directory(path)) throw std::runtime_error{"cannot create private recording fixture directory"};
  }
  ~temporary_directory() {
    std::error_code error;
    std::filesystem::remove_all(path,error);
  }
};
void require(bool const condition) {
  if(!condition) throw std::runtime_error{"streaming recording assertion failed"};
}
template<typename Action> void rejects(Action const &action) {
  try { action(); } catch(std::runtime_error const&) { return; }
  throw std::runtime_error{"invalid recording stream was accepted"};
}
} // anonymous namespace

int main() try {
  using tfx::recording::decode;
  using tfx::recording::encode_frame;
  using tfx::recording::playback;
  using tfx::recording::recorder;
  using tfx::recording::scene_frame;
  temporary_directory directory;
  auto const path{directory.path / "video.rec"};
  scene_frame first;
  first.data.models.resize(tfx::render::model_queue_capacity);
  for(unsigned int index{0}; index != first.data.models.size(); ++index) {
    first.data.order.push_back({
      .priority{static_cast<int16_t>(index)},
      .slot{static_cast<uint16_t>(index)},
    });
    first.data.models[index].angles = {static_cast<uint16_t>(index * 100),0,65535};
    first.data.models[index].camera_origin = {-12345678,static_cast<int32_t>(index),7654321};
    first.model_names.push_back(index % 2 ? "Folder\\FIRST.3" : "second");
  }
  scene_frame second;
  second.data.order.push_back({
    .priority{-123},
    .slot{0},
  });
  second.data.models.resize(1);
  second.data.models[0].parameters[7] = 200;
  second.model_names = {"replacement.3"};
  recorder output;
  output.begin(path);require(output.active());
  output.append(first);output.append(second);
  require(output.frame_count() == 2);output.finish();require(!output.active());
  auto const encoded{tfx::resources::read_file(path)};
  auto const complete{decode(encoded)};
  require(complete.frames.size() == 2);
  require(encode_frame(complete.frames[0]) == encode_frame(first.data));
  {
    playback movie{path};require(movie.frame_count() == 2);
    auto one{movie.next()},two{movie.next()};
    require(one && two && !movie.next());
    require(encode_frame(one->data) == encode_frame(first.data));
    require(encode_frame(two->data) == encode_frame(second.data));
    require(one->model_names[0] == "second" && one->model_names[1] == "first");
    require(two->model_names == std::vector<std::string>{"replacement"});
  }
  // Starting a fresh capture replaces the old file and resource dictionary.
  output.begin(path);output.append(second);output.finish();
  {
    playback movie{path};require(movie.frame_count() == 1);
    require(movie.next()->model_names == std::vector<std::string>{"replacement"});
    require(!movie.next());
  }
  // Scope exit finalises the file even when its caller returns from flight.
  {
    recorder scoped;scoped.begin(path);scoped.append(first);
  }
  {
    playback movie{path};require(movie.frame_count() == 1 && movie.next().has_value());
  }
  output.begin(path);output.finish();
  {
    playback movie{path};require(movie.frame_count() == 0 && !movie.next());
  }
  rejects([&]{ output.begin(directory.path / "absent" / "video.rec"); });
  auto broken{encoded};broken[0] = 3;                                          // A claimed third frame beyond the body.
  {
    std::ofstream target{path,std::ios::binary};
    target.write(reinterpret_cast<char const*>(broken.data()),static_cast<std::streamsize>(broken.size()));
  }
  {
    playback movie{path};require(movie.next().has_value() && movie.next().has_value());
    rejects([&]{ movie.next(); });
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << '\n';return EXIT_FAILURE;
}
