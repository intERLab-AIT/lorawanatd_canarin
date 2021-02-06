/* vim: set autoindent noexpandtab tabstop=4 shiftwidth=4 */

#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>

#define log(priority, fmt, ...) _log(priority, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

static inline void _log(int priority, const char *file, const int line, const char *fmt, ...)
{
	char buf[8196];
	va_list args;
  struct timeval t;
  gettimeofday(&t, NULL);
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);
#ifdef PSTDOUT
	printf("[%d][%ld.%ld]%s:%d:%s\n", getpid(), t.tv_sec, t.tv_usec, file, line, buf);
#else
	syslog(priority, "[%d]%s:%d:%s", getpid(), file, line, buf);
#endif
}


#endif
