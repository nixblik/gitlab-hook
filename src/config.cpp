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
#include "config.h"
#include <system_error>
#include <toml.hpp>
#include <type_traits>



// Transparent hash functor for strings. Enables std::string_view lookups in a
// map with std::string keys.
struct toml_string_hash
{
  using is_transparent = void;

  std::size_t operator()(std::string_view str) const
  { return base(str); }

  std::size_t operator()(const std::string& str) const
  { return base(str); }

  std::hash<std::string_view> base;
};



// Transparent string comparison functor for strings. Enables das::string_view
// lookups in a map with std::string keys.
struct toml_string_eq
{
  using is_transparent = void;

  bool operator()(const std::string& s1, const std::string& s2) const noexcept
  { return s1 == s2; }

  bool operator()(std::string_view s1, const std::string& s2) const noexcept
  { return s1 == s2; }
};



// Node types for libtoml11
template<typename Key, typename Value>
using toml_table_type = std::unordered_map<Key,Value,toml_string_hash,toml_string_eq>;

template<typename Value>
using toml_array_type = std::vector<Value>;

using native_type = toml::basic_value<toml::discard_comments,toml_table_type,toml_array_type>;
using native_ref  = const native_type*;

static_assert(std::is_same_v<bool, toml::boolean>);
static_assert(std::is_same_v<config::int_type, toml::integer>);



inline config::item::item(const void* native) noexcept
  : mItem{native}
{}


bool config::item::is_string() const noexcept
{ return static_cast<native_ref>(mItem)->is_string(); }



auto config::item::to_int_range(int_type low, int_type high) const -> int_type
{
  auto value = static_cast<native_ref>(mItem)->as_integer();
  if (value >= low && value <= high)
    return value;

  auto loc = static_cast<native_ref>(mItem)->location();
  throw std::out_of_range{toml::detail::format_underline("value out of range", {{std::move(loc), "here"}})};
}



const std::string& config::item::to_string() const
{ return static_cast<native_ref>(mItem)->as_string(); }



std::string_view config::item::to_string_view() const
{
  auto& s = static_cast<native_ref>(mItem)->as_string();
  return std::string_view{s.str};
}



bool config::item::contains(std::string_view key) const
{
  auto& table = static_cast<native_ref>(mItem)->as_table();
  return table.contains(key);
}



std::size_t config::item::size() const
{
  auto& array = static_cast<native_ref>(mItem)->as_array();
  return array.size();
}



auto config::item::operator[](std::string_view key) const -> item
{
  auto& table = static_cast<native_ref>(mItem)->as_table();
  auto  iter  = table.find(key);

  if (iter != table.end())
    return item{&iter->second};

  toml::detail::throw_key_not_found_error(*static_cast<native_ref>(mItem), std::string{key});
}



auto config::item::operator[](size_t index) const -> item
{ return item{&static_cast<native_ref>(mItem)->at(index)}; }



struct config::file::impl
{
  native_type root;
};



auto config::file::load(const std::string& fileName) -> file
{
  std::ifstream stream{fileName, std::ios_base::in|std::ios_base::binary};
  if (!stream)
    throw std::system_error(make_error_code(std::errc::no_such_file_or_directory), fileName);

  auto cfg = toml::parse<toml::discard_comments,toml_table_type,toml_array_type>(stream, fileName);
  return file{new impl{std::move(cfg)}};
}


inline config::file::file(impl* pimpl) noexcept
  : m{pimpl}
{}


void config::file::impl_delete::operator()(impl* p) noexcept
{ delete p; }


auto config::file::root() const noexcept -> item
{ return item{&m->root}; }


bool config::file::contains(std::string_view key) const
{ return item{&m->root}.contains(key); }


auto config::file::operator[](std::string_view key) const -> item
{ return item{&m->root}[key]; }
