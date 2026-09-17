#include "qtkeymanager.h"

#include "Core/Shared/KeyDefinitions.h"

#include <QEvent>
#include <QKeyEvent>
#include <Qt>

QtKeyManager::QtKeyManager(QObject &eventSource)
{
    for (const KeyDefinition &definition : KeyDefinition::GetSharedKeyDefinitions()) {
        _keyNames[definition.keyCode] = definition.name;
        _keyCodes[definition.name] = definition.keyCode;
    }

    for (int offset = 0; offset < 26; ++offset) {
        const char name[] = {static_cast<char>('A' + offset), '\0'};
        addKey(Qt::Key_A + offset, name);
    }
    for (int offset = 0; offset < 10; ++offset) {
        const char name[] = {static_cast<char>('0' + offset), '\0'};
        addKey(Qt::Key_0 + offset, name);
    }
    addKey(Qt::Key_Enter, "Enter");
    addKey(Qt::Key_Return, "Enter");
    addKey(Qt::Key_Shift, "Right Shift");
    addKey(Qt::Key_Left, "Left Arrow");
    addKey(Qt::Key_Up, "Up Arrow");
    addKey(Qt::Key_Right, "Right Arrow");
    addKey(Qt::Key_Down, "Down Arrow");
    addKey(Qt::Key_Space, "Space");
    addKey(Qt::Key_Escape, "Esc");

    ResetKeyState();
    eventSource.installEventFilter(this);
}

void QtKeyManager::RefreshState()
{
}

void QtKeyManager::UpdateDevices()
{
}

bool QtKeyManager::IsMouseButtonPressed(MouseButton button)
{
    return IsKeyPressed(BaseMouseButtonIndex + static_cast<uint16_t>(button));
}

bool QtKeyManager::IsKeyPressed(uint16_t keyCode)
{
    return !_disabled && keyCode < _keyState.size() && _keyState[keyCode];
}

vector<uint16_t> QtKeyManager::GetPressedKeys()
{
    vector<uint16_t> pressedKeys;
    for (uint16_t keyCode = 1; keyCode < _keyState.size(); ++keyCode) {
        if (IsKeyPressed(keyCode)) {
            pressedKeys.push_back(keyCode);
        }
    }
    return pressedKeys;
}

string QtKeyManager::GetKeyName(uint16_t keyCode)
{
    auto name = _keyNames.find(keyCode);
    return name == _keyNames.end() ? string() : name->second;
}

uint16_t QtKeyManager::GetKeyCode(string keyName)
{
    auto code = _keyCodes.find(keyName);
    return code == _keyCodes.end() ? 0 : code->second;
}

bool QtKeyManager::SetKeyState(uint16_t scanCode, bool state)
{
    if (scanCode >= _keyState.size()) {
        return false;
    }
    return _keyState[scanCode].exchange(state) != state;
}

void QtKeyManager::ResetKeyState()
{
    for (std::atomic_bool &state : _keyState) {
        state = false;
    }
}

void QtKeyManager::SetDisabled(bool disabled)
{
    _disabled = disabled;
    if (disabled) {
        ResetKeyState();
    }
}

bool QtKeyManager::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        const auto *keyEvent = static_cast<QKeyEvent *>(event);
        auto coreKey = _qtToCore.find(keyEvent->key());
        if (coreKey != _qtToCore.end()) {
            SetKeyState(coreKey->second, event->type() == QEvent::KeyPress);
        }
    } else if (event->type() == QEvent::FocusOut) {
        ResetKeyState();
    }
    return QObject::eventFilter(watched, event);
}

void QtKeyManager::addKey(int qtKey, const char *coreName)
{
    const uint16_t coreKey = GetKeyCode(coreName);
    if (coreKey != 0) {
        _qtToCore[qtKey] = coreKey;
    }
}