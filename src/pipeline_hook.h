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
#include "hook.h"
#include <set>



/// A webhook for the Gitlab "pipeline event".
class pipeline_hook : public hook
{
  public:
    explicit pipeline_hook(config::item configuration);

  protected:
    void execute(http::request request, const nlohmann::json& json) override;

  private:
    std::set<std::string_view> mJobNames;
    bool mOnlyOnSuccess;
};
