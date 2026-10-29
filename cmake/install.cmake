# The portable distribution is an explicit list; never include local game data.
install(TARGETS tfx RUNTIME DESTINATION . COMPONENT Runtime)
install(FILES scripts/roms.txt scripts/game-files.txt scripts/cd-cue.txt
  DESTINATION scripts COMPONENT Runtime)
install(FILES README.md LICENSE NOTICE.md DESTINATION . COMPONENT Runtime)
install(FILES docs/controls.md docs/build-release.md DESTINATION docs COMPONENT Runtime)
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
