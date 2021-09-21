/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qquickimagecapture_p.h"
#include "qquickimagepreviewprovider_p.h"

#include <QtCore/qurl.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype ImageCapture
    \instantiates QQuickImageCapture
    \brief An interface for capturing camera images.
    \ingroup multimedia_qml
    \inqmlmodule QtMultimedia
    \ingroup camera_qml

    This type allows you to capture still images and be notified when they
    are available or saved to disk.

    \qml
    Item {
        width: 640
        height: 360

        CaptureSession {
            imageCapture : ImageCapture {
                id: imageCapture
            }
            camera: Camera {
                id: camera
            }

            videoOutput: videoOutput
        }
        VideoOutput {
            id: videoOutput
            anchors.fill: parent

            MouseArea {
                anchors.fill: parent;
                onClicked: imageCapture.capture();
            }
        }

        Image {
            id: photoPreview
            source: imageCapture.preview // always shows the last captured image
        }
    }
    \endqml

*/

QQuickImageCapture::QQuickImageCapture(QObject *parent)
    : QImageCapture(parent)
{
    connect(this, SIGNAL(imageCaptured(int,QImage)), this, SLOT(_q_imageCaptured(int,QImage)));
}

QQuickImageCapture::~QQuickImageCapture() = default;

/*!
    \qmlproperty bool QtMultimedia::ImageCapture::readyForCapture

    This property holds a bool value indicating whether the camera
    is ready to capture photos or not.

    Calling capture() or captureToFile() while \e ready is \c false is not permitted and
    results in an error.
*/

/*!
    \qmlproperty bool QtMultimedia::ImageCapture::preview

    This property holds a url to the latest captured image. It can be connected to the
    source property of an \l Image element to show the last captured image.

    \qml
    CaptureSession {
        camera: Camera {}
        imageCapture: ImageCapture {
            id: capture
        }
    }
    Image {
        source: capture.preview
    }
    \endqml

    \sa saveToFile
*/

/*!
    \qmlmethod QtMultimedia::ImageCapture::capture()

    Start image capture.  The \l imageCaptured and \l imageSaved signals will
    be emitted when the capture is complete.

    The captured image will be available through the preview property that can be
    used as the source for a QML Image item. The saveToFile() method can then be used
    save the image.

    Camera saves all the capture parameters like exposure settings or
    image processing parameters, so changes to camera parameters after
    capture() is called do not affect previous capture requests.

    capture() returns the capture requestId parameter, used with
    imageExposed(), imageCaptured(), imageMetadataAvailable() and imageSaved() signals.

    \sa ready, preview
*/

/*!
    \qmlmethod QtMultimedia::ImageCapture::captureToFile(location)

    Does the same as capture() but additionally automatically saves the captured image to the specified
    \a location.

    \sa capture
*/

QString QQuickImageCapture::preview() const
{
    return m_capturedImagePath;
}

/*!
    \qmlmethod QtMultimedia::ImageCapture::saveToFile(location)

    Saves the last captured image to \a location.

    \sa capture, preview
*/
void QQuickImageCapture::saveToFile(const QUrl &location) const
{
    m_lastImage.save(location.toLocalFile());
}

void QQuickImageCapture::_q_imageCaptured(int id, const QImage &preview)
{
    QString previewId = QString::fromLatin1("preview_%1").arg(id);
    QQuickImagePreviewProvider::registerPreview(previewId, preview);
    m_lastImage = preview;
    emit previewChanged();
}

/*!
    \qmlsignal QtMultimedia::ImageCapture::errorOccurred(requestId, Error, message)

    This signal is emitted when an error occurs during capture with \a requestId.
    A descriptive message is available in \a message.
*/

/*!
    \qmlsignal QtMultimedia::ImageCapture::imageCaptured(requestId, previewImage)

    This signal is emitted when an image with \a requestId has been captured
    but not yet saved to the filesystem.  The \a previewImage
    parameter is the captured image.

    \sa imageSaved, preview
*/

/*!
    \qmlsignal QtMultimedia::ImageCapture::imageSaved(requestId, path)

    This signal is emitted after the image with \a requestId has been written to the filesystem.
    The \a path is a local file path, not a URL.

    \sa imageCaptured
*/


/*!
    \qmlsignal QtMultimedia::ImageCapture::imageMetadataAvailable(requestId, key, value)

    This signal is emitted when the image with \a requestId has new metadata
    available with the key \a key and value \a value.

    \sa imageCaptured
*/


QT_END_NAMESPACE

#include "moc_qquickimagecapture_p.cpp"
