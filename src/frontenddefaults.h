#pragma once

class EmuSettings;
class IKeyManager;

class FrontendDefaults final
{
public:
    static void apply(EmuSettings &settings, IKeyManager &keyManager);
};