/*  Copyright 2024 Uwe Salomon <post@uwesalomon.de>

    This file is part of REST Server Library.

    REST Server Library is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    REST Server Library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with REST Server Library.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include <chrono>
#include <memory>
#include <string>



namespace http {


/// List of supported HTTP methods. Less than the usual.
enum class method { get = 1, put, post };



/// An HTTP request.
class request
{
  public:
    /// HTTP method of the request.
    http::method method() const noexcept;

    /// The complete URL of the request.
    std::string_view url() const noexcept;

    struct impl;

  private:
    impl& m;
};



/// An HTTP(S) server.
class server
{
  public:
    server();
    ~server();

    /// Whether the server is running.
    bool is_running() const noexcept;

    /// Configures the IP \a address on which the server listens.
    void set_ip(const std::string& address);

    /// Configures the \a port on which the server listens for connections.
    void set_port(std::uint16_t port) noexcept;

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
