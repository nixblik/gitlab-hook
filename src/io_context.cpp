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
#include "io_context.h"
#include <event2/event.h>



struct free_event_base
{
  constexpr free_event_base() noexcept = default;

  void operator()(event_base* p) noexcept
  { if (p) event_base_free(p); }
};



struct io_context::impl
{
  std::unique_ptr<event_base,free_event_base> base;

  impl() noexcept;
};



io_context::io_context()
  : m{new impl}
{}


io_context::impl::impl() noexcept
  : base{event_base_new()}
{}


void io_context::impl_delete::operator()(impl* p) noexcept
{ delete p; }


event_base* io_context::native_handle() noexcept
{ return m->base.get(); }


void io_context::run()
{ event_base_loop(m->base.get(), 0); }


void io_context::stop() noexcept
{ event_base_loopbreak(m->base.get()); }
