#pragma once

#ifndef DELAY_H_
#define DELAY_H_

#include <deque>
#include <vector>
#include <chrono>
#include <map>

enum ControlByte {
  NORMAL,
  INCREMENT,
  DECREMENT,
};
struct Input {
  int frame;
  uint8_t control; ///< 正常运行，增加一帧，减少一帧
  uint16_t key;    ///< 按键信息
  uint32_t ping;    ///< 按键信息
};

enum COMMAND {
  CMD_PING = 0x20,
  CMD_PONG = 0x21,
};

extern std::deque<Input> myPlayKey, peerPlayKey;
extern int delayFrameCounter;
extern std::vector<int> delayPing;
extern std::chrono::steady_clock::time_point sendAckBeg;

namespace delay {
void startDelayNetwork();
void stopDelayNetwork();

void runOneFrame(bool draw, uint16_t key);
void endOfFrame();

uint32_t getNowMilliseconds();
}

#endif //DELAY_H_
