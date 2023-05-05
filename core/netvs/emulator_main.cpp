#include <thread>

#include "ticks.h"
#include "emulator.h"
#include "emulator.hpp"
#include "netvs_common.h"

using namespace sen;

namespace sen {
using namespace std::chrono_literals;

void Emulator::run() {
  auto beg = std::chrono::steady_clock::now();
  if (isState(State::WaitToRun)) {
    state_ = State::Running;

    swap_thread_ = std::thread(&Emulator::rendOneFrame, this);
#ifndef USE_SDL
    input_thread = std::thread(&Emulator::inputThread, this);
#endif
  }

  while (state_ == State::Running) {
    std::this_thread::sleep_for(1000us);
    if (!kNetVSGame && pause_flag_) {
      continue;
    }

    pthread_mutex_lock(&gEmuStateMutex);
    if (state_ != State::Running) {
      pthread_mutex_unlock(&gEmuStateMutex);
      break;
    }

    // update keys
    if (!pause_flag_ && receive_key_) {
      NETVSRunOneFrame(1, getKeys(0), getKeys(1), 0, 0);
    } else {
      NETVSRunOneFrame(1, 0, 0, 0, 0);
    }

    if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - beg).count() >= 1) {
      ZGS_LOG("real = %d virtual = %d total = %d", frame_counter_, virtual_frame_counter_, total_frame_counter_);

      beg = std::chrono::steady_clock::now();
      frame_counter_ = 0;
      virtual_frame_counter_ = 0;
    }

    pthread_mutex_unlock(&gEmuStateMutex);
  }

  swap_thread_run_ = false;
  if (swap_thread_.joinable() && swap_thread_.get_id() != std::this_thread::get_id()) {
    swap_thread_.join();
  }
  input_thread_run = false;
  if (input_thread.joinable() && input_thread.get_id() != std::this_thread::get_id())
	  input_thread.join();

  terminate();
}

// TODO-FIX: Added Pause state to replace pause flag_
void Emulator::pause() {
  pause_flag_ = true;
}
void Emulator::resume() {
  pause_flag_ = false;
}
}// namespace sen
