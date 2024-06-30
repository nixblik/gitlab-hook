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
    mToken{configuration["token"].to_string_view()}
{
  if (configuration.contains("peer_address"))
    mAllowedAddress = configuration["peer_address"].to_string_view();

  if (configuration.contains("script"))
    mScript = configuration["script"].to_string_view();
}



hook::~hook()
= default;



void hook::operator()(http::request request)
{
  char peerAddress[INET6_ADDRSTRLEN+1];
  if (!to_string(request.peer_address(), peerAddress))
  {
    log_warning("failed to obtain peer address");
    return; // drops request
  }

  if (!mAllowedAddress.empty() && peerAddress != mAllowedAddress)
  {
    log_warning("unauthorized request from %s to %s: address not allowed", peerAddress, uri_path.c_str());
    return request.respond(http::code::unauthorized, "unauthorized");
  }

  if (!mToken.empty() && request.header("X-Gitlab-Token") != mToken)
  {
    log_warning("unauthorized request from %s to %s: bad token", peerAddress, uri_path.c_str());
    return request.respond(http::code::unauthorized, "unauthorized");
  }

  if (request.method() != http::method::post)
  {
    log_warning("bad request from %s to %s: method not allowed", peerAddress, uri_path.c_str());
    return request.respond(http::code::method_not_allowed, "method not allowed");
  }

  if (request.path() != uri_path)
  {
    auto rpath = request.path();
    log_warning("bad request from %s to %.*s: not found", peerAddress, static_cast<int>(rpath.size()), rpath.data());
    return request.respond(http::code::not_found, "not found");
  }

  log_info("request from %s to %s", peerAddress, uri_path.c_str());
  request.accept([this](http::request request) noexcept
  {
    try {
      auto json = nlohmann::json::parse(request.content());
      process(request, json);
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



bool hook::to_string(const sockaddr* addr, char* buffer) noexcept
{
  if (!addr)
    return false;

  const void* inaddr;
  switch (addr->sa_family)
  {
    case AF_INET:  inaddr = &reinterpret_cast<const sockaddr_in*>(addr)->sin_addr; break;
    case AF_INET6: inaddr = &reinterpret_cast<const sockaddr_in6*>(addr)->sin6_addr; break;
    default:       return false;
  }

  return inet_ntop(addr->sa_family, inaddr, buffer, INET6_ADDRSTRLEN+1);
}



void hook::execute(http::request request, const nlohmann::json& json, process::environment environment) const
{
  environment.set("CI_PROJECT_ID", std::to_string(json.at("project").at("id").get<int>()));
  environment.set("CI_PROJECT_NAME", json.at("project").at("name").get_ref<const std::string&>());
  environment.set("CI_PROJECT_SLUG", json.at("project").at("path_with_namespace").get_ref<const std::string&>());
  environment.set("CI_GITLAB_URL", gitlabServerFrom(json));

  if (json.contains("commit"))
    environment.set("CI_COMMIT_ID", json.at("commit").at("id").get_ref<const std::string&>());

  std::vector<std::string> args;
  args.push_back("-c");
  args.push_back(std::string{mScript});

  class process proc{action_list::get_io_context()};
  proc.set_program("/bin/sh"); // FIXME: Shell configurable
  proc.set_arguments(std::move(args));
  proc.set_environment(std::move(environment));
  action_list::push(std::move(proc)); // FIXME: Log that action is started, and also when it has finished

  request.respond(http::code::ok, {});
}



std::string_view hook::gitlabServerFrom(const nlohmann::json& json)
{
  std::string_view projectUrl = json.at("project").at("web_url").get_ref<const std::string&>();

  auto protoPos = projectUrl.find("://");
  if (protoPos == projectUrl.npos)
    throw std::runtime_error{"invalid project.web_url in Gitlab JSON payload"};

  auto serverPos = projectUrl.find('/', protoPos + 3);
  if (serverPos == projectUrl.npos)
    throw std::runtime_error{"invalid project.web_url in Gitlab JSON payload"};

  return projectUrl.substr(0, serverPos);
}
