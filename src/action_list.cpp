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
#include "io_context.h"
#include "log.h"
#include <cassert>
#include <event2/event.h>
#include <list>



struct action_list::impl
{
  static impl* singleton;

  io_context& io;
  event* execEv;
  std::list<process> processes;

  explicit impl(io_context& context) noexcept;
  ~impl();

  static void executeNext(int, short, void* cls) noexcept;
};


action_list::impl* action_list::impl::singleton = nullptr;



action_list::action_list(io_context& context)
  : m{new impl{context}}
{}



action_list::impl::impl(io_context& context) noexcept
  : io{context},
    execEv{event_new(io.native_handle(), -1, 0, &executeNext, this)}
{
  assert(!singleton);
  singleton = this;
}



void action_list::impl_delete::operator()(impl* p) noexcept
{ delete p; }


action_list::impl::~impl()
{ singleton = nullptr; }


io_context& action_list::get_io_context() noexcept
{ return impl::singleton->io; }



void action_list::push(process process)
{
  auto self = impl::singleton;
  assert(self);

  self->processes.push_back(std::move(process));
  if (self->processes.size() == 1)
    event_active(self->execEv, 0, 0);
}



void action_list::impl::executeNext(int, short, void* cls) noexcept
{
  auto self = static_cast<impl*>(cls);
  self->processes.front().start([self](std::error_code error, int exitCode)
  {
    // FIXME: Also do some kind of timeout, configurable. Otherwise next action will never start
    if (error)
      log_error("action failed: %s", error.message().c_str()); // FIXME: Better error message
    else if (exitCode != 0)
      log_error("action failed: exited with code %i", exitCode);
    else
      log_info("action executed successfully");

    self->processes.pop_front();
    if (!self->processes.empty())
      event_active(self->execEv, 0, 0);
  });
}
