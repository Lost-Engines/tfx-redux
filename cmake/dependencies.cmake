find_package(OpenGL REQUIRED)
find_package(glfw3 3.4 CONFIG QUIET)
if(NOT TARGET glfw)
  include(FetchContent)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation")
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples")
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests")
  set(GLFW_INSTALL OFF CACHE BOOL "Install GLFW")
  FetchContent_Declare(glfw
    URL https://codeload.github.com/glfw/glfw/tar.gz/a74efa0d5628b74adc0426af4c5710e287fa7c2c
    URL_HASH SHA256=7e85e39ade1efdda00734d323a0230b5c938074e9fb2ddbe453bdbb281064d2c
  )
  FetchContent_MakeAvailable(glfw)
endif()

include(FetchContent)
find_package(Threads REQUIRED)
FetchContent_Declare(miniaudio
  URL https://codeload.github.com/mackron/miniaudio/tar.gz/f40cf03f80cdb7e741d43e53b7e706e8c1394bcf
  URL_HASH SHA256=412326cf55133404cbfb81ec8974b10149dc68732ce5bdeee8ba9cfc2695d646
  SOURCE_SUBDIR unused_upstream_build
)
FetchContent_MakeAvailable(miniaudio)

set(libmt32emu_SHARED OFF CACHE BOOL "Link Munt into the game")
FetchContent_Declare(munt
  URL https://codeload.github.com/munt/munt/tar.gz/5ee078a761d35cdd3d27000b551c12d11df6a60b
  URL_HASH SHA256=f2c0885a6a9b40329019424afddf800d53b4a85b2f8d23658a246d2883201b0c
  SOURCE_SUBDIR mt32emu
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(munt)

FetchContent_Declare(dosbox_opl
  URL https://codeload.github.com/dosbox-staging/dosbox-staging/tar.gz/e164e788f9819d5ab898d705f863e2046baf8b03
  URL_HASH SHA256=efed2dc8c2807503352e817f3160dcad6236824ba4f16347a260bb9293965c50
  SOURCE_SUBDIR unused_upstream_build
)
FetchContent_MakeAvailable(dosbox_opl)

FetchContent_Declare(nuked_sc55
  URL https://codeload.github.com/jcmoyer/Nuked-SC55/tar.gz/f3464753f64a7da5f5fd3a96fd72197628369589
  URL_HASH SHA256=241cf3ebc1022ce5a519d42b6c318e3c8a8359994e7598227d5e4af050542a76
  SOURCE_SUBDIR unused_upstream_build
)
FetchContent_MakeAvailable(nuked_sc55)

# Source inclusion only: compile the reusable component into tfx_audio.
FetchContent_Declare(qsound
  URL https://codeload.github.com/Lost-Engines/qsound-cpp/tar.gz/9bae937d9797708a7ec971457b416fccf920b29f
  URL_HASH SHA256=f138d25c6cbfd2c3080588d33fd41d6d5b9d753ff98e5c530bddd436e0ca4fab
  SOURCE_SUBDIR unused_upstream_build
)
FetchContent_MakeAvailable(qsound)
include("${qsound_SOURCE_DIR}/qsound/sources.cmake")
