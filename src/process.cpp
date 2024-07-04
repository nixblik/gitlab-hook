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
#include "io_context.h"
#include "log.h"
#include "process.h"
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <event2/event.h>
#include <sys/wait.h>
#include <unistd.h>



class process_errors : public std::error_category
{
  public:
    static const process_errors& singleton() noexcept;
    const char* name() const noexcept override;
    std::string message(int code) const override;
};



const process_errors& process_errors::singleton() noexcept
{
  static process_errors object;
  return object;
}



const char* process_errors::name() const noexcept
{ return "process"; }


inline std::error_code make_process_killed_error(int signo) noexcept
{ return std::error_code{signo, process_errors::singleton()}; }


std::string process_errors::message(int code) const
{ return "process killed by signal " + std::to_string(code); }



struct process::impl
{
  io_context& io;
  std::string program;
  std::vector<std::string> args;
  environment env;
  user_group user;
  handler_type handler;
  impl* next{nullptr};
  pid_t pid{-1};

  explicit impl(io_context& context) noexcept
    : io{context}
  {}
};



class process::list
{
  public:
    static process::list& singleton(io_context& context);
    void add(process::impl* process) noexcept;
    void remove(process::impl* process) noexcept;

  private:
    explicit list(io_context& context) noexcept;
    static void onSigchld(int, short, void* cls) noexcept;

    process::impl* mProcesses{nullptr};
    event* mSigchld;
};



process::process(io_context& context)
  : m{new impl{context}}
{}


void process::impl_delete::operator()(impl* p) noexcept
{ delete p; }


void process::set_program(std::string program) noexcept
{ m->program = std::move(program); }


void process::set_arguments(std::vector<std::string>&& arguments) noexcept
{ m->args = std::move(arguments); }


void process::set_environment(environment environment) noexcept
{ m->env = std::move(environment); }


void process::set_user_group(user_group impersonate) noexcept
{ m->user = std::move(impersonate); }



void process::start(handler_type handler)
{
  pid_t pid = fork();
  if (pid == -1)
    throw std::system_error{errno, std::system_category(), "failed to fork child process"};

  if (pid != 0)
  {
    // In parent process
    m->handler = std::move(handler);
    m->pid     = pid;

    list::singleton(m->io).add(m.get());
    return;
  }

  // In child process...
  try {
    sigset_t sigMask;
    sigfillset(&sigMask);
    sigprocmask(SIG_UNBLOCK, &sigMask, nullptr);

    std::vector<const char*> args;
    args.reserve(m->args.size() + 2);
    args.push_back(m->program.c_str());
    for (const auto& arg: m->args)
      args.push_back(arg.c_str());
    args.push_back(nullptr);

    if (m->user)
      m->user.impersonate();

    auto env = m->env.get();
    execve(m->program.c_str(), const_cast<char* const*>(args.data()), const_cast<char* const*>(env.data()));
    throw std::system_error{errno, std::system_category()};
  }
  catch (const std::exception& e)
  {
    fprintf(stderr, "execute %s failed: %s\n", m->program.c_str(), e.what());
    std::exit(-1);
  }
}



void process::terminate() noexcept
{
  assert(m->pid != -1);
  if (::kill(m->pid, SIGTERM) == -1)
    log_error("failed to send termination signal to child process: %s", strerror(errno));
}



void process::kill() noexcept
{
  assert(m->pid != -1);
  if (::kill(m->pid, SIGKILL) == -1)
    log_error("failed to kill child process: %s", strerror(errno));

  m->pid     = -1;
  m->handler = handler_type{};
  list::singleton(m->io).remove(m.get());
}



process::list& process::list::singleton(io_context& context)
{
  static list instance{context};
  return instance;
}



process::list::list(io_context& context) noexcept
  : mSigchld{event_new(context.native_handle(), SIGCHLD, EV_SIGNAL|EV_PERSIST, &onSigchld, this)}
{
  event_add(mSigchld, nullptr);
}



inline void process::list::add(process::impl* process) noexcept
{
  // No need to sync here, application is single-threaded
  process->next = mProcesses;
  mProcesses    = process;
}



inline void process::list::remove(process::impl* process) noexcept
{
  for (auto iter = &mProcesses;; iter = &(*iter)->next)
  {
    assert(*iter);
    if (*iter == process)
    {
      *iter = process->next;
      return;
    }
  }
}



void process::list::onSigchld(int, short, void* cls) noexcept
{
  auto self = static_cast<list*>(cls);

  siginfo_t sigInfo;
  sigInfo.si_pid = 0;

  if (waitid(P_ALL, 0, &sigInfo, WEXITED|WNOHANG) == -1 && errno != ECHILD)
    log_fatal("wait on child process failed: %s", strerror(errno));

  if (sigInfo.si_pid == 0)
    return;

  for (auto iter = &self->mProcesses;; iter = &(*iter)->next)
  {
    auto proc = *iter;
    if (!proc)
      break;

    if (proc->pid != sigInfo.si_pid)
      continue;

    std::error_code error;
    int exitCode{0};
    switch (sigInfo.si_code)
    {
      case CLD_EXITED: exitCode = sigInfo.si_status; break;
      case CLD_KILLED:
      case CLD_DUMPED: error = make_process_killed_error(sigInfo.si_status); break;
      default:         assert(false); std::exit(-3);
    }

    *iter     = proc->next;
    proc->pid = -1;

    // Handler would have to be executed in process's io context but this application has just one thread
    auto handler = std::move(proc->handler);
    handler(error, exitCode);
    break;
  }
}



void process::environment::set(std::string_view var, std::string_view value)
{
  std::string entry;
  entry.reserve(var.size() + 1 + value.size());
  entry.append(var);
  entry.push_back('=');
  entry.append(value);

  mEntries.push_back(std::move(entry));
}



template<typename Container>
void process::environment::set_list(std::string_view var, const Container& values)
{
  size_t size = var.size() + 1;
  for (std::string_view value: values)
    size += value.size() + 1;

  std::string entry;
  entry.reserve(size);
  entry.append(var);

  char sep = '=';
  for (std::string_view value: values)
  {
    entry.push_back(sep);
    entry.append(value.data(), value.size());
    sep = ' ';
  }

  mEntries.push_back(std::move(entry));
}



std::vector<const char*> process::environment::get() const
{
  std::vector<const char*> result;
  result.reserve(mEntries.size() + 1);

  for (auto& entry: mEntries)
    result.push_back(entry.c_str());

  result.push_back(nullptr);
  return result;
}



template void process::environment::set_list(std::string_view, const std::vector<std::string>&);
template void process::environment::set_list(std::string_view, const std::vector<std::string_view>&);
