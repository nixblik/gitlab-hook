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
#include "http_server.h"
#include "log.h"
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <map>
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
  std::map<std::string,handler_type,std::less<>> handlers;

  static MHD_Result answerCb(void* cls, MHD_Connection* conn, const char* url, const char* method, const char* version, const char* upload, size_t* uploadSz, void** connCls) noexcept;
  static int completedCb(void* cls, MHD_Connection* conn, void** connCls, MHD_RequestTerminationCode toe) noexcept;

  MHD_Result sendStaticResponse(MHD_Connection* conn, http::code code, std::string_view content) noexcept;
  std::pair<request::impl*,MHD_Result> newRequest(MHD_Connection* conn, const char* url, const char* method);
  const handler_type* findHandler(std::string_view path) const noexcept;
};



enum class http::request::state
{ created, accepted, completed, responded };



struct http::request::impl
{
  std::string_view url;
  http::method method;
  request::state state{state::created};
  MHD_PostProcessor* postProc{nullptr};
  MHD_Response* response{nullptr};
  std::string responseBody;
  http::code responseCode;

  ~impl();
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



void http::server::add_handler(std::string path, handler_type handler)
{
  if (path.empty() || path.front() != '/')
    throw std::invalid_argument{"invalid HTTP server path"};

  if (path.back() == '/' && path.size() > 1)
    path.pop_back();

  bool inserted = m->handlers.try_emplace(std::move(path), std::move(handler)).second;
  if (!inserted)
    throw std::invalid_argument{"duplicate HTTP server path"};
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



MHD_Result http::server::impl::answerCb(void* cls, MHD_Connection* conn, const char* url, const char* method, const char*, const char* upload, size_t* uploadSz, void** connCls) noexcept
try {
  auto self = static_cast<impl*>(cls);

  if (!*connCls)
  {
    auto res = self->newRequest(conn, url, method);
    *connCls = res.first;
    return res.second;
  }

  auto request = static_cast<request::impl*>(*connCls);
  if (*uploadSz)
  {
    auto result = request->postProcess(upload, *uploadSz);
    *uploadSz   = 0;
    return result;
  }

  // FIXME: Finish upload, invoke handler
  return MHD_NO;
}
catch (...) {
  auto self       = static_cast<impl*>(cls);
  self->exception = std::current_exception();
  return MHD_NO;
}



auto http::server::impl::newRequest(MHD_Connection* conn, const char* url, const char* method) -> std::pair<request::impl*,MHD_Result>
{
  log_info("HTTP %s %s", method, url);

  auto httpMethod = methodFrom(method);
  if (httpMethod == http::method{})
    return {nullptr, sendStaticResponse(conn, http::code::method_not_allowed, "method not allowed")};

  auto handler = findHandler(url);
  if (!handler)
    return {nullptr, sendStaticResponse(conn, http::code::not_found, "not found")};

  auto request    = std::make_unique<request::impl>();
  request->method = httpMethod;
  request->url    = url; // FIXME: Consider copying URL

  handler->operator()(http::request{request.get()});

  MHD_Result result = MHD_NO;
  switch (request->state)
  {
    case request::state::created:   request.reset(); result = MHD_NO; break;  // not handled at all
    case request::state::accepted:  result = MHD_YES; break;
    case request::state::completed: assert(false); request.reset(); result = MHD_NO; break;
    case request::state::responded: result = MHD_queue_response(conn, static_cast<uint>(request->responseCode), request->response); break;
  }

  return {request.release(), result};
}



auto http::server::impl::findHandler(std::string_view path) const noexcept -> const handler_type*
{
  if (path.empty() || path.front() != '/')
    return nullptr;

  for (;;)
  {
    auto iter = handlers.find(path);
    if (iter != handlers.end())
      return &iter->second;

    auto slash = path.rfind('/');
    if (slash == 0)
      break;

    path = std::string_view{path.data(), slash};
  }

  auto iter = handlers.find(std::string_view{"/"});
  if (iter != handlers.end())
    return &iter->second;

  return nullptr;
}



MHD_Result http::server::impl::sendStaticResponse(MHD_Connection* conn, http::code code, std::string_view content) noexcept
{
  auto response = MHD_create_response_from_buffer(content.size(), const_cast<char*>(content.data()), MHD_RESPMEM_PERSISTENT);
  if (!response)
    return MHD_NO; // TODO: Logging + error logging

  auto result = MHD_queue_response(conn, static_cast<uint>(code), response);
  MHD_destroy_response(response); // decrements refcount
  return result;
}



int http::server::impl::completedCb(void*, MHD_Connection*, void** connCls, MHD_RequestTerminationCode) noexcept
{
  auto request = static_cast<request::impl*>(*connCls);
  *connCls     = nullptr;

  delete request;
  return MHD_YES;
}



inline http::request::request(impl* pimpl) noexcept
  : m{pimpl}
{}



http::request::impl::~impl()
{
  if (response)
    MHD_destroy_response(response);
}



auto http::request::method() const noexcept -> http::method
{ return m->method; }


std::string_view http::request::url() const noexcept
{ return m->url; }



void http::request::accept() noexcept
{
  assert(m->state == state::created);
  assert(m->method == method::put || m->method == method::post);

  m->state = state::accepted;
  m->postProc = nullptr; // FIXME: Create post processor
}



MHD_Result http::request::impl::postProcess(const char* upload, std::size_t size) noexcept
{
  if (postProc)
    return MHD_post_process(postProc, upload, size);

  // Request encoding was unknown FIXME: Assemble content, can we do this streaming?
  return MHD_NO;
}



void http::request::respond(http::code code, std::string&& body)
{
  assert(!m->response);

  m->responseBody.swap(body);
  respond_static(code, m->responseBody);
}



void http::request::respond_static(http::code code, std::string_view body)
{
  assert(!m->response);

  m->response = MHD_create_response_from_buffer(body.size(), const_cast<char*>(body.data()), MHD_RESPMEM_PERSISTENT);
  if (!m->response)
    throw std::runtime_error{"failed to create HTTP response"}; // FIXME: Is this rather an assertion?

  m->responseCode = code;
  m->state        = state::responded;
}
