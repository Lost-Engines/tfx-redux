#pragma once

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
  vec2<int> render_size;
  vec2<int> source_size{render::screen_width,render::screen_height};

public:
  window(unsigned int scale, vec2<int> const &resolution = {render::screen_width,render::screen_height});
  ~window();
  window(window const&) = delete;
  window &operator=(window const&) = delete;
  GLFWwindow *get() const;
  bool focused() const;
  vec2<int> const &resolution() const;
  bool poll();
  std::optional<vec2d> cursor_coordinates() const;
  std::optional<vec2<int16_t>> cursor_position() const;
  void title(std::string const &text);
  void present(std::span<render::rgba_pixel const> pixels, vec2<int> const &size = {render::screen_width,render::screen_height});
};

} // namespace tfx::platform
