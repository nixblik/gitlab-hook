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
#include "io_context.h"
#include "log.h"
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <event2/event.h>
#include <map>
#include <microhttpd.h>
#include <optional>
using namespace std::chrono_literals;



struct delete_event
{
  constexpr delete_event() noexcept = default;

  void operator()(event* p) noexcept
  { event_free(p); }
};



struct delete_response
{
  constexpr delete_response() noexcept = default;

  void operator()(MHD_Response* p) noexcept
  { MHD_destroy_response(p); }
};



struct http::server::impl
{
  io_context& io;
  std::unique_ptr<event,delete_event> listener;
  MHD_Daemon* daemon{nullptr};
  struct timeval timeout;

  std::string localCert;
  std::string privateKey;
  std::optional<in_addr> address;
  uint16_t port{80};
  int maxConns{0};
  int maxConnsPerIp{0};
  int connTimeout{0};
  std::intptr_t memLimit{0};
  std::size_t contentLimit{SIZE_MAX};
  std::map<std::string,handler_type,std::less<>> handlers;

  static void eventCb(int fd, short what, void* cls) noexcept;
  static MHD_Result answerCb(void* cls, MHD_Connection* conn, const char* url, const char* method, const char* version, const char* upload, size_t* uploadSz, void** connCls) noexcept;
  static int completedCb(void* cls, MHD_Connection* conn, void** connCls, MHD_RequestTerminationCode toe) noexcept;

  explicit impl(io_context& context) noexcept;
  void listen();
  MHD_Result sendStaticResponse(MHD_Connection* conn, http::code code, std::string_view content) noexcept;
  std::pair<request::impl*,MHD_Result> newRequest(MHD_Connection* conn, const char* url, const char* method);
  MHD_Result completeRequest(request::impl* request, MHD_Connection* conn);
  const handler_type* findHandler(std::string_view path) const noexcept;
};



enum class http::request::state
{ created, accepted, completed, responded };



struct http::request::impl
{
  MHD_Connection* conn{nullptr};
  std::string_view url;
  http::method method;
  request::state state{state::created};
  handler_type handler;
  std::string content;
  std::unique_ptr<MHD_Response,delete_response> response;
  std::string responseBody;
  size_t contentLimit;
  http::code responseCode;

  MHD_Result addContent(const char* upload, std::size_t size) noexcept;
};



http::server::server(io_context& context)
  : m{new impl{context}}
{}


inline http::server::impl::impl(io_context& context) noexcept
  : io{context}
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



void http::server::set_local_cert(std::string certificate) noexcept
{
  assert(!certificate.empty());
  m->localCert = std::move(certificate);
}



void http::server::set_private_key(std::string key) noexcept
{ m->privateKey = std::move(key); }



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



template<std::size_t Capacity>
struct server_options
{
  void set(MHD_OPTION option, intptr_t value) noexcept
  {
    assert(size < Capacity);
    data[size].option = option;
    data[size].value  = value;
    ++size;
  }

  void set(MHD_OPTION option, void* value) noexcept
  {
    assert(size < Capacity);
    data[size].option    = option;
    data[size].ptr_value = value;
    ++size;
  }

  void terminate() noexcept
  {
    assert(size < Capacity);
    data[size].option    = MHD_OPTION_END;
    data[size].ptr_value = nullptr;
    ++size;
  }

  MHD_OptionItem data[Capacity];
  std::size_t size{0};
};



void http::server::start()
{
  assert(!m->daemon);

  uint flags = MHD_USE_EPOLL;

  if (!m->localCert.empty())
    flags |= MHD_USE_TLS;

  server_options<8> options;
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

  if (!m->localCert.empty())
    options.set(MHD_OPTION_HTTPS_MEM_CERT, m->localCert.data());

  if (!m->privateKey.empty())
    options.set(MHD_OPTION_HTTPS_MEM_KEY, m->privateKey.data());

  options.terminate();

  m->daemon = MHD_start_daemon(flags, m->port, nullptr, nullptr, &impl::answerCb, m.get(),
                               MHD_OPTION_ARRAY, options.data,
                               MHD_OPTION_NOTIFY_COMPLETED, reinterpret_cast<void*>(&impl::completedCb), m.get(),
                               MHD_OPTION_END);
  if (!m->daemon)
    throw std::runtime_error("failed to start HTTP server");

  auto info = MHD_get_daemon_info(m->daemon, MHD_DAEMON_INFO_EPOLL_FD);
  if (!info)
    throw std::runtime_error{"HTTP server library does not support epoll"};

  m->listener.reset(event_new(m->io.native_handle(), info->epoll_fd, EV_TIMEOUT|EV_READ, &impl::eventCb, m.get()));
  m->listen();
}



void http::server::impl::listen()
{
  unsigned long long msecs;
  if (MHD_get_timeout(daemon, &msecs) != MHD_YES)
    msecs = 1000;

  timeval tm;
  tm.tv_sec  = msecs / 1000;
  tm.tv_usec = static_cast<long>((msecs - static_cast<unsigned long long>(tm.tv_sec) * 1000u) * 1000);

  event_add(listener.get(), &tm);
}



void http::server::impl::eventCb(int, short /*what*/, void* cls) noexcept
{
  auto self = static_cast<impl*>(cls);
  MHD_run(self->daemon);
  self->listen();
}



void http::server::stop() noexcept
{
  m->listener.reset();

  if (m->daemon)
  {
    MHD_stop_daemon(m->daemon);
    m->daemon = nullptr;
  }
}



inline http::method methodFrom(const char* str) noexcept
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
    auto result = request->addContent(upload, *uploadSz);
    *uploadSz   = 0;
    return result;
  }

  return self->completeRequest(request, conn);
}
catch (const std::exception& e)
{
  log_error("exception in HTTP handler: %s", e.what());
  return MHD_NO;
}



auto http::server::impl::newRequest(MHD_Connection* conn, const char* url, const char* method) -> std::pair<request::impl*,MHD_Result>
{
  log_debug("received HTTP %s %s", method, url);

  auto httpMethod = methodFrom(method);
  if (httpMethod == http::method{})
    return {nullptr, sendStaticResponse(conn, http::code::method_not_allowed, "method not allowed")};

  auto handler = findHandler(url);
  if (!handler)
    return {nullptr, sendStaticResponse(conn, http::code::not_found, "not found")};

  auto request          = std::make_unique<request::impl>();
  request->conn         = conn;
  request->method       = httpMethod;
  request->url          = url;
  request->contentLimit = contentLimit;

  handler->operator()(http::request{request.get()});

  MHD_Result result = MHD_NO;
  switch (request->state)
  {
    case request::state::created:   result = MHD_NO; break;  // handler did nothing
    case request::state::accepted:  result = MHD_YES; break;
    case request::state::completed: assert(false); result = MHD_NO; break;
    case request::state::responded: result = MHD_queue_response(conn, static_cast<uint>(request->responseCode), request->response.get()); break;
  }

  return {request.release(), result};
}



MHD_Result http::server::impl::completeRequest(request::impl* request, MHD_Connection* conn)
{
  assert(request->state == request::state::accepted);

  request->state = request::state::completed;
  request->handler(http::request{request});

  MHD_Result result = MHD_NO;
  switch (request->state)
  {
    case request::state::created:
    case request::state::accepted:  assert(false); result = MHD_NO; break;
    case request::state::completed: result = MHD_NO; break;  // handler did nothing
    case request::state::responded: result = MHD_queue_response(conn, static_cast<uint>(request->responseCode), request->response.get()); break;
  }

  return result;
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
  {
    log_error("failed to create HTTP response");
    return MHD_NO;
  }

  log_debug("respond HTTP %i", code);
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



const sockaddr* http::request::peer_address() const noexcept
{
  if (auto info = MHD_get_connection_info(m->conn, MHD_CONNECTION_INFO_CLIENT_ADDRESS))
    return info->client_addr;
  else
    return nullptr;
}



auto http::request::method() const noexcept -> http::method
{ return m->method; }


std::string_view http::request::path() const noexcept
{ return m->url; }



std::string_view http::request::header(const char* key) const noexcept
{
  const char* result = MHD_lookup_connection_value(m->conn, MHD_HEADER_KIND, key);
  return result ? result : std::string_view{};
}



std::string_view http::request::query(const char* key) const noexcept
{
  const char* result = MHD_lookup_connection_value(m->conn, MHD_GET_ARGUMENT_KIND, key);
  return result ? result : std::string_view{};
}



const std::string& http::request::content() const noexcept
{
  assert(m->method == method::put || m->method == method::post);
  assert(m->state == state::completed);
  return m->content;
}



void http::request::accept(handler_type handler) noexcept
{
  assert(m->method == method::put || m->method == method::post);
  assert(m->state == state::created);

  m->handler = std::move(handler);
  m->state   = state::accepted;
}



MHD_Result http::request::impl::addContent(const char* upload, std::size_t size) noexcept
{
  if (content.size() + size <= contentLimit)
  {
    content.append(upload, size);
    return MHD_YES;
  }

  assert(!response);
  response.reset(MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT));
  if (!response)
  {
    log_error("failed to create HTTP response");
    return MHD_NO;
  }

  responseCode = code::payload_too_large;
  state        = state::responded;

  log_debug("respond HTTP %i", responseCode);
  return MHD_YES;
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

  m->response.reset(MHD_create_response_from_buffer(body.size(), const_cast<char*>(body.data()), MHD_RESPMEM_PERSISTENT));
  if (!m->response)
    throw std::runtime_error{"failed to create HTTP response"};

  m->responseCode = code;
  m->state        = state::responded;

  log_debug("respond HTTP %i", code);
}
