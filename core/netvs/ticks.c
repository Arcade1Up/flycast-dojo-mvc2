#include <stdint.h>
#include <time.h>

static struct timespec startTicks;

void TicksInitialize() {
  clock_gettime(CLOCK_MONOTONIC, &startTicks);
}

int64_t TicksGetUSECs() {
  int64_t time;
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  time = (now.tv_sec - startTicks.tv_sec) * 1000000 + (now.tv_nsec - startTicks.tv_nsec) / 1000;
  return time;
}
unsigned int TicksGetMSECs() {
  unsigned int time;
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  time = (now.tv_sec - startTicks.tv_sec) * 1000 + (now.tv_nsec - startTicks.tv_nsec) / 1000000;
  return time;
}
unsigned int TicksGetSECs() {
  unsigned int time;
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  time = now.tv_sec - startTicks.tv_sec;
  return time;
}
