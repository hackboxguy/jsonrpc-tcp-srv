#include "ADRuntimeMinCounter.hpp"
#include <stdio.h>
using namespace std;
int ADRuntimeMinCounter::timer_notification() {
  if (counting == false)
    return 0;
  ten_sec_counter += heartbeat_ms_time;
  if (ten_sec_counter < TEN_SECONDS)
    return 0;
  ten_sec_counter = 0;
  if (get_uptime_in_seconds(&current_uptime_sec) != 0)
    return 0;
  if ((current_uptime_sec - last_uptime_sec) < 60)
    return 0;
  unsigned long diff = current_uptime_sec - last_uptime_sec;
  diff = diff / 60;
  IncreaseMinuteCount(diff);
  last_uptime_sec = current_uptime_sec;
  return 0;
}
ADRuntimeMinCounter::ADRuntimeMinCounter() {
  pMyTimer = NULL;
  ten_sec_counter = 0;
  current_uptime_sec = 0;
  last_uptime_sec = 0;
  heartbeat_ms_time = 100;
  Counter = 0;
  counting = false;
  ADRTC_SEMA_INIT();
}
ADRuntimeMinCounter::~ADRuntimeMinCounter() { counting = false; }
int ADRuntimeMinCounter::AttachHeartBeat(ADTimer *pTimer, int time_ms) {
  if (pMyTimer != NULL)
    return 0;
  pMyTimer = pTimer;
  heartbeat_ms_time = time_ms;
  pTimer->subscribe_timer_notification(this);
  return 0;
}
int ADRuntimeMinCounter::StartCounting(void) {
  if (pMyTimer == NULL) {
    printf(
        "warning: HeartBeat must be attached before calling StartCounting\n");
    return -1;
  }
  get_uptime_in_seconds(&last_uptime_sec);
  counting = true;
  return 0;
}
int ADRuntimeMinCounter::StopCounting(void) {
  counting = false;
  return 0;
}
int ADRuntimeMinCounter::IncreaseMinuteCount(int new_val) {
  ADRTC_SEMA_LOCK();
  Counter += new_val;
  ADRTC_SEMA_UNLOCK();
  return 0;
}
int ADRuntimeMinCounter::SetCounterValue(unsigned long minutes) {
  ADRTC_SEMA_LOCK();
  Counter = minutes;
  ADRTC_SEMA_UNLOCK();
  return 0;
}
unsigned long ADRuntimeMinCounter::GetCounterValue(void) {
  unsigned long temp_count;
  ADRTC_SEMA_LOCK();
  temp_count = Counter;
  ADRTC_SEMA_UNLOCK();
  return temp_count;
}
int ADRuntimeMinCounter::get_uptime_in_seconds(unsigned long *uptime) {
  FILE *cmdline = fopen("/proc/uptime", "rb");
  char *arg = 0;
  size_t size = 0;
  double tmp_time = 0.0;
  while (getdelim(&arg, &size, 0, cmdline) != -1) {
    ;
  }
  sscanf(arg, "%lf", &tmp_time);
  *uptime = (unsigned long)tmp_time;
  free(arg);
  fclose(cmdline);
  return 0;
}
