# Gitlab-Hook

Gitlab-hook is a configurable HTTP(S) server that receives
[Webhook events from Gitlab](https://docs.gitlab.com/ee/user/project/integrations/webhook_events.html)
and processes them. If the event matches the configured criteria, gitlab-hook
executes the configured action. Typically, it executes a custom script.

These webhook event types are currently supported:

- [Pipeline events](https://docs.gitlab.com/ee/user/project/integrations/webhook_events.html#pipeline-events)

**NOTE** This project is not finished yet, it is under active development.



## Table of Contents

- [Installation](#installation)
- [Usage](#usage)
- [Build](#build)



## Installation

On a Debian system, gitlab-hook can be installed from a PPA as follows:

    sudo add-apt-repository ppa:nixblik/ppa
    sudo apt-get update
    sudo apt-get install gitlab-hook



## Usage

TODO Explain configuration here...



## Build

Gitlab-hook has these build dependencies:

- [CMake](https://cmake.org/)
- [Boost program_options](https://www.boost.org/doc/libs/1_85_0/doc/html/program_options.html)
- [Libevent](https://libevent.org/)
- [Libgtest](https://google.github.io/googletest/)
- [Libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)
- [Libsystemd](https://github.com/systemd/systemd)

You can compile gitlab-hook using CMake as follows:

    mkdir tmp
    cd tmp
    cmake ..
    cmake --build .
