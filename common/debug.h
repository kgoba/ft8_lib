#pragma once

#include <stdio.h>


#define LOG_INFO    0
#define LOG_WARN    1
#define LOG_ERROR   2
#define LOG_FATAL   3


#define LOG(level, ...)     if (level >= LOG_LEVEL) printf(__VA_ARGS__)
