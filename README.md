# FT8 library 

A C++ implementation of FT8 protocol, mostly intended for experimental use on microcontrollers.

The intent of this library is to foster experimentation with e.g. automated beacons. For example, FT8 supports free-text messages and raw telemetry data (71 bits).

The encoding process is relatively light on resources, and an Arduino should be perfectly capable of running this code.

Decoding is is still work in progress. So far a 15-second WAV file can be decoded on a desktop machine or SBC, and the routines are somewhat ready for a fast microcontroller, perhaps with some tradeoffs. One definite limitation is the necessity to store the whole 15-second window in some representation (either waveform or DFT spectrum) for decoding at the end of the time slot. The current implementation uses about 200 KB of RAM for that purpose. 

# What works

Currently the basic message set of the revised FT8 protocol with 77-bit payload (introduced since WSJT-X version 2.0) is supported:
* CQ {call} {grid}, e.g. CQ CA0LL GG77
* CQ {xy} {call} {grid}, e.g. CQ JA CA0LL GG77
* {call} {call} {report}, e.g. CA0LL OT7ER R-07
* {call} {call} 73/RRR/RR73, e.g. OT7ER CA0LL 73
* Free-text messages (up to 13 characters from a limited alphabet) (decoding only, untested)
* Telemetry data (71 bits as 18 hex symbols)

There is historical code that supports the same set of FT8 version 1 (75-bit) messages minus telemetry data.

# What doesn't

I'm currently working on decoding, which still needs refactoring. The code is not yet really a library, rather a collection of routines and example code.

These features are low on my priority list:
* Contest modes
* Compound callsigns with country prefixes and special callsigns

# What to do with it

You can generate 15-second WAV files with your own messages as a proof of concept or for testing purposes. They can either be played back or opened directly from WSJT-X. To do that, run ```make``` and build ```gen_ft8```. Then run it. Currently messages are modulated at 1000-1050 Hz.

# References and credits

Thanks to Robert Morris, AB1HL, whose Python code (https://github.com/rtmrtmrtmrtm/weakmon) inspired this and helped to test various parts of the code.

This would not of course be possible without the original WSJT-X code, which is mostly written in Fortran (http://physics.princeton.edu/pulsar/K1JT/wsjtx.html). I believe that is the only 'documentation' of the FT8 protocol available, and the source code was used as such in this project.

Thanks to Mark Borgerding for his FFT implementation (https://github.com/mborgerding/kissfft). I have included a portion of his code.

Karlis Goba,
YL3JG
