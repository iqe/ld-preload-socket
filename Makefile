CFLAGS += -std=c99 -Wall

default: ld-preload-socket.so

ld-preload-socket.so: ld-preload-socket.c
	gcc $(CFLAGS) -shared -fPIC ld-preload-socket.c -o ld-preload-socket.so -ldl

test: ld-preload-socket.so
	bash ld-preload-socket-test.sh

clean:
	rm *.so
