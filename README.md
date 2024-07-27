# CommandGateway
CommandGateway is a self-contained UNIX socket server aimed at receiving commands from clients in unprivileged environment, performing custom permission checking, and executing these commands in a privileged environment.
It has been adapted to serve as a command gateway for Docker containers, allowing processes within them to securely request a service from the host OS.
# Features
 - Isolated request handling: Each client request is processed in a separate child process
 - 5 built-in privilege levels associated with client's group credentials
 - 5 log verbosity levels
 - Concurrent log access protection
 - Debug mode (logging every failed function call)
# Environment variables
 - SOCK_PATH (mandatory) - Path to the socket where the container will listen to, relative to the container root folder
 - CONTAINER_SUBVOL_ID (mandatory) - name of the container root directory in /var/lib/btrfs/subvolumes
 - LOG_LEVEL (optional, default: INFO) - Possible values: NONE, ERROR, WARNING, INFO, DEBUG
 - TIMEOUT (optional, default: 5) - Time in seconds for the maximum duration of socket operations
# Compiling
`gcc *.c -Wall -Werror -Wno-parentheses -o server`
# TODO
 - Make container subvolumes path a configuration option (instead of hardcoded value)
 - Make group names associated with privileges a configuration option (instead of hardcoded value)
