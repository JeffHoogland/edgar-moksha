# edgar-moksha
Backport of the edgar module with support for moksha.

Edgar Gadget Loader

This module load python gadgets in Enlightenment.


Requirements
============

* Moksha >= 0.1
* Python >= 3.2 (also the -dev package if your disto need them)
* Python-EFL >= 1.11 (built for python3)
* python3-dbus
* python3-psutil for cpu and mem gadget


Install
=======

To install the edgar module use the standard:
```bash
  ./autogen.sh
  make
  sudo make install
```
If py3 is not the default on your system you need to specify the version:

  PYTHON_VERSION=3.x ./autogen.sh
  
Bodhi users:
```bash
 PYTHON_VERSION=3.4 ./autogen.sh
```

Gadgets are in the GADGETS/ folder, to install them just use:

  (sudo) make install


The Audio gadget
================

The gadget provide PulseAudio + mpris2 integration.

It dbus to speak with PulseAudio, thus you need to enable dbus in pulse,
in the /etc/pulse/default.pa you need to have the line:

  load-module module-dbus-protocol

Usage tips:
  * the main speaker act on the pulse fallback channel, you can change that
    using the pavucontrol application.
  * mouse-wheel on the speaker to change volume.
  * middle-click on the speaker or any slider to toggle mute.


The Dropbox gadget
==================

A simple gadget that show the dropbox daemon status, it also have the ability
to start/stop the daemon.


The LedClock gadget
====================

Usage tips:
  * click on the first led column to trigger fancy animations.


How to write your own gadget
============================

I suggest to start from the ruler gadget, just copy it's folder and
start hacking, a minimal gadget require:

base_folder/   (need to have the same name as the gadget)
  Makefile     (the provided makefile should work for you)
  __init__.py  (the gadget python script)
  gadget.edc   (the gadget edje file)
  images/
    ...
  fonts/
    ...

Look at the e.py file (in the python/ folder in sources) for more
documentation.
