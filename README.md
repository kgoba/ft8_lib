# FT8 library 

A C++ implementation of FT8 protocol, mostly intended for experimental use on microcontrollers. At the moment only encoding is implemented.

The encoding process is relatively light on resources, and an Arduino should be perfectly capable of running this code.

The intent of this library is to foster experimentation with e.g. automated beacons. For example, FT8 supports free-text messages and raw telemetry data (71 bits).

Work is in progress (possibly taking forever) to explore decoding options. On a fast 32-bit microcontroller decoding might be possible, perhaps with some tradeoffs.

# What works

Currently the basic message set of the revised FT8 protocol with 77-bit payload (introduced since WSJT-X version 2.0) is supported:
* CQ {call} {grid}, e.g. CQ CA0LL GG77
* CQ {xy} {call} {grid}, e.g. CQ JA CA0LL GG77
* {call} {call} {report}, e.g. CA0LL OT7ER R-07
* {call} {call} 73/RRR/RR73, e.g. OT7ER CA0LL 73

So far only encoding is implemented. There is historical code that supports the same set of FT8 version 1 (75-bit) messages plus free-text messages.

# What doesn't

I'm currently working on these features:
* Encoding free-text messages (up to 13 characters from a limited alphabet)
* Encoding telemetry data
* Decoding

These features are low on my priority list:
* Encoding contest mode messages
* Encoding compound callsigns with country prefixes and mode suffixes

# What to do with it

You can generate 15-second WAV files with your own messages as a proof of concept or for testing purposes. They can either be played back or opened directly from WSJT-X. To do that, run ```make``` and build ```gen_ft8```. Then run it. Currently messages are modulated at 1000-1050 Hz.

# References and credits

Thanks to Robert Morris, AB1HL, whose Python code (https://github.com/rtmrtmrtmrtm/weakmon) inspired this and helped to test various parts of the code.

This would not of course be possible without the original WSJT-X code, which is mostly written in Fortran (http://physics.princeton.edu/pulsar/K1JT/wsjtx.html). I believe that is the only 'documentation' of the FT8 protocol available, and the source code was used as such in this project.

Thanks to Mark Borgerding for his FFT implementation (https://github.com/mborgerding/kissfft). I have included a portion of his code.

Karlis Goba,
YL3JG
