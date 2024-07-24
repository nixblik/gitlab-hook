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
{
  if (configuration.contains("command"))
    throw std::runtime_error{"must not specify command for debug hook '" + name + "'"};
}



auto debug_hook::process(http::request request, const nlohmann::json& json) const -> outcome
{
  auto event = std::string{request.header("X-Gitlab-Event")};
  auto sjson = json.dump(2, ' ', true, nlohmann::json::error_handler_t::replace);

  return execute(request,
    [event = std::move(event), json = std::move(sjson)]()
    {
      printf("X-Gitlab-Event: %s\n"
             "%s\n"
             "--------------------------------------------------------------------------------\n",
             event.c_str(), json.c_str());

      fflush(stdout);
    }
  );
}
