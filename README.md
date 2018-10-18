# FT8 library 

A C++ implementation of FT8 protocol, mostly intended for experimental use on microcontrollers. At the moment only encoding is implemented.

The encoding process is relatively light on resources, and an Arduino should be perfectly capable of running this code.

The intent of this library is to foster experimentation with e.g. automated beacons. FT8 already supports free-text messages and the upcoming new version will support raw telemetry data (71 bits).

Work is in progress (possibly taking forever) to explore decoding options. On a fast 32-bit microcontroller decoding might be possible, perhaps with some tradeoffs.

# What works

So far only encoding is implemented. It should support the basic set of FT8 messages:
* CQ {call} {grid}, e.g. CQ CA0LL GG77
* CQ {xy} {call} {grid}, e.g. CQ JA CA0LL GG77
* {call} {call} {report}, e.g. CA0LL OT7ER R-07
* {call} {call} 73/RRR/RR73, e.g. OT7ER CA0LL 73

# What doesn't

* Encoding free-text messages
* Encoding contest mode messages
* Encoding compound callsigns with country prefixes and mode suffixes
* Decoding

# What to do with it

You can generate 15-second WAV files with your own messages as a proof of concept or for testing purposes. They can either be played back or opened directly from WSJT-X. To do that, run ```make``` and build ```gen_ft8```. Then run it. Currently messages are modulated at 1000-1050 Hz.

# References and credits

Thanks to Robert Morris, AB1HL, whose Python code (https://github.com/rtmrtmrtmrtm/weakmon) inspired this and helped to test various parts of the code.

This would not of course be possible without the original WSJT-X code, which is mostly written in Fortran (http://physics.princeton.edu/pulsar/K1JT/wsjtx.html). I believe that is the only 'documentation' of the FT8 protocol available, and the source code was used as such in this project.

Karlis Goba,
YL3JG
