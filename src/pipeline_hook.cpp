/*  Copyright 2024 Uwe Salomon <post@uwesalomon.de>

    This file is part of Gitlab-hook.

    Gitlab-hook is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Gitlab-hook is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Gitlab-hook.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "log.h"
#include "pipeline_hook.h"
#include <nlohmann/json.hpp>



pipeline_hook::pipeline_hook(config::item configuration)
  : hook{configuration},
    mOnlyOnSuccess{configuration["only_on_success"].to_bool()}
{
  auto jobnames = configuration["jobname"];
  if (jobnames.is_string())
    mJobNames.insert(jobnames.to_string_view());
  else
    for (size_t i = 0, endi = jobnames.size(); i != endi; ++i)
      mJobNames.insert(jobnames[i].to_string_view());
}



void pipeline_hook::process(http::request request, const nlohmann::json& json)
{
  if (request.header("X-Gitlab-Event") != "Pipeline Hook")
  {
    log_debug("request discarded: bad event type");
    return request.respond(http::code::not_found, "not a pipeline event");
  }

  if (mOnlyOnSuccess && json.at("object_attributes").at("status").get_ref<const std::string&>() != "success")
  {
    log_debug("request discarded: pipeline not successful");
    return request.respond(http::code::ok, "ignored");
  }

  std::vector<std::string_view> jobNames;
  std::vector<std::string> jobIds;

  for (const auto& job: json.at("builds").get_ref<const nlohmann::json::array_t&>())
  {
    std::string_view jobName{job.at("name").get_ref<const std::string&>()};
    if (mJobNames.contains(jobName) && job.at("status").get_ref<const std::string&>() == "success")
    {
      jobNames.push_back(jobName);
      jobIds.push_back(std::to_string(job.at("id").get<int>()));
    }
  }

  if (!jobNames.empty())
  {
    process::environment env;
    env.set_list("CI_JOBNAMES", jobNames);
    env.set_list("CI_JOBIDS", jobIds);
    execute(request, json, std::move(env));
  }
  else
    request.respond(http::code::ok, "no action");
}
