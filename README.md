AVB-linux
========

Alsa driver and supporting tools based on the OpenAvnu project.

Currently this implementation will create an alsa PCM device and connect a single 8in/8out AVB stream to an AVB endpoint.


LICENSING AND CONTRIBUTION GUIDELINES
======================================
To the extent possible, content is licensed under BSD licensing terms. Linux 
kernel mode components are provided under a GPLv2 license. 
Licensing information is included in the various directories to eliminate confusion. 
Please review the ‘LICENSE’ file included in the head of the 
various subdirectories for details.


GIT SUBMODULES
==============

After checking out the avb-linux git repository submodules should be
configured by going::

    git submodule init
    git submodule update

    Then compile the jdksavdecc library:

    cd  jdksavdecc-c
    cmake .
    make

    cd ..
    make
    sudo make install

The 'sudo make install' will create the directory $HOME/.avb and copy the binaries and kernel-module there
The bash scripts 'avb_up.sh' and 'avb_down.sh' are copied to /usr/local/bin

To start, enter in a console:

sudo avb_up.sh \<ethernet interface name\> \<samplerate\>

To stop

sudo avb_down.sh \<ethernet interface name\>

RELATED OPEN SOURCE PROJECTS
============================

OpenAvnu
--------

The OpenAvnu project where most of the code originates from.

+ https://github.com/AVnu/OpenAvnu

AVDECC
------
AVDECC library by Jeff Koftinoff

+ https://github.com/jdkoftinoff/jdksavdecc-c

