#include "emulator.h"
#include "emulator.hpp"
#include "network/ggpo.h"
#include <sys/time.h>

namespace sen {

void Emulator::inputThread()
{
	constexpr long FRAME_TIME_NS = 16666666;
	constexpr double expFactor = 0.05;
	timeval lastTime{};
	double delta = 0.0;

	input_thread_run = true;
#ifdef USE_SDL
	input_device_ = std::make_unique<SDLKey>();
#else
	input_device_ = std::make_unique<UmidoKey>();
#endif
	input_device_->initialize();

	while (input_thread_run)
	{
#ifdef USE_SDL
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
		  switch (event.type) {
			case SDL_QUIT:
			  flycast::quit();
			  os_UninstallFaultHandler();
			  SDL_Quit();
			  return;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
			  if (event.key.repeat == 0) {
				if (event.key.type == SDL_KEYDOWN) {
				  switch (event.key.keysym.scancode) {
					case SDL_SCANCODE_F1:
					  ggpo::setOption("PauseInput", "true");
					  break;
					case SDL_SCANCODE_F2:
					  ggpo::setOption("PauseInput", "false");
					  break;
					case SDL_SCANCODE_F5:
					  sen::watch.initialize();
					  break;
					case SDL_SCANCODE_F6:
					  sen::watch.updateChanged();
					  break;
					case SDL_SCANCODE_F7:
					  sen::watch.updateUnchanged();
					  break;
					case SDL_SCANCODE_F8:
					  sen::watch.print();
					  break;
					case SDL_SCANCODE_F9:
					  WriteMem32(0x0c2d7608, 0x0001);
					  WriteMem32(0x0c2d8150, 0x0001);
					  WriteMem32(0x0c2d8c98, 0x0001);
					  ZGS_LOG("%s", "p1 lose");
					  break;
					case SDL_SCANCODE_F10:
					  WriteMem32(0x0c2d7bac, 0x0001);
					  WriteMem32(0x0c2d86f4, 0x0001);
					  WriteMem32(0x0c2d923c, 0x0001);
					  ZGS_LOG("%s", "p2 lose");
					  break;
					case SDL_SCANCODE_P:
					  sen::emulator.transferPause();
					  break;
					case SDL_SCANCODE_TAB:
					  {
						// Cable Air Hyper Viper Beam combo
						std::lock_guard<std::mutex> lock(input_mutex);
						inputs[0].push_back((uint16_t)SDLKey::KeyMapping::SDL_UP);
						inputs[0].push_back((uint16_t)SDLKey::KeyMapping::SDL_DOWN);
						inputs[0].push_back((uint16_t)SDLKey::KeyMapping::SDL_DOWN | (uint16_t)SDLKey::KeyMapping::SDL_RIGHT);
						inputs[0].push_back((uint16_t)SDLKey::KeyMapping::SDL_RIGHT);
						inputs[0].push_back((uint16_t)SDLKey::KeyMapping::SDL_A | (uint16_t)SDLKey::KeyMapping::SDL_B);
					  }
					  break;
					default:
					  break;
				  }
				}
				sen::SDLKey::updateSDLKey(event.key);
			  }
			  break;
		  }
		}
#else
		input_device_->readInput();
#endif

		uint16_t keys1 = input_device_->getP1Keys();
		uint16_t keys2 = input_device_->getP2Keys();
		{
			std::lock_guard<std::mutex> lock(input_mutex);
			inputs[0].push_back(keys1);
			inputs[1].push_back(keys2);
		}

		long wait = FRAME_TIME_NS - (long)delta;
		if (wait > 0)
		{
			timespec ts { 0, wait };
			timespec rem;
			nanosleep(&ts, &rem);
		}

		timeval tv;
		gettimeofday(&tv, nullptr);
		if (lastTime.tv_sec != 0)
		{
			timeval diff;
			timersub(&tv, &lastTime, &diff);
			long lastDelta = diff.tv_sec * 1000000 + diff.tv_usec;
			lastDelta = (lastDelta * 1000) - FRAME_TIME_NS;
			delta = expFactor * lastDelta + (1.0 - expFactor) * delta;
		}
		lastTime = tv;
	}
	// terminate the input device
	input_device_->terminate();
	input_device_.reset();
}

uint16_t Emulator::getKeys(int player)
{
	std::lock_guard<std::mutex> lock(input_mutex);
	std::deque<uint16_t>& input = inputs[player];

	if (input.empty())
		return lastInputs[player];
	uint16_t keys = input.front();
	input.pop_front();
	while (!input.empty() && keys == input.front())
		input.pop_front();
	lastInputs[player] = keys;

	return keys;
}

}
