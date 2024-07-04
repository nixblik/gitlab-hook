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
#include "hook.h"
#include "log.h"
#include "pipeline_hook.h"
#include <arpa/inet.h>
#include <nlohmann/json.hpp>



std::string_view hook::shellCommand;



void hook::init_global(config::item configuration)
{
  shellCommand = configuration["shell"].to_string_view();
}



std::unique_ptr<hook> hook::create(config::item configuration)
{
  auto type = configuration["type"].to_string_view();
  if (type == "pipeline")
    return std::make_unique<pipeline_hook>(configuration);
  else
    throw std::runtime_error{"invalid hook type"};
}



hook::hook(config::item configuration)
  : uri_path{configuration["uri_path"].to_string()},
    name{configuration["name"].to_string()},
    mToken{configuration["token"].to_string_view()}
{
  if (configuration.contains("peer_address"))
    mAllowedAddress = configuration["peer_address"].to_string_view();

  if (configuration.contains("script"))
    mScript = configuration["script"].to_string_view();
}



hook::~hook()
= default;



void hook::chain(std::unique_ptr<hook> other) noexcept
{
  assert(!mChain);
  mChain = std::move(other);
}



void hook::operator()(http::request request) const
{
  auto peerAddress = to_string(request.peer_address());
  if (peerAddress.empty())
    throw std::runtime_error{"failed to obtain peer address"};

  if (request.method() != http::method::post)
    return request.respond(http::code::method_not_allowed, "method not allowed");

  if (request.path() != uri_path)
    return request.respond(http::code::not_found, "not found");

  auto reqToken = request.header("X-Gitlab-Token");
  if (reqToken.empty())
    return request.respond(http::code::unauthorized, "unauthorized");

  bool allowed = false;
  for (const hook* iter = this; iter; iter = iter->mChain.get())
    if (iter->mToken == reqToken)
      if (iter->mAllowedAddress.empty() || peerAddress == iter->mAllowedAddress)
        allowed = true;

  if (!allowed)
    return request.respond(http::code::forbidden, "forbidden");

  request.accept([this, peerAddress = std::move(peerAddress)](http::request request) noexcept
  {
    try {
      auto reqToken = request.header("X-Gitlab-Token");
      auto json     = nlohmann::json::parse(request.content());
      int  count    = 0;

      for (const hook* iter = this; iter; iter = iter->mChain.get())
        if (iter->mToken == reqToken)
          if (iter->mAllowedAddress.empty() || peerAddress == iter->mAllowedAddress)
            switch (iter->process(request, json))
            {
              case outcome::stop:     return;
              case outcome::ignored:  continue;
              case outcome::accepted: ++count; continue;
            }

      if (count)
        return request.respond(http::code::accepted, "accepted");
      else
        return request.respond(http::code::no_content, "ignored");
    }
    catch (const nlohmann::json::exception& e)
    {
      log_warning("invalid request to %s: %s", uri_path.c_str(), e.what());
      return request.respond(http::code::bad_request, e.what());
    }
    catch (const std::exception& e)
    {
      log_error("failed processing request to %s: %s", uri_path.c_str(), e.what());
      return request.respond(http::code::internal_server_error, "internal server error");
    }
  });
}



std::string hook::to_string(const sockaddr* addr)
{
  if (!addr)
    return {};

  const void* inaddr;
  switch (addr->sa_family)
  {
    case AF_INET:  inaddr = &reinterpret_cast<const sockaddr_in*>(addr)->sin_addr; break;
    case AF_INET6: inaddr = &reinterpret_cast<const sockaddr_in6*>(addr)->sin6_addr; break;
    default:       return {};
  }

  std::string result;
  result.resize(INET6_ADDRSTRLEN + 1);

  if (!inet_ntop(addr->sa_family, inaddr, result.data(), static_cast<socklen_t>(result.size())))
    return {};

  result.resize(strnlen(result.data(), result.size()));
  return result;
}



auto hook::execute(http::request, const nlohmann::json& json, process::environment environment) const -> outcome
{
  assert(!shellCommand.empty());

  environment.set("CI_PROJECT_ID", std::to_string(json.at("project").at("id").get<int>()));
  environment.set("CI_PROJECT_NAME", json.at("project").at("name").get_ref<const std::string&>());
  environment.set("CI_PROJECT_SLUG", json.at("project").at("path_with_namespace").get_ref<const std::string&>());
  environment.set("CI_GITLAB_URL", gitlabServerFrom(json));

  if (json.contains("commit"))
    environment.set("CI_COMMIT_ID", json.at("commit").at("id").get_ref<const std::string&>());

  std::vector<std::string> args;
  args.reserve(2);
  args.push_back("-c");
  args.push_back(std::string{mScript});

  class process proc{action_list::get_io_context()};
  proc.set_program(std::string{shellCommand});
  proc.set_arguments(std::move(args));
  proc.set_environment(std::move(environment));
  action_list::push(name.c_str(), std::move(proc));

  log_debug("scheduled action: %s", name.c_str());
  return outcome::accepted;
}



std::string_view hook::gitlabServerFrom(const nlohmann::json& json)
{
  std::string_view projectUrl = json.at("project").at("web_url").get_ref<const std::string&>();

  auto protoPos = projectUrl.find("://");
  if (protoPos != projectUrl.npos)
  {
    auto serverPos = projectUrl.find('/', protoPos + 3);
    if (serverPos != projectUrl.npos)
      return projectUrl.substr(0, serverPos);
  }

  throw std::runtime_error{"invalid project.web_url in Gitlab JSON payload"};
}
