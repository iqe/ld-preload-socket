# ld-preload-socket

Use this library with LD_PRELOAD to redirect unix domain sockets and inet sockets to different paths or ports.

This is useful if you're working with software that does not allow to reconfigure which ports or unix domains sockets it uses. Works for servers and clients.

## Installation

Compile the library with make. The resulting file is called `ld-preload-socket.so`:

```
$> make
gcc -std=c99 -Wall -shared -fPIC ld-preload-socket.c -o ld-preload-socket.so -ldl

```

## Usage

The two environment variables `LD_PRELOAD_SOCKET_UNIX_SOCK_MAP` and `LD_PRELOAD_SOCKET_INET_PORT_MAP` can contain one or more mappings from source to target paths/ports in the format `source:destination`. Use `,` to specify multiple pairs.

When preloading the library with LD_PRELOAD, all calls to `connect()` and `bind()` are mapped according to those environment variables. Any port/path not mentioned is left untouched.

Additionally, any call to `unlink()` for one of the source paths is mapped to its target path. This enables the "unlink then bind" pattern that is often used by unix socket servers.

## Examples

Redirect the listening port of a server:

```
$> export LD_PRELOAD_SOCKET_INET_PORT_MAP="1234:5678"
$>
$> LD_PRELOAD=`pwd`/ld-preload-socket.so netcat -l 1234
LD_PRELOAD_SOCKET: 0 mappings defined for UNIX socket paths
LD_PRELOAD_SOCKET: 1 mappings defined for INET socket ports
LD_PRELOAD_SOCKET:    1234 -> 5678
LD_PRELOAD_SOCKET: Mapping AF_INET bind(1234) to bind(5678)

# Netcat is now listening on port 5678. But it thinks it is listening on port 1234.

```

Redirect the port that a client connects to:

```
$> export LD_PRELOAD_SOCKET_INET_PORT_MAP="80:8080"
$>
$> LD_PRELOAD=`pwd`/ld-preload-socket.so curl localhost:80
LD_PRELOAD_SOCKET: 0 mappings defined for UNIX socket paths
LD_PRELOAD_SOCKET: 1 mappings defined for INET socket ports
LD_PRELOAD_SOCKET:    80 -> 8080
LD_PRELOAD_SOCKET: Mapping AF_INET connect(80) to connect(8080)

# Curl is now connected to port 8080. But it thinks it is connected to port 80.
```

Redirect the unix socket path of a server:

```
$> export LD_PRELOAD_SOCKET_UNIX_PATH_MAP="/tmp/socket-a:/tmp/socket-b"
$>
$> LD_PRELOAD=`pwd`/ld-preload-socket.so netcat -l -U /tmp/socket-a
LD_PRELOAD_SOCKET: 1 mappings defined for UNIX socket paths
LD_PRELOAD_SOCKET:    '/tmp/socket-a' -> '/tmp/socket-b'
LD_PRELOAD_SOCKET: 0 mappings defined for INET socket ports
LD_PRELOAD_SOCKET: Mapping AF_UNIX unlink(/tmp/socket-a) to unlink(/tmp/socket-b)
LD_PRELOAD_SOCKET: Mapping AF_UNIX bind(/tmp/socket-a) to bind(/tmp/socket-b)

# Netcat is now listening on /tmp/socket-b. But it thinks it is listening on port /tmp/socket-a.
```

Redirect the unix socket path that a client connects to:

```
$> export LD_PRELOAD_SOCKET_UNIX_PATH_MAP="/tmp/socket-a:/tmp/socket-b"
$>
$> LD_PRELOAD=`pwd`/ld-preload-socket.so netcat -U /tmp/socket-a
LD_PRELOAD_SOCKET: 1 mappings defined for UNIX socket paths
LD_PRELOAD_SOCKET:    '/tmp/socket-a' -> '/tmp/socket-b'
LD_PRELOAD_SOCKET: 0 mappings defined for INET socket ports
LD_PRELOAD_SOCKET: Mapping AF_UNIX connect(/tmp/socket-a) to connect(/tmp/socket-b)

# Netcat is now connected to /tmp/socket-b. But it thinks it is connected to port /tmp/socket-a.
```

## Limitations

* Parsing of environment variables does not detect malformed input

## License

MIT
