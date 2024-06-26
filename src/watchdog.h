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
#include <memory>
class io_context;
struct event;



/// An automatic trigger for the SystemD watchdog.
class watchdog
{
  public:
    /// Constructs and starts the watchdog. As long as the \a context's event
    /// loop is running, it will regularly trigger the SystemD watchdog to
    /// prevent it from shutting down the application.
    explicit watchdog(io_context& context);

  private:
    struct delete_event
    {
      constexpr delete_event() noexcept = default;
      void operator()(event* p) noexcept;
    };

    std::unique_ptr<event,delete_event> m;
};
