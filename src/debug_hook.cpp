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
#include "log.h"
#include "debug_hook.h"
#include <nlohmann/json.hpp>



debug_hook::debug_hook(config::item configuration)
  : hook{configuration}
{}



auto debug_hook::process(http::request request, const nlohmann::json& json) const -> outcome
{
  auto event = request.header("X-Gitlab-Event");

  printf("X-Gitlab-Event: %.*s\n"
         "--------------------------------------------------------------------------------\n"
         "%s\n"
         "--------------------------------------------------------------------------------\n",
         static_cast<int>(event.size()), event.data(),
         json.dump(2, ' ', true, nlohmann::json::error_handler_t::replace).c_str());

  fflush(stdout);
  return execute(request, json, process::environment{});
}
