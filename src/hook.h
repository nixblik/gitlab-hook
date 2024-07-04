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
    /// Initializes configuration for all hooks.
    static void init_global(config::item configuration);

    /// Constructs a webhook from the given \a configuration.
    static std::unique_ptr<hook> create(config::item configuration);

    /// Constructs the hook from the given \a configuration.
    explicit hook(config::item configuration);

    virtual ~hook();

    /// Forms a chain with an \a other hook with the same uri_path.
    void chain(std::unique_ptr<hook> other) noexcept;

    /// Processes an incoming HTTP \a request.
    void operator()(http::request request) const;

    const std::string& uri_path;
    const std::string& name;

  protected:
    enum class outcome { stop = 1, ignored, accepted };

    /// Processes an incoming HTTP \a request, with its content already parsed
    /// to \a json. Must generate a response if it returns outcome::stop,
    /// in all other cases not. To be implemented in derived classes.
    virtual outcome process(http::request request, const nlohmann::json& json) const = 0;

    /// Executes the hook's action (a shell script) with the given process
    /// \a environment for the \a request. Amends the \a environment with
    /// information from the \a request's \a json content.
    outcome execute(http::request request, const nlohmann::json& json, process::environment environment) const;

  private:
    hook(const hook&) = delete;
    hook& operator=(const hook&) = delete;

    static std::string to_string(const sockaddr* addr);
    static std::string_view gitlabServerFrom(const nlohmann::json& json);
    static std::string_view shellCommand;

    hook* findMatchingHookInChain(http::request request, const std::string& peerAddress) noexcept;
    void log_request(http::request request, const std::string& peerAddress, const nlohmann::json& json) const;

    std::unique_ptr<hook> mChain;
    std::string_view mAllowedAddress;
    std::string_view mToken;
    std::string_view mScript;
    std::chrono::seconds mTimeout;
};
