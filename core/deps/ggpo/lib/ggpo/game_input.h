/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _GAMEINPUT_H
#define _GAMEINPUT_H

#include <stdio.h>
#include <memory.h>

// GAMEINPUT_MAX_BYTES * GAMEINPUT_MAX_PLAYERS * 8 must be less than
// 2^BITVECTOR_NIBBLE_SIZE (see bitvector.h)

#define GAMEINPUT_MAX_BYTES      9
#define GAMEINPUT_MAX_PLAYERS    2

struct GameInput {
  enum Constants {
    NullFrame = -1
  };
  int frame;
  int size; /* size in bytes of the entire input for all players */
  char bits[GAMEINPUT_MAX_BYTES * GAMEINPUT_MAX_PLAYERS];

  auto isNull() const -> bool { return frame == NullFrame; }
  auto init(int frame, char *bits, int size, int offset) -> void;
  auto init(int frame, char *bits, int size) -> void;
  auto value(int i) const -> bool { return (bits[i / 8] & (1 << (i % 8))) != 0; }
  auto set(int i) -> void { bits[i / 8] |= (1 << (i % 8)); }
  auto clear(int i) -> void { bits[i / 8] &= ~(1 << (i % 8)); }
  auto erase() -> void { memset(bits, 0, sizeof(bits)); }
  auto desc(char *buf, size_t buf_size, bool show_frame = true) const -> void;
  auto log(char *prefix, bool show_frame = true) const -> void;
  auto equal(GameInput &input, bool bitsonly = false) -> bool;
};

#endif
