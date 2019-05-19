AVB-linux
========

ALsa driver and supporting tools based on the OpenAvnu project.
For more information about AVB/TSN, see also the Avnu Alliance webpage at
www.avnu.org.

Currently this implementation will create an alsa PCM device and connect a single 8in/8out AVB stream to an AVB endpoint.


LICENSING AND CONTRIBUTION GUIDELINES
======================================
To the extent possible, content is licensed under BSD licensing terms. Linux 
kernel mode components are provided under a GPLv2 license. The specific license 
information is included in the various directories to eliminate confusion. We 
encourage you to review the ‘LICENSE’ file included in the head of the 
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

    
    make
    sudo make install

The 'sudo make install' will create the directory $HOME/.avb and copy the binaries and kernel-module there
The bash scripts 'avb_up.sh' and 'avb_down.sh' are copied to /usr/local/bin

To start, enter in a console:

sudo avb_up.sh <ethernet interface name> <samplerate>

To stop

sudo avb_down.sh <ethernet interface name>

RELATED OPEN SOURCE PROJECTS
============================

OpenAvnu
--------

The OpenAvnu project where most of the code originates from.

+ https://github.com/AVnu/OpenAvnu

AVDECC
------
Jeff Koftinoff maintains a repository of AVDECC example open 
source code. AVDECC is a management layer, similar to SNMP MIB formats, 
which enables remote devices to detect, enumerate and configure AVB/TSN-related
devices based on their standardized management properties.

+ https://github.com/jdkoftinoff/jdksavdecc-c

