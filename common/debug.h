#ifndef _INCLUDE_DEBUG_H_
#define _INCLUDE_DEBUG_H_

#include <stdio.h>

#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARN    2
#define LOG_ERROR   3
#define LOG_FATAL   4
#define LOG_NONE    5

#ifndef LOG_LEVEL
#define LOG_LEVEL   LOG_NONE
#endif

#define LOG(level, ...)     if (level >= LOG_LEVEL) fprintf(stderr, __VA_ARGS__)

#endif // _INCLUDE_DEBUG_H_
