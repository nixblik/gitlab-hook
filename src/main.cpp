#include "config.h"
#include "http_server.h"
#include "log.h"
#include <boost/program_options.hpp>
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
              << "Runs a Gitlab hook receiver.\n\n" // FIXME: Write --help text
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
  const auto configuration = config::file::load(cmdline.configFile);

  http::server httpd;
  httpd.set_ip(configuration["httpd"]["ip"].to_string());
  httpd.set_port(configuration["httpd"]["port"].to<std::uint16_t>());

  httpd.add_handler("/", [](http::request req)
  {
    log_info("handle %i at %s", req.method(), req.url().data());
    req.respond(http::code::ok, "thank you");
  });
  httpd.start();

  log_info("Starting gitlab-hook");
  sd_notify(0, "READY=1\nSTATUS=Normal operation\n");
  return 0;
}
catch (const std::exception& e)
{
  sd_notifyf(0, "STATUS=%s\n", e.what());
  log_fatal("%s", e.what());
}
