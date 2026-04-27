cmake_minimum_required(VERSION 3.16)
project(altinf CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Wt REQUIRED)
find_package(OpenSSL REQUIRED)
find_library(CMARK_LIB cmark REQUIRED)
find_program(SASS_EXECUTABLE sass REQUIRED)

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(altinf ${SOURCES})

target_include_directories(altinf PRIVATE src)

target_link_libraries(altinf PRIVATE
  Wt::Wt Wt::HTTP wtdbo wtdbosqlite3
  OpenSSL::SSL OpenSSL::Crypto
  ${CMARK_LIB}
)

# Compile SCSS → CSS when any .scss source is newer than the output file
file(GLOB_RECURSE SCSS_SOURCES ${CMAKE_SOURCE_DIR}/resources/scss/*.scss)

add_custom_command(
  OUTPUT  ${CMAKE_SOURCE_DIR}/resources/css/altinf.css
  COMMAND ${SASS_EXECUTABLE} --no-source-map
          ${CMAKE_SOURCE_DIR}/resources/scss/altinf.scss
          ${CMAKE_SOURCE_DIR}/resources/css/altinf.css
  DEPENDS ${SCSS_SOURCES}
  COMMENT "Compiling SCSS → CSS"
)

add_custom_target(scss_compile
  DEPENDS ${CMAKE_SOURCE_DIR}/resources/css/altinf.css
)

add_dependencies(altinf scss_compile)

# Copy resources and posts to the build directory.
# ALL + no OUTPUT means this always runs, so the build dir stays current
# even when only SCSS or post files change (not just when the binary relinks).
add_custom_target(copy_resources ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:altinf>/resources
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/posts     $<TARGET_FILE_DIR:altinf>/posts
  COMMENT "Copying resources and posts to build directory"
)
add_dependencies(copy_resources altinf)

enable_testing()
add_subdirectory(tests)
