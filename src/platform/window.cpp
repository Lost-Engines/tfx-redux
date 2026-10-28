#include "window.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include "render/view_settings.h"
#ifdef _WIN32
#include <GL/glext.h>
#endif // _WIN32

namespace tfx::platform {
namespace {
// Modal menus borrow GLFW's user pointer, independently of wheel delivery.
std::unordered_map<GLFWwindow*,vec2d> wheel_offsets;
struct display_area { vec2<int> position; vec2<int> size; };
display_area viewport(int const width, int const height, vec2<int> const &size, bool const integer_scale = true) {
  double const available{std::min(width / static_cast<double>(size.x),height / static_cast<double>(size.y))};
  double const fit{integer_scale && available >= 1 ? std::floor(available) : available};
  vec2<int> const fitted{std::max(1,static_cast<int>(size.x * fit)),std::max(1,static_cast<int>(size.y * fit))};
  return {
    .position{(width - fitted.x) / 2,(height - fitted.y) / 2},
    .size{vec2<int>{fitted}},
  };
}
} // anonymous namespace


glfw_runtime::glfw_runtime() {
  glfwSetErrorCallback([](int const code, char const *const message){ std::cerr << "ERROR: GLFW " << code << ": " << message << std::endl; });
  if(!glfwInit()) throw std::runtime_error{"GLFW could not initialise a desktop display"};
}
glfw_runtime::~glfw_runtime() {
  glfwTerminate();
}

window::window(unsigned int const scale, vec2<int> const &size, bool const follow) : handle{nullptr, glfwDestroyWindow}, render_size{size}, follow_window{follow} {
  /// Match Darker's floating-window sizing and nearest-neighbour presentation; only the final CPU image uses OpenGL
  bool const automatic{scale == 0}, compositor_sizing{automatic && glfwGetPlatform() == GLFW_PLATFORM_WAYLAND};
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
  glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
  glfwWindowHint(GLFW_MAXIMIZED,compositor_sizing ? GLFW_TRUE : GLFW_FALSE);
  auto const initial_scale{static_cast<int>(scale ? scale : 2)};
  // Query the work area using a small hidden window before requesting a
  // potentially much larger render size; the texture is independent of it.
  handle.reset(glfwCreateWindow((automatic || follow_window ? render::screen_width : size.x) * initial_scale,(automatic || follow_window ? render::screen_height : size.y) * initial_scale,"TFX Redux — scene inspection",nullptr,nullptr));
  if(!handle) throw std::runtime_error{"cannot create the TFX window"};
  if(automatic && !compositor_sizing) {
    if(auto *monitor{glfwGetPrimaryMonitor()}) {
      int x{0}, y{0}, width{0}, height{0}, left{0}, top{0}, right{0}, bottom{0};
      glfwGetMonitorWorkarea(monitor,&x,&y,&width,&height);
      glfwGetWindowFrameSize(handle.get(),&left,&top,&right,&bottom);
      auto const fitted{follow_window ? display_area{{0,0},{std::max(1,width - left - right),std::max(1,height - top - bottom)}} : viewport(std::max(1,width - left - right),std::max(1,height - top - bottom),size)};
      glfwSetWindowSize(handle.get(),fitted.size.x,fitted.size.y);
      glfwSetWindowPos(handle.get(),x + left + fitted.position.x,y + top + fitted.position.y);
    }
  }
  glfwShowWindow(handle.get());
  if(compositor_sizing) {
    int width{0}, height{0};
    glfwGetWindowSize(handle.get(),&width,&height);
    glfwRestoreWindow(handle.get());
    auto const deadline{glfwGetTime() + 1};
    while(glfwGetWindowAttrib(handle.get(),GLFW_MAXIMIZED) && glfwGetTime() < deadline) glfwWaitEventsTimeout(.01);
    auto const fitted{follow_window ? display_area{{0,0},{width,height}} : viewport(width,height,size)};
    glfwSetWindowSize(handle.get(),fitted.size.x,fitted.size.y);
  }
  glfwMakeContextCurrent(handle.get());
  glfwSwapInterval(1);
  glGenTextures(1,&texture);
  glBindTexture(GL_TEXTURE_2D,texture);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,render::screen_width,render::screen_height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
  if(glGetError() != GL_NO_ERROR) throw std::runtime_error{"cannot allocate the TFX display texture"};
  glDisable(GL_DITHER);
  glEnable(GL_TEXTURE_2D);
  glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
  wheel_offsets.emplace(handle.get(),vec2d{0,0});
  glfwSetScrollCallback(handle.get(),[](GLFWwindow *target, double x, double y) {
    if(auto entry{wheel_offsets.find(target)}; entry!=wheel_offsets.end()) entry->second += vec2d{x,y};
  });
  glfwSetWindowFocusCallback(handle.get(),[](GLFWwindow *const target, int const focused){
    if(!focused) glfwSetInputMode(target,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
  });
}

window::~window() {
  wheel_offsets.erase(handle.get());
  glDeleteTextures(1,&texture);
}
GLFWwindow *window::get() const {
  return handle.get();
}
bool window::focused() const {
  return glfwGetWindowAttrib(handle.get(),GLFW_FOCUSED) == GLFW_TRUE;
}
vec2d window::take_scroll() { return std::exchange(wheel_offsets.at(handle.get()),vec2d{0,0}); }
bool window::poll() {
  glfwPollEvents();
  return !glfwWindowShouldClose(handle.get());
}
std::optional<vec2d> window::cursor_coordinates() const {
  int width{0}, height{0}, framebuffer_width{0}, framebuffer_height{0};
  glfwGetWindowSize(handle.get(),&width,&height);
  glfwGetFramebufferSize(handle.get(),&framebuffer_width,&framebuffer_height);
  if(width <= 0 || height <= 0 || framebuffer_width <= 0 || framebuffer_height <= 0) return {};
  double x{0}, y{0};
  glfwGetCursorPos(handle.get(),&x,&y);
  auto const area{viewport(framebuffer_width,framebuffer_height,source_size,!follow_window)};
  x = x * framebuffer_width / width - area.position.x;
  y = y * framebuffer_height / height - (framebuffer_height - area.position.y - area.size.y);
  render::interface_layout const layout{source_size};
  return (vec2d{x * source_size.x / area.size.x,y * source_size.y / area.size.y} - layout.offset) / layout.scale;
}
std::optional<vec2<int16_t>> window::cursor_position() const {
  auto const position{cursor_coordinates()};
  if(!position || position->x < 0 || position->y < 0 || position->x >= render::screen_width || position->y >= render::screen_height) return {};
  return vec2<int16_t>{*position};
}
void window::title(std::string const &text) {
  glfwSetWindowTitle(handle.get(),text.c_str());
}
vec2<int> const &window::resolution() const {
  if(follow_window) {
    int width{0}, height{0};glfwGetFramebufferSize(handle.get(),&width,&height);
    if(width > 0 && height > 0) {
      render::framebuffer::pixel_count({width,height});
      render_size = {width,height};
    }
  }
  return render_size;
}
void window::configure(vec2<int> const &size, bool const automatic, std::optional<unsigned int> const scale) {
  render::framebuffer::pixel_count(size);
  if(scale && *scale>16) throw std::invalid_argument{"window scale must be 0..16"};
  render_size=vec2<int>{size};follow_window=automatic;
  if(!scale) return;
  if(*scale) {
    glfwRestoreWindow(handle.get());
    auto const base{automatic ? vec2<int>{render::screen_width,render::screen_height} : vec2<int>{size}};
    glfwSetWindowSize(handle.get(),base.x*static_cast<int>(*scale),base.y*static_cast<int>(*scale));
  } else if(glfwGetPlatform()==GLFW_PLATFORM_WAYLAND) {
    // The compositor chooses the work area and monitor; Wayland does not
    // expose window positions. Automatic rendering follows its size event.
    glfwMaximizeWindow(handle.get());
  } else {
    glfwRestoreWindow(handle.get());
    int x{0},y{0},width{0},height{0};glfwGetWindowPos(handle.get(),&x,&y);glfwGetWindowSize(handle.get(),&width,&height);
    auto *monitor{glfwGetPrimaryMonitor()};int count{0};auto **monitors{glfwGetMonitors(&count)};
    for(int i=0;i<count;++i) {
      int mx{0},my{0},mw{0},mh{0};glfwGetMonitorWorkarea(monitors[i],&mx,&my,&mw,&mh);
      if(x+width/2>=mx && x+width/2<mx+mw && y+height/2>=my && y+height/2<my+mh) {monitor=monitors[i];break;}
    }
    if(!monitor) return;
    int left{0},top{0},right{0},bottom{0};glfwGetMonitorWorkarea(monitor,&x,&y,&width,&height);
    glfwGetWindowFrameSize(handle.get(),&left,&top,&right,&bottom);
    width=std::max(1,width-left-right);height=std::max(1,height-top-bottom);
    auto const fitted{automatic ? display_area{{0,0},{width,height}} : viewport(width,height,size)};
    glfwSetWindowSize(handle.get(),fitted.size.x,fitted.size.y);
    glfwSetWindowPos(handle.get(),x+left+fitted.position.x,y+top+fitted.position.y);
  }
}
void window::present(std::span<render::rgba_pixel const> const pixels, vec2<int> const &size) {
  if(pixels.size() != static_cast<size_t>(size.x) * static_cast<size_t>(size.y)) throw std::invalid_argument{"display pixels do not match their resolution"};
  glBindTexture(GL_TEXTURE_2D,texture);
  if(source_size != size) {
    source_size = vec2<int>{size};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,size.x,size.y,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    if(glGetError() != GL_NO_ERROR) throw std::runtime_error{"cannot allocate the display resolution"};
  }
  int width{0}, height{0};
  glfwGetFramebufferSize(handle.get(),&width,&height);
  if(width <= 0 || height <= 0) return;
  auto const area{viewport(width,height,source_size,!follow_window)};
  glViewport(0,0,width,height);
  glClearColor(0,0,0,1);
  glClear(GL_COLOR_BUFFER_BIT);
  glViewport(area.position.x,area.position.y,area.size.x,area.size.y);
  glBindTexture(GL_TEXTURE_2D,texture);
  glTexSubImage2D(GL_TEXTURE_2D,0,0,0,size.x,size.y,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
  glBegin(GL_TRIANGLE_STRIP);
  glTexCoord2f(0,0); glVertex2f(-1,1);
  glTexCoord2f(0,1); glVertex2f(-1,-1);
  glTexCoord2f(1,0); glVertex2f(1,1);
  glTexCoord2f(1,1); glVertex2f(1,-1);
  glEnd();
  glfwSwapBuffers(handle.get());
}

} // namespace tfx::platform
