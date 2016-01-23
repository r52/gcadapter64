#pragma once

#ifndef __OP64_COMPILE__
#include <iostream>

#define LOG_TRACE(name) std::clog
#define LOG_DEBUG(name) std::clog
#define LOG_INFO(name) std::clog
#define LOG_WARNING(name) std::clog
#define LOG_ERROR(name) std::clog
#define LOG_FATAL(name) std::clog

namespace op
{
    size_t strlcpy(char *dst, const char *src, size_t dsize);
}
#endif
