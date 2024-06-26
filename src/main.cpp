/*  Copyright 2024 Uwe Salomon <post@uwesalomon.de>

    This file is part of gitlab-hook.

    Gitlab-hook is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    Gitlab-hook is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along
    with gitlab-hook. If not, see <http://www.gnu.org/licenses/>.
*/
#include "config.h"
#include "http_server.h"
#include "io_context.h"
#include "log.h"
#include "signal_listener.h"
#include "watchdog.h"
#include <boost/program_options.hpp>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <systemd/sd-daemon.h>



struct command_line
{
  command_line(int argc, char** argv);

  std::string configFile;
  log_severity logLevel;
};



command_line::command_line(int argc, char** argv)
{
  using namespace boost::program_options;

  int verbosity{-1};

  options_description options{"Options:"};
  options.add_options()
      ("help,h", "Show this help.")
      ("version", "Show version information.")
      ("config", value<std::string>(&configFile)->default_value(DEFAULT_CONFIG_FILE), "Sets the configuration file to use.")
      ("verbose", value<int>(&verbosity)->implicit_value(0), "Increases the amount of log messages.");

  variables_map vm;
  store(parse_command_line(argc, argv, options), vm);
  notify(vm);

  if (vm.count("help"))
  {
    std::cout << EXECUTABLE" [OPTION]...\n\n"
              << "Runs a Gitlab hook receiver.\n\n" // TODO: Write --help text
              << options;
    std::exit(0);
  }

  if (vm.count("version"))
  {
    std::cout << VERSION"\n";
    std::exit(0);
  }

  if (verbosity < 0)
    logLevel = log_severity::warning;
  else if (verbosity == 0)
    logLevel = log_severity::info;
  else
    logLevel = log_severity::debug;
}



int main(int argc, char** argv)
try {
  const command_line cmdline{argc, argv};
  set_log_level(cmdline.logLevel);

  log_info("using configuration file %s", cmdline.configFile.c_str());
  const auto configuration = config::file::load(cmdline.configFile);

  io_context io;
  watchdog watchdog{io};

  signal_listener sigs{io};
  sigs.add(SIGHUP, SIGINT, SIGTERM);
  sigs.wait([&io](int sig)
  {
    log_warning("signal %i raised, quit application", sig);
    io.stop();
  });

  http::server httpd{io};
  httpd.set_ip(configuration["httpd"]["ip"].to_string());
  httpd.set_port(configuration["httpd"]["port"].to<std::uint16_t>());

  httpd.add_handler("/", [](http::request req)
  {
    log_info("handle %i at %s", static_cast<int>(req.method()), req.path().data());
    log_info("  query %s=%s", "x", req.query("x").data());
    log_info("  User-Agent=%s", req.header("User-Agent").data());
    switch (req.method())
    {
      case http::method::get:  return req.respond(http::code::ok, "here you are\n");
      case http::method::put:  return req.respond(http::code::method_not_allowed, "method not allowed\n");
      case http::method::post: return req.accept([](http::request req)
        {
          log_info("finish %i at %s with %zu bytes", static_cast<int>(req.method()), req.path().data(), req.content().size());
          req.respond(http::code::ok, "thank you\n");
        });
    }
  });

  httpd.start();

  log_info("Started gitlab-hook");
  sd_notify(0, "READY=1\nSTATUS=Normal operation\n");
  io.run();
  return 0;
}
catch (const std::exception& e)
{
  sd_notifyf(0, "STATUS=%s\n", e.what());
  log_fatal("%s", e.what());
}
