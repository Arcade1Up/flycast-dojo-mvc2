#pragma once
#ifndef EMULATOR_HPP_
#define EMULATOR_HPP_

#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <memory>
#include <deque>

#include "types.hpp"
#include "hw/sh4/sh4_mem.h"
#include "oslib/oslib.h"

#include "ticks.h"

#ifdef USE_SDL
#include "sdl/sdl.h"
#endif

#include "emulator/input.ipp"
#include "emulator/hiscore.ipp"

namespace sen {
struct MemWatch {
  void initialize();

  void updateChanged();
  void updateUnchanged();

  void print();

  std::map<uint32_t, uint8_t> mem8;
  std::map<uint32_t, uint32_t> mem32;
};
}

namespace sen {
extern uint32_t nFramesEmulated;

extern uint32_t nFramesRendered;
extern uint8_t *pBurnDraw;

extern int16_t *pBurnSoundOut;
extern int nBurnSoundLen;

struct Ticks {
  bool initialize() {
    TicksInitialize();
    return true;
  }
  void terminate() {}
};
extern Ticks ticks;

struct Emulator {
  static constexpr int FrameTime59_5 = 10'000'000 / 595;
  static constexpr int FrameTime60_5 = 10'000'000 / 605;

  static constexpr int FrameTime59 = 1'000'000 / 59;
  static constexpr int FrameTime60 = 1'000'000 / 60;
  static constexpr int FrameTime61 = 1'000'000 / 61;
  static constexpr int FrameTime120 = 1'000'000 / 120;
  static constexpr int FrameTime240 = 1'000'000 / 240;
  static constexpr int FrameTime360 = 1'000'000 / 360;

  enum class State {
    WaitToRun,
    Running,
    Finish
  };

  bool initialize();
  void terminate();

  void run();

  inline void transferPause() { pause_flag_ = !pause_flag_; }
  void pause();
  void resume();

  void setState(State state) { state_ = state; }
  bool isState(State state) const { return state_ == state; }

  void runOneFrame(bool draw);
  void endOneFrame();
  void rendOneFrame();

  int totalFrameCount() const { return total_frame_counter_; }

  void updateInput(uint16_t p1keys, uint16_t p2keys, uint16_t p3keys, uint16_t p4keys);
  void updateOneInput(uint16_t keys, int port);
  void inputThread();
  uint16_t getKeys(int player);

  void loadGameState(int index);
  void loadLocalState();
  void setReceiveKey(bool receive_key) { receive_key_ = receive_key; }
  inline void setDisableRender(bool disable_render) { disable_render_ = disable_render; }

  inline void setReloadState(int index) { reload_state = true; reload_state_index = index; }
  inline bool getReloadState() const { return reload_state; }

 protected:
  State state_{State::Finish};

  int frame_counter_{0};
  int virtual_frame_counter_{0};
  int total_frame_counter_{0};
  bool pause_flag_{false};
  bool disable_render_{false};

  bool receive_key_{false};
  std::unique_ptr<InputDevice> input_device_;
  uint16_t prev_keys_[4];

  bool swap_thread_run_{false};
  std::thread swap_thread_;

  bool input_thread_run{false};
  std::thread input_thread;
  std::mutex input_mutex;
  std::deque<uint16_t> inputs[2];
  uint16_t lastInputs[2];

  bool reload_state{false};
  int reload_state_index{false};
};
extern Emulator emulator;

extern MemWatch watch;
}// namespace sen

extern uint32_t nCurrentFrame;
unsigned short NeoAutoVSKey(int gameid, int player);
bool isNeoCanAdjustEnergy(int);
void SetTmntReset();

#endif// EMULATOR_HPP_
