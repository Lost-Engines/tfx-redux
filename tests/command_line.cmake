if(NOT DEFINED TFX_EXECUTABLE)
  message(FATAL_ERROR "TFX_EXECUTABLE is required")
endif()

# A deliberately absent installation keeps these checks independent of assets,
# audio devices and desktop sessions, including on Windows and macOS runners.
set(missing_install "${CMAKE_CURRENT_BINARY_DIR}/command line missing installation")
if(EXISTS "${missing_install}")
  message(FATAL_ERROR "The command-line fixture path must not contain an installation")
endif()

function(check_cli expected_result expected_text)
  execute_process(
    COMMAND "${TFX_EXECUTABLE}" ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 10
  )
  if(expected_result STREQUAL "success")
    if(NOT result STREQUAL "0")
      message(FATAL_ERROR "${ARGN}: expected success, got ${result}\n${output}${error}")
    endif()
  elseif(result STREQUAL "0")
    message(FATAL_ERROR "${ARGN}: unexpectedly succeeded\n${output}${error}")
  elseif(NOT result MATCHES "^[0-9]+$")
    message(FATAL_ERROR "${ARGN}: crashed or timed out: ${result}\n${output}${error}")
  endif()
  if(NOT "${output}${error}" MATCHES "${expected_text}")
    message(FATAL_ERROR "${ARGN}: expected ${expected_text}\n${output}${error}")
  endif()
endfunction()

execute_process(COMMAND "${TFX_EXECUTABLE}" --help
  RESULT_VARIABLE help_result OUTPUT_VARIABLE help ERROR_VARIABLE error TIMEOUT 10)
if(NOT help_result STREQUAL "0")
  message(FATAL_ERROR "--help failed: ${help_result}\n${error}")
endif()
foreach(option data-dir mission skip-intro language flight-model control joystick
    music cd-cue cd-audio-dir qsound rom-dir scale fullscreen)
  if(NOT help MATCHES "--${option}")
    message(FATAL_ERROR "--${option} is missing from generated help")
  endif()
endforeach()
check_cli(success "--fullscreen" -h)
check_cli(success "--data-dir" --fullscreen --skip-intro --help)
check_cli(success "--scale" --scale=1 --scale 2 --help)
check_cli(failure "unrecognised option" --not-an-option)
check_cli(failure "unrecognised option" --fulls --help)
check_cli(failure "option|positional" unexpected-positional-argument)
check_cli(failure "argument" --data-dir)
check_cli(failure "argument" --mission)
check_cli(failure "argument" --fullscreen=false --help)
check_cli(failure "argument" --skip-intro=true --help)
check_cli(failure "non-negative integer" --mission=-1)
check_cli(failure "non-negative integer" --mission=501tail)
check_cli(failure "non-negative integer" --mission=4294967296)
check_cli(failure "between 0 and 15" --joystick=16)
check_cli(failure "control must be" --control=banana)
check_cli(failure "flight model must be" --flight-model=banana)
check_cli(failure "music must be" --music=banana)
check_cli(failure "qsound must be" --qsound=banana)
check_cli(failure "unknown TFX language" --language=banana)
check_cli(failure "choose either" --cd-cue disc.cue --cd-audio-dir music)
check_cli(failure "TFX needs the original game files" --data-dir "${missing_install}"
  --rom-dir "${missing_install}/ROM path" --mission=501 --music none
  --joystick 1 --control=keyboard --flight-model military-spec --language english
  --qsound off --scale 2 --scale=1 --fullscreen --fullscreen --skip-intro)

if(help MATCHES "--inspect")
  foreach(option career-rules menu presentation inspect flight-steps flight-frames
      environment grid cell scene-radius screenshot frames)
    if(NOT help MATCHES "--${option}")
      message(FATAL_ERROR "Developer --${option} is missing from help")
    endif()
  endforeach()
  check_cli(failure "requires --mission" --career-rules)
  check_cli(failure "--cell expects X,Z" --cell=1)
  check_cli(failure "cell must lie within" --cell=1,200)
  check_cli(failure "scene radius must be" --scene-radius=21)
  check_cli(failure "non-negative integer" --frames=-1)
  check_cli(failure "non-negative integer" --flight-steps=4294967296)
  check_cli(failure "TFX needs the original game files" --data-dir "${missing_install}"
    --mission 501 --career-rules --presentation cdintro:main --menu mastopts
    --inspect --cell=137,113 --scene-radius=8 --environment yugos_n --grid yugoslav
    --flight-steps 0 --flight-frames=0 --frames=1 --screenshot "${missing_install}/still.ppm")
else()
  foreach(option inspect career-rules presentation screenshot frames)
    check_cli(failure "unrecognised option" "--${option}")
  endforeach()
endif()

if(help MATCHES "--resolution")
  foreach(option replay resolution fov draw-distance lod-scale)
    if(NOT help MATCHES "--${option}")
      message(FATAL_ERROR "Redux --${option} is missing from help")
    endif()
  endforeach()
  check_cli(failure "TFX needs the original game files" --data-dir "${missing_install}"
    --resolution=913x617 --resolution auto --scale=2 --fov 90
    --draw-distance=infinite --lod-scale=0.1 --replay "${missing_install}/video.rec")
  if(help MATCHES "--inspect")
    check_cli(failure "requires --mission and --screenshot" --capture-scene scene.txt)
  else()
    check_cli(failure "unrecognised option" --capture-scene scene.txt)
  endif()
else()
  check_cli(failure "non-negative integer" --scale=-1)
  check_cli(failure "between 0.*and 16" --scale=17)
endif()

message(STATUS "Command-line help, syntax, repeated overrides, validation and build-specific options passed")
