/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "sync.h"

Sync::Sync(UdpMsg::connect_status *connect_status) :
    input_queues_(nullptr),
    local_connect_status_(connect_status) {
  frame_count_ = 0;
  last_confirmed_frame_ = -1;
  max_prediction_frames_ = 0;
  memset(&saved_state_, 0, sizeof(saved_state_));
}

Sync::~Sync() {
  /*
   * Delete frames manually here rather than in a destructor of the SavedFrame
   * structure so we can efficently copy frames via weak references.
   */
  for (auto &frame : saved_state_.frames) {
    callbacks_.free_buffer(frame.buf);
  }
  delete[] input_queues_;
  input_queues_ = nullptr;
}

auto Sync::initialize(Sync::Config &config) -> void {
  config_ = config;
  callbacks_ = config.callbacks;
  frame_count_ = 0;
  rolling_back_ = false;

  max_prediction_frames_ = config.num_prediction_frames;

  createQueues(config);
}
auto Sync::setLastConfirmedFrame(int frame) -> void {
  last_confirmed_frame_ = frame;
  if (last_confirmed_frame_ > 0) {
    for (int i = 0; i < config_.num_players; i++) {
      input_queues_[i].DiscardConfirmedFrames(frame - 1);
    }
  }
}
auto Sync::addLocalInput(int queue, GameInput &input) -> bool {
  int frames_behind = frame_count_ - last_confirmed_frame_;
  if (frame_count_ >= max_prediction_frames_ && frames_behind >= max_prediction_frames_) {
    ggpoLog("Rejecting input from emulator: reached prediction barrier.\n");
    return false;
  }

  if (frame_count_ == 0) {
    saveCurrentFrame();
  }

  ggpoLog("Sending undelayed local frame %d to queue %d.\n", frame_count_, queue);
  input.frame = frame_count_;
  input_queues_[queue].AddInput(input);

  return true;
}
auto Sync::addRemoteInput(int queue, GameInput &input) -> void {
  input_queues_[queue].AddInput(input);
}
auto Sync::getConfirmedInputs(void *values, int size, int frame) -> int {
  int disconnect_flags = 0;
  char *output = (char *) values;

  ASSERT(size >= config_.num_players * config_.input_size);

  memset(output, 0, size);
  for (int i = 0; i < config_.num_players; i++) {
    GameInput input{};
    if (local_connect_status_[i].disconnected && frame > local_connect_status_[i].last_frame) {
      disconnect_flags |= (1 << i);
      input.erase();
    } else {
      input_queues_[i].GetConfirmedInput(frame, &input);
    }
    memcpy(output + (i * config_.input_size), input.bits, config_.input_size);
  }
  return disconnect_flags;
}
auto Sync::synchronizeInputs(void *values, int size) -> int {
  int disconnect_flags = 0;
  char *output = (char *) values;

  ASSERT(size >= config_.num_players * config_.input_size);

  memset(output, 0, size);
  for (int i = 0; i < config_.num_players; i++) {
    GameInput input{};
    if (local_connect_status_[i].disconnected && frame_count_ > local_connect_status_[i].last_frame) {
      disconnect_flags |= (1 << i);
      input.erase();
    } else {
      input_queues_[i].GetInput(frame_count_, &input);
    }
    memcpy(output + (i * config_.input_size), input.bits, config_.input_size);
  }
  return disconnect_flags;
}
auto Sync::checkSimulation(int timeout) -> void {
  int seek_to;
  if (!checkSimulationConsistency(&seek_to)) {
    adjustSimulation(seek_to);
  }
}
auto Sync::incrementFrame() -> void {
  frame_count_++;
  saveCurrentFrame();
}
auto Sync::adjustSimulation(int seek_to) -> void {
  int frame_count = frame_count_;
  int count = frame_count_ - seek_to;

  ggpoLog("Catching up\n");
  rolling_back_ = true;

  /*
   * Flush our input queue and load the last frame.
   */
  loadFrame(seek_to);
  ASSERT(frame_count_ == seek_to);

  /*
   * Advance frame by frame (stuffing notifications back to
   * the master).
   */
  resetPrediction(frame_count_);
  for (int i = 0; i < count; i++) {
    callbacks_.advance_frame(0);
  }
  ASSERT(frame_count_ == frame_count);

  rolling_back_ = false;

  ggpoLog("---\n");
}

auto Sync::loadFrame(int frame) -> void {
  // find the frame in question
  if (frame == frame_count_) {
    ggpoLog("Skipping NOP.\n");
    return;
  }

  // Move the head pointer back and load it up
  saved_state_.head = findSavedFrameIndex(frame);
  SavedFrame &state = saved_state_.frames[saved_state_.head];

  ggpoLog("=== Loading frame info %d (size: %d  checksum: %08x).\n",
      state.frame, state.cbuf, state.checksum);

  ASSERT(state.buf && state.cbuf);
  callbacks_.load_game_state(state.buf, state.cbuf);

  // Reset framecount and the head of the state ring-buffer to point in
  // advance of the current frame (as if we had just finished executing it).
  frame_count_ = state.frame;
  saved_state_.head = (saved_state_.head + 1) % saved_state_.frames.size();
}
auto Sync::saveCurrentFrame() -> void {
  /*
   * See StateCompress for the real save feature implemented by FinalBurn.
   * Write everything into the head, then advance the head pointer.
   */
  SavedFrame &state = saved_state_.frames[saved_state_.head];
  if (state.buf) {
    callbacks_.free_buffer(state.buf);
    state.buf = nullptr;
  }
  state.frame = frame_count_;
  callbacks_.save_game_state(&state.buf, &state.cbuf, &state.checksum, state.frame);

  ggpoLog("=== Saved frame info %d (size: %d  checksum: %08x).\n", state.frame, state.cbuf, state.checksum);
  saved_state_.head = int((saved_state_.head + 1) % saved_state_.frames.size());
}
auto Sync::getLastSavedFrame() -> Sync::SavedFrame & {
  int i = saved_state_.head - 1;
  if (i < 0) {
    i = ARRAY_SIZE(saved_state_.frames) - 1;
  }
  return saved_state_.frames[i];
}
auto Sync::findSavedFrameIndex(int frame) -> int {
  int i, count = ARRAY_SIZE(saved_state_.frames);
  for (i = 0; i < count; i++) {
    if (saved_state_.frames[i].frame == frame) {
      break;
    }
  }
  if (i == count) {
    ASSERT(false);
  }
  return i;
}
bool Sync::createQueues(Config &config) {
  delete[] input_queues_;
  input_queues_ = new InputQueue[config_.num_players];

  for (int i = 0; i < config_.num_players; i++) {
    input_queues_[i].Init(i, config_.input_size);
  }
  return true;
}
bool Sync::checkSimulationConsistency(int *seekTo) {
  int first_incorrect = GameInput::NullFrame;
  for (int i = 0; i < config_.num_players; i++) {
    int incorrect = input_queues_[i].GetFirstIncorrectFrame();
    ggpoLog("considering incorrect frame %d reported by queue %d.\n", incorrect, i);

    if (incorrect != GameInput::NullFrame && (first_incorrect == GameInput::NullFrame || incorrect < first_incorrect)) {
      first_incorrect = incorrect;
    }
  }

  if (first_incorrect == GameInput::NullFrame) {
    ggpoLog("prediction ok.  proceeding.\n");
    return true;
  }
  *seekTo = first_incorrect;
  return false;
}
auto Sync::setFrameDelay(int queue, int delay) -> void {
  input_queues_[queue].SetFrameDelay(delay);
}
auto Sync::resetPrediction(int frameNumber) -> void {
  for (int i = 0; i < config_.num_players; i++) {
    input_queues_[i].ResetPrediction(frameNumber);
  }
}
bool Sync::getEvent(Event &e) {
  if (event_queue_.size()) {
    e = event_queue_.front();
    event_queue_.pop();
    return true;
  }
  return false;
}


