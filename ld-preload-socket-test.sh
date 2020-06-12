#!/bin/bash

set -e

SO_PATH=`pwd`/ld-preload-socket.so

echo ""
echo "Testing empty mappings"
export LD_PRELOAD_SOCKET_UNIX_SOCK_MAP=
export LD_PRELOAD_SOCKET_INET_PORT_MAP=

LD_PRELOAD=$SO_PATH /bin/true 2>&1 | grep "LD_PRELOAD_SOCKET: 0 mappings defined for UNIX"
LD_PRELOAD=$SO_PATH /bin/true 2>&1 | grep "LD_PRELOAD_SOCKET: 0 mappings defined for INET"

echo ""
echo "Testing inet mappings"
export LD_PRELOAD_SOCKET_UNIX_SOCK_MAP=
export LD_PRELOAD_SOCKET_INET_PORT_MAP="1234:5678, 2345 : 6789
  3456: 7890"

LD_PRELOAD=$SO_PATH /bin/true 2>&1 | grep "1234 -> 5678"
LD_PRELOAD=$SO_PATH /bin/true 2>&1 | grep "2345 -> 6789"
LD_PRELOAD=$SO_PATH /bin/true 2>&1 | grep "3456 -> 7890"

echo ""
echo "Testing inet server"
export LD_PRELOAD_SOCKET_INET_PORT_MAP="1234:5678"
LD_PRELOAD=$SO_PATH netcat -n -q 0 -l 1234 &
sleep 0.5
echo "." | netcat -N localhost 5678

echo ""
echo "Testing inet client"
netcat -n -q 0 -l 5678 &
sleep 0.5
echo "." | LD_PRELOAD=$SO_PATH netcat -N localhost 1234

echo ""
echo "Testing unix mappings"

export LD_PRELOAD_SOCKET_UNIX_SOCK_MAP="/tmp/ld-preload-socket-test-1:/tmp/ld-preload-socket-test-2, /tmp/ld-preload-socket-test5 :/tmp/ld-preload-socket-test6
 ld-preload-socket-test3 : ld-preload-socket-test4,
 ld-preload-socket-test5:ld-preload-socket-test6"
export LD_PRELOAD_SOCKET_INET_PORT_MAP=

LD_PRELOAD=$SO_PATH /bin/true 2>&1 | grep "'/tmp/ld-preload-socket-test-1' -> '/tmp/ld-preload-socket-test-2'"
LD_PRELOAD=$SO_PATH /bin/true 2>&1 | grep "'/tmp/ld-preload-socket-test5' -> '/tmp/ld-preload-socket-test6'"
LD_PRELOAD=$SO_PATH /bin/true 2>&1 | grep "'ld-preload-socket-test3' -> 'ld-preload-socket-test4'"
LD_PRELOAD=$SO_PATH /bin/true 2>&1 | grep "'ld-preload-socket-test5' -> 'ld-preload-socket-test6'"

echo ""
echo "Testing unix server"
export LD_PRELOAD_SOCKET_UNIX_SOCK_MAP="/tmp/ld-preload-socket-test-source:/tmp/ld-preload-socket-test-target"

LD_PRELOAD=$SO_PATH netcat -n -U -q 0 -l /tmp/ld-preload-socket-test-source &
sleep 0.5
echo "." | netcat -N -U /tmp/ld-preload-socket-test-target

echo ""
echo "Testing unix client"

netcat -n -U -q 0 -l /tmp/ld-preload-socket-test-target &
sleep 0.5
echo "." | LD_PRELOAD=$SO_PATH netcat -N -U /tmp/ld-preload-socket-test-source
