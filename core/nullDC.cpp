#ifndef LIBRETRO
#include "types.h"
#include "emulator.h"
#include "emulator_api.h"
#include "hw/mem/_vmem.h"
#include "cfg/cfg.h"
#include "cfg/option.h"
#include "log/LogManager.h"
#include "oslib/oslib.h"
#include "debug/gdb_server.h"
#include "archive/rzip.h"
#include "stdclass.h"
#include "serialize.h"
#include "sched.h"
#include "rend/imgui_driver.h"

namespace flycast {
using namespace sen;

std::thread emulator_run_;
bool initialize(int argc, char *argv[]) {
  if (!_vmem_reserve()) {
    ERROR_LOG(VMEM, "Failed to alloc mem");
    return false;
  }

  configs.initialize();
  configs.parseArgs(argc, argv);
  set_user_config_dir(configs.parse_result_["config-dir"].as<std::string>());
  set_user_data_dir(configs.parse_result_["data-dir"].as<std::string>());
  settings.content.path = configs.parse_result_["game-dir"].as<std::string>();
  config::UseLowPass = configs.parse_result_["use-low-pass"].as<bool>();
  config::AudioVolume.set(configs.parse_result_["volume"].as<int>());
  config::AudioBackend = configs.parse_result_["backend"].as<std::string>();

  config::Settings::instance().reset();
  LogManager::Shutdown();
  os_SetupInput();

  debugger::init();

#if !defined(ANDROID)
  Emulator_NotificationCallback(
      [](int name, int value, const char *arg1, const char *arg2, int arg3, int arg4, const char *arg5, const char *arg6, int arg7) -> void {
        if (name != 24) {
          ZGS_LOG("name %d, value %d, %s %s %d %d", name, value, arg1, arg2, arg3, arg4);
        }
      });
#endif

  return true;
}
void terminate() {
  emu.term();
  os_TermInput();
}

void start(int argc, char *argv[]) {
  if (emulator_run_.joinable())
    return;

  emulator_run_ = std::thread([](int argc, char **argv) -> void {
    if (!initialize(argc, argv))
      return;

    int player_num = configs.parse_result_["player"].as<int>();
    ZGS_LOG("player_num: %d", player_num);
#if 0
    if (player_num == 0) {
      Emulator_Start("mvsc2u.zip", 0, 1, 1, 1, 0, 0, "", 0, "", 0, 0, "", "", 0, 0);
    } else if (player_num == 1) {
      Emulator_Start("mvsc2u.zip", 1, 10, 0, 5000010, 0, 0, "", 0, "", 6, 0, "8.136.234.80", "8.136.234.80", 0, 0);
    } else if (player_num == 2) {
      Emulator_Start("mvsc2u.zip", 2, 10, 0, 5000011, 0, 0, "", 0, "", 6, 0, "8.136.234.80", "8.136.234.80", 0, 0);
    }
#else
    Emulator_Start(
        "mvsc2u.zip",          // game name
        player_num,            // player_num
        configs.parse_result_["game-id"].as<int>(),                   // game id
        configs.parse_result_["room-id"].as<int>(),                   // room id
        configs.parse_result_["user-id"].as<int>(),                   // user id
        0,                     // sp mode ?
        0,                     // record rep mode ?
        "",                    // aRecRepFileOrBuffer
        configs.parse_result_["watch-args"].as<int>(),                // aWatchArgs
        "",                    // aResetState
        configs.parse_result_["room-index"].as<int>(),                // aRoomServerID
        0,                     // aUserCMode
        configs.parse_result_["room-ip"].as<std::string>().c_str(),   // aRoomServerIP
        configs.parse_result_["watch-server-ip"].as<std::string>().c_str(),// aWatchServerIP
        0,                     // ggdiff
        0                      // aSavMode
    );
#endif

    terminate();
  }, argc, argv);
}

void quit() {
  Emulator_Quit();
  if (emulator_run_.joinable() && emulator_run_.get_id() != std::this_thread::get_id())
    emulator_run_.join();
}
}

int flycast_init(int argc, char *argv[]) {
  if (!_vmem_reserve()) {
    ERROR_LOG(VMEM, "Failed to alloc mem");
    return -1;
  }

  sen::configs.initialize();
  sen::configs.parseArgs(argc, argv);

  set_user_config_dir(sen::configs.parse_result_["config-dir"].as<std::string>());
  set_user_data_dir(sen::configs.parse_result_["data-dir"].as<std::string>());
  settings.content.path = sen::configs.parse_result_["game-dir"].as<std::string>();
  // TODO: add use low pass to the global config
  // config::UseLowPass = sen::configs.parse_result_["use-low-pass"].as<bool>();
  config::AudioVolume.set(sen::configs.parse_result_["volume"].as<int>());
  config::AudioBackend = sen::configs.parse_result_["backend"].as<std::string>();

  config::Settings::instance().reset();
  LogManager::Shutdown();
  if (!cfgOpen()) {
    LogManager::Init();
    NOTICE_LOG(BOOT, "Config directory is not set. Starting onboarding");
  } else {
    LogManager::Init();
    config::Settings::instance().load(false);
  }
  os_CreateWindow();
  os_SetupInput();

  debugger::init();

  return 0;
}

void dc_exit() {
  emu.stop();
}

void flycast_term() {
  emu.term();
  os_TermInput();
}

void dc_savestate(int index) {
  Serializer ser;
  dc_serialize(ser);

  void *data = malloc(ser.size());
  if (data == nullptr) {
    ZGS_LOG("SAVESTATE: Failed to save state - could not malloc %ld bytes", ser.size());
    return;
  }

  ser = Serializer(data, ser.size());
  dc_serialize(ser);

  std::string filename = hostfs::getSavestatePath(index, true);
#if 0
  FILE *f = nowide::fopen(filename.c_str(), "wb") ;

  if ( f == NULL )
  {
      WARN_LOG(SAVESTATE, "Failed to save state - could not open %s for writing", filename.c_str()) ;
      free(data);
      return;
  }

  std::fwrite(data, 1, ser.size(), f) ;
  std::fclose(f);
#else
  RZipFile zipFile;
  if (!zipFile.Open(filename, true)) {
    ZGS_LOG("SAVESTATE: Failed to save state - could not open %s for writing", filename.c_str());
    free(data);
    return;
  }
  if (zipFile.Write(data, ser.size()) != ser.size()) {
    ZGS_LOG("SAVESTATE: Failed to save state - error writing %s", filename.c_str());
    zipFile.Close();
    free(data);
    return;
  }
  zipFile.Close();
#endif

  free(data);
  ZGS_LOG("Saved state to %s size %ld", filename.c_str(), ser.size());
}

auto dc_loadstate(int index) -> bool {
  u32 total_size = 0;
  FILE *f = nullptr;

  // emu.stop();
  std::string filename = hostfs::getSavestatePath(index, false);
  RZipFile zipFile;
  if (zipFile.Open(filename, false)) {
    total_size = (u32)zipFile.Size();
    if (index == -1 && config::GGPOEnable) {
      f = zipFile.rawFile();
      long pos = std::ftell(f);
      MD5Sum().add(f)
          .getDigest(settings.network.md5.savestate);
      std::fseek(f, pos, SEEK_SET);
      f = nullptr;
    }
  } else {
    f = nowide::fopen(filename.c_str(), "rb");

    if (f == nullptr) {
      ZGS_LOG("SAVESTATE: Failed to load state - could not open %s for reading", filename.c_str());
      return false;
    }
    if (index == -1 && config::GGPOEnable)
      MD5Sum().add(f)
          .getDigest(settings.network.md5.savestate);
    std::fseek(f, 0, SEEK_END);
    total_size = (u32)std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
  }
  void *data = malloc(total_size);
  if (data == nullptr) {
    ZGS_LOG("SAVESTATE: Failed to load state - could not malloc %d bytes", total_size);
    if (f != nullptr)
      std::fclose(f);
    else
      zipFile.Close();
    return false;
  }

  size_t read_size;
  if (f == nullptr) {
    read_size = zipFile.Read(data, total_size);
    zipFile.Close();
  } else {
    read_size = fread(data, 1, total_size, f);
    std::fclose(f);
  }
  if (read_size != total_size) {
    ZGS_LOG("SAVESTATE: Failed to load state - I/O error");
    free(data);
    return false;
  }

  try {
    Deserializer deser(data, total_size);
    dc_loadstate(deser);
    if (deser.size() != total_size)
      ZGS_LOG("SAVESTATE: Savestate size %d but only %ld bytes used", total_size, deser.size());
  } catch (const Deserializer::Exception &e) {
    ZGS_LOG("SAVESTATE: %s", e.what());
    return false;
  }

  free(data);
  EventManager::event(Event::LoadState);
  ZGS_LOG("SAVESTATE: Loaded state from %s size %d", filename.c_str(), total_size);
  return true;
}

#endif

std::unique_ptr<ImGuiDriver> imguiDriver;

