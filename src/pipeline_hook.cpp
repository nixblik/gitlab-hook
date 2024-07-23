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



static std::set<std::string_view> string_set_from(config::item configuration)
{
  std::set<std::string_view> result;

  if (configuration.is_string())
    result.insert(configuration.to_string_view());
  else
    for (size_t i = 0, endi = configuration.size(); i != endi; ++i)
      result.insert(configuration[i].to_string_view());

  return result;
}



pipeline_hook::pipeline_hook(config::item configuration)
  : hook{configuration},
    mJobNames{string_set_from(configuration["job_name"])}
{
  if (configuration.contains("status"))
    mStatuses = string_set_from(configuration["status"]);
}



auto pipeline_hook::process(http::request request, const nlohmann::json& json) const -> outcome
{
  if (request.header("X-Gitlab-Event") != "Pipeline Hook")
    return outcome::ignored;

  auto& status = json.at("object_attributes").at("status").get_ref<const std::string&>();
  if (!mStatuses.empty() && !mStatuses.contains(status))
  {
    log_debug("hook '%s': no matching status '%s'", name.c_str(), status.c_str());
    return outcome::ignored;
  }

  std::vector<std::string_view> jobNames;
  std::vector<std::string> jobIds;

  for (const auto& job: json.at("builds").get_ref<const nlohmann::json::array_t&>())
  {
    std::string_view jobName{job.at("name").get_ref<const std::string&>()};
    if (mJobNames.contains(jobName) && job.at("status").get_ref<const std::string&>() == "success")
    {
      jobNames.push_back(jobName);
      jobIds.push_back(std::to_string(job.at("id").get<uint64_t>()));
    }
  }

  if (jobNames.empty())
  {
    log_debug("hook '%s': no matching job names", name.c_str());
    return outcome::ignored;
  }

  process::environment environment;
  environment.set_list("CI_JOB_IDS", jobIds);
  environment.set_list("CI_JOB_NAMES", jobNames);

  auto& json_obj_attrs = json.at("object_attributes");
  environment.set("CI_COMMIT_REF_NAME", json_obj_attrs.at("ref").get_ref<const std::string&>());
  environment.set("CI_COMMIT_SHA", json_obj_attrs.at("sha").get_ref<const std::string&>());
  environment.set("CI_PIPELINE_ID", std::to_string(json_obj_attrs.at("id").get<uint64_t>()));

  if (json_obj_attrs.contains("tag") && json_obj_attrs["tag"].is_string())
    environment.set("CI_COMMIT_TAG", json_obj_attrs["tag"].get_ref<const std::string&>());

  return execute(request, json, std::move(environment));
}
