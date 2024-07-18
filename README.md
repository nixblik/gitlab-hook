# Gitlab-Hook

Gitlab-hook is a configurable HTTP(S) server that receives
[Webhook events from Gitlab](https://docs.gitlab.com/ee/user/project/integrations/webhook_events.html)
and processes them. If the event matches the configured criteria, gitlab-hook
executes the configured action. Typically, it executes a custom script.

These webhook event types are currently supported:

- [Pipeline events](https://docs.gitlab.com/ee/user/project/integrations/webhook_events.html#pipeline-events)

Gitlab-hook could be extended, and it could use some regression tests. But it
works fine for now.



## Table of Contents

- [Installation](#installation)
- [Usage](#usage)
- [Build](#build)



## Installation

On a Debian system, gitlab-hook can be installed from a PPA as follows:

    sudo add-apt-repository ppa:nixblik/ppa
    sudo apt-get update
    sudo apt-get install gitlab-hook

This will automatically enable gitlab-hook as a systemctl background service.
The start will fail, though, until you provide a valid configuration file.



## Usage

When starting, gitlab-hook reads a configuration file that contains a
description of its interface towards Gitlab and its actions. The default
location of the configuration file is `/etc/gitlab-hook/config.ini`. You have
to create that configuration yourself, because it is specific for your
installation. We recommend the following setup procedure.

First, create the configuration directory and a subdirectory for the
certificate used by gitlab-hook, and protect it:

    sudo mkdir /etc/gitlab-hook
    sudo mkdir /etc/gitlab-hook/certs
    sudo chmod 0700 /etc/gitlab-hook/certs

Create a private key and a certificate in PEM format and put it in the cert
directory. We use [certtool](https://www.man7.org/linux/man-pages/man1/certtool.1.html)
here, which is part of the *gnutls-bin* package:

    certtool --generate-privkey --outfile key.pem --ecc
    certtool --generate-self-signed --load-privkey key.pem --outfile cert.pem
    sudo mv key.pem cert.pem /etc/gitlab-hook/certs/

This self-signed certificate only serves to facilitate TLS encryption of the
communication between Gitlab and gitlab-hook. The assumption is that your are
in a private, relatively secure network. If you need real authentication of
gitlab-hook towards Gitlab, you have to obtain a real certificate for the
server gitlab-hook is running on.

Then start writing the configuration file `/etc/gitlab-hook/config.ini`. The
file has [TOML](https://toml.io) syntax. First write the global section and
configure gitlab-hook's HTTP server:

    shell = "/bin/sh"

    [httpd]
    ip = "0.0.0.0"
    port = 8080
    certificate = "/etc/gitlab-hook/certs/cert.pem"
    private_key = "/etc/gitlab-hook/certs/key.pem"
    max_connections = 8
    max_connections_per_ip = 8
    content_size_limit = 65535

You can choose these values as you see fit; the last three are optional. The
special value "0.0.0.0" for the listening IP address makes gitlab-hook listen
on all network addresses of the server. You could now restart gitlab-hook and
see whether it runs fine:

    sudo systemctl restart gitlab-hook
    sudo systemctl status gitlab-hook

If all is good, navigate your browser to https://gitlab.hook.ip:8080/status
and, after accepting the security warning caused by the self-signed
certificate, you should see the status page of gitlab-hook.

Finally, limit access to the configuration file, because it will soon contain
sensitive information:

    sudo chmod 0600 /etc/gitlab-hook/config.ini
    

### Configuration of Hooks

After that, you can configure the hooks. A hook is a path on your server that
can be triggered by Gitlab using an HTTP POST request, and if certain
conditions are fulfilled, gitlab-hook will execute the action configured for
the hook. As an example, say that you want to deploy an artifact generated by
your Gitlab CI/CD pipeline. You create a pipeline hook in the configuration
file as follows:

    [[hooks]]
    type = "pipeline"
    name = "deploy my artifact"
    uri_path = "/api/deploy"
    token = "t0p$ecre4"
    jobname = ["build-this", "build-that"]
    status = "success"
    command = "echo Hello, world!"
    run_as.user = "nobody"

Then go to your Gitlab project, and configure a webhook with URL
https://gitlab.hook.ip:8080/api/deploy and secret token "t0p$ecre4" and trigger
"Pipeline Hook" and SSL verification disabled (in case you use a self-signed
certificate). If you run the next pipeline in that project, and the pipeline is
successful and contains a successful job named "build-this" or "build-that",
gitlab-hook will execute the configured command and print "Hello, world!" to
its log.

The following table lists the configuration entries for all types of hooks:

Configuration | Type   | Optionality | Meaning
--------------|--------|-------------|-----------------------------------------
type          | string | mandatory   | type of the hook, e.g. "pipeline" for a pipeline hook
name          | string | mandatory   | name of the hook for gitlab-hook's log
uri_path      | string | mandatory   | URI-Path at which the hook can be triggered on the server
token         | string | mandatory   | secret token to authenticate Gitlab
peer_address  | string | optional    | only allow request to hook from that IP address
command       | string | optional    | shell command to execute
timeout       | int    | optional    | amount of seconds after which the running command will be killed
run_as        | dict   | depends     | contains:
run_as.user   | string | mandatory   | the Linux user account with which to execute the command
run_as.group  | string | optional    | the Linux group with which to execute the command

The timeout defaults to 60 seconds. The "run_as" entry is only optional if
gitlab-hook is executed with a regular user account. If gitlab-hook is executed
by root, "run_as" must be specified. To confirm that the command is to be
executed with root privileges, you must say explicitly:

    run_as.user = "root"

You can configure multiple hooks at the same URI-Path, in which case an
incoming request will check all matching hooks, possibly executing multiple
commands.

A hook with type "pipeline" has the following additional configuration entries:

Configuration | Type         | Optionality | Meaning
--------------|--------------|-------------|-----------------------------------------
job_name      | string/array | mandatory   | the Gitlab CI job name or an array of names this hook matches
status        | string/array | optional    | Gitlab pipeline status or array of status values this hook matches

To aid you with debugging your hook triggers, there is a special type "debug"
hook which writes the JSON payload received from Gitlab to gitlab-hook's log.


### Hook Commands

For your hooks, the typical command will be to invoke a shell script. We
recommend to create a directory `/etc/gitlab-hook/scripts` and put all command
scripts there.

Commands will not be executed concurrently. Gitlab-hook will schedule the
commands triggered by Gitlab, and execute them one after the other. Commands
are ordered as the incoming requests, and as they appear in the configuration
file.

A command will be invoked with certain environment variables set by gitlab-hook
to control the script's behavior. These variables are similar to the
[CI/CD variables provided by Gitlab](https://docs.gitlab.com/ee/ci/variables/).
The following table lists all variables set by gitlab-hook:

Variable           | Hook type | Meaning
-------------------|-----------|---------------------
CI_COMMIT_REF_NAME | pipeline  | branch or tag name which the project is built for
CI_COMMIT_SHA      | pipeline  | commit revision which the project is built for
CI_COMMIT_TAG      | pipeline  | commit tag name; only if pipeline executed for a tag
CI_JOB_IDS         | pipeline  | ID of the Gitlab jobs matched for the hook
CI_JOB_NAMES       | pipeline  | names of the Gitlab jobs matched for the hook
CI_PIPELINE_ID     | pipeline  | instance-level ID of the Gitlab pipeline
CI_PROJECT_ID      | all       | ID of the Gitlab project
CI_PROJECT_PATH    | all       | path of the Gitlab project, including the namespace
CI_PROJECT_TITLE   | all       | human-readable name of the Gitlab project
CI_PROJECT_URL     | all       | HTTP(S) address of the Gitlab project
CI_SERVER_URL      | all       | base URL of the GitLab instance, including protocol and port

The CI_JOB_IDS and CI_JOB_NAMES can be lists of job IDs and names, if the hook
configuration contained a list in the "job_name" entry.



## Build

Gitlab-hook has these build dependencies:

- [cmake](https://cmake.org/)
- [boost program_options](https://www.boost.org/doc/libs/1_85_0/doc/html/program_options.html)
- [libevent](https://libevent.org/)
- [libgtest](https://google.github.io/googletest/)
- [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)
- [libsystemd](https://github.com/systemd/systemd)
- [libtoml11](https://github.com/ToruNiina/toml11)
- [nlohmann json](https://github.com/nlohmann/json)

You can compile gitlab-hook using CMake as follows:

    mkdir tmp
    cd tmp
    cmake ..
    cmake --build .

Or create a Debian package:

    debuild -i -us -uc -b
