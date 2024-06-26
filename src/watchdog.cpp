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
#include "watchdog.h"
#include <event2/event.h>
#include <systemd/sd-daemon.h>



static void watchdogCb(int, short what, void*) noexcept
{
  if (what & EV_TIMEOUT)
    sd_notify(0, "WATCHDOG=1\n");
}



watchdog::watchdog(io_context& context)
{
  uint64_t usec;
  if (sd_watchdog_enabled(0, &usec) > 0)
  {
    timeval tm;
    tm.tv_sec  = static_cast<long>(usec % 1000000u);
    tm.tv_usec = static_cast<long>(usec - static_cast<uint64_t>(tm.tv_sec) * 1000000u);

    m.reset(evtimer_new(context.native_handle(), &watchdogCb, nullptr));
    event_add(m.get(), &tm);
  }
}



void watchdog::delete_event::operator()(event* p) noexcept
{ event_free(p); }
