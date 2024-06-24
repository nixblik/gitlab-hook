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
#include "http_server.h"
#include "log.h"
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <microhttpd.h>
#include <optional>
using namespace std::chrono_literals;



struct http::server::impl
{
  MHD_Daemon* daemon{nullptr};
  struct timeval timeout;
  std::exception_ptr exception{nullptr};

  std::optional<in_addr> address;
  uint16_t port{80};
  int maxConns{0};
  int maxConnsPerIp{0};
  int connTimeout{0};
  std::intptr_t memLimit{0};
  std::size_t contentLimit{SIZE_MAX};

  static MHD_Result answerCb(void* cls, MHD_Connection* conn, const char* url, const char* method, const char* version, const char* upload, size_t* uploadSz, void** connCls) noexcept;
  static int completedCb(void* cls, MHD_Connection* conn, void** connCls, MHD_RequestTerminationCode toe) noexcept;

  request::impl* newRequest(MHD_Connection* conn, const char* url, const char* method, const char* /*unused*/, const char* upload, size_t* uploadSz);
};



struct http::request::impl
{
  std::string_view url;
  http::method method;

  MHD_Result postProcess(const char* upload, std::size_t size) noexcept;
};



http::server::server()
  : m{new impl}
{}


http::server::~server()
{ stop(); }


void http::server::impl_delete::operator()(impl *p) noexcept
{ delete p; }


bool http::server::is_running() const noexcept
{ return !!m->daemon; }



void http::server::set_ip(const std::string& address)
{
  assert(!m->daemon);

  in_addr addr;
  if (!inet_aton(address.c_str(), &addr))
    throw std::invalid_argument{"invalid HTTP server IP address"};

  m->address = addr;
}



void http::server::set_port(std::uint16_t port) noexcept
{
  assert(!m->daemon);
  m->port = port;
}



void http::server::set_max_connections(int number) noexcept
{
  assert(!m->daemon);
  assert(number >= 1);
  m->maxConns = number;
}



void http::server::set_max_connections_per_ip(int number) noexcept
{
  assert(!m->daemon);
  assert(number >= 1);
  m->maxConnsPerIp = number;
}



void http::server::set_memory_limit(size_t bytes) noexcept
{
  assert(!m->daemon);

  if (bytes > INTPTR_MAX)
    bytes = INTPTR_MAX;

  m->memLimit = static_cast<std::intptr_t>(bytes);
}



void http::server::set_content_size_limit(size_t bytes) noexcept
{
  assert(!m->daemon);
  m->contentLimit = bytes;
}



void http::server::set_connection_timeout(std::chrono::seconds seconds) noexcept
{
  assert(!m->daemon);
  assert(seconds >= 0s && seconds <= 300s);
  m->connTimeout = static_cast<int>(seconds.count());
}



struct server_options
{
  constexpr static std::size_t capacity = 16;

  void set(MHD_OPTION option, intptr_t value) noexcept
  {
    assert(size < capacity);
    data[size].option = option;
    data[size].value  = value;
    ++size;
  }

  void set(MHD_OPTION option, void* value) noexcept
  {
    assert(size < capacity);
    data[size].option    = option;
    data[size].ptr_value = value;
    ++size;
  }

  void terminate() noexcept
  {
    assert(size < capacity);
    data[size].option    = MHD_OPTION_END;
    data[size].ptr_value = nullptr;
    ++size;
  }

  MHD_OptionItem data[capacity];
  std::size_t size{0};
};



void http::server::start()
{
  assert(!m->daemon);

  uint flags = MHD_USE_EPOLL;
  server_options options;
  sockaddr_in address;

  if (m->address)
  {
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port   = htons(m->port);
    address.sin_addr   = *m->address;
    options.set(MHD_OPTION_SOCK_ADDR, &address);
  }

  if (m->maxConns)
    options.set(MHD_OPTION_CONNECTION_LIMIT, m->maxConns);

  if (m->maxConnsPerIp)
    options.set(MHD_OPTION_PER_IP_CONNECTION_LIMIT, m->maxConnsPerIp);

  if (m->memLimit)
    options.set(MHD_OPTION_CONNECTION_MEMORY_LIMIT, m->memLimit);

  if (m->connTimeout)
    options.set(MHD_OPTION_CONNECTION_TIMEOUT, m->connTimeout);

  options.terminate();

  m->daemon = MHD_start_daemon(flags, m->port, nullptr, nullptr, &impl::answerCb, m.get(),
                             MHD_OPTION_ARRAY, options.data,
                             MHD_OPTION_NOTIFY_COMPLETED, reinterpret_cast<void*>(&impl::completedCb), m.get(),
                             MHD_OPTION_END);

  if (!m->daemon)
    throw std::runtime_error("failed to start HTTP server");
}



void http::server::stop() noexcept
{
  if (m->daemon)
  {
    MHD_stop_daemon(m->daemon);
    m->daemon = nullptr;
  }
}



inline http::method methodFrom(const char* str)
{
  if (strcmp(str, "GET")  == 0) return http::method::get;
  if (strcmp(str, "PUT")  == 0) return http::method::put;
  if (strcmp(str, "POST") == 0) return http::method::post;
  return http::method{};
}



MHD_Result http::server::impl::answerCb(void* cls, MHD_Connection* conn, const char* url, const char* method, const char* version, const char* upload, size_t* uploadSz, void** connCls) noexcept
try {
  auto self = static_cast<impl*>(cls);

  if (!*connCls)
  {
    auto request = self->newRequest(conn, url, method, version, upload, uploadSz);
    *connCls     = request;
    return request ? MHD_YES : MHD_NO;
  }

  auto request = static_cast<request::impl*>(*connCls);
  if (*uploadSz)
  {
    auto result = request->postProcess(upload, *uploadSz);
    *uploadSz   = 0;
    return result;
  }

  // FIXME: Finish upload, invoke handler
}
catch (...) {
  auto self       = static_cast<impl*>(cls);
  self->exception = std::current_exception();
  return MHD_NO;
}



auto http::server::impl::newRequest(MHD_Connection* conn, const char* url, const char* method, const char* /*unused*/, const char* upload, size_t* uploadSz) -> request::impl*
{
  log_info("HTTP %s %s", method, url);

  auto httpMethod = methodFrom(method);
  if (httpMethod == http::method{})
    return nullptr;

  // FIXME: Check that URL has a handler.

  auto request    = std::make_unique<request::impl>();
  request->method = httpMethod;
  request->url    = url;

  // FIXME: Invoke handler

  return request.release();
}



int http::server::impl::completedCb(void*, MHD_Connection*, void** connCls, MHD_RequestTerminationCode) noexcept
{
  auto request = static_cast<request::impl*>(*connCls);
  *connCls     = nullptr;

  delete request;
  return MHD_YES;
}
