# The portable distribution is an explicit list; never include local game data.
install(TARGETS tfx RUNTIME DESTINATION . COMPONENT Runtime)
install(FILES scripts/roms.txt scripts/game-files.txt scripts/cd-cue.txt
  DESTINATION scripts COMPONENT Runtime)
install(FILES README.md LICENSE NOTICE.md DESTINATION . COMPONENT Runtime)
install(FILES docs/controls.md docs/build-release.md docs/recording.md docs/installation.md
  DESTINATION docs COMPONENT Runtime)
if(WIN32)
  install(PROGRAMS scripts/fetch-assets.bat DESTINATION scripts COMPONENT Runtime)
  install(CODE [[
    file(GLOB_RECURSE scripts
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/scripts/*.bat"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/scripts/*.txt")
    foreach(script IN LISTS scripts)
      file(READ "${script}" contents)
      string(REPLACE "\r\n" "\n" contents "${contents}")
      string(REPLACE "\n" "\r\n" contents "${contents}")
      file(WRITE "${script}" "${contents}")
    endforeach()
  ]] COMPONENT Runtime)
else()
  install(PROGRAMS scripts/fetch-assets.sh DESTINATION scripts COMPONENT Runtime)
endif()

# Preserve the notices shipped with the statically linked hardware/window cores.
install(FILES "${munt_SOURCE_DIR}/mt32emu/COPYING.txt"
  "${munt_SOURCE_DIR}/mt32emu/COPYING.LESSER.txt"
  DESTINATION licenses/munt COMPONENT Runtime)
install(FILES "${dosbox_opl_SOURCE_DIR}/COPYING"
  DESTINATION licenses/dosbox COMPONENT Runtime)
install(FILES "${nuked_sc55_SOURCE_DIR}/LICENSE"
  DESTINATION licenses/nuked-sc55 COMPONENT Runtime)
install(FILES "${miniaudio_SOURCE_DIR}/LICENSE"
  DESTINATION licenses/miniaudio COMPONENT Runtime)
install(FILES "${qsound_SOURCE_DIR}/LICENSE" "${qsound_SOURCE_DIR}/NOTICE.md"
  DESTINATION licenses/qsound COMPONENT Runtime)
if(DEFINED glfw_SOURCE_DIR AND EXISTS "${glfw_SOURCE_DIR}/LICENSE.md")
  install(FILES "${glfw_SOURCE_DIR}/LICENSE.md"
    DESTINATION licenses/glfw COMPONENT Runtime)
endif()
