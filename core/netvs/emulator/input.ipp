#pragma once
#ifndef EMULATOR_INPUT_IPP_
#define EMULATOR_INPUT_IPP_

namespace sen {

class InputDevice {
 public:
  enum class KeyMapping {
    UP    = 0x0001,
    DOWN  = 0x0002,
    LEFT  = 0x0004,
    RIGHT = 0x0008,
    START = 0x0010,
    BTN_1 = 0x0020,
    BTN_2 = 0x0040,
    BTN_3 = 0x0080,
    BTN_4 = 0x0100,
    BTN_5 = 0x0200,
    BTN_6 = 0x0400,
    UNKNOWN = 0x8000 // 未知
  };

  virtual ~InputDevice() = default;
  virtual void initialize() = 0;
  virtual void terminate() = 0;
  virtual void readInput() = 0;
  virtual uint16_t getP1Keys() { return 0; }
  virtual uint16_t getP2Keys() { return 0; }
  virtual uint16_t getP3Keys() { return 0; }
  virtual uint16_t getP4Keys() { return 0; }

  virtual KeyMapping getKeyMapping(uint16_t key) = 0;
};
// Umido key input
class UmidoKey : public InputDevice {
 public:
  enum class KeyMapping {
    UK_RIGHT = 0x0001,
    UK_LEFT = 0x0002,
    UK_UP = 0x0004,
    UK_DOWN = 0x0008,
    UK_START = 0x0020,
    UK_X = 0x0040,
    UK_Y = 0x0080,
    UK_C = 0x0100,
    UK_B = 0x0200,
    UK_A = 0x4000,
    UK_Z = 0x8000
  };
  ~UmidoKey() override {
    if (umido_key_device_ > 0) {
      close(umido_key_device_);
    }
  }

  void initialize() override;
  void terminate() override;
  void readInput() override;

  uint16_t getP1Keys() override { return (p1_p2_keys_ & 0x0000'ffff) >> 0x00; }
  uint16_t getP2Keys() override { return (p1_p2_keys_ & 0xffff'0000) >> 0x10; }

  InputDevice::KeyMapping getKeyMapping(uint16_t key) override;

 private:
  int umido_key_device_{-1};
  uint32_t p1_p2_keys_{0x0000'0000};
};

#ifdef USE_SDL
class SDLKey : public InputDevice {
 public:
  enum class KeyMapping {
    SDL_RIGHT = 0x0001,
    SDL_LEFT = 0x0002,
    SDL_UP = 0x0004,
    SDL_DOWN = 0x0008,
    SDL_START = 0x0020,
    SDL_X = 0x0040,
    SDL_Y = 0x0080,
    SDL_C = 0x0100,
    SDL_B = 0x0200,
    SDL_A = 0x4000,
    SDL_Z = 0x8000
  };

  static void updateSDLKey(SDL_KeyboardEvent key_event);

  void initialize() override {}
  void terminate() override {}
  void readInput() override {}

  uint16_t getP1Keys() override { return keys_[0]; }
  uint16_t getP2Keys() override { return keys_[1]; }
  InputDevice::KeyMapping getKeyMapping(uint16_t key) override;

 private:
  static uint16_t keys_[4];
};
#endif
}

#endif//EMULATOR_INPUT_IPP_
