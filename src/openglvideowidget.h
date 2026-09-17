#pragma once

#include "Core/Shared/Interfaces/IRenderingDevice.h"

#include <QOpenGLWidget>
#include <QSize>

#include <cstdint>
#include <mutex>
#include <vector>

class Emulator;

class OpenGLVideoWidget final : public QOpenGLWidget
{
public:
    explicit OpenGLVideoWidget(QWidget *parent = nullptr);

    void submitFrame(const uint32_t *pixels, uint32_t width, uint32_t height, double aspectRatio, bool bilinearInterpolation);
    void submitHud(const uint32_t *emuHud, uint32_t emuHudWidth, uint32_t emuHudHeight,
                   const uint32_t *scriptHud, uint32_t scriptHudWidth, uint32_t scriptHudHeight);
    void clearFrame();
    QSize frameSize() const;

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    mutable std::mutex _frameMutex;
    std::vector<uint32_t> _framePixels;
    std::vector<uint32_t> _emuHudPixels;
    std::vector<uint32_t> _scriptHudPixels;
    uint32_t _frameWidth = 0;
    uint32_t _frameHeight = 0;
    uint32_t _emuHudWidth = 0;
    uint32_t _emuHudHeight = 0;
    uint32_t _scriptHudWidth = 0;
    uint32_t _scriptHudHeight = 0;
    double _aspectRatio = 1.0;
    bool _bilinearInterpolation = false;
};

class QtRenderingDevice final : public IRenderingDevice
{
public:
    QtRenderingDevice(Emulator &emulator, OpenGLVideoWidget &videoWidget);
    ~QtRenderingDevice() override;

    void UpdateFrame(RenderedFrame &frame) override;
    void ClearFrame() override;
    void Render(RenderSurfaceInfo &emuHud, RenderSurfaceInfo &scriptHud) override;
    void Reset() override;
    void SetFullscreenMode(FullscreenSettings settings) override;

private:
    Emulator &_emulator;
    OpenGLVideoWidget &_videoWidget;
};