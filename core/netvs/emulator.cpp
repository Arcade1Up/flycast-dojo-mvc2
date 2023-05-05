#include "emulator.hpp"
#include "cfg/option.h"
#include "emulator.h"
#include "hw/pvr/Renderer_if.h"
#include "hw/sh4/sh4_if.h"
#include "netvs_common.h"
#include "network/ggpo.h"
#include "rend/imgui_driver.h"
#include "delay.h"
#include "hw/sh4/sh4_sched.h"
#include <iomanip>

namespace sen {
uint32_t nFramesEmulated;

uint32_t nFramesRendered;
uint8_t *pBurnDraw;
int nBurnFPS = 6000;

int16_t *pBurnSoundOut;
int nBurnSoundRate = 44100;
int nBurnSoundLen;

bool Emulator::initialize() {
  state_ = State::WaitToRun;
  ticks.initialize();

  if (settings.content.path.empty()) {
    ZGS_LOG("The game path is empty!");
    return false;
  }

  emu.loadGame(settings.content.path.c_str(), nullptr);

  if (kHostFlag == SINGLEPLAYER) {
    loadLocalState();
  } else {
    dc_loadstate(2);

    receive_key_ = true;

    frame_counter_ = 0;
    virtual_frame_counter_ = 0;
    total_frame_counter_ = 0;

    nCurrentFrame = 0;
  }

  emu.start();

  highScore.initialize();

  swap_thread_run_ = false;

  config::LimitFPS = true;
  config::VSync = true;
  config::Fog = true;
  config::AutoSkipFrame = 1;
  // config::SkipFrame = 0;
  return true;
}
void Emulator::terminate() {
  highScore.terminate();
  emu.stop();
  emu.term();
  // todo: termination of components

  ticks.terminate();// Terminate timing components
  state_ = State::Finish;
}
auto Emulator::loadLocalState() -> void {
  highScore.terminate();

  dc_loadstate(0);
  receive_key_ = false;
  if (!game_dip_changed) {
    if (!dc_loadstate(1)) {
      game_dip_changed = true;
      // delete the file

    } else {
      receive_key_ = true;
    }
  }

  frame_counter_ = 0;
  virtual_frame_counter_ = 0;
  total_frame_counter_ = 0;

  nCurrentFrame = 0;
  highScore.initialize();
}

void Emulator::runOneFrame(bool draw) {
  settings.disableRenderer = !draw || disable_render_;
  settings.aica.muteAudio = pBurnSoundOut == nullptr;
  emu.run();
}
void Emulator::endOneFrame() {
  delay::endOfFrame();
  if (!sh4_cpu.IsCpuRunning()) {
    return;
  }
  if (ggpo::rollbacking()) {
    sh4_cpu.Stop();
    return;
  }
  ++virtual_frame_counter_;
  ++total_frame_counter_;
  sh4_cpu.Stop();
  settings.aica.muteAudio = false;
  settings.disableRenderer = false;

  if (kHostFlag == SINGLEPLAYER) {
    if (total_frame_counter_ == 330 && game_dip_changed) {
      dc_savestate(1);
      game_dip_changed = false;
      receive_key_ = true;
    }
  }

  if (kHostFlag == SINGLEPLAYER) {
    highScore.updateScores();
    // uint32_t addr = 0x0c2f8688;
    // for (int i = 0; i < 42; ++i) {
    //   ZGS_LOG("0x%016x, 0x%016x, 0x%016x", ReadMem64(addr), ReadMem64(addr + 8), ReadMem64(addr + 16));
    //   addr += 0x18;
    // }
  }

  if (reload_state) {
    // emu.unloadGame();
    // emu.loadGame(settings.content.path.c_str(), nullptr);
    loadGameState(reload_state_index);
    reload_state = false;
  }
}
void Emulator::rendOneFrame() {
  os_CreateWindow();
  rend_init_renderer();
  rend_resize_renderer();

  swap_thread_run_ = true;
  while (swap_thread_run_) {
    if (rend_single_frame(true) /*&& virtual_frame_counter_ % 6 != 0*/) {
      imguiDriver->present();
      ++frame_counter_;
    }
  }

  rend_term_renderer();// Terminate rendering components

#if defined(USE_SDL)
  sdl_window_destroy();
#endif
}

void Emulator::loadGameState(int index) {
  emu.stop();
  dc_loadstate(index);
  receive_key_ = true;

  frame_counter_ = 0;
  virtual_frame_counter_ = 0;
  total_frame_counter_ = 0;
  emu.start();

  nCurrentFrame = 0;
}

Ticks ticks;
Emulator emulator;
MemWatch watch;

void MemWatch::initialize() {
  for (uint32_t addr = 0x0c000000; addr < 0xd000000; addr += 4) {
    mem32[addr] = ReadMem32(addr);
  }

  ZGS_LOG("Mem initialize: %d", mem32.size());
}
void MemWatch::updateChanged() {
  for (auto beg = mem32.begin(); beg != mem32.end(); ) {
    uint32_t val = ReadMem32(beg->first);
    if (val == beg->second) {
      beg = mem32.erase(beg);
    } else {
      beg->second = val;
      ++beg;
    }
  }

  ZGS_LOG("Mem remain: %d", mem32.size());
}
void MemWatch::updateUnchanged() {
  for (auto beg = mem32.begin(); beg != mem32.end(); ) {
    uint32_t val = ReadMem32(beg->first);
    if (val != beg->second) {
      beg = mem32.erase(beg);
    } else {
      ++beg;
    }
  }

  ZGS_LOG("Mem remain: %d", mem32.size());
}
void MemWatch::print() {
  for (auto &val : mem32) {
    ZGS_LOG("0x%08x: 0x%08x", val.first, val.second);
  }
}
}// namespace sen

#include "emulator/hiscore.icc"
#include "emulator/input.icc"
