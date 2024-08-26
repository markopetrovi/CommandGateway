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
 - SOCK_PATH (default: /tmp/cgsocket) - Path to the socket where the server will listen to and client connect to, relative to the ROOT_PATH
 - ROOT_PATH (server-only, default: /) - name of the root directory relative to which /etc/group and /etc/passwd are searched. For client, always defaults to /
 - LOG_LEVEL (default: WARNING) - Possible values: NONE, ERROR, WARNING, INFO, DEBUG
 - LOG_PATH (server-only, default: /var/log/cg.log) - Where to save the daemon log
 - TIMEOUT (default: 5) - Time in seconds for the maximum duration of socket operations that are expected not to wait long
 - GROUP_TESTDEV, GROUP_DEV, GROUP_ADMIN, GROUP_SUPERUSER (server-only): Specify the group name associated with given privilege level. Overrides defaults (jmatestdev, jmadev, jmaadmin, jmaroot)
# Command line arguments (server-only)
 - `-f / --foreground` Do not daemonize the server. Logs are redirected to stdout
# Compiling
 - To build codebase as server, use:
`make` or `make server`
 - To build codebase as client, use:
`make client`
 - To clean the repository from generated binaries, use:
`make clean`
