#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <GLFW/glfw3.h>
#include "render/framebuffer.h"

namespace tfx::platform {

class glfw_runtime {
public:
  glfw_runtime();
  ~glfw_runtime();
  glfw_runtime(glfw_runtime const&) = delete;
  glfw_runtime &operator=(glfw_runtime const&) = delete;
};

class window {
private:
  glfw_runtime runtime;
  std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> handle;
  unsigned int texture{0};
  GLFWkeyfun key_callback{nullptr};
  std::array<bool,2> fullscreen_keys{};
  vec2<int> windowed_position{0,0}, windowed_size{render::display_width,render::display_height};
  bool windowed_maximized{false};
  void set_fullscreen(bool enabled);
  static void dispatch_key(GLFWwindow *host, int key, int scan, int action, int modifiers);
  std::vector<render::rgba_pixel> display_pixels;
  mutable vec2<int> render_size;
  bool follow_window;
  vec2<int> source_size{render::screen_width,render::screen_height};

public:
  window(unsigned int scale, vec2<int> const &resolution = {render::screen_width,render::screen_height}, bool follow_window = false, bool fullscreen = false);
  ~window();
  window(window const&) = delete;
  window &operator=(window const&) = delete;
  GLFWwindow *get() const;
  bool focused() const;
  vec2<int> const &resolution() const;
  // Window shortcuts stay active while modal screens replace their input handler.
  GLFWkeyfun set_key_callback(GLFWkeyfun callback);
  bool key_down(int key) const;
  bool poll();
  // Change render sizing in place; only an explicit scale request resizes the window.
  void configure(vec2<int> const &resolution, bool automatic, std::optional<unsigned int> scale = {});
  vec2d take_scroll();
  std::optional<vec2d> cursor_coordinates() const;
  std::optional<vec2<int16_t>> cursor_position() const;
  void title(std::string const &text);
  void present(std::span<render::rgba_pixel const> pixels, vec2<int> const &size = {render::screen_width,render::screen_height});
  void present_frame(render::framebuffer const &image, render::palette const &colours);
};

} // namespace tfx::platform
