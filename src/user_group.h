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
#include <climits>
#include <string>



// Represents a POSIX user and group identity.
class user_group
{
  public:
    /// Constructs a null identity.
    constexpr user_group() noexcept = default;

    /// Constructs an identity for the user with given \a userName. Its group
    /// will be the user's default group.
    explicit user_group(const std::string& userName);

    /// Constructs an identity for the user with given \a userName and
    /// \a groupName.
    user_group(const std::string& userName, const std::string& groupName);

    /// Whether this is a (default-constructed) null identity.
    explicit operator bool() const noexcept
    { return mUid != Invalid && mGid != Invalid; }

    /// Attempts to set the process's real and effective user and group ID
    /// to this identity. This will effectively drop super-user privileges, if
    /// this process had them before. Throws if not successful.
    void impersonate();

  private:
    constexpr static unsigned int Invalid = UINT_MAX;

    unsigned int mUid{Invalid};
    unsigned int mGid{Invalid};
};
