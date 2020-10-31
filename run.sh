#!/usr/bin/env bash

echo "setting proper permissions"
sudo setcap cap_net_raw+ep /home/spence/code/networking/ping/cmake-build-debug/ping

echo "running ping"
/home/spence/code/networking/ping/cmake-build-debug/ping
