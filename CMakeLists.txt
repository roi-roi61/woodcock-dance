cmake_minimum_required(VERSION 3.24)

project(WoodcockDance VERSION 0.3.4 LANGUAGES CXX)

add_executable(WoodcockDance WIN32
  src/main.cpp
  src/resource.h
  src/resources.rc
)

target_compile_features(WoodcockDance PRIVATE cxx_std_17)

target_compile_definitions(WoodcockDance PRIVATE
  UNICODE
  _UNICODE
  NOMINMAX
)

target_compile_options(WoodcockDance PRIVATE /utf-8)

target_link_libraries(WoodcockDance PRIVATE
  gdiplus
  user32
  gdi32
  shell32
  advapi32
  ole32
)

set(WOODCOCK_ASSETS
  assets/woodcock-base.png
  assets/variant-01-backpack.png
  assets/variant-02-scarf.png
  assets/variant-03-cafe.png
  assets/variant-04-classic.png
  assets/variant-05-skate.png
  assets/variant-06-hoverboard.png
)

set_target_properties(WoodcockDance PROPERTIES
  OUTPUT_NAME "WoodcockDance"
  WIN32_EXECUTABLE TRUE
)

add_custom_command(TARGET WoodcockDance POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:WoodcockDance>/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/assets/woodcock-base.png"
    "$<TARGET_FILE_DIR:WoodcockDance>/assets/woodcock-base.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/assets/variant-01-backpack.png"
    "$<TARGET_FILE_DIR:WoodcockDance>/assets/variant-01-backpack.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/assets/variant-02-scarf.png"
    "$<TARGET_FILE_DIR:WoodcockDance>/assets/variant-02-scarf.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/assets/variant-03-cafe.png"
    "$<TARGET_FILE_DIR:WoodcockDance>/assets/variant-03-cafe.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/assets/variant-04-classic.png"
    "$<TARGET_FILE_DIR:WoodcockDance>/assets/variant-04-classic.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/assets/variant-05-skate.png"
    "$<TARGET_FILE_DIR:WoodcockDance>/assets/variant-05-skate.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/assets/variant-06-hoverboard.png"
    "$<TARGET_FILE_DIR:WoodcockDance>/assets/variant-06-hoverboard.png"
)

install(TARGETS WoodcockDance RUNTIME DESTINATION .)
install(FILES ${WOODCOCK_ASSETS} DESTINATION assets)
