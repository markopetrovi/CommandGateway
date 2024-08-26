# CommandGateway
CommandGateway is a self-contained UNIX socket server aimed at receiving commands from clients in unprivileged environment, performing custom permission checking, and executing these commands in a privileged environment.
It has been adapted to serve as a command gateway for Docker containers, allowing processes within them to securely request a service from the host OS.
# Features
 - Isolated request handling: Each client request is processed in a separate child process
 - 5 built-in privilege levels associated with client's group credentials
 - 5 log verbosity levels
 - Concurrent log access protection
 - Debug mode (logging every failed function call in detail)
# Environment variables
 - SOCK_PATH (mandatory) - Path to the socket where the server will listen to and client connect to, relative to the ROOT_PATH
 - ROOT_PATH (mandatory, server-only) - name of the root directory relative to which /etc/group and /etc/passwd are searched. For client, defaults to /
 - LOG_LEVEL (optional, default: WARNING) - Possible values: NONE, ERROR, WARNING, INFO, DEBUG
 - TIMEOUT (optional, default: 5) - Time in seconds for the maximum duration of socket operations that are expected not to wait long
# Command line arguments (server-only)
 - `-f / --foreground` Do not daemonize the server. Logs are redirected to stdout
# Compiling
 - To build codebase as server, use:
`make` or `make server`
 - To build codebase as client, use:
`make client`
 - To clean the repository from generated binaries, use:
`make clean`
# TODO
 - Make container subvolumes path a configuration option (instead of hardcoded value)
 - Make group names associated with privileges a configuration option (instead of hardcoded value)
