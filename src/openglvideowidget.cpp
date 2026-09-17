#include "openglvideowidget.h"

#include "Core/Shared/EmuSettings.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/RenderedFrame.h"
#include "Core/Shared/Video/VideoRenderer.h"

#include <QImage>
#include <QMetaObject>
#include <QOpenGLFunctions>
#include <QPainter>

#include <algorithm>
#include <cstring>

OpenGLVideoWidget::OpenGLVideoWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
}

void OpenGLVideoWidget::submitFrame(const uint32_t *pixels, uint32_t width, uint32_t height, double aspectRatio, bool bilinearInterpolation)
{
    if (!pixels || width == 0 || height == 0) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(_frameMutex);
        _frameWidth = width;
        _frameHeight = height;
        _aspectRatio = aspectRatio;
        _bilinearInterpolation = bilinearInterpolation;
        _framePixels.resize(static_cast<size_t>(width) * height);
        std::memcpy(_framePixels.data(), pixels, _framePixels.size() * sizeof(uint32_t));
    }

    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

void OpenGLVideoWidget::submitHud(const uint32_t *emuHud, uint32_t emuHudWidth, uint32_t emuHudHeight,
                                  const uint32_t *scriptHud, uint32_t scriptHudWidth, uint32_t scriptHudHeight)
{
    {
        std::lock_guard<std::mutex> lock(_frameMutex);
        _emuHudWidth = emuHudWidth;
        _emuHudHeight = emuHudHeight;
        _emuHudPixels.assign(emuHud, emuHud + static_cast<size_t>(emuHudWidth) * emuHudHeight);
        _scriptHudWidth = scriptHudWidth;
        _scriptHudHeight = scriptHudHeight;
        _scriptHudPixels.assign(scriptHud, scriptHud + static_cast<size_t>(scriptHudWidth) * scriptHudHeight);
    }
    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

QSize OpenGLVideoWidget::frameSize() const
{
    std::lock_guard<std::mutex> lock(_frameMutex);
    return QSize(static_cast<int>(_frameWidth), static_cast<int>(_frameHeight));
}

void OpenGLVideoWidget::clearFrame()
{
    {
        std::lock_guard<std::mutex> lock(_frameMutex);
        std::fill(_framePixels.begin(), _framePixels.end(), 0xFF000000);
    }
    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

void OpenGLVideoWidget::initializeGL()
{
    QOpenGLFunctions *functions = context()->functions();
    functions->glDisable(GL_DEPTH_TEST);
    functions->glClearColor(0.025F, 0.025F, 0.025F, 1.0F);
}

void OpenGLVideoWidget::paintGL()
{
    context()->functions()->glClear(GL_COLOR_BUFFER_BIT);

    std::lock_guard<std::mutex> lock(_frameMutex);
    if (_framePixels.empty()) {
        return;
    }

    const QImage frame(
        reinterpret_cast<const uchar *>(_framePixels.data()),
        static_cast<int>(_frameWidth),
        static_cast<int>(_frameHeight),
        static_cast<int>(_frameWidth * sizeof(uint32_t)),
        QImage::Format_ARGB32);

    const double targetRatio = _aspectRatio > 0 ? _aspectRatio : static_cast<double>(_frameWidth) / _frameHeight;
    QSize scaledSize = size();
    if (static_cast<double>(scaledSize.width()) / scaledSize.height() > targetRatio) {
        scaledSize.setWidth(static_cast<int>(scaledSize.height() * targetRatio));
    } else {
        scaledSize.setHeight(static_cast<int>(scaledSize.width() / targetRatio));
    }
    const QRect targetRect(QPoint((width() - scaledSize.width()) / 2, (height() - scaledSize.height()) / 2), scaledSize);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, _bilinearInterpolation);
    painter.drawImage(targetRect, frame);

    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    if (!_emuHudPixels.empty()) {
        const QImage emuHud(
            reinterpret_cast<const uchar *>(_emuHudPixels.data()),
            static_cast<int>(_emuHudWidth),
            static_cast<int>(_emuHudHeight),
            static_cast<int>(_emuHudWidth * sizeof(uint32_t)),
            QImage::Format_ARGB32_Premultiplied);
        painter.drawImage(targetRect, emuHud);
    }
    if (!_scriptHudPixels.empty()) {
        const QImage scriptHud(
            reinterpret_cast<const uchar *>(_scriptHudPixels.data()),
            static_cast<int>(_scriptHudWidth),
            static_cast<int>(_scriptHudHeight),
            static_cast<int>(_scriptHudWidth * sizeof(uint32_t)),
            QImage::Format_ARGB32_Premultiplied);
        painter.drawImage(targetRect, scriptHud);
    }
}

QtRenderingDevice::QtRenderingDevice(Emulator &emulator, OpenGLVideoWidget &videoWidget)
    : _emulator(emulator)
    , _videoWidget(videoWidget)
{
    _emulator.GetVideoRenderer()->RegisterRenderingDevice(this);
}

QtRenderingDevice::~QtRenderingDevice()
{
    _emulator.GetVideoRenderer()->UnregisterRenderingDevice(this);
}

void QtRenderingDevice::UpdateFrame(RenderedFrame &frame)
{
    const VideoConfig &video = _emulator.GetSettings()->GetVideoConfig();
    const FrameInfo frameSize = {frame.Width, frame.Height};
    _videoWidget.submitFrame(
        static_cast<const uint32_t *>(frame.FrameBuffer),
        frame.Width,
        frame.Height,
        _emulator.GetSettings()->GetAspectRatio(_emulator.GetRegion(), frameSize),
        video.UseBilinearInterpolation);
}

void QtRenderingDevice::ClearFrame()
{
    _videoWidget.clearFrame();
}

void QtRenderingDevice::Render(RenderSurfaceInfo &emuHud, RenderSurfaceInfo &scriptHud)
{
    _videoWidget.submitHud(
        emuHud.Buffer, emuHud.Width, emuHud.Height,
        scriptHud.Buffer, scriptHud.Width, scriptHud.Height);
}

void QtRenderingDevice::Reset()
{
}

void QtRenderingDevice::SetFullscreenMode(FullscreenSettings)
{
}