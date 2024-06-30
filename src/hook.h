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
#pragma once
#include "config.h"
#include "http_server.h"
#include "process.h"
#include <nlohmann/json_fwd.hpp>



/// Base class for a Gitlab webhook.
class hook
{
  public:
    /// Constructs a webhook from the given \a configuration.
    static std::unique_ptr<hook> create(config::item configuration);

    /// Constructs the hook from the given \a configuration.
    explicit hook(config::item configuration);

    virtual ~hook();

    /// Processes an incoming HTTP \a request.
    void operator()(http::request request);

    const std::string& uri_path;

  protected:
    /// Processes an incoming HTTP \a request, with its content already parsed
    /// to \a json. To be implemented in derived classes.
    virtual void process(http::request request, const nlohmann::json& json) = 0;

    /// Executes the hook's action (a shell script) with the given process
    /// \a environment for the \a request. Amends the \a environment with
    /// information from the \a request's \a json content.
    void execute(http::request request, const nlohmann::json& json, process::environment environment) const;

  private:
    hook(const hook&) = delete;
    hook& operator=(const hook&) = delete;

    static bool to_string(const sockaddr* addr, char* buffer) noexcept;
    static std::string_view gitlabServerFrom(const nlohmann::json& json);

    std::string_view mAllowedAddress;
    std::string_view mToken;
    std::string_view mScript;
};
