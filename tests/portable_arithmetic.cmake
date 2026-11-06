# Check first-party sources; upstream math-library overloads are not game uses.
foreach(directory IN ITEMS src tests tools)
  file(GLOB_RECURSE sources "${ROOT}/${directory}/*.cpp" "${ROOT}/${directory}/*.h")
  foreach(source IN LISTS sources)
    file(READ "${source}" content)
    if(content MATCHES "(^|[^A-Za-z0-9_])long[ \t\r\n]+double([^A-Za-z0-9_]|$)"
       OR content MATCHES "math/(floating_environment|x87)\\.h"
       OR content MATCHES "[0-9]\\.[0-9]*[lL]([^A-Za-z0-9_]|$)")
      message(FATAL_ERROR "Nonportable arithmetic returned in ${source}")
    endif()
  endforeach()
endforeach()
