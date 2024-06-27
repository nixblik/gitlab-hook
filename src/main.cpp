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
#include "hook.h"
#include "http_server.h"
#include "io_context.h"
#include "log.h"
#include "signal_listener.h"
#include "watchdog.h"
#include <boost/program_options.hpp>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <systemd/sd-daemon.h>
#include <vector>
using namespace std::chrono_literals;



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
              << "Runs an HTTP(S) server that listens for Gitlab webhook events.\n\n"
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



static std::string load_file(config::item cfg)
{
  std::filesystem::path filepath{cfg.to_string()};
  if (filepath.is_relative())
  {
    std::filesystem::path cfgFile{cfg.file_name()};
    filepath = cfgFile.parent_path() / filepath;
  }

  std::ifstream in{filepath};
  if (!in)
    throw std::system_error{make_error_code(std::errc::no_such_file_or_directory), filepath};

  in.seekg(0, std::ios::end);
  auto size = in.tellg();
  if (size <= 0)
    throw std::system_error{make_error_code(std::errc::io_error), filepath};

  std::string buffer(static_cast<size_t>(size), ' ');
  in.seekg(0);
  in.read(&buffer[0], size);

  return buffer;
}



class http_server : public http::server
{
  public:
    http_server(config::item cfg, io_context& io)
      : http::server{io}
    {
      set_ip(cfg["ip"].to_string());
      set_port(cfg["port"].to<std::uint16_t>());
      set_connection_timeout(30s);

      if (cfg.contains("certificate"))
        set_local_cert(load_file(cfg["certificate"]));

      if (cfg.contains("private_key"))
        set_private_key(load_file(cfg["private_key"]));

      if (cfg.contains("max_connections"))
        set_max_connections(cfg["max_connections"].to<int>());

      if (cfg.contains("max_connections_per_ip"))
        set_max_connections_per_ip(cfg["max_connections_per_ip"].to<int>());

      if (cfg.contains("memory_limit"))
        set_memory_limit(static_cast<size_t>(cfg["memory_limit"].to_int_range(0, SSIZE_MAX)));

      if (cfg.contains("content_size_limit"))
        set_content_size_limit(cfg["content_size_limit"].to<uint>());
    }
};



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

  http_server httpd{configuration["httpd"], io};
  httpd.start();

  auto hooksCfg = configuration["hooks"];
  std::vector<std::unique_ptr<hook>> hooks;
  hooks.reserve(hooksCfg.size());
  for (size_t i = 0, endi = hooksCfg.size(); i != endi; ++i)
  {
    auto hook = hook::create(hooksCfg[i]);
    httpd.add_handler(std::string{hook->uri_path}, std::ref(*hook));
    hooks.emplace_back(std::move(hook));
  }

  log_info("started gitlab-hook");
  sd_notify(0, "READY=1\nSTATUS=Normal operation\n");
  io.run();
  return 0;
}
catch (const std::exception& e)
{
  sd_notifyf(0, "STATUS=%s\n", e.what());
  log_fatal("%s", e.what());
}
