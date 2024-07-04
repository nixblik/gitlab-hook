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
#include "process.h"
#include <chrono>
#include <memory>
class io_context;



/// A list of actions = external processes to be executed. A singleton.
class action_list
{
  public:
    /// Constructs the global action list singleton.
    explicit action_list(io_context& context);

    /// The I/O context that must be used for constructing process objects.
    static io_context& get_io_context() noexcept;

    /// Appends a new \a process to be executed to the global list, with the
    /// hook \a name for logging purposes.
    static void append(const char* name, process process, std::chrono::seconds timeout);

  private:
    struct item;
    struct impl;
    struct impl_delete
    {
      constexpr impl_delete() noexcept = default;
      void operator()(impl* p) noexcept;
    };

    std::unique_ptr<impl,impl_delete> m;
};
