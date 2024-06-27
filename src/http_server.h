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
#include <chrono>
#include <memory>
#include <functional>
#include <string>
class io_context;
struct sockaddr;



namespace http {


/// List of supported HTTP methods. Less than the usual.
enum class method
{ get = 1, put, post };


// A selection of HTTP status codes.
enum class code
{
  ok = 200,

  bad_request                     = 400,
  unauthorized                    = 401,
  forbidden                       = 403,
  not_found                       = 404,
  method_not_allowed              = 405,
  not_acceptable                  = 406,
  payload_too_large               = 413,
  uri_too_long                    = 414,
  unsupported_media_type          = 415,
  too_many_requests               = 429,
  request_header_fields_too_large = 431,

  internal_server_error           = 500,
  not_implemented                 = 501,
  bad_gateway                     = 502,
  service_unavailable             = 503,
  gateway_timeout                 = 504,
  http_version_not_supported      = 505,
  variant_also_negotiates         = 506,
  insufficient_storage            = 507,
  loop_detected                   = 508,
  not_extended                    = 510,
  network_authentication_required = 511,
};



/// An HTTP request.
class request
{
  public:
    using handler_type = std::function<void(request)>;

    /// The address of the peer. May be nullptr.
    const sockaddr* peer_address() const noexcept;

    /// HTTP method of the request.
    http::method method() const noexcept;

    /// The HTTP header entry with name \a key. Returns an empty string if not
    /// present in this request.
    std::string_view header(const char* key) const noexcept;

    /// The URI-Path of the request.
    std::string_view path() const noexcept;

    /// The URI-Query value with given \a key. Returns an empty string if not
    /// present in the URI.
    std::string_view query(const char* key) const noexcept;

    /// The body of a PUT or POST request.
    const std::string& content() const noexcept;

    /// Accepts a PUT or POST request and starts receiving its content(). After
    /// the content has been received, invokes the \a handler to finish the
    /// request.
    void accept(handler_type handler) noexcept;

    /// Sends a response to this request with given HTTP response \a code and
    /// a constant string \a body.
    template<std::size_t N>
    void respond(http::code code, const char (&body)[N])
    { respond_static(code, body); }

    /// Sends a response to this request with given HTTP response \a code and
    /// \a body.
    void respond(http::code code, std::string&& body);

    struct impl;
    enum class state;
    explicit request(impl* pimpl) noexcept;

  private:
    void respond_static(http::code code, std::string_view body);

    impl* m;
};



/// An HTTP(S) server.
class server
{
  public:
    using handler_type = std::function<void(request)>;

    /// Constructs the server, with asynchronous I/O being done via the given
    /// I/O \a context.
    explicit server(io_context& context);
    ~server();

    /// Whether the server is running.
    bool is_running() const noexcept;

    /// Configures the IP \a address on which the server listens.
    void set_ip(const std::string& address);

    /// Configures the \a port on which the server listens for connections.
    void set_port(std::uint16_t port) noexcept;

    /// Sets the server \a certificate and enables HTTPS. The buffer must be in
    /// PEM format.
    void set_local_cert(std::string certificate) noexcept;

    /// Sets the private \a key of the server certificate. The buffer must be
    /// in PEM format.
    void set_private_key(std::string key) noexcept;

    /// Configures the maximum \a number of open connections which the server
    /// accepts simultaneously.
    void set_max_connections(int number) noexcept;

    /// Configures the maximum \a number of open connections per requester IP
    /// address which the server accepts simultaneously.
    void set_max_connections_per_ip(int number) noexcept;

    /// Configures a memory limit in \a bytes for the underlying HTTP daemon
    /// library.
    void set_memory_limit(size_t bytes) noexcept;

    /// Configures a limit in \a bytes for the size of a request's message
    /// body which the server accepts.
    void set_content_size_limit(size_t bytes) noexcept;

    /// Configures the timeout in \a seconds after which an inactive connection
    /// is terminated. Must be in range [0,300].
    void set_connection_timeout(std::chrono::seconds seconds) noexcept;

    /// Adds a \a handler for the given request \a path. The \a handler will be
    /// invoked for all incoming requests that target \a path or a sub-path of
    /// it, except if there is a more specified handler for that sub-path.
    void add_handler(std::string path, handler_type handler);

    /// Starts the server, that is, opens the port and waits for requests.
    void start();

    /// Stops the server and closes the port.
    void stop() noexcept;

  private:
    struct impl;
    struct impl_delete
    {
      constexpr impl_delete() noexcept = default;
      void operator()(impl* p) noexcept;
    };

    std::unique_ptr<impl,impl_delete> m;
};
} // namespace http
