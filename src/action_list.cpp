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



struct free_event
{
  constexpr free_event() noexcept = default;

  void operator()(event* p) noexcept
  { event_free(p); }
};



struct action_list::item
{
  item(const char* n, process&& p, std::chrono::seconds t) noexcept
    : name{n},
      process{std::move(p)},
      timeout{t}
  {}

  item(const char* n, std::function<void()>&& f) noexcept
    : name{n},
      function{std::move(f)}
  {}

  const char* name;
  std::function<void()> function;
  class process process;
  std::chrono::seconds timeout;
};



struct action_list::impl
{
  static impl* singleton;

  io_context& io;
  std::unique_ptr<event,free_event> execEv;
  std::unique_ptr<event,free_event> timeoutEv;
  std::unique_ptr<event,free_event> killEv;
  std::list<item> actions;

  explicit impl(io_context& context) noexcept;
  ~impl();

  void executeProcess(item& action);
  void executeFunction(item& action);
  void finishExecuteAction() noexcept;

  static void executeNextAction(int, short, void* cls) noexcept;
  static void terminateCurrentAction(int, short, void* cls) noexcept;
  static void killCurrentAction(int, short, void* cls) noexcept;
  static void addFailure() noexcept;
};


action_list::impl* action_list::impl::singleton = nullptr;
size_t action_list::actionsExecuted{0};
size_t action_list::actionsFailed{0};
time_t action_list::actionFailTm{0};



action_list::action_list(io_context& context)
  : m{new impl{context}}
{}



action_list::impl::impl(io_context& context) noexcept
  : io{context},
    execEv{event_new(io.native_handle(), -1, 0, &executeNextAction, this)},
    timeoutEv{event_new(io.native_handle(), -1, EV_TIMEOUT, &terminateCurrentAction, this)},
    killEv{event_new(io.native_handle(), -1, EV_TIMEOUT, &killCurrentAction, this)}
{
  assert(!singleton);
  singleton = this;
}



void action_list::impl_delete::operator()(impl* p) noexcept
{ delete p; }



action_list::impl::~impl()
{
  if (!actions.empty())
  {
    log_warning("%zu pending hook(s) will not be executed/completed:", actions.size());
    for (const auto& action: actions)
      log_warning("* %s", action.name);
  }

  singleton = nullptr;
}



io_context& action_list::get_io_context() noexcept
{ return impl::singleton->io; }



void action_list::append(const char* name, process process, std::chrono::seconds timeout)
{
  auto self = impl::singleton;
  assert(self);

  self->actions.emplace_back(name, std::move(process), timeout);
  if (self->actions.size() == 1)
    event_active(self->execEv.get(), 0, 0);
}



void action_list::append(const char* name, std::function<void()> function)
{
  auto self = impl::singleton;
  assert(self);

  self->actions.emplace_back(name, std::move(function));
  if (self->actions.size() == 1)
    event_active(self->execEv.get(), 0, 0);
}



void action_list::impl::executeNextAction(int, short, void* cls) noexcept
{
  auto  self   = static_cast<impl*>(cls);
  auto& action = self->actions.front();

  log_info("executing hook '%s'", action.name);
  fflush(stderr);

  if (action.process)
    self->executeProcess(action);
  else if (action.function)
    self->executeFunction(action);
  else
    assert(false);

  ++actionsExecuted;
}



void action_list::impl::executeProcess(item& action)
{
  action.process.start([this](std::error_code error, int exitCode) noexcept
  {
    event_del(timeoutEv.get());
    event_del(killEv.get());

    fputs("--------------------------------------------------------------------------------\n", stdout);
    fflush(stdout);

    auto& action = actions.front();
    if (error)
      log_error("hook '%s': %s", action.name, error.message().c_str());  // hope that message() does not throw
    else if (exitCode != 0)
      log_error("hook '%s': exited with code %i", action.name, exitCode);
    else
      log_info("completed hook '%s'", action.name);

    if (error || exitCode != 0)
      addFailure();

    finishExecuteAction();
  });

  timeval tm{};
  tm.tv_sec = action.timeout.count();
  event_add(timeoutEv.get(), &tm);
}



void action_list::impl::executeFunction(item& action)
{
  try {
    action.function();
    log_info("completed hook '%s'", action.name);
  }
  catch (const std::exception& e)
  {
    log_error("hook '%s': %s", action.name, e.what());
    addFailure();
  }

  finishExecuteAction();
}



void action_list::impl::finishExecuteAction() noexcept
{
  actions.pop_front();
  if (!actions.empty())
    event_active(execEv.get(), 0, 0);
}



void action_list::impl::terminateCurrentAction(int, short, void* cls) noexcept
{
  auto  self   = static_cast<impl*>(cls);
  auto& action = self->actions.front();

  log_error("hook '%s': timed out", action.name);
  action.process.terminate();

  timeval tm{};
  tm.tv_sec = 1;
  event_add(self->killEv.get(), &tm);
}



void action_list::impl::killCurrentAction(int, short, void* cls) noexcept
{
  auto  self   = static_cast<impl*>(cls);
  auto& action = self->actions.front();

  log_error("hook '%s': killing process", action.name);
  action.process.kill();

  self->actions.pop_front();
  if (!self->actions.empty())
    event_active(self->execEv.get(), 0, 0);
}



inline void action_list::impl::addFailure() noexcept
{
  ++actionsFailed;
  actionFailTm = std::time(nullptr);
}
