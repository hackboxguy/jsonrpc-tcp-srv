// Small leveled logger for xmproxysrv: timestamp, level, message on stdout.
// Level is process-wide; --debuglog selects DEBUG, --loglevel=... overrides.
#ifndef __XM_LOG_H_
#define __XM_LOG_H_
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

typedef enum XMLOG_LEVEL_T {
  XMLOG_ERROR = 0,
  XMLOG_WARN,
  XMLOG_INFO,
  XMLOG_DEBUG
} XMLOG_LEVEL;

int xmlog_get_level();
void xmlog_set_level(XMLOG_LEVEL level);
// returns -1 when the name is unknown
int xmlog_set_level_by_name(const char *name);
void xmlog_write(XMLOG_LEVEL level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#define XMLOG_ERR(...) xmlog_write(XMLOG_ERROR, __VA_ARGS__)
#define XMLOG_WRN(...) xmlog_write(XMLOG_WARN, __VA_ARGS__)
#define XMLOG_INF(...) xmlog_write(XMLOG_INFO, __VA_ARGS__)
#define XMLOG_DBG(...)                                                         \
  do {                                                                         \
    if (xmlog_get_level() >= XMLOG_DEBUG)                                      \
      xmlog_write(XMLOG_DEBUG, __VA_ARGS__);                                   \
  } while (0)
#endif
