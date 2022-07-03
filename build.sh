#!/usr/bin/sh

# g++ -Iinclude -Llib $(find . -name "*.c") -ldiscord-rpc -pthread -O3 -s -o bedd -fpermissive
gcc -Iinclude -Llib $(find . -name "*.c") -ldiscord-rpc -pthread -O3 -s -o bedd -lstdc++ -lutil
