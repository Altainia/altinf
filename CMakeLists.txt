cmake_minimum_required(VERSION 3.16)
project(altinf CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Wt REQUIRED COMPONENTS Wt HTTP DBO DBOSQLite3)
find_library(CMARK_LIB   cmark  REQUIRED)
find_library(WT_AUTH_LIB wtauth REQUIRED)

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(altinf ${SOURCES})

target_include_directories(altinf PRIVATE src)

target_link_libraries(altinf PRIVATE
  Wt::Wt Wt::HTTP Wt::DBO Wt::DBOSQLite3
  ${WT_AUTH_LIB}
  ${CMARK_LIB}
)

# Copy resources and posts next to the binary
add_custom_command(TARGET altinf POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:altinf>/resources
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/posts $<TARGET_FILE_DIR:altinf>/posts
)
