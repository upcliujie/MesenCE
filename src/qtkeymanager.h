#pragma once

#include "Core/Shared/Interfaces/IKeyManager.h"

#include <QObject>

#include <array>
#include <atomic>
#include <unordered_map>

class QtKeyManager final : public QObject, public IKeyManager
{
public:
    explicit QtKeyManager(QObject &eventSource);

    void RefreshState() override;
    void UpdateDevices() override;
    bool IsMouseButtonPressed(MouseButton button) override;
    bool IsKeyPressed(uint16_t keyCode) override;
    vector<uint16_t> GetPressedKeys() override;
    string GetKeyName(uint16_t keyCode) override;
    uint16_t GetKeyCode(string keyName) override;
    bool SetKeyState(uint16_t scanCode, bool state) override;
    void ResetKeyState() override;
    void SetDisabled(bool disabled) override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void addKey(int qtKey, const char *coreName);

    std::array<std::atomic_bool, BaseMouseButtonIndex + 5> _keyState;
    std::unordered_map<int, uint16_t> _qtToCore;
    std::unordered_map<uint16_t, string> _keyNames;
    std::unordered_map<string, uint16_t> _keyCodes;
    std::atomic_bool _disabled = false;
};