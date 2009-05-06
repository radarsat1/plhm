#!/bin/bash
/sbin/modprobe usbserial vendor=0x0f44 product=0xff12
/home/steve/liberty/libertyd 8 >/dev/null 2>&1 &
