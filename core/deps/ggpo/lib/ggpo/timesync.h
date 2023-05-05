/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _TIMESYNC_H
#define _TIMESYNC_H

#include "game_input.h"
#include "ggpo_types.h"
#include <array>

#define FRAME_WINDOW_SIZE           40
#define MIN_UNIQUE_FRAMES           10
#define MIN_FRAME_ADVANTAGE          3
#define MAX_FRAME_ADVANTAGE          9

class TimeSync {
 public:
  TimeSync();
  virtual ~TimeSync();

  auto advanceFrame(GameInput &input, int advantage, int radvantage) -> void;
  auto recommendFrameWaitDuration(bool require_idle_input) -> int;

 protected:
  std::array<int, FRAME_WINDOW_SIZE> local_;
  std::array<int, FRAME_WINDOW_SIZE> remote_;
  std::array<GameInput, MIN_UNIQUE_FRAMES> last_inputs_;
  int next_prediction_;
};

#endif
