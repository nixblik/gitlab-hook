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
#pragma once



/// List of logging severities.
enum class log_severity
{ fatal = 1, error, warning, info, debug };

// FIXME: Add log level and make the log functions do that automatically.

/// Logs a fatal error message composed from a printf-like \a format string and
/// the arguments. Then aborts the program.
[[noreturn, gnu::format(printf,1,2)]]
void log_fatal(const char* format, ...) noexcept;

/// Logs an error message composed from a printf-like \a format string and the
/// arguments.
[[gnu::format(printf,1,2)]]
void log_error(const char* fmt, ...) noexcept;

/// Logs a warning message composed from a printf-like \a format string and the
/// arguments.
[[gnu::format(printf,1,2)]]
void log_warning(const char* fmt, ...) noexcept;

/// Logs an informational message composed from a printf-like \a format string
/// and the arguments.
[[gnu::format(printf,1,2)]]
void log_info(const char* fmt, ...) noexcept;

/// Logs a debug message composed from a printf-like \a format string and the
/// arguments.
[[gnu::format(printf,1,2)]]
void log_debug(const char* fmt, ...) noexcept;
