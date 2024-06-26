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
#include <initializer_list>
#include <functional>
#include <memory>
class io_context;



/// A listener for POSIX signals.
class signal_listener
{
  public:
    using handler_type = std::function<void(int)>;

    /// Constructs the signal listener, with asynchronous I/O being done via the given
    /// I/O \a context.
    explicit signal_listener(io_context& context);

    /// Adds the signal \a numbers to the listener.
    template<std::same_as<int>... Args>
    void add(Args... args)
    { add(std::initializer_list<int>{args...}); }

    /// Waits for one of the signals to occur, then invokes the \a handler.
    void wait(handler_type handler);

  private:
    struct impl;
    struct impl_delete
    {
      constexpr impl_delete() noexcept = default;
      void operator()(impl* p) noexcept;
    };

    void add(std::initializer_list<int> numbers);

    std::unique_ptr<impl,impl_delete> m;
};
