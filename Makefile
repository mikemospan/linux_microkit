export C_INCLUDE_PATH=./include

.PHONY: all clean

all: linux_main test.so

linux_main: src/linux_main.c src/pd_main.c
	gcc -I../include src/linux_main.c src/pd_main.c -o linux_main

test.so: ./user/test.c
	gcc -shared -fPIC -I ../include ./user/test.c -o ./user/test.so

clean:
	rm -f linux_main ./user/test.so
