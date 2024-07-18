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
#include "action_list.h"
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
#include <iomanip>
#include <iostream>
#include <sstream>
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
              << "Runs an HTTP(S) server that listens for Gitlab webhook events and processes\n"
              << "them. If the event matches the configured criteria, gitlab-hook executes the\n"
              << "configured action. Typically, it executes a custom script.\n\n"
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



class StatusPage
{
  public:
    void operator()(http::request request) const;

  private:
    const time_t mStart{std::time(nullptr)};
};



void StatusPage::operator()(http::request request) const
{
  if (request.method() != http::method::get)
    return request.respond(http::code::method_not_allowed, "method not allowed");

  struct tm startTm;
  localtime_r(&mStart, &startTm);

  auto lastFailure = action_list::lastFailure();
  struct tm lastFailureTm;
  localtime_r(&lastFailure, &lastFailureTm);

  std::ostringstream body;
  body << R"(<!doctype html>
<html lang="en" class="h-100">
<head>
 <meta charset="utf-8">
 <meta name="viewport" content="width=device-width, initial-scale=1, shrink-to-fit=no">
 <title>Gitlab-Hook Status</title>
 <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap@4.3.1/dist/css/bootstrap.min.css" integrity="sha384-ggOyR0iXCbMQv3Xipma34MD+dH/1fQ784/j6cY/iJTQUOhcWr7x9JvoRxT2MZw1T" crossorigin="anonymous">
</head>
<body class="d-flex flex-column h-100">
 <main role="main" class="flex-shrink-0">
  <div class="container">
   <h1 class="mt-5">Gitlab-Hook Status</h1>
   <dl class="mt-4 row" id="infos">
    <dt class="col-sm-3">Up since:</dt><dd class="col-sm-9">)" << std::put_time(&startTm, "%Y-%m-%d %X") << R"(</dd>
    <dt class="col-sm-3">Good requests:</dt><dd class="col-sm-9">)" << hook::goodRequestCount() << R"(</dd>
    <dt class="col-sm-3">Rejected requests:</dt><dd class="col-sm-9">)" << hook::requestCount() - hook::goodRequestCount() << R"(</dd>
    <dt class="col-sm-3">Hooks scheduled:</dt><dd class="col-sm-9">)" << hook::scheduledCount() << R"(</dd>
    <dt class="col-sm-3">Hooks executed:</dt><dd class="col-sm-9">)" << action_list::executedCount() << R"(</dd>
    <dt class="col-sm-3">Hooks failed:</dt><dd class="col-sm-9">)" << action_list::failedCount() << R"(</dd>
    <dt class="col-sm-3">Last failure:</dt><dd class="col-sm-9">)";
      if (lastFailure) body << std::put_time(&lastFailureTm, "%Y-%m-%d %X"); body << R"(</dd>
   </dl>
  </div>
 </main>
 <footer class="footer mt-auto py-3">
  <div class="container">
   <span class="text-muted">
    Gitlab-Hook v)" VERSION R"( &mdash; Copyright &copy; 2024 Uwe Salomon<br />
    This program comes with ABSOLUTELY NO WARRANTY. This is free software, and you are welcome to
    redistribute it <a href="https://www.gnu.org/licenses/gpl-3.0.en.html">under certain conditions</a>.
   </span>
  </div>
 </footer>
</body>
</html>)";

  request.respond(http::code::ok, std::move(body).str());
}



int main(int argc, char** argv)
try {
  const command_line cmdline{argc, argv};
  set_log_level(cmdline.logLevel);
  log_info("using configuration file %s", cmdline.configFile.c_str());

  io_context io;
  for (;;)
  {
    const auto configuration = config::file::load(cmdline.configFile);
    watchdog watchdog{io};
    action_list actions{io};

    signal_listener sigs1{io};
    sigs1.add(SIGHUP, SIGINT, SIGTERM);
    sigs1.wait([&io](int sig)
    {
      log_warning("signal %i raised, quit application", sig);
      io.stop();
    });

    bool restart = false;
    signal_listener sigs2{io};
    sigs2.add(SIGUSR1);
    sigs2.wait([&io, &restart](int sig)
    {
      log_warning("signal %i raised, reload application", sig);
      restart = true;
      io.stop();
    });

    http_server httpd{configuration["httpd"], io};
    httpd.start();

    StatusPage statusPage;
    httpd.add_handler("/status", std::ref(statusPage));

    auto hooksCfg = configuration["hooks"];
    std::vector<std::unique_ptr<hook>> hooks;
    hooks.reserve(hooksCfg.size());
    hook::init_global(configuration.root());

    for (size_t i = 0, endi = hooksCfg.size(); i != endi; ++i)
    {
      auto nhook = hook::create(hooksCfg[i]);
      auto same  = std::find_if(hooks.rbegin(), hooks.rend(), [&nhook](const std::unique_ptr<hook>& other)
      { return nhook->uri_path == other->uri_path; });

      if (same == hooks.rend())
      {
        httpd.add_handler(std::string{nhook->uri_path}, std::ref(*nhook));
        hooks.emplace_back(std::move(nhook));
      }
      else
        (*same)->chain(std::move(nhook));
    }

    log_info("started gitlab-hook");
    sd_notify(0, "READY=1\nSTATUS=Normal operation\n");
    io.run();

    if (!restart)
      break;
  }

  return 0;
}
catch (const std::exception& e)
{
  sd_notifyf(0, "STATUS=%s\n", e.what());
  log_fatal("%s", e.what());
}
