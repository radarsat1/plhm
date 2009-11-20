plhm
====

Introduction
------------

This is a small C library `libplhm` to encapsulate communication with
Polhemus motion tracking devices.  These devices communicate with a
computer using a USB serial port or RS232 port.  Included is a small
program `plhm` which can be used to request data from the device, and
record this to a file or send it across the network using the [Open
Sound Control](http://opensoundcontrol.org) protocol.

We needed such a program to record motion tracking data during
experimental sessions in our lab for studying musical gesture, and for
communicating with [_Max/MSP_](http://cycling74.org) and [_Pure
Data_](http://puredata.info) to interact with real-time sound
synthesis.  The result is this small driver that can be used for any
purpose.

The protocol has been derived from documentation from
[Polhemus](http://polhemus.com), available in the [Liberty Manual][1]

Run `plhm --help` to get information on usage.

The Polhemus USB interface can be used under Linux by loading the
`visor` kernel module with the appropriate vendor and product
identifiers:

    modprobe visor vendor=0x0f44 product=0xff12

This should make the device show up as a USB serial device, `ttyUSB0`.
If you wish to use a different device, pass the `-d` option to `plhm`.
Ensure you have permission to read and write the device.  For example,

    $ ls -l /dev/ttyUSB0
    crw-rw---- 1 root dialout 4, 64 2009-11-18 15:43 /dev/ttyUSB0

This indicates that your user must be in the `dialout` group before
running `plhm`.

Note that `plhm` can be run in the daemon mode (option `-D`).  This
makes `plhm` wait until the device is available, and then opens it and
allows control of the device using Open Sound Control.  We use this
mode in our lab, executed by a boot script on the dedicated headless
computer.  This allows us to treat the computer and device as an
OSC-controlled "appliance", interacting over the network with
_Max/MSP_.

Status
------

Currently the library does not cover the full functionality of the
tracker.  It has also been only tested on an 8-station Polhemus
Liberty device, and may require some small modifications for the
Patriot device.  It is currently a Linux-only program.

Please feel free to add functionality, make suggestions, or submit
testing reports on other equipment.

Installing
----------

This program uses the autotools build system.  A normal install
process consists of the following steps:

    ./configure
    make
    make install

See [INSTALL](INSTALL) for further instructions.

Authors
-------

This library has primarily been authored by Stephen Sinclair, in the
Input Devices and Music Interaction Lab at McGill University.  It is
based on previous code by Mark Mashall, with some small contributions
from Mike Collicut.

The homepage for this program can be found at [idmil.org][2], and the
source is also available on
[github](http://github.com/radarsat1/plhm/blob/master/README.markdown).

Please report any bugs to Stephen at
[sinclair@music.mcgill.ca](mailto:sinclair@music.mcgill.ca).

Copyright
---------

This code is licensed under the GNU General Public License v2.1 or
later.  Please see [COPYING](COPYING) for more information.

[1]: http://www.polhemus.com/polhemus_editor/assets/LIBERTY%20Rev%20F%20URM03PH156.pdf

[2]: http://idmil.org/software/plhm
