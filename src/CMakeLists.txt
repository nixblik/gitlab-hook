add_executable(gitlab-hook
  main.cpp
  config.h config.cpp
  log.h log.cpp
  io_context.h io_context.cpp
  signal_listener.h signal_listener.cpp
  watchdog.h watchdog.cpp
  http_server.h http_server.cpp
  hook.h hook.cpp
  pipeline_hook.h pipeline_hook.cpp
  process.h process.cpp
  action_list.h action_list.cpp)
target_compile_definitions(gitlab-hook PRIVATE
  EXECUTABLE="gitlab-hook"
  VERSION="${CMAKE_PROJECT_VERSION}"
  DEFAULT_CONFIG_FILE="${CMAKE_INSTALL_SYSCONFDIR}/gitlab-hook/config.ini")
target_link_libraries(gitlab-hook
  boost_program_options event_core microhttpd systemd)
install(TARGETS gitlab-hook)

include(coverage)
target_enable_coverage(gitlab-hook)

include(help2man)
help2man(TARGET gitlab-hook SECTION 8 NAME "Webhook listener for Gitlab")
