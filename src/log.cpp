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
#include "log.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <systemd/sd-daemon.h>



static log_severity log_level{log_severity::warning};
static bool log_systemd{false};



void set_log_level(log_severity severity) noexcept
{ log_level = severity; }

void set_log_systemd(bool enabled) noexcept
{ log_systemd = enabled; }



static void write_log_message(log_severity severity, const char* format, va_list args) noexcept
{
  if (severity > log_level)
    return;

  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);

  const char* format2 = "%s\n";
  switch (severity)
  {
    case log_severity::fatal:   format2 = log_systemd ? SD_CRIT"fatal error: %s\n" : "fatal error: %s\n"; break;
    case log_severity::error:   format2 = log_systemd ? SD_ERR"error: %s\n" : "error: %s\n"; break;
    case log_severity::warning: format2 = log_systemd ? SD_WARNING"warning: %s\n" : "warning: %s\n"; break;
    case log_severity::info:    format2 = log_systemd ? SD_NOTICE"%s\n" : "%s\n"; break;
    case log_severity::debug:   format2 = log_systemd ? SD_DEBUG"%s\n" : "%s\n"; break;
  }

  fprintf(stderr, format2, buffer);
}



void log_fatal(const char* format, ...) noexcept
{
  va_list args;
  va_start(args, format);
  write_log_message(log_severity::fatal, format, args);
  va_end(args);
  std::exit(-1);
}



void log_error(const char* format, ...) noexcept
{
  va_list args;
  va_start(args, format);
  write_log_message(log_severity::error, format, args);
  va_end(args);
}



void log_warning(const char* format, ...) noexcept
{
  va_list args;
  va_start(args, format);
  write_log_message(log_severity::warning, format, args);
  va_end(args);
}



void log_info(const char* format, ...) noexcept
{
  va_list args;
  va_start(args, format);
  write_log_message(log_severity::info, format, args);
  va_end(args);
}



void log_debug(const char* format, ...) noexcept
{
  va_list args;
  va_start(args, format);
  write_log_message(log_severity::debug, format, args);
  va_end(args);
}
