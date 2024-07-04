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
#include "user_group.h"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
class io_context;



/// A child process.
class process
{
  public:
    class environment;
    using handler_type = std::function<void(std::error_code, int)>;

    /// Creates a manager for a child process, with asynchronous I/O being done
    /// via the I/O \a context.
    explicit process(io_context& context);

    process(process&&) noexcept = default;
    process& operator=(process&&) noexcept = default;

    /// Kills the process if it is still running.
    ~process() = default;

    /// Sets the \a program to start.
    void set_program(std::string program) noexcept;

    /// Sets the command-line \a arguments for the child process.
    void set_arguments(std::vector<std::string>&& arguments) noexcept;

    /// Sets the \a environment the child process will execute in.
    void set_environment(environment environment) noexcept;

    /// Sets the user and group the child process will \a impersonate and get
    /// its access rights from.
    void set_user_group(user_group impersonate) noexcept;

    /// Starts the child process. The \a handler will be executed when the
    /// process finishes or execution fails somehow.
    void start(handler_type handler);

    /// Attempts to terminate the child process.
    void terminate() noexcept;

    /// Kills the child process. The handler given to start() will not be
    /// executed.
    void kill() noexcept;

  private:
    class  list;
    struct impl;
    struct impl_delete
    {
      constexpr impl_delete() noexcept = default;
      void operator()(impl* p) noexcept;
    };

    std::unique_ptr<impl,impl_delete> m;
};



/// The environment of a child process.
class process::environment
{
  public:
    environment() noexcept = default;
    environment(environment&&) noexcept = default;
    environment& operator=(environment&&) noexcept = default;

    /// Adds an environment variable \a var with given \a value.
    void set(std::string_view var, std::string_view value);

    /// Adds an environment variable \a name with the \a values separated by a
    /// space character.
    template<typename Container>
    void set_list(std::string_view name, const Container& values);

    /// The environment, as needed for execve(), including termination.
    std::vector<const char*> get() const;

  private:
    std::vector<std::string> mEntries;
};

extern template void process::environment::set_list(std::string_view, const std::vector<std::string>&);
extern template void process::environment::set_list(std::string_view, const std::vector<std::string_view>&);
