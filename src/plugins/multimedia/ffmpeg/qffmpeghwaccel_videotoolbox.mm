/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qffmpeghwaccel_videotoolbox_p.h"

#if !defined(Q_OS_DARWIN)
#error "Configuration error"
#endif

#include <qvideoframeformat.h>
#include <qffmpegvideobuffer_p.h>
#include "private/qvideotexturehelper_p.h"

#include <private/qrhi_p.h>
#include <private/qrhimetal_p.h>
#include <private/qrhigles2_p.h>

#include <CoreVideo/CVMetalTexture.h>
#include <CoreVideo/CVMetalTextureCache.h>

#include <qopenglcontext.h>

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>

namespace QFFmpeg
{

static CVMetalTextureCacheRef &mtc(void *&cache) { return reinterpret_cast<CVMetalTextureCacheRef &>(cache); }

class VideoToolBoxTextureSet : public TextureSet
{
public:
    ~VideoToolBoxTextureSet();
    qint64 texture(int plane) override;

    QRhi *rhi = nullptr;
    CVMetalTextureRef cvMetalTexture[3] = {};

#if defined(Q_OS_MACOS)
    CVOpenGLTextureRef cvOpenGLTexture = nullptr;
#elif defined(Q_OS_IOS)
    CVOpenGLESTextureRef cvOpenGLESTexture = nullptr;
#endif

    CVImageBufferRef m_buffer = nullptr;
};

VideoToolBoxAccel::VideoToolBoxAccel(AVBufferRef *hwContext)
    : HWAccelBackend(hwContext)
{
}

VideoToolBoxAccel::~VideoToolBoxAccel()
{
    freeTextureCaches();
}

void VideoToolBoxAccel::freeTextureCaches()
{
    if (cvMetalTextureCache)
        CFRelease(cvMetalTextureCache);
    cvMetalTextureCache = nullptr;
#if defined(Q_OS_MACOS)
    if (cvOpenGLTextureCache)
        CFRelease(cvOpenGLTextureCache);
    cvOpenGLTextureCache = nullptr;
#elif defined(Q_OS_IOS)
    if (cvOpenGLESTextureCache)
        CFRelease(cvOpenGLESTextureCache);
    cvOpenGLESTextureCache = nullptr;
#endif
}

void VideoToolBoxAccel::setRhi(QRhi *_rhi)
{
    if (rhi == _rhi)
        return;
    qDebug() << "VideoToolBoxAccel::setRhi" << _rhi;
    freeTextureCaches();
    rhi = _rhi;

    if (!rhi)
        return;
    if (rhi->backend() == QRhi::Metal) {
        qDebug() << "    using metal backend";
        const auto *metal = static_cast<const QRhiMetalNativeHandles *>(rhi->nativeHandles());

        // Create a Metal Core Video texture cache from the pixel buffer.
        Q_ASSERT(!cvMetalTextureCache);
        if (CVMetalTextureCacheCreate(
                        kCFAllocatorDefault,
                        nil,
                        (id<MTLDevice>)metal->dev,
                        nil,
                        &mtc(cvMetalTextureCache)) != kCVReturnSuccess) {
            qWarning() << "Metal texture cache creation failed";
            rhi = nullptr;
        }
    } else if (rhi->backend() == QRhi::OpenGLES2) {
#if QT_CONFIG(opengl)
#ifdef Q_OS_MACOS
        const auto *gl = static_cast<const QRhiGles2NativeHandles *>(rhi->nativeHandles());

        auto nsGLContext = gl->context->nativeInterface<QNativeInterface::QCocoaGLContext>()->nativeContext();
        auto nsGLPixelFormat = nsGLContext.pixelFormat.CGLPixelFormatObj;

        // Create an OpenGL CoreVideo texture cache from the pixel buffer.
        if (CVOpenGLTextureCacheCreate(
                        kCFAllocatorDefault,
                        nullptr,
                        reinterpret_cast<CGLContextObj>(nsGLContext.CGLContextObj),
                        nsGLPixelFormat,
                        nil,
                        &cvOpenGLTextureCache)) {
            qWarning() << "OpenGL texture cache creation failed";
            rhi = nullptr;
        }
#endif
#ifdef Q_OS_IOS
        // Create an OpenGL CoreVideo texture cache from the pixel buffer.
        if (CVOpenGLESTextureCacheCreate(
                        kCFAllocatorDefault,
                        nullptr,
                        [EAGLContext currentContext],
                        nullptr,
                        &cvOpenGLESTextureCache)) {
            qWarning() << "OpenGL texture cache creation failed";
            rhi = nullptr;
        }
#endif
#else
        rhi = nullptr;
#endif // QT_CONFIG(opengl)
    }

}


static MTLPixelFormat rhiTextureFormatToMetalFormat(QRhiTexture::Format f)
{
    switch (f) {
    default:
    case QRhiTexture::UnknownFormat:
        return MTLPixelFormatInvalid;
    case QRhiTexture::RGBA8:
        return MTLPixelFormatRGBA8Unorm;
    case QRhiTexture::BGRA8:
        return MTLPixelFormatBGRA8Unorm;
    case QRhiTexture::R8:
        return MTLPixelFormatR8Unorm;
    case QRhiTexture::RG8:
        return MTLPixelFormatRG8Unorm;
    case QRhiTexture::R16:
        return MTLPixelFormatR16Unorm;
    case QRhiTexture::RG16:
        return MTLPixelFormatRG16Unorm;

    case QRhiTexture::RGBA16F:
        return MTLPixelFormatRGBA16Float;
    case QRhiTexture::RGBA32F:
        return MTLPixelFormatRGBA32Float;
    case QRhiTexture::R16F:
        return MTLPixelFormatR16Float;
    case QRhiTexture::R32F:
        return MTLPixelFormatR32Float;
    }
}

TextureSet *VideoToolBoxAccel::getTextures(AVFrame *frame)
{
    if (!rhi)
        return nullptr;

    bool needsConversion = false;
    QVideoFrameFormat::PixelFormat pixelFormat = QFFmpegVideoBuffer::toQtPixelFormat(format(frame), &needsConversion);
    if (needsConversion) {
        qDebug() << "XXXXXXXXXXXX pixel format needs conversion" << pixelFormat << format(frame);
        return nullptr;
    }

    CVPixelBufferRef buffer = (CVPixelBufferRef)frame->data[3];

    VideoToolBoxTextureSet *textureSet = new VideoToolBoxTextureSet;
    textureSet->m_buffer = buffer;
    textureSet->rhi = rhi;
    CVPixelBufferRetain(buffer);

    auto *textureDescription = QVideoTextureHelper::textureDescription(pixelFormat);
    int bufferPlanes = CVPixelBufferGetPlaneCount(buffer);
//    qDebug() << "XXXXX getTextures" << pixelFormat << bufferPlanes << buffer;

    if (rhi->backend() == QRhi::Metal) {
        for (int plane = 0; plane < bufferPlanes; ++plane) {
            size_t width = CVPixelBufferGetWidth(buffer);
            size_t height = CVPixelBufferGetHeight(buffer);
            width = textureDescription->widthForPlane(width, plane);
            height = textureDescription->heightForPlane(height, plane);

            // Create a CoreVideo pixel buffer backed Metal texture image from the texture cache.
            auto ret = CVMetalTextureCacheCreateTextureFromImage(
                            kCFAllocatorDefault,
                            mtc(cvMetalTextureCache),
                            buffer, nil,
                            rhiTextureFormatToMetalFormat(textureDescription->textureFormat[plane]),
                            width, height,
                            plane,
                            &textureSet->cvMetalTexture[plane]);

            if (ret != kCVReturnSuccess)
                qWarning() << "texture creation failed" << ret;
//            auto t = CVMetalTextureGetTexture(textureSet->cvMetalTexture[plane]);
//            qDebug() << "    metal texture for plane" << plane << "is" << quint64(textureSet->cvMetalTexture[plane]) << width << height;
//            qDebug() << "    " << t.iosurfacePlane << t.pixelFormat << t.width << t.height;
        }
    } else if (rhi->backend() == QRhi::OpenGLES2) {
#if QT_CONFIG(opengl)
#ifdef Q_OS_MACOS
        CVOpenGLTextureCacheFlush(cvOpenGLTextureCache, 0);
        CVReturn cvret;
        // Create a CVPixelBuffer-backed OpenGL texture image from the texture cache.
        cvret = CVOpenGLTextureCacheCreateTextureFromImage(
                        kCFAllocatorDefault,
                        cvOpenGLTextureCache,
                        buffer,
                        nil,
                        &textureSet->cvOpenGLTexture);

        Q_ASSERT(CVOpenGLTextureGetTarget(textureSet->cvOpenGLTexture) == GL_TEXTURE_RECTANGLE);
#endif
#ifdef Q_OS_IOS
        CVOpenGLESTextureCacheFlush(cvOpenGLESTextureCache, 0);
        CVReturn cvret;
        // Create a CVPixelBuffer-backed OpenGL texture image from the texture cache.
        cvret = CVOpenGLESTextureCacheCreateTextureFromImage(
                        kCFAllocatorDefault,
                        cvOpenGLESTextureCache,
                        buffer,
                        nil,
                        GL_TEXTURE_2D,
                        GL_RGBA,
                        CVPixelBufferGetWidth(buffer),
                        CVPixelBufferGetHeight(buffer),
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        0,
                        &textureSet->cvOpenGLESTexture);
#endif
#endif
    }

    return textureSet;
}

AVPixelFormat VideoToolBoxAccel::format(AVFrame *frame) const
{
    if (!rhi)
        return AVPixelFormat(frame->format);
    auto *hwFramesContext = (AVHWFramesContext *)frame->hw_frames_ctx->data;
    return AVPixelFormat(hwFramesContext->sw_format);
}

VideoToolBoxTextureSet::~VideoToolBoxTextureSet()
{
    for (int i = 0; i < 4; ++i)
        if (cvMetalTexture[i])
            CFRelease(cvMetalTexture[i]);
#if defined(Q_OS_MACOS)
    if (cvOpenGLTexture)
        CVOpenGLTextureRelease(cvOpenGLTexture);
#elif defined(Q_OS_IOS)
    if (cvOpenGLESTexture)
        CFRelease(cvOpenGLESTexture);
#endif
    CVPixelBufferRelease(m_buffer);
}

qint64 VideoToolBoxTextureSet::texture(int plane)
{
    if (rhi->backend() == QRhi::Metal)
        return cvMetalTexture[plane] ? qint64(CVMetalTextureGetTexture(cvMetalTexture[plane])) : 0;
#if QT_CONFIG(opengl)
    Q_ASSERT(plane == 0);
#ifdef Q_OS_MACOS
    return CVOpenGLTextureGetName(cvOpenGLTexture);
#endif
#ifdef Q_OS_IOS
    return CVOpenGLESTextureGetName(cvOpenGLESTexture);
#endif
#endif
}

}

QT_END_NAMESPACE
