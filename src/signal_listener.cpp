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
#include "signal_listener.h"
#include <cassert>
#include <event2/event.h>
#include <map>



struct delete_event
{
  constexpr delete_event() noexcept = default;

  void operator()(event* p) noexcept
  { event_free(p); }
};



struct signal_listener::impl
{
  io_context& io;
  std::map<int,std::unique_ptr<event,delete_event>> sigs;
  handler_type handler;

  static void callback(int fd, short what, void* cls) noexcept;

  explicit impl(io_context& context) noexcept;
};



signal_listener::signal_listener(io_context& context)
  : m{new impl{context}}
{}


inline signal_listener::impl::impl(io_context& context) noexcept
  : io{context}
{}


void signal_listener::impl_delete::operator()(impl* p) noexcept
{ delete p; }



void signal_listener::add(std::initializer_list<int> numbers)
{
  for (int number: numbers)
  {
    assert(!m->sigs.contains(number));

    auto ev = event_new(m->io.native_handle(), number, EV_SIGNAL|EV_PERSIST, &impl::callback, m.get());
    m->sigs.try_emplace(number, std::unique_ptr<event,delete_event>{ev});
    event_add(ev, nullptr);
  }
}



void signal_listener::wait(handler_type handler)
{ m->handler = std::move(handler); }



void signal_listener::impl::callback(int fd, short what, void* cls) noexcept
{
  auto self = static_cast<impl*>(cls);
  if (what & EV_SIGNAL)
    if (self->handler)
      self->handler(fd);
}
