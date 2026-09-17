#include "frontenddefaults.h"

#include "Core/Shared/EmuSettings.h"
#include "Core/Shared/Interfaces/IKeyManager.h"

#include <algorithm>
#include <array>
#include <iterator>

namespace
{
constexpr std::array<uint32_t, 64> DefaultNesPalette = {
    0xFF666666,
    0xFF002A88,
    0xFF1412A7,
    0xFF3B00A4,
    0xFF5C007E,
    0xFF6E0040,
    0xFF6C0600,
    0xFF561D00,
    0xFF333500,
    0xFF0B4800,
    0xFF005200,
    0xFF004F08,
    0xFF00404D,
    0xFF000000,
    0xFF000000,
    0xFF000000,
    0xFFADADAD,
    0xFF155FD9,
    0xFF4240FF,
    0xFF7527FE,
    0xFFA01ACC,
    0xFFB71E7B,
    0xFFB53120,
    0xFF994E00,
    0xFF6B6D00,
    0xFF388700,
    0xFF0C9300,
    0xFF008F32,
    0xFF007C8D,
    0xFF000000,
    0xFF000000,
    0xFF000000,
    0xFFFFFEFF,
    0xFF64B0FF,
    0xFF9290FF,
    0xFFC676FF,
    0xFFF36AFF,
    0xFFFE6ECC,
    0xFFFE8170,
    0xFFEA9E22,
    0xFFBCBE00,
    0xFF88D800,
    0xFF5CE430,
    0xFF45E082,
    0xFF48CDDE,
    0xFF4F4F4F,
    0xFF000000,
    0xFF000000,
    0xFFFFFEFF,
    0xFFC0DFFF,
    0xFFD3D2FF,
    0xFFE8C8FF,
    0xFFFBC2FF,
    0xFFFEC4EA,
    0xFFFECCC5,
    0xFFF7D8A5,
    0xFFE4E594,
    0xFFCFEF96,
    0xFFBDF4AB,
    0xFFB3F3CC,
    0xFFB5EBF2,
    0xFFB8B8B8,
    0xFF000000,
    0xFF000000,
};

void configureDirections(KeyMapping &mapping, IKeyManager &keyManager)
{
    mapping.Up = keyManager.GetKeyCode("W");
    mapping.Down = keyManager.GetKeyCode("S");
    mapping.Left = keyManager.GetKeyCode("A");
    mapping.Right = keyManager.GetKeyCode("D");
}

void configureNes(NesConfig &nes, IKeyManager &keyManager)
{
    nes.Port1.Type = ControllerType::NesController;
    nes.Port1.Keys.TurboSpeed = 2;

    KeyMapping &keyboard = nes.Port1.Keys.Mapping1;
    keyboard.A = keyManager.GetKeyCode("K");
    keyboard.B = keyManager.GetKeyCode("J");
    keyboard.TurboA = keyManager.GetKeyCode("I");
    keyboard.TurboB = keyManager.GetKeyCode("U");
    keyboard.Start = keyManager.GetKeyCode("Enter");
    keyboard.Select = keyManager.GetKeyCode("Right Shift");
    configureDirections(keyboard, keyManager);

    std::fill(std::begin(nes.ChannelVolumes), std::end(nes.ChannelVolumes), 100);
    std::fill(std::begin(nes.UserPalette), std::end(nes.UserPalette), 0);
    std::copy(DefaultNesPalette.begin(), DefaultNesPalette.end(), std::begin(nes.UserPalette));
    nes.IsFullColorPalette = false;
}

void configureGba(GbaConfig &gba, IKeyManager &keyManager)
{
    gba.Controller.Type = ControllerType::GbaController;
    gba.Controller.Keys.TurboSpeed = 2;

    KeyMapping &keyboard = gba.Controller.Keys.Mapping1;
    keyboard.A = keyManager.GetKeyCode("K");
    keyboard.B = keyManager.GetKeyCode("J");
    keyboard.L = keyManager.GetKeyCode("U");
    keyboard.R = keyManager.GetKeyCode("I");
    keyboard.TurboA = keyManager.GetKeyCode("M");
    keyboard.TurboB = keyManager.GetKeyCode("N");
    keyboard.Start = keyManager.GetKeyCode("Enter");
    keyboard.Select = keyManager.GetKeyCode("Right Shift");
    configureDirections(keyboard, keyManager);

    gba.BlendFrames = true;
    gba.GbaAdjustColors = true;
    gba.RamPowerOnState = RamState::AllZeros;
    gba.ChannelAVol = 100;
    gba.ChannelBVol = 100;
    gba.Square1Vol = 100;
    gba.Square2Vol = 100;
    gba.NoiseVol = 100;
    gba.WaveVol = 100;
}
}

void FrontendDefaults::apply(EmuSettings &settings, IKeyManager &keyManager)
{
    AudioConfig audio = settings.GetAudioConfig();
    audio.EnableAudio = true;
    audio.MasterVolume = 100;
    audio.SampleRate = 48000;
    audio.AudioLatency = 60;
    settings.SetAudioConfig(audio);

    NesConfig nes = settings.GetNesConfig();
    configureNes(nes, keyManager);
    settings.SetNesConfig(nes);

    GbaConfig gba = settings.GetGbaConfig();
    configureGba(gba, keyManager);
    settings.SetGbaConfig(gba);
}