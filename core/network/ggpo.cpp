/*
	Copyright 2021 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "ggpo.h"
#include "hw/maple/maple_cfg.h"
#include "hw/maple/maple_devs.h"
#include "input/gamepad_device.h"
#include "input/keyboard_device.h"
#include "input/mouse.h"
#include "cfg/option.h"
#include <algorithm>
#include "netvs_common.h"
#include "emulator.hpp"

extern void okaiGameEndCallback();

void UpdateInputState();

namespace ggpo {

bool inRollback;

static void getLocalInput(MapleInputState inputState[4]) {
  UpdateInputState();
  std::lock_guard<std::mutex> lock(relPosMutex);
  for (int player = 0; player < 4; player++) {
    MapleInputState &state = inputState[player];
    state.kcode = kcode[player];
    state.halfAxes[PJTI_L] = lt[player];
    state.halfAxes[PJTI_R] = rt[player];
    state.fullAxes[PJAI_X1] = joyx[player];
    state.fullAxes[PJAI_Y1] = joyy[player];
    state.fullAxes[PJAI_X2] = joyrx[player];
    state.fullAxes[PJAI_Y2] = joyry[player];
    state.mouseButtons = mo_buttons[player];
    state.absPos.x = mo_x_abs[player];
    state.absPos.y = mo_y_abs[player];
    state.keyboard.shift = kb_shift[player];
    memcpy(state.keyboard.key, kb_key[player], sizeof(kb_key[player]));
    int relX = std::round(mo_x_delta[player]);
    int relY = std::round(mo_y_delta[player]);
    int wheel = std::round(mo_wheel_delta[player]);
    state.relPos.x += relX;
    state.relPos.y += relY;
    state.relPos.wheel += wheel;
    mo_x_delta[player] -= relX;
    mo_y_delta[player] -= relY;
    mo_wheel_delta[player] -= wheel;
  }
}

}

#ifdef USE_GGPO
#include "ggponet.h"
#include "emulator.h"
#include "hw/mem/mem_watch.h"
#include <string.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <numeric>
#include "miniupnp.h"
#include "hw/naomi/naomi_cart.h"
#include "emulator_api.h"

namespace ggpo {
using namespace std::chrono;

constexpr int MAX_PLAYERS = 2;
constexpr int SERVER_PORT = 19713;

constexpr u32 BTN_TRIGGER_LEFT = DC_BTN_RELOAD << 1;
constexpr u32 BTN_TRIGGER_RIGHT = DC_BTN_RELOAD << 2;

#pragma pack(push, 1)
struct VerificationData {
  const int protocol = 2;
  u8 gameMD5[16]{};
  u8 stateMD5[16]{};
};
#pragma pack(pop)

static GGPOSession *ggpoSession;
static int localPlayerNum;
static GGPOPlayerHandle localPlayer;
static GGPOPlayerHandle remotePlayer;
static GGPONetworkStats remoteStats;
static time_point<steady_clock> lastNetworkStatusTime;

static bool synchronized;
static std::recursive_mutex ggpoMutex;
static std::array<int, 5> msPerFrame;
static int msPerFrameIndex;
static time_point<steady_clock> lastFrameTime;
static int msPerFrameAvg;
static bool _endOfFrame;
static MiniUPnP miniupnp;
static int analogAxes;
static bool absPointerPos;
static bool keyboardGame;
static bool mouseGame;
static int inputSize;
static void (*chatCallback)(int playerNum, const std::string &msg);
static bool loadingState{false};

static bool canChangeBattleStart;
static bool battleStart;
static bool pauseInput;

static void scoreUpdate() {
  if (ReadMem32(0x0c2d76a4) == 0x00000101 && ReadMem32(0x0c2d7c48) == 0x00000101) {
    int64_t p1_life = ReadMem32(0x0c2d7608) + ReadMem32(0x0c2d8150) + ReadMem32(0x0c2d8c98);
    int64_t p2_life = ReadMem32(0x0c2d7bac) + ReadMem32(0x0c2d86f4) + ReadMem32(0x0c2d923c);
    int64_t remain_time = ReadMem32(0x0c2f84d8);

    if (battleStart) {
      if ((remain_time == 0x0000 || p1_life == 0x0000 || p2_life == 0x0000)) {
        if (p1_life == p2_life) {
          // ZGS_LOG("DRAW GAME");
          UpdateOption(CMD_MATCHRKEY, DRAW, mInternetIP, pInternetIP, 0, 0);
        } else if (p1_life > p2_life) {
          // ZGS_LOG("P1 WIN");
          UpdateOption(CMD_MATCHRKEY, P1WIN, mInternetIP, pInternetIP, 0, 0);
        } else {
          // ZGS_LOG("P2 WIN");
          UpdateOption(CMD_MATCHRKEY, P2WIN, mInternetIP, pInternetIP, 0, 0);
        }

        battleStart = false;
      }
    } else {
      if (!canChangeBattleStart) {
        if (remain_time == 0x0000 && p1_life == 0x0000 && p2_life == 0x0000) {
          canChangeBattleStart = true;
        }
      } else {
        if (remain_time == 0x7863 && p1_life == 0x01b0 && p2_life == 0x01b0) {
          battleStart = true;
          canChangeBattleStart = false;
          UpdateOption(CMD_ONLINE_START_GAME, 0, "", "", 0, 0);
        }
      }
    }
  }
}

struct MemPages {
  void load() {
    memwatch::ramWatcher.getPages(ram);
    memwatch::vramWatcher.getPages(vram);
    memwatch::aramWatcher.getPages(aram);
    memwatch::elanWatcher.getPages(elanram);
  }
  memwatch::PageMap ram;
  memwatch::PageMap vram;
  memwatch::PageMap aram;
  memwatch::PageMap elanram;
};
static std::unordered_map<int, MemPages> deltaStates;
static int lastSavedFrame = -1;

static int timesyncOccurred;

#pragma pack(push, 1)
struct Inputs {
  u32 kcode: 20;
  u32 mouseButtons: 4;
  u32 kbModifiers: 8;

  union {
    struct {
      u8 x;
      u8 y;
    } analog;
    struct {
      s16 x;
      s16 y;
    } absPos;
    struct {
      s16 x;
      s16 y;
      s16 wheel;
    } relPos;
    u8 keys[6];
  } u;
};
static_assert(sizeof(Inputs) == 10, "wrong Inputs size");

struct GameEvent {
  enum : char {
    Chat
  } type;
  union {
    struct {
      u8 playerNum;
      char message[512 - sizeof(playerNum) - sizeof(type)];
    } chat;
  } u;

  constexpr static int chatMessageLen(int len) { return len - sizeof(u.chat.playerNum) - sizeof(type); }
};
#pragma pack(pop)

/*
 * begin_game callback - This callback has been deprecated.  You must
 * implement it, but should ignore the 'game' parameter.
 */
static bool begin_game(const char *) {
  DEBUG_LOG(NETWORK, "Game begin");
  rend_allow_rollback();
  return true;
}

/*
 * on_event - Notification that something has happened.  See the GGPOEventCode
 * structure for more information.
 */
static bool on_event(GGPOEvent *info) {
  switch (info->code) {
    case GGPO_EVENTCODE_CONNECTED_TO_PEER:
      ZGS_LOG("NETWORK: Connected to peer %d", info->u.connected.player);
      UpdateOption(CMD_SYNCSTATEKEY, 0, "", "", 0, 0);
      break;
    case GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER:
      ZGS_LOG("NETWORK: Synchronizing with peer %d", info->u.synchronizing.player);
      UpdateOption(CMD_SYNCSTATEKEY, 1, "", "", 0, 0);
      break;
    case GGPO_EVENTCODE_SYNCHRONIZED_WITH_PEER:
      ZGS_LOG("NETWORK: Synchronized with peer %d", info->u.synchronized.player);
      UpdateOption(CMD_SYNCSTATEKEY, 2, "", "", P2PCMode, 0);
      break;
    case GGPO_EVENTCODE_RUNNING: ZGS_LOG("NETWORK: Running");
      synchronized = true;
      sen::emulator.loadGameState(-1);
      break;
    case GGPO_EVENTCODE_DISCONNECTED_FROM_PEER:
      ZGS_LOG("NETWORK: Disconnected from peer %d", info->u.disconnected.player);
      UpdateOption(CMD_NETWORK_STATE, CONNETCTION_DISCONNECTED, "", "", 0, 0);
      // stopSession();
      okaiGameEndCallback();
      break;
    case GGPO_EVENTCODE_TIMESYNC: ZGS_LOG("NETWORK: Timesync: %d frames ahead", info->u.timesync.frames_ahead);
      timesyncOccurred += 5;

      // todo: wait or remove?
      std::this_thread::sleep_for(std::chrono::milliseconds(17));
      // std::this_thread::sleep_for(std::chrono::milliseconds(1000 * info->u.timesync.frames_ahead / (msPerFrameAvg >= 25 ? 30 : 60)));
      break;
    case GGPO_EVENTCODE_CONNECTION_INTERRUPTED: ZGS_LOG("NETWORK: Connection interrupted with player %d", info->u.connection_interrupted.player);
      UpdateOption(CMD_NETWORK_STATE, CONNETCTION_INTTERUPT, "", "", 0, 0);
      break;
    case GGPO_EVENTCODE_CONNECTION_RESUMED: ZGS_LOG("NETWORK: Connection resumed with player %d", info->u.connection_resumed.player);
      UpdateOption(CMD_NETWORK_STATE, CONNETCTION_RESUMED, "", "", 0, 0);
      break;
  }
  return true;
}

/*
 * advance_frame - Called during a rollback.  You should advance your game
 * state by exactly one frame.  Before each frame, call ggpo_synchronize_input
 * to retrieve the inputs you should use for that frame.  After each frame,
 * you should call ggpo_advance_frame to notify GGPO.net that you're
 * finished.
 *
 * The flags parameter is reserved.  It can safely be ignored at this time.
 */
static bool advance_frame(int) {
  INFO_LOG(NETWORK, "advanceFrame");
  settings.aica.muteAudio = true;
  settings.disableRenderer = true;
  inRollback = true;

  emu.run();
  ggpo_advance_frame(ggpoSession);

  settings.aica.muteAudio = false;
  settings.disableRenderer = false;
  inRollback = false;
  _endOfFrame = false;

  return true;
}

/*
 * load_game_state - GGPO.net will call this function at the beginning
 * of a rollback.  The buffer and len parameters contain a previously
 * saved state returned from the save_game_state function.  The client
 * should make the current game state match the state contained in the
 * buffer.
 */
static bool load_game_state(unsigned char *buffer, int len) {
  INFO_LOG(NETWORK, "load_game_state");

  rend_start_rollback();
  loadingState = true;
  // FIXME dynarecs
  Deserializer deser(buffer, len, true);
  int frame;
  deser >> frame;
  memwatch::unprotect();

  for (int f = lastSavedFrame - 1; f >= frame; f--) {
    const MemPages &pages = deltaStates[f];
    for (const auto &pair : pages.ram)
      memcpy(memwatch::ramWatcher.getMemPage(pair.first), &pair.second.data[0], PAGE_SIZE);
    for (const auto &pair : pages.vram)
      memcpy(memwatch::vramWatcher.getMemPage(pair.first), &pair.second.data[0], PAGE_SIZE);
    for (const auto &pair : pages.aram)
      memcpy(memwatch::aramWatcher.getMemPage(pair.first), &pair.second.data[0], PAGE_SIZE);
    for (const auto &pair : pages.elanram)
      memcpy(memwatch::elanWatcher.getMemPage(pair.first), &pair.second.data[0], PAGE_SIZE);
    DEBUG_LOG(NETWORK, "Restored frame %d pages: %d ram, %d vram, %d eram, %d aica ram", f, (u32) pages.ram.size(),
              (u32) pages.vram.size(), (u32) pages.elanram.size(), (u32) pages.aram.size());
  }
  dc_deserialize(deser);
  if (deser.size() != (u32) len) {
    ERROR_LOG(NETWORK, "load_game_state len %d used %d", len, (int) deser.size());
    die("fatal");
  }
  rend_allow_rollback();    // ggpo might load another state right after this one
  memwatch::reset();
  memwatch::protect();
  loadingState = false;
  return true;
}

/*
 * save_game_state - The client should allocate a buffer, copy the
 * entire contents of the current game state into it, and copy the
 * length into the *len parameter.  Optionally, the client can compute
 * a checksum of the data and store it in the *checksum argument.
 */
static bool save_game_state(unsigned char **buffer, int *len, int *checksum, int frame) {
  verify(!sh4_cpu.IsCpuRunning());
  lastSavedFrame = frame;
  size_t allocSize = (settings.platform.isNaomi() ? 20 : 10) * 1024 * 1024;
  *buffer = (unsigned char *) malloc(allocSize);
  if (*buffer == nullptr) {
    WARN_LOG(NETWORK, "Memory alloc failed");
    *len = 0;
    return false;
  }
  Serializer ser(*buffer, allocSize, true);
  ser << frame;
  dc_serialize(ser);
  verify(ser.size() < allocSize);
  *len = ser.size();
  memwatch::protect();
  if (frame > 0) {
    // Save the delta to frame-1
    deltaStates[frame - 1].load();
    DEBUG_LOG(NETWORK, "Saved frame %d pages: %d ram, %d vram, %d eram, %d aica ram", frame - 1, (u32) deltaStates[frame - 1].ram.size(),
              (u32) deltaStates[frame - 1].vram.size(), (u32) deltaStates[frame - 1].elanram.size(), (u32) deltaStates[frame - 1].aram.size());
  }

  return true;
}

/*
 * log_game_state - Used in diagnostic testing.  The client should use
 * the ggpo_log function to write the contents of the specified save
 * state in a human readible form.
 */
static bool log_game_state(char *filename, unsigned char *buffer, int len) {
  return true;
}

/*
 * free_buffer - Frees a game state allocated in save_game_state.  You
 * should deallocate the memory contained in the buffer.
 */
static void free_buffer(void *buffer) {
  if (buffer != nullptr) {
    Deserializer deser(buffer, 1024 * 1024, true);
    int frame;
    deser >> frame;
    deltaStates.erase(frame);
    free(buffer);
  }
}

static void on_message(u8 *msg, int len) {
  if (len == 0)
    return;
  GameEvent *event = (GameEvent *) msg;
  switch (event->type) {
    case GameEvent::Chat:
      if (chatCallback != nullptr && GameEvent::chatMessageLen(len) > 0)
        chatCallback(event->u.chat.playerNum, std::string(event->u.chat.message, GameEvent::chatMessageLen(len)));
      break;

    default: WARN_LOG(NETWORK, "Unknown app message type %d", event->type);
      break;
  }
}

void startSession(int localPort, int localPlayerNum) {
  GGPOSessionCallbacks cb{};
  cb.begin_game = begin_game;
  cb.advance_frame = advance_frame;
  cb.load_game_state = load_game_state;
  cb.save_game_state = save_game_state;
  cb.free_buffer = free_buffer;
  cb.on_event = on_event;
  cb.log_game_state = log_game_state;
  cb.on_message = on_message;

  if (settings.platform.isConsole())
    analogAxes = config::GGPOAnalogAxes;
  else {
    analogAxes = 0;
    absPointerPos = false;
    keyboardGame = false;
    mouseGame = false;
    if (settings.input.JammaSetup == JVS::LightGun || settings.input.JammaSetup == JVS::LightGunAsAnalog)
      absPointerPos = true;
    else if (settings.input.JammaSetup == JVS::Keyboard)
      keyboardGame = true;
    else if (settings.input.JammaSetup == JVS::RotaryEncoders)
      mouseGame = true;
    else if (NaomiGameInputs != nullptr) {
      for (const auto &axis : NaomiGameInputs->axes) {
        if (axis.name == nullptr)
          break;
        if (axis.type == Full)
          analogAxes = std::max(analogAxes, (int) axis.axis + 1);
      }
    }
    NOTICE_LOG(NETWORK, "GGPO: Using %d full analog axes", analogAxes);
  }
  inputSize = sizeof(kcode[0]) + analogAxes + (int) absPointerPos * sizeof(Inputs::u.absPos)
      + (int) keyboardGame * sizeof(Inputs::u.keys) + (int) mouseGame * sizeof(Inputs::u.relPos);

  VerificationData verif;
  MD5Sum().add(settings.network.md5.bios)
          .add(settings.network.md5.game)
          .getDigest(verif.gameMD5);
  auto &digest = settings.network.md5.savestate;
  if (std::find_if(std::begin(digest), std::end(digest), [](u8 b) { return b != 0; }) != std::end(digest))
    memcpy(verif.stateMD5, digest, sizeof(digest));
  else {
    MD5Sum().add(settings.network.md5.nvmem)
            .add(settings.network.md5.nvmem2)
            .add(settings.network.md5.eeprom)
            .add(settings.network.md5.vmu)
            .getDigest(verif.stateMD5);
  }

  GGPOErrorCode result = ggpo_start_session(&ggpoSession, &cb, settings.content.gameId.c_str(), MAX_PLAYERS, inputSize, localPort,
                                            &verif, sizeof(verif));
  if (result != GGPO_OK) {
    WARN_LOG(NETWORK, "GGPO start session failed: %d", result);
    ggpoSession = nullptr;
    throw FlycastException("GGPO network initialization failed");
  }

  // automatically disconnect clients after 3000 ms and start our count-down timer
  // for disconnects after 1000 ms.   To completely disable disconnects, simply use
  // a value of 0 for ggpo_set_disconnect_timeout.
  ggpo_set_disconnect_timeout(ggpoSession, 3000);
  ggpo_set_disconnect_notify_start(ggpoSession, 1000);

  ggpo::localPlayerNum = localPlayerNum;
  GGPOPlayer player{sizeof(GGPOPlayer), GGPO_PLAYERTYPE_LOCAL, localPlayerNum + 1};
  result = ggpo_add_player(ggpoSession, &player, &localPlayer);
  if (result != GGPO_OK) {
    WARN_LOG(NETWORK, "GGPO cannot add local player: %d", result);
    stopSession();
    throw FlycastException("GGPO cannot add local player");
  }
  // ggpo_set_frame_delay(ggpoSession, localPlayer, config::GGPODelay);
  ggpo_set_frame_delay(ggpoSession, localPlayer, config::GGPODelay);

  player.type = GGPO_PLAYERTYPE_REMOTE;
  strcpy(player.u.remote.ip_address, pInternetIP);
  player.u.remote.port = pP2PUDPPort;
  player.player_num = (1 - localPlayerNum) + 1;
  result = ggpo_add_player(ggpoSession, &player, &remotePlayer);
  if (result != GGPO_OK) {
    WARN_LOG(NETWORK, "GGPO cannot add remote player: %d", result);
    stopSession();
    throw FlycastException("GGPO cannot add remote player");
  }
  DEBUG_LOG(NETWORK, "GGPO session started");

  // ggpo_set_disconnect_timeout(ggpoSession, 30 * 1000);
  battleStart = false;
  canChangeBattleStart = false;
  pauseInput = false;
#endif
}

void stopSession() {
  std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
  if (ggpoSession == nullptr)
    return;
  ggpo_close_session(ggpoSession);
  ggpoSession = nullptr;
  miniupnp.Term();
  emu.setNetworkState(false);
  config::SkipFrame = 0;
  config::AutoSkipFrame = 1;
}

void getInput(MapleInputState inputState[4]) {
  std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
  if (ggpoSession == nullptr) {
    getLocalInput(inputState);
    return;
  }
  for (int player = 0; player < 4; player++)
    inputState[player] = {};

  std::vector<u8> inputData(inputSize * MAX_PLAYERS);
  // should not call any callback
  GGPOErrorCode error = ggpo_synchronize_input(ggpoSession, (void *) &inputData[0], inputData.size(), nullptr);
  if (error != GGPO_OK) {
    stopSession();
    throw FlycastException("GGPO error");
  }

  for (int player = 0; player < MAX_PLAYERS; player++) {
    MapleInputState &state = inputState[player];
    const Inputs *inputs = (Inputs *) &inputData[player * inputSize];

    state.kcode = ~inputs->kcode;

    if (analogAxes > 0) {
      state.fullAxes[PJAI_X1] = inputs->u.analog.x;
      if (analogAxes >= 2)
        state.fullAxes[PJAI_Y1] = inputs->u.analog.y;
    } else if (absPointerPos) {
      state.absPos.x = inputs->u.absPos.x;
      state.absPos.y = inputs->u.absPos.y;
    } else if (keyboardGame) {
      memcpy(state.keyboard.key, inputs->u.keys, sizeof(state.keyboard.key));
      state.keyboard.shift = inputs->kbModifiers;
    } else if (mouseGame) {
      state.relPos.x = inputs->u.relPos.x;
      state.relPos.y = inputs->u.relPos.y;
      state.relPos.wheel = inputs->u.relPos.wheel;
      state.mouseButtons = ~inputs->mouseButtons;
    }
    state.halfAxes[PJTI_R] = (state.kcode & BTN_TRIGGER_RIGHT) == 0 ? 255 : 0;
    state.halfAxes[PJTI_L] = (state.kcode & BTN_TRIGGER_LEFT) == 0 ? 255 : 0;
  }
}

bool nextFrame() {
  if (!_endOfFrame)
    return false;
  _endOfFrame = false;
  if (inRollback)
    return true;
  auto now = std::chrono::steady_clock::now();
  if (lastFrameTime != time_point<steady_clock>()) {
    msPerFrame[msPerFrameIndex++] = duration_cast<milliseconds>(now - lastFrameTime).count();
    if (msPerFrameIndex >= (int) msPerFrame.size())
      msPerFrameIndex = 0;
    msPerFrameAvg = std::accumulate(msPerFrame.begin(), msPerFrame.end(), 0) / msPerFrame.size();
  }
  lastFrameTime = now;

  std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
  if (ggpoSession == nullptr)
    return false;
  // will call save_game_state
  GGPOErrorCode error = ggpo_advance_frame(ggpoSession);

  // may rollback
  if (error == GGPO_OK)
    error = ggpo_idle(ggpoSession, 0);
  if (error != GGPO_OK) {
    stopSession();
    if (error == GGPO_ERRORCODE_INPUT_SIZE_DIFF)
      throw FlycastException("GGPO analog settings are different from peer");
    else if (error != GGPO_OK)
      throw FlycastException("GGPO error");
  }

  // may call save_game_state
  do {
    UpdateInputState();
    Inputs inputs = {};
    if (!pauseInput) {
      inputs.kcode = ~kcode[0];

      if (rt[0] >= 64)
        inputs.kcode |= BTN_TRIGGER_RIGHT;
      else
        inputs.kcode &= ~BTN_TRIGGER_RIGHT;
      if (lt[0] >= 64)
        inputs.kcode |= BTN_TRIGGER_LEFT;
      else
        inputs.kcode &= ~BTN_TRIGGER_LEFT;
      if (analogAxes > 0) {
        inputs.u.analog.x = joyx[0];
        if (analogAxes >= 2)
          inputs.u.analog.y = joyy[0];
      } else if (absPointerPos) {
        inputs.u.absPos.x = mo_x_abs[0];
        inputs.u.absPos.y = mo_y_abs[0];
      } else if (keyboardGame) {
        inputs.kbModifiers = kb_shift[0];
        memcpy(inputs.u.keys, kb_key[0], sizeof(kb_key[0]));
      } else if (mouseGame) {
        std::lock_guard<std::mutex> lock(relPosMutex);
        inputs.mouseButtons = ~mo_buttons[0];
        inputs.u.relPos.x = std::round(mo_x_delta[0]);
        inputs.u.relPos.y = std::round(mo_y_delta[0]);
        inputs.u.relPos.wheel = std::round(mo_wheel_delta[0]);
        mo_x_delta[0] -= inputs.u.relPos.x;
        mo_y_delta[0] -= inputs.u.relPos.y;
        mo_wheel_delta[0] -= inputs.u.relPos.wheel;
      }
    }

    GGPOErrorCode result = ggpo_add_local_input(ggpoSession, localPlayer, &inputs, inputSize);
    if (result == GGPO_OK)
      break;
    if (result != GGPO_ERRORCODE_PREDICTION_THRESHOLD) {
      WARN_LOG(NETWORK, "ggpo_add_local_input failed %d", result);
      stopSession();
      throw FlycastException("GGPO error");
    }
    DEBUG_LOG(NETWORK, "ggpo_add_local_input prediction barrier reached");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    ggpo_idle(ggpoSession, 0);
  } while (active());
  return active();
}

bool active() {
  return ggpoSession != nullptr;
}

std::future<bool> startNetwork() {
  synchronized = false;
  return std::async(std::launch::async, [] {
    {
      std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
      if (config::EnableUPnP) {
        miniupnp.Init();
        miniupnp.AddPortMapping(SERVER_PORT, false);
      }

      try {
        startSession(SERVER_PORT, NETVS_P1 == kHostFlag ? 0 : 1);
      } catch (...) {
        if (config::EnableUPnP) {
          miniupnp.Term();
        }
        throw;
      }
    }
    while (!synchronized && active()) {
      {
        std::lock_guard<std::recursive_mutex> lock(ggpoMutex);
        if (ggpoSession == nullptr)
          break;
        GGPOErrorCode result = ggpo_idle(ggpoSession, 0);
        if (result == GGPO_ERRORCODE_VERIFICATION_ERROR)
          throw FlycastException("Peer verification failed");
        else if (result != GGPO_OK) {
          WARN_LOG(NETWORK, "ggpo_idle failed %d", result);
          throw FlycastException("GGPO error");
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    emu.setNetworkState(active());
    return active();
  });
}

void endOfFrame() {
  if (active()) {
    _endOfFrame = true;

    time_point<steady_clock> now = steady_clock::now();
    if (now - lastNetworkStatusTime > 2s) {
      ggpo_get_network_stats(ggpoSession, remotePlayer, &remoteStats);

      UpdateOption(CMD_PINGKEY, 0, "", "", remoteStats.network.local_ping, remoteStats.network.remote_ping);
      lastNetworkStatusTime = now;
    }

    scoreUpdate();
  }
}

bool ggpoLoadingState() {
  if (ggpo::active()) {
    return loadingState;
  } else {
    return false;
  }
}

/**
 * 设置模拟器属性的函数
 * @param key
 * @param value
 */
void setOption(const char *key, const char *value) {
#if defined(USE_GGPO_ROLLBACK)
  if (strcmp(key, "PlayerQuit") == 0) {
    if (strcmp(value, "true") == 0) {
      okaiGameEndCallback();
    } else {
      // nothing
    }
  } else
#endif
  if (strcmp(key, "EmuSkipFrame") == 0) {
    config::SkipFrame = std::stoi(value);
  } else if (strcmp(key, "PauseInput") == 0) {
    if (strcmp(value, "true") == 0) {
      pauseInput = true;
      sen::emulator.setDisableRender(true);
    } else if (strcmp(value, "false") == 0) {
      pauseInput = false;
      sen::emulator.setDisableRender(false);
    }
  } else {

    Emulator_SetOption(key, value);
  }
}

}

