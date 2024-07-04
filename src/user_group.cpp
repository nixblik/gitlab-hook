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
#include "user_group.h"
#include <cassert>
#include <cerrno>
#include <grp.h>
#include <pwd.h>
#include <system_error>
#include <unistd.h>



inline gid_t groupIdForName(const char* name)
{
  auto grp = getgrnam(name);
  if (!grp)
    throw std::system_error{errno, std::system_category(), std::string{"failed to find group "} + name};

  return grp->gr_gid;
}



user_group::user_group(const std::string& userName)
{
  if (auto info = getpwnam(userName.c_str()))  // NOTE: This program is single-threaded.
  {
    mUid = info->pw_uid;
    mGid = info->pw_gid;
  }
  else
    throw std::system_error{errno, std::system_category(), std::string{"failed to find user "} + userName};
}



user_group::user_group(const std::string& userName, const std::string& groupName)
{
  if (auto info = getpwnam(userName.c_str()))
    mUid = info->pw_uid;
  else
    throw std::system_error{errno, std::system_category(), std::string{"failed to read information for user "} + userName};

  if (auto info = getgrnam(groupName.c_str()))
    mGid = info->gr_gid;
  else
    throw std::system_error{errno, std::system_category(), std::string{"failed to read information for group "} + groupName};
}



void user_group::impersonate()
{
  assert(mUid != Invalid && mGid != Invalid);

  auto info = getpwuid(mUid);
  if (!info)
    throw std::system_error{errno, std::system_category(), "failed to read user information"};

  if (initgroups(info->pw_name, info->pw_gid) == -1)
    throw std::system_error{errno, std::system_category(), "failed to set additional process groups"};

  if (setgid(mGid) == -1)
    throw std::system_error{errno, std::system_category(), "failed to set process group id"};

  if (setuid(mUid) == -1)
    throw std::system_error{errno, std::system_category(), "failed to set process user id"};
}
