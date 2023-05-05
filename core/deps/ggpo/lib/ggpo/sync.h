/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _SYNC_H
#define _SYNC_H

#include <array>
#include <queue>
#include "ggponet.h"
#include "game_input.h"
#include "ggpo_types.h"
#include "input_queue.h"
#include "network/udp_msg.h"

#define MAX_PREDICTION_FRAMES    8

class SyncTestBackend;

class Sync {
 public:
  struct Config {
    GGPOSessionCallbacks callbacks;
    int num_prediction_frames;
    int num_players;
    int input_size;
  };
  struct Event {
    enum {
      ConfirmedInput,
    } type;
    union {
      struct {
        GameInput input;
      } confirmedInput;
    } u;
  };

 public:
  Sync(UdpMsg::connect_status *connect_status);
  virtual ~Sync();

  auto initialize(Config &config) -> void;

  auto setLastConfirmedFrame(int frame) -> void;
  auto setFrameDelay(int queue, int delay) -> void;
  auto addLocalInput(int queue, GameInput &input) -> bool;
  auto addRemoteInput(int queue, GameInput &input) -> void;
  auto getConfirmedInputs(void *values, int size, int frame) -> int;
  auto synchronizeInputs(void *values, int size) -> int;

  auto checkSimulation(int timeout) -> void;
  auto adjustSimulation(int seek_to) -> void;
  auto incrementFrame() -> void;

  auto frameCount() const -> int { return frame_count_; }
  auto rollingBack() const -> bool { return rolling_back_; }
  auto predictedFrames() const -> int { return frame_count_ - last_confirmed_frame_; }

  auto getEvent(Event &e) -> bool;

 protected:
  friend SyncTestBackend;

  struct SavedFrame {
    byte *buf{nullptr};
    int cbuf{0};
    int frame{-1};
    int checksum{0};
  };
  struct SavedState {
    std::array<SavedFrame, MAX_PREDICTION_FRAMES + 2> frames;
    int head{0};
  };

  auto loadFrame(int frame) -> void;
  auto saveCurrentFrame() -> void;
  auto findSavedFrameIndex(int frame) -> int;
  auto getLastSavedFrame() -> SavedFrame &;

  auto createQueues(Config &config) -> bool;
  auto checkSimulationConsistency(int *seekTo) -> bool;
  auto resetPrediction(int frameNumber) -> void;

 protected:
  GGPOSessionCallbacks callbacks_{};
  SavedState saved_state_;
  Config config_{};

  bool rolling_back_{};
  int last_confirmed_frame_;
  int frame_count_;
  int max_prediction_frames_;

  InputQueue *input_queues_;

  std::queue<Event> event_queue_;
  UdpMsg::connect_status *local_connect_status_;
};

#endif

