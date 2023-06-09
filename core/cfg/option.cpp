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
#include "option.h"
#include "network/naomi_network.h"

namespace config {
bool LimitFPS = false;

// Dynarec
Option<bool> DynarecEnabled("Dynarec.Enabled", true);
Option<bool> DynarecIdleSkip("Dynarec.idleskip", true);

// General

Option<int> Cable("Dreamcast.Cable", 3);			// TV Composite
Option<int> Region("Dreamcast.Region", 1);			// USA
Option<int> Broadcast("Dreamcast.Broadcast", 0);	// NTSC
Option<int> Language("Dreamcast.Language", 1);		// English
Option<bool> FullMMU("Dreamcast.FullMMU");
Option<bool> ForceWindowsCE("Dreamcast.ForceWindowsCE");
Option<bool> ForceFreePlay("ForceFreePlay", true);

// Sound

Option<bool> DSPEnabled("aica.DSPEnabled", false);
bool UseLowPass = false;
#if HOST_CPU == CPU_ARM
Option<int> AudioBufferSize("aica.BufferSize", 5644);	// 128 ms
#else
Option<int> AudioBufferSize("aica.BufferSize", 2822);	// 64 ms
#endif

OptionString AudioBackend("backend", "auto", "audio");
AudioVolumeOption AudioVolume;

// Rendering
RendererOption RendererType;
Option<bool> UseMipmaps("rend.UseMipmaps", true);
Option<bool> Widescreen("rend.WideScreen");
Option<bool> SuperWidescreen("rend.SuperWideScreen");
Option<bool> RenderToTextureBuffer("rend.RenderToTextureBuffer");
Option<bool> TranslucentPolygonDepthMask("rend.TranslucentPolygonDepthMask");
Option<bool> ModifierVolumes("rend.ModifierVolumes", false);
Option<int> TextureUpscale("rend.TextureUpscale", 1);
Option<int> MaxFilteredTextureSize("rend.MaxFilteredTextureSize", 256);
Option<float> ExtraDepthScale("rend.ExtraDepthScale", 1.f);
Option<bool> CustomTextures("rend.CustomTextures");
Option<bool> DumpTextures("rend.DumpTextures");
Option<int> ScreenStretching("rend.ScreenStretching", 100);
// Option<bool> Fog("rend.Fog", false);
bool Fog = false;
Option<bool> Rotate90("rend.Rotate90");
Option<bool> PerStripSorting("rend.PerStripSorting", true);
Option<bool> DelayFrameSwapping("rend.DelayFrameSwapping", false);
Option<bool> WidescreenGameHacks("rend.WidescreenGameHacks");
bool SkipAllFrame = false;
int SkipFrame = 0;
Option<int> MaxThreads("pvr.MaxThreads", 5);
int AutoSkipFrame = 0;
Option<int> RenderResolution("rend.Resolution", 480);
// Option<bool> VSync("rend.vsync", true);
bool VSync = true;
Option<int64_t> PixelBufferSize("rend.PixelBufferSize", 512 * 1024 * 1024);
Option<int> AnisotropicFiltering("rend.AnisotropicFiltering", 1);
Option<int> TextureFiltering("rend.TextureFiltering", 0); // Default
// Option<bool> ThreadedRendering("rend.ThreadedRendering", true);
Option<int> PerPixelLayers("rend.PerPixelLayers", 32);
Option<bool> NativeDepthInterpolation("rend.NativeDepthInterpolation", false);

// Misc

Option<bool> SerialConsole("Debug.SerialConsoleEnabled");
Option<bool> SerialPTY("Debug.SerialPTY");
Option<bool> UseReios("UseReios");
Option<bool> FastGDRomLoad("FastGDRomLoad", false);

Option<bool> OpenGlChecks("OpenGlChecks", false, "validate");

Option<std::vector<std::string>, false> ContentPath("Dreamcast.ContentPath");
Option<bool, false> HideLegacyNaomiRoms("Dreamcast.HideLegacyNaomiRoms", true);

// Network

Option<bool> NetworkEnable("Enable", false, "network");
Option<bool> ActAsServer("ActAsServer", false, "network");
OptionString DNS("DNS", "46.101.91.123", "network");
OptionString NetworkServer("server", "", "network");
Option<int> LocalPort("LocalPort", NaomiNetwork::SERVER_PORT, "network");
Option<bool> EmulateBBA("EmulateBBA", false, "network");
Option<bool> EnableUPnP("EnableUPnP", true, "network");
// Option<bool> GGPOEnable("GGPO", false, "network");
// Option<int> GGPODelay("GGPODelay", 0, "network");
int GGPODelay = 3;

Option<int> GGPOAnalogAxes("GGPOAnalogAxes", 0, "network");

#ifdef USE_OMX
Option<int> OmxAudioLatency("audio_latency", 100, "omx");
Option<bool> OmxAudioHdmi("audio_hdmi", true, "omx");
#endif

// Maple

Option<bool> EnableDiagonalCorrection ("EnableDiagonalCorrection", true, "input");

Option<int> MouseSensitivity("MouseSensitivity", 100, "input");

std::array<Option<MapleDeviceType>, 4> MapleMainDevices {
	Option<MapleDeviceType>("device1", MDT_SegaController, "input"),
	Option<MapleDeviceType>("device2", MDT_None, "input"),
	Option<MapleDeviceType>("device3", MDT_None, "input"),
	Option<MapleDeviceType>("device4", MDT_None, "input"),
};
std::array<std::array<Option<MapleDeviceType>, 2>, 4> MapleExpansionDevices {
	Option<MapleDeviceType>("device1.1", MDT_SegaVMU, "input"),
	Option<MapleDeviceType>("device1.2", MDT_SegaVMU, "input"),

	Option<MapleDeviceType>("device2.1", MDT_None, "input"),
	Option<MapleDeviceType>("device2.2", MDT_None, "input"),

	Option<MapleDeviceType>("device3.1", MDT_None, "input"),
	Option<MapleDeviceType>("device3.2", MDT_None, "input"),

	Option<MapleDeviceType>("device4.1", MDT_None, "input"),
	Option<MapleDeviceType>("device4.2", MDT_None, "input"),
};
#ifdef _WIN32
Option<bool> UseRawInput("RawInput", false, "input");
#endif

#ifdef USE_LUA
OptionString LuaFileName("LuaFileName", "flycast.lua");
#endif

} // namespace config
