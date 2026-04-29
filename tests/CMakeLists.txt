find_package(Catch2 3 QUIET)

if(NOT Catch2_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
  )
  FetchContent_MakeAvailable(Catch2)
endif()

include(CTest)
include(Catch)

# test_permissions — header-only, no extra libs
add_executable(test_permissions test_permissions.cpp)
target_include_directories(test_permissions PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_permissions PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(test_permissions)

# test_blog_loader
add_executable(test_blog_loader test_blog_loader.cpp
  ${CMAKE_SOURCE_DIR}/src/blog/blog_loader.cpp)
target_include_directories(test_blog_loader PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_blog_loader PRIVATE Catch2::Catch2WithMain Wt::Wt)
catch_discover_tests(test_blog_loader)

# test_link_db
add_executable(test_link_db test_link_db.cpp
  ${CMAKE_SOURCE_DIR}/src/links/link_db.cpp)
target_include_directories(test_link_db PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_link_db PRIVATE Catch2::Catch2WithMain Wt::Wt wtdbo wtdbosqlite3)
catch_discover_tests(test_link_db)

# test_kanban_db
add_executable(test_kanban_db test_kanban_db.cpp
  ${CMAKE_SOURCE_DIR}/src/kanban/kanban_db.cpp)
target_include_directories(test_kanban_db PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_kanban_db PRIVATE Catch2::Catch2WithMain Wt::Wt wtdbo wtdbosqlite3)
catch_discover_tests(test_kanban_db)

# test_org_db
add_executable(test_org_db test_org_db.cpp
  ${CMAKE_SOURCE_DIR}/src/org/org_db.cpp)
target_include_directories(test_org_db PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_org_db PRIVATE Catch2::Catch2WithMain Wt::Wt wtdbo wtdbosqlite3)
catch_discover_tests(test_org_db)

# test_gantt (JavaScript — requires Node.js)
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
  add_test(
    NAME    gantt_js
    COMMAND ${NODE_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/js/test_gantt.js
  )
else()
  message(STATUS "Node.js not found — skipping gantt JS tests")
endif()

# test_notification_hub — notification_hub with injected synchronous dispatcher
add_executable(test_notification_hub test_notification_hub.cpp
  ${CMAKE_SOURCE_DIR}/src/widgets/notification_hub.cpp)
target_include_directories(test_notification_hub PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_notification_hub PRIVATE Catch2::Catch2WithMain Wt::Wt)
catch_discover_tests(test_notification_hub)

# test_user_db — needs OpenSSL for SHA-256 and token generation
add_executable(test_user_db test_user_db.cpp
  ${CMAKE_SOURCE_DIR}/src/auth/user_db.cpp)
target_include_directories(test_user_db PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_user_db PRIVATE Catch2::Catch2WithMain Wt::Wt wtdbo wtdbosqlite3
  OpenSSL::SSL OpenSSL::Crypto)
catch_discover_tests(test_user_db)
