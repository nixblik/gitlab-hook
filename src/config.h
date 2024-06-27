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
#include <concepts>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>



namespace config {


/// The type used for integer configuration file entries.
using int_type = std::int64_t;



/// Reference to a configuration file item. Could be a plain value or an
/// array or dictionary of sub-items. You must keep the parent config
/// object as long as you use the item.
class item
{
  public:
    explicit item(const void* native) noexcept;

    /// Whether the item has a string value.
    bool is_string() const noexcept;

    /// The boolean value of the item.
    bool to_bool() const;

    /// The integer value of the item, which must be in the range [\a low, \a high].
    int_type to_int_range(int_type low, int_type high) const;

    /// The integer value of the item converted to type \a T. If the item is
    /// not an integer or is not in the range representable by \a T, this
    /// function invokes the das::config::error_handler callback with a
    /// description of the error, then aborts the program.
    template<std::integral T>
    T to() const
    {
      static_assert(std::in_range<int_type>(std::numeric_limits<T>::min()));
      static_assert(std::in_range<int_type>(std::numeric_limits<T>::max()));
      return to_int_range(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    }

    /// The string value of the item.
    const std::string& to_string() const;

    /// The string value of the item. The returned string reference is only
    /// valid as long as the parent config::file exists.
    std::string_view to_string_view() const;

    /// Whether there is a child item with given \a key.
    bool contains(std::string_view key) const;

    /// The number of child items in an array.
    std::size_t size() const;

    /// The child item with the given \a key.
    item operator[](std::string_view key) const;

    /// The child item at \a index.
    item operator[](size_t index) const;

  private:
    const void* mItem;
};



/// A configuration file parsed into a tree of key/value pairs.
class file
{
  public:
    /// Loads the configuration from the given \a fileName.
    ///
    /// \throws std::exception if the file could not be opened or contains
    /// syntax errors.
    static file load(const std::string& fileName);

    /// The root item, typically a dictionary.
    operator item() const noexcept
    { return root(); }

    /// The root item, typically a dictionary.
    item root() const noexcept;

    /// Whether the root item contains a child item with given \a key.
    bool contains(std::string_view key) const;

    /// The child item with the given \a key.
    item operator[](std::string_view key) const;

  private:
    struct impl;
    struct impl_delete
    {
      constexpr impl_delete() noexcept = default;
      void operator()(impl* p) noexcept;
    };

    explicit file(impl* pimpl) noexcept;

    std::unique_ptr<impl,impl_delete> m;
};
} // namespace config
