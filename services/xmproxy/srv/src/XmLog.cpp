#include "XmLog.h"
#include <atomic>
#include <mutex>
static std::atomic<int> g_level(XMLOG_INFO);
static std::mutex g_mutex;
int xmlog_get_level() { return g_level; }
void xmlog_set_level(XMLOG_LEVEL level) { g_level = level; }
int xmlog_set_level_by_name(const char *name) {
  if (name == NULL)
    return -1;
  if (strcmp(name, "error") == 0)
    g_level = XMLOG_ERROR;
  else if (strcmp(name, "warn") == 0 || strcmp(name, "warning") == 0)
    g_level = XMLOG_WARN;
  else if (strcmp(name, "info") == 0)
    g_level = XMLOG_INFO;
  else if (strcmp(name, "debug") == 0)
    g_level = XMLOG_DEBUG;
  else
    return -1;
  return 0;
}
void xmlog_write(XMLOG_LEVEL level, const char *fmt, ...) {
  if (level > g_level)
    return;
  static const char *names[] = {"ERROR", "WARN ", "INFO ", "DEBUG"};
  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm tmv;
  time_t sec = tv.tv_sec;
  localtime_r(&sec, &tmv);
  char stamp[40];
  strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tmv);
  char msg[2048];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  std::lock_guard<std::mutex> lock(g_mutex);
  printf("%s.%03ld [%s] %s\n", stamp, (long)(tv.tv_usec / 1000), names[level],
         msg);
  fflush(stdout);
}
