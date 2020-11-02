cmake_minimum_required(VERSION 3.16)
project(ping C)

add_executable(ping src/main.c src/prelude.h src/net.h src/debug.h)
