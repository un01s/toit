file(GLOB toit_compiler_SRC
    "*.h"
    "*.cc"
    "lsp/*.h"
    "lsp/*.cc"
    "optimizations/*.h"
    "optimizations/*.cc"
)
list(FILTER toit_compiler_SRC EXCLUDE REGEX "/toitc.cc$")

add_subdirectory(third_party/libyaml EXCLUDE_FROM_ALL)
add_subdirectory(third_party/semver EXCLUDE_FROM_ALL)

add_library(
  toit_compiler
  ${toit_compiler_SRC}
  )

target_link_libraries(
  toit_compiler
  ${YAML_STATIC_LIB_NAME}
  ${SEMVER_STATIC_LIB_NAME}
)

add_executable(
  toit.compile
  toitc.cc
  )

target_link_libraries(toit.compile -static-libstdc++)

if ("${CMAKE_SYSTEM_NAME}" MATCHES "MSYS")
  set(TOIT_WINDOWS_LIBS ws2_32 rpcrt4)
elseif ("${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
  set(TOIT_WINDOWS_LIBS rpcrt4)
else()
  set(TOIT_WINDOWS_LIBS )
endif()

target_link_libraries(
  toit.compile
  toit_core
  toit_compiler
  toit_core
  mbedtls
  ${TOIT_WINDOWS_LIBS}
  pthread
  ${CMAKE_DL_LIBS}
  )

if (APPLE)
  set(LN_FORCE "F")
else()
  set(LN_FORCE "Tf")
endif()

set(TOIT_LIB_DIR "${CMAKE_BINARY_DIR}/sdk/lib")

add_custom_command(
  OUTPUT ${TOIT_LIB_DIR}
  COMMAND ln -s${LN_FORCE} ${TOIT_SDK_SOURCE_DIR}/lib ${TOIT_LIB_DIR}
  VERBATIM
)

add_custom_target(
  generate-lib-symlink
  DEPENDS ${TOIT_LIB_DIR}
  )

add_dependencies(toit.compile generate-lib-symlink)
