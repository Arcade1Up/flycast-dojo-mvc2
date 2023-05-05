/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "timesync.h"
#include <algorithm>

TimeSync::TimeSync() {
  local_.fill(0);
  remote_.fill(0);
  next_prediction_ = FRAME_WINDOW_SIZE * 3;
}

TimeSync::~TimeSync() = default;

auto TimeSync::advanceFrame(GameInput &input, int advantage, int radvantage) -> void {
  // Remember the last frame and frame advantage
  last_inputs_[input.frame % last_inputs_.size()] = input;
  local_[input.frame % local_.size()] = advantage;
  remote_[input.frame % remote_.size()] = radvantage;
}

auto TimeSync::recommendFrameWaitDuration(bool require_idle_input) -> int {
  // Average our local and remote frame advantages
  int sum = 0;
  float advantage, radvantage;

  for (int i : local_) { sum += i; }
  advantage = sum / (float) local_.size();

  sum = 0;
  for (int i : remote_) { sum += i; }
  radvantage = sum / (float) remote_.size();

  static int count = 0;
  count++;

  // See if someone should take action.  The person furthest ahead
  // needs to slow down so the other user can catch up.
  // Only do this if both clients agree on who's ahead!!
  if (advantage >= radvantage) {
    return 0;
  }

  // Both clients agree that we're the one ahead.  Split
  // the difference between the two to figure out how long to
  // sleep for.
  int sleep_frames = (int) (((radvantage - advantage) / 2) + 0.5);

  // Some things just aren't worth correcting for.  Make sure
  // the difference is relevant before proceeding.
  if (sleep_frames < MIN_FRAME_ADVANTAGE) {
    return 0;
  }

  // Make sure our input had been "idle enough" before recommending
  // a sleep.  This tries to make the emulator sleep while the
  // user's input isn't sweeping in arcs (e.g. fireball motions in
  // Street Fighter), which could cause the player to miss moves.
  if (require_idle_input) {
    for (size_t i = 1; i < last_inputs_.size(); i++) {
      if (!last_inputs_[i].equal(last_inputs_[0], true)) {
        ggpoLog("iteration %d:  rejecting due to input stuff at position %d...!!!\n", count, i);
        return 0;
      }
    }
  }

  // Success!!! Recommend the number of frames to sleep and adjust
  return std::min(sleep_frames, MAX_FRAME_ADVANTAGE);
}
